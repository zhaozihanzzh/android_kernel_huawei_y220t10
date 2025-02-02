/* drivers/misc/lowmemorykiller.c
 *
 * The lowmemorykiller driver lets user-space specify a set of memory thresholds
 * where processes with a range of oom_adj values will get killed. Specify the
 * minimum oom_adj values in /sys/module/lowmemorykiller/parameters/adj and the
 * number of free pages in /sys/module/lowmemorykiller/parameters/minfree. Both
 * files take a comma separated list of numbers in ascending order.
 *
 * For example, write "0,8" to /sys/module/lowmemorykiller/parameters/adj and
 * "1024,4096" to /sys/module/lowmemorykiller/parameters/minfree to kill processes
 * with a oom_adj value of 8 or higher when the free memory drops below 4096 pages
 * and kill processes with a oom_adj value of 0 or higher when the free memory
 * drops below 1024 pages.
 *
 * The driver considers memory used for caches to be free, but if a large
 * percentage of the cached memory is locked this can be very inaccurate
 * and processes may not get killed until the normal oom killer is triggered.
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/notifier.h>
#ifdef CONFIG_ZRAM_FOR_ANDROID
#include <linux/swap.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/mm_inline.h>
#endif /* CONFIG_ZRAM_FOR_ANDROID */

#ifdef CONFIG_ZRAM
#include <linux/fs.h>
#endif

#include <linux/swap.h>
#ifdef CONFIG_ANDROID_OOM_NOTIFY
extern int oom_notify_enable;
#endif
static uint32_t lowmem_debug_level = 2;

#ifdef CONFIG_ZRAM
#define LOWMEM_ADJ_ZRAM_LEVEL	9
static short lowmem_adj[LOWMEM_ADJ_ZRAM_LEVEL] = {
	0,
	1,
	2,
	4,
	6,
	8,
	9,
	12,
	15,
};
static int lowmem_adj_size = LOWMEM_ADJ_ZRAM_LEVEL;
static size_t lowmem_minfree[LOWMEM_ADJ_ZRAM_LEVEL] = {
	4 * 256,	/* 4MB */
	12 * 256,	/* 12MB */
	16 * 256,	/* 16MB */
	24 * 256,	/* 24MB */
	28 * 256,	/* 28MB */
	32 * 256,	/* 32MB */
	36 * 256,	/* 36MB */
	40 * 256,	/* 40MB */
	48 * 256,	/* 48MB */
};
static int lowmem_minfree_size = LOWMEM_ADJ_ZRAM_LEVEL;
#else /* CONFIG_ZRAM */
static short lowmem_adj[6] = {
	0,
	1,
	6,
	12,
};
static int lowmem_adj_size = 4;
static size_t lowmem_minfree[6] = {
	3 * 512,	/* 6MB */
	2 * 1024,	/* 8MB */
	4 * 1024,	/* 16MB */
	16 * 1024,	/* 64MB */
};
static int lowmem_minfree_size = 4;
#endif /* CONFIG_ZRAM */

static pid_t last_killed_pid = 0;


#ifdef CONFIG_ZRAM_FOR_ANDROID
static unsigned int  default_interval_time = 2*HZ;
static  unsigned int  swap_interval_time = 2*HZ;
#endif


#ifdef CONFIG_ZRAM_FOR_ANDROID
static unsigned int kill_home_adj_wmark = LOWMEM_ADJ_ZRAM_LEVEL;
static uint32_t lowmem_swap_app_enable = 1;
static uint32_t lowmem_minfreeswap_check = 0;
static uint32_t lowmem_last_swap_time = 0;
static int lowmem_minfreeswap_size = LOWMEM_ADJ_ZRAM_LEVEL;

/* the percentage of free swap */
static size_t lowmem_minfreeswap[LOWMEM_ADJ_ZRAM_LEVEL] = {
	0,
	6,
	12,
	20,
	25,
	32,
	50,
	63,
	80,
};

static struct class *lmk_class;
static struct device *lmk_dev;
static int lmk_kill_pid = 0;
static int lmk_kill_ok = 0;

extern atomic_t optimize_comp_on;

int lowmemkiller_reclaim_adj = 1;
int swap_to_zram(int  nr_to_scan,  int  min_adj, int max_adj);

extern int isolate_lru_page_compcache(struct page *page);
extern void putback_lru_page(struct page *page);
extern unsigned int zone_id_shrink_pagelist(struct zone *zone_id,struct list_head *page_list, unsigned int nr_to_reclaim);
#define lru_to_page(_head) (list_entry((_head)->prev, struct page, lru))

#define SWAP_PROCESS_DEBUG_LOG 0
/* free RAM 8M(2048 pages) */
#define CHECK_FREE_MEMORY 2048
/* free swap (10240 pages) */
#define CHECK_FREE_SWAPSPACE  10240

static unsigned int check_free_memory = 0;

enum pageout_io {
	PAGEOUT_IO_ASYNC,
	PAGEOUT_IO_SYNC,
};


#endif /* CONFIG_ZRAM_FOR_ANDROID */

static struct task_struct *lowmem_deathpending;
static unsigned long lowmem_deathpending_timeout;

#define lowmem_print(level, x...)			\
	do {						\
		if (lowmem_debug_level >= (level))	\
			printk(x);			\
	} while (0)

static int
task_notify_func(struct notifier_block *self, unsigned long val, void *data);

static struct notifier_block task_nb = {
	.notifier_call	= task_notify_func,
};

static int
task_notify_func(struct notifier_block *self, unsigned long val, void *data)
{
	struct task_struct *task = data;

	if (task == lowmem_deathpending)
		lowmem_deathpending = NULL;

	return NOTIFY_OK;
}

#ifdef  CONFIG_ZRAM_FOR_ANDROID
int getbuddyfreepages(void)
{
	struct zone *zone = NULL;
	int  total = 0;
	for_each_populated_zone(zone) 
	{
		unsigned long  flags, order;
//		spin_lock_irqsave(&zone->lock, flags);
		for (order = 0; order < MAX_ORDER; order++) 
		{
			total += zone->free_area[order].nr_free << order;
		}
//		spin_unlock_irqrestore(&zone->lock, flags);
	}
	return total;
}
#endif

static int lowmem_shrink(struct shrinker *s, int nr_to_scan, gfp_t gfp_mask)
{
	struct task_struct *p;
	struct task_struct *selected = NULL;
	int rem = 0;
	int tasksize;
	int i;
	int min_adj = OOM_ADJUST_MAX + 1;
	int selected_tasksize = 0;
	int selected_oom_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);
	int other_free = global_page_state(NR_FREE_PAGES) - totalreserve_pages;
	int other_file = global_page_state(NR_FILE_PAGES) -
						global_page_state(NR_SHMEM);

#ifdef CONFIG_ZRAM_FOR_ANDROID			
	int to_reclaimed = 0;
	struct sysinfo si = {0};
	si_swapinfo(&si);
#endif  /*CONFIG_ZRAM_FOR_ANDROID*/

#ifdef CONFIG_ZRAM
	other_file -= total_swapcache_pages;
	lowmem_print(4, "other_free %d, other_file %d, "
			"totalreserve %d, totalswap %d\n",
			other_free, other_file,
			totalreserve_pages, total_swapcache_pages);
#endif /* CONFIG_ZRAM */


	/*
	 * If we already have a death outstanding, then
	 * bail out right away; indicating to vmscan
	 * that we have nothing further to offer on
	 * this pass.
	 *
	 */
	if (lowmem_deathpending &&
	    time_before_eq(jiffies, lowmem_deathpending_timeout))
		return 0;

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;
	for (i = 0; i < array_size; i++) {
		if (other_free < lowmem_minfree[i] &&
		    other_file < lowmem_minfree[i]) {
			min_adj = lowmem_adj[i];
			break;
		}
	}
#ifdef CONFIG_ZRAM_FOR_ANDROID
/*
	we don't want to reduce min_adj under 1 due to no free swap space.
*/
	if (lowmem_minfreeswap_check && si.totalswap && min_adj > 1) {
		unsigned int freeswap_rate = si.freeswap * 100 / si.totalswap;

		/* Reduce min_adj according to the predefined freeswap levels
		 */
		if (min_adj == OOM_ADJUST_MAX + 1)
			i = array_size - 1;
		for (; i > 0; --i)
			if (lowmem_minfreeswap[i] < freeswap_rate)
				break;
		if (i != array_size - 1)
			min_adj = lowmem_adj[i];
		if(min_adj == 0)
		{
			min_adj = 1;
		}

		lowmem_print(4, "lowmem_shrink freeswap %d rate %d%\n",
			     si.freeswap, freeswap_rate);
	}
#endif
	if (nr_to_scan > 0)
		lowmem_print(3, "lowmem_shrink %d, %x, ofree %d %d, ma %d\n",
			     nr_to_scan, gfp_mask, other_free, other_file,
			     min_adj);
	rem = global_page_state(NR_ACTIVE_ANON) +
		global_page_state(NR_ACTIVE_FILE) +
		global_page_state(NR_INACTIVE_ANON) +
		global_page_state(NR_INACTIVE_FILE);
	if (nr_to_scan <= 0 || min_adj == OOM_ADJUST_MAX + 1) {
		lowmem_print(5, "lowmem_shrink %d, %x, return %d\n",
			     nr_to_scan, gfp_mask, rem);
		return rem;
	}
	selected_oom_adj = min_adj;

#ifdef  CONFIG_ZRAM_FOR_ANDROID
	if (lowmem_swap_app_enable && si.totalswap) {
		if (current_is_kswapd() &&
			((jiffies -lowmem_last_swap_time) >= swap_interval_time)
			 && (min_adj > 1))
		{
			int times;
			/* buddy pages/2 */
			int swap_to_scan = getbuddyfreepages() >> 1;

			if(swap_to_scan > 1024)
				swap_to_scan = 1024;

			if(swap_to_scan > (SWAP_CLUSTER_MAX  << 1) )  //Only run at buddy pages enough
				to_reclaimed = swap_to_zram(swap_to_scan, min_adj, 1);

			lowmem_print(2,"[LMK]swap_to_scan:%d, to_reclaimed:%d, time:%d s\r\n",  swap_to_scan, to_reclaimed, jiffies/HZ);

			if(to_reclaimed >= swap_to_scan)
			{
				swap_interval_time = default_interval_time;
				return rem - to_reclaimed;
			}

			if(0 == to_reclaimed)
			       times = 5;
			else
			{
				times = swap_to_scan/to_reclaimed;
				if(times > 5)
					times = 5;
			}
			swap_interval_time = default_interval_time * times;
			lowmem_last_swap_time = jiffies;

			if ((si.freeswap * 100 / si.totalswap) >
				lowmem_minfreeswap[i])
				return rem - to_reclaimed;
		}
	}
#endif

	read_lock(&tasklist_lock);
	for_each_process(p) {
		struct mm_struct *mm;
		struct signal_struct *sig;
		int oom_adj;

		task_lock(p);
		mm = p->mm;
		sig = p->signal;
		if (!mm || !sig) {
			task_unlock(p);
			continue;
		}
		oom_adj = sig->oom_adj;
#ifdef CONFIG_ZRAM_FOR_ANDROID	
		if((oom_adj == 6) && (min_adj >= kill_home_adj_wmark))
		{
		     task_unlock(p);
		    continue;	 
		}
#endif	 /*CONFIG_ZRAM_FOR_ANDROID*/	
		if (oom_adj < min_adj) {
			task_unlock(p);
			continue;
		}
#ifdef CONFIG_ZRAM
		tasksize = get_mm_rss(p->mm) + get_mm_counter(p->mm, MM_SWAPENTS);
#else		
		tasksize = get_mm_rss(mm);
#endif
		task_unlock(p);
		if (tasksize <= 0)
			continue;
		if (selected) {
			if (oom_adj < selected_oom_adj)
				continue;
			if (oom_adj == selected_oom_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_adj = oom_adj;
		lowmem_print(2, "select %d (%s), adj %d, size %d, to kill\n",
			     p->pid, p->comm, oom_adj, tasksize);
	}
	if (selected) {
		if (fatal_signal_pending(selected)) {
			pr_warning("process %d is suffering a slow death\n",
				   selected->pid);
			read_unlock(&tasklist_lock);
			return rem;
		}		
		lowmem_print(1, "send sigkill to %d (%s), adj %d, size %d\n",
			     selected->pid, selected->comm,
			     selected_oom_adj, selected_tasksize);
		lowmem_deathpending = selected;
		lowmem_deathpending_timeout = jiffies + HZ;
#ifdef CONFIG_ANDROID_OOM_NOTIFY
		//printk("oom:  selected->signal->oom_adj %d\n",selected->signal->oom_adj);
		if(oom_notify_enable && ( selected->signal->oom_adj == 0))
		{
			char comm[TASK_COMM_LEN+1]={0};
			//printk("the current process will be kill for oom\n");
			memcpy(comm,selected->comm, TASK_COMM_LEN);
			do_mm_report(comm);
		}
#endif
		force_sig(SIGKILL, selected);
		if(selected->pid == last_killed_pid && selected->signal->oom_adj > 2) {
			selected->signal->oom_adj--;
		} else
			last_killed_pid = selected->pid;
		rem -= selected_tasksize;
	}
	lowmem_print(4, "lowmem_shrink %d, %x, return %d\n",
		     nr_to_scan, gfp_mask, rem);
	read_unlock(&tasklist_lock);
	return rem;
}

#ifdef CONFIG_ZRAM_FOR_ANDROID
/*
 * zone_id_shrink_pagelist() clear page flags,
 * update the memory zone status, and swap pagelist
 */

static unsigned int shrink_pages(struct mm_struct *mm,
				 struct list_head *zone0_page_list,
				 struct list_head *zone1_page_list,
				 unsigned int num_to_scan)
{
	unsigned long addr;
	unsigned int isolate_pages_countter = 0;

	struct vm_area_struct *vma = mm->mmap;
	while (vma != NULL) {

		for (addr = vma->vm_start; addr < vma->vm_end;
		     addr += PAGE_SIZE) {
			struct page *page;
			/*get the page address from virtual memory address */
			page = follow_page(vma, addr, FOLL_GET);

			if (page && !IS_ERR(page)) {

				put_page(page);
				/* only moveable, anonymous and not dirty pages can be swapped  */
				if ((!PageUnevictable(page))
				    && (!PageDirty(page)) && ((PageAnon(page)))
				    && (0 == page_is_file_cache(page))) {
					switch (page_zone_id(page)) {
					case 0:
						if (!isolate_lru_page_compcache(page)) {
							/* isolate page from LRU and add to temp list  */
							/*create new page list, it will be used in shrink_page_list */
							list_add_tail(&page->lru, zone0_page_list);
							isolate_pages_countter++;
						}
						break;
					case 1:
						if (!isolate_lru_page_compcache(page)) {
							/* isolate page from LRU and add to temp list  */
							/*create new page list, it will be used in shrink_page_list */
							list_add_tail(&page->lru, zone1_page_list);
							isolate_pages_countter++;
						}
						break;
					default:
						break;
					}
				}
			}

			if (isolate_pages_countter >= num_to_scan) {
				return isolate_pages_countter;
			}
		}

		vma = vma->vm_next;
	}

	return isolate_pages_countter;
}

/*
 * swap_application_pages() will search the
 * pages which can be swapped, then call
 * zone_id_shrink_pagelist to update zone
 * status
 */
static unsigned int swap_pages(struct list_head *zone0_page_list,
			       struct list_head *zone1_page_list, unsigned int  nr_to_reclaim)
{
	struct zone *zone_id_0 = &NODE_DATA(0)->node_zones[0];
	struct zone *zone_id_1 = &NODE_DATA(0)->node_zones[1];
	unsigned int pages_counter = 0;

	/*if the page list is not empty, call zone_id_shrink_pagelist to update zone status */
	if ((zone_id_0) && (!list_empty(zone0_page_list))) {
		pages_counter +=
		    zone_id_shrink_pagelist(zone_id_0, zone0_page_list, nr_to_reclaim);
	}
	if ((zone_id_1) && (!list_empty(zone1_page_list))) {
		pages_counter +=
		    zone_id_shrink_pagelist(zone_id_1, zone1_page_list, nr_to_reclaim);
	}
	return pages_counter;
}


int swap_to_zram(int  nr_to_scan,  int  min_adj, int   max_adj)
{
	struct task_struct *p = NULL;
	int pages_tofree = 0, pages_freed = 0;
	int  oom_adj_wmark = 0;
	LIST_HEAD(zone0_page_list);
	LIST_HEAD(zone1_page_list);
	struct sysinfo ramzswap_info = { 0 };
	int  shrink_to_scan = nr_to_scan ;
    
	si_swapinfo(&ramzswap_info);
	si_meminfo(&ramzswap_info);
	
	if(ramzswap_info.freeswap  <  CHECK_FREE_SWAPSPACE)
	{
		return 0;
	}

	for(oom_adj_wmark = min_adj;  oom_adj_wmark >= max_adj;  oom_adj_wmark--)
	{
		pages_tofree = 0;
		
		read_lock(&tasklist_lock);
		for_each_process(p) 
		{
			struct mm_struct *mm;
			struct signal_struct *sig;
			int oom_adj;

			task_lock(p);
			mm = p->mm;
			sig = p->signal;
			if (!mm || !sig) 
			{
				task_unlock(p);
				continue;
			}
			
			oom_adj = sig->oom_adj;
			if ( (oom_adj < oom_adj_wmark) ||
			      (__task_cred(p)->uid  <= 10000) || (p->flags & PF_KTHREAD) )
			{
				task_unlock(p);
				continue;
			}

			atomic_inc(&mm->mm_users);

			task_unlock(p);

			down_read(&mm->mmap_sem);
			pages_tofree += shrink_pages(mm, &zone0_page_list, &zone1_page_list, shrink_to_scan);
			up_read(&mm->mmap_sem);

			mmput(mm);

			pr_debug("%s, name:%s, adj:%d, policy:%u, uid:%u\r\n", 
							__func__, p->comm, oom_adj,p->policy, __task_cred(p)->uid );
			
		}
		read_unlock(&tasklist_lock);

		if(pages_tofree)
		{
			pages_freed += swap_pages(&zone0_page_list, &zone1_page_list, pages_tofree);
		}

		if(pages_freed >= shrink_to_scan)
		{
			break;
		}
		
	}

	return pages_freed;
}


static ssize_t lmk_state_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d,%d\n", lmk_kill_pid, lmk_kill_ok);
}

/*
 * lmk_state_store() will called by framework,
 * the framework will send the pid of process that need to be swapped
 */
static ssize_t lmk_state_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	sscanf(buf, "%d,%d", &lmk_kill_pid, &lmk_kill_ok);

	/* if the screen on, the optimized compcache will stop */
	if (atomic_read(&optimize_comp_on) != 1)
		return size;

	if (lmk_kill_ok == 1) {
		struct task_struct *p;
		struct task_struct *selected = NULL;
		struct sysinfo ramzswap_info = { 0 };
		struct mm_struct *mm_scan = NULL;

		/*
		 * check the free RAM and swap area,
		 * stop the optimized compcache in cpu idle case;
		 * leave some swap area for using in low memory case
		 */
		si_swapinfo(&ramzswap_info);
		si_meminfo(&ramzswap_info);

		if ((ramzswap_info.freeswap < CHECK_FREE_SWAPSPACE) ||
		    (ramzswap_info.freeram < check_free_memory)) {
#if SWAP_PROCESS_DEBUG_LOG > 0
			printk(KERN_INFO "idletime compcache is ignored : free RAM %lu, free swap %lu\n",
			ramzswap_info.freeram, ramzswap_info.freeswap);
#endif
			lmk_kill_ok = 0;
			return size;
		}

		read_lock(&tasklist_lock);
		for_each_process(p) {
			if ((p->pid == lmk_kill_pid) &&
			    (__task_cred(p)->uid > 10000)) {
				task_lock(p);
				selected = p;
				if (!selected->mm || !selected->signal) {
					task_unlock(p);
					selected = NULL;
					break;
				}
				mm_scan = selected->mm;
				if (mm_scan) {
					if (selected->flags & PF_KTHREAD)
						mm_scan = NULL;
					else
						atomic_inc(&mm_scan->mm_users);
				}
				task_unlock(selected);

#if SWAP_PROCESS_DEBUG_LOG > 0
				printk(KERN_INFO "idle time compcache: swap process pid %d, name %s, oom %d, task size %ld\n",
					p->pid, p->comm,
					p->signal->oom_adj,
					get_mm_rss(p->mm));
#endif
				break;
			}
		}
		read_unlock(&tasklist_lock);

		if (mm_scan) {
			LIST_HEAD(zone0_page_list);
			LIST_HEAD(zone1_page_list);
			int pages_tofree = 0, pages_freed = 0;

			down_read(&mm_scan->mmap_sem);
			pages_tofree =
			shrink_pages(mm_scan, &zone0_page_list,
					&zone1_page_list, 0x7FFFFFFF);
			up_read(&mm_scan->mmap_sem);
			mmput(mm_scan);
			pages_freed =
			    swap_pages(&zone0_page_list,
				       &zone1_page_list, pages_tofree);
			lmk_kill_ok = 0;

		}
	}

	return size;
}

static DEVICE_ATTR(lmk_state, 0664, lmk_state_show, lmk_state_store);

#endif /* CONFIG_ZRAM_FOR_ANDROID */

/* This will provide a true status based off minfree calculation of free ram
 * There are two numbers free_file and free_other as calculated above
 */
static int param_get_minfree_stat(char *buffer, struct kernel_param *kp)
{
	int other_free = global_page_state(NR_FREE_PAGES) - totalreserve_pages;
	int other_file = global_page_state(NR_FILE_PAGES) -
				global_page_state(NR_SHMEM);
#ifdef CONFIG_ZRAM
	other_file -= total_swapcache_pages;
#endif /* CONFIG_ZRAM */

	return sprintf(buffer,"other_free:  %d kB\nother_file:  %d kB",
			other_free << (PAGE_SHIFT - 10),
			other_file << (PAGE_SHIFT - 10));
}
module_param_call(minfree_stat, NULL, param_get_minfree_stat, NULL, S_IRUGO);

static struct shrinker lowmem_shrinker = {
	.shrink = lowmem_shrink,
	.seeks = DEFAULT_SEEKS * 16
};

static int __init lowmem_init(void)
{
#ifdef CONFIG_ZRAM_FOR_ANDROID
	struct zone *zone;
	unsigned int high_wmark = 0;
	lowmem_last_swap_time =  jiffies;
#endif
	task_free_register(&task_nb);
	register_shrinker(&lowmem_shrinker);
#ifdef CONFIG_ZRAM_FOR_ANDROID
	for_each_zone(zone) {
		if (high_wmark < zone->watermark[WMARK_HIGH])
			high_wmark = zone->watermark[WMARK_HIGH];
	}
	check_free_memory = (high_wmark != 0) ? high_wmark : CHECK_FREE_MEMORY;

	lmk_class = class_create(THIS_MODULE, "lmk");
	if (IS_ERR(lmk_class)) {
		printk(KERN_ERR "Failed to create class(lmk)\n");
		return 0;
	}
	lmk_dev = device_create(lmk_class, NULL, 0, NULL, "lowmemorykiller");
	if (IS_ERR(lmk_dev)) {
		printk(KERN_ERR
		       "Failed to create device(lowmemorykiller)!= %ld\n",
		       IS_ERR(lmk_dev));
		return 0;
	}
	if (device_create_file(lmk_dev, &dev_attr_lmk_state) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
		       dev_attr_lmk_state.attr.name);
#endif /* CONFIG_ZRAM_FOR_ANDROID */
	return 0;
}

static void __exit lowmem_exit(void)
{
	unregister_shrinker(&lowmem_shrinker);
	task_free_unregister(&task_nb);
}

#ifdef CONFIG_ZRAM_FOR_ANDROID
module_param_named(lowmem_swap_app_enable, lowmem_swap_app_enable, uint, S_IRUGO | S_IWUSR);
module_param_named(lowmem_minfreeswap_check, lowmem_minfreeswap_check, uint, S_IRUGO | S_IWUSR);
module_param_named(kill_home_adj_wmark, kill_home_adj_wmark, uint, S_IRUGO | S_IWUSR);
module_param_array_named(lowmem_minfreeswap, lowmem_minfreeswap, uint,
			 &lowmem_minfreeswap_size, S_IRUGO | S_IWUSR);
module_param_named(default_interval_time, default_interval_time, uint, S_IRUGO | S_IWUSR);

#endif

module_param_named(cost, lowmem_shrinker.seeks, int, S_IRUGO | S_IWUSR);

module_param_array_named(adj, lowmem_adj, short, &lowmem_adj_size,
			 S_IRUGO | S_IWUSR);
module_param_array_named(minfree, lowmem_minfree, uint, &lowmem_minfree_size,
			 S_IRUGO | S_IWUSR);
module_param_named(debug_level, lowmem_debug_level, uint, S_IRUGO | S_IWUSR);

/*
 * late_initcall guarantees the lowmemory shrink is at the tail of shinkers
 */
late_initcall(lowmem_init);
module_exit(lowmem_exit);

MODULE_LICENSE("GPL");

