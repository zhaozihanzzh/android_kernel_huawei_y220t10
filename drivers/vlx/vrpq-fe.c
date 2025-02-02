/*****************************************************************************
 *                                                                           *
 *  Component: VLX Virtual Remote Procedure Queue (VRPQ).                    *
 *             VRPQ frontend kernel driver implementation.                   *
 *                                                                           *
 *  Copyright (C) 2011, Red Bend Software. All Rights Reserved.              *
 *                                                                           *
 *  #ident  "%Z%%M% %I%     %E% Red Bend Software"                           *
 *                                                                           *
 *  Contributor(s):                                                          *
 *    Sebastien Laborie <sebastien.laborie@redbend.com>                      *
 *                                                                           *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/poll.h>
#include "vrpq.h"

MODULE_DESCRIPTION("VRPQ Front-End Driver");
MODULE_AUTHOR("Sebastien Laborie <sebastien.laborie@redbend.com>");
MODULE_LICENSE("GPL");

static noinline int vrpq_adm_call (VrpqCltSession* session,
				   unsigned int    func,
				   VrpqSize        in_size,
				   void*           in,
				   VrpqSize        out_size,
				   void*           out);

#define VRPQ_ADM_CALL(s, f, i, o)			\
    vrpq_adm_call(s, f, sizeof(*i), i, sizeof(*o), o)

    /*
     *
     */
    static inline void
vrpq_peer_notify (VrpqCltPeer* peer)
{
    nkops.nk_xirq_trigger(peer->xirq_req, peer->osid);
}

    /*
     *
     */
    static inline void
vrpq_params_do_free (VrpqParamRing* param_ring)
{
    VrpqParamChunk* params;
    VrpqParamChunk* params_limit;
    VrpqParamChunk* chunk_head;
    VrpqParamChunk* chunk;
    VrpqParamChunk* chunk_next;
    VrpqSize        chunk_size;

    params       = param_ring->params;
    params_limit = param_ring->paramsLimit;
    chunk_head   = param_ring->head;
    chunk        = param_ring->tail;

    while ((chunk != chunk_head) && !(chunk->flags.inuse)) {
	chunk_size = VRPQ_PARAM_CHUNK_SIZE_SAFE(chunk->size);
	chunk_next = VRPQ_PARAM_CHUNK_NEXT(chunk, chunk_size);
	if (likely(chunk_next < params_limit)) {
	    chunk = chunk_next;
	    continue;
	}
	chunk  = params;
	params = chunk_head;
    }
    param_ring->tail = chunk;

    return;
}

    /*
     *
     */
    static inline void
vrpq_params_free (VrpqCltDev* dev)
{
    VrpqReqAlloc* req_alloc;

    req_alloc = &dev->req_alloc;
    if (xchg(&req_alloc->free_excl, 1) == 0) {
	vrpq_params_do_free(&dev->usr_param_alloc.ring);
	req_alloc->free_excl = 0;
    }
}

    /*
     *
     */
    static noinline void
vrpq_reqs_do_free (VrpqCltDev* dev, VrpqReqMsg* req)
{
    VrpqPmemLayout* pmem;
    nku8_f*         pmem_base;
    VrpqSize        pmem_size;
    VrpqReqRing*    req_ring;
    VrpqReqMsg*     reqs;
    VrpqRingIdx     head_idx;
    VrpqRingIdx     tail_idx;
    VrpqRingIdx     mod_mask;

    pmem      = &dev->gen_dev.pmem_layout;
    pmem_base = pmem->pmemVaddr;
    pmem_size = pmem->pmemSize;

    req_ring  = &dev->req_alloc.ring;
    reqs      = req_ring->reqs;
    head_idx  = req_ring->headIdx;
    tail_idx  = req_ring->tailIdx;
    mod_mask  = req_ring->modMask;

    VRPQ_PROF_INC(dev, reqs_free);
    do {
	VRPQ_PARAM_RECYCLE(req->procReq.inOffset, pmem_base, pmem_size);
	tail_idx++;
	VRPQ_PROF_INC(dev, reqs_per_free);
    } while (!VRPQ_REQ_RING_IS_EMPTY(head_idx, tail_idx) &&
	     (!((req = &reqs[tail_idx & mod_mask])->flags.inuse)));

    req_ring->tailIdx = tail_idx;

    vrpq_params_do_free(&dev->usr_param_alloc.ring);

    if (!dev->full_flag) {
	return;
    }

    dev->full_flag = 0;
    wake_up_interruptible(&dev->full_wait);
}

    /*
     *
     */
    static inline void
vrpq_reqs_free (VrpqCltDev* dev)
{
    VrpqReqAlloc* req_alloc;
    VrpqReqRing*  req_ring;
    VrpqRingIdx   tail_idx;
    VrpqReqMsg*   req;

    req_alloc = &dev->req_alloc;
    if (xchg(&req_alloc->free_excl, 1) == 0) {
	req_ring = &req_alloc->ring;
	tail_idx = req_ring->tailIdx;
	if (!VRPQ_REQ_RING_IS_EMPTY(req_ring->headIdx, tail_idx)) {
	    req = &req_ring->reqs[tail_idx & req_ring->modMask];
	    if (!req->flags.inuse) {
		vrpq_reqs_do_free(dev, req);
	    }
	}
	req_alloc->free_excl = 0;
    }
}

    /*
     *
     */
    static noinline int
vrpq_full_wait (VrpqCltChan* chan, VrpqRingIdx tail_idx)
{
    VrpqCltDev*           dev             = chan->dev;
    VrpqParamAlloc*       adm_param_alloc = &dev->adm_param_alloc;
    volatile VrpqRingIdx* new_tail_idx;
    int                   diag;

    VLINK_DTRACE(dev->gen_dev.vlink,
		 "request or parameter ring is full, waiting...\n");

    dev->req_alloc.ring.global->fullFlag = dev->full_flag = 1;

    vrpq_peer_notify(&dev->peer);    

    vrpq_reqs_free(dev);

    new_tail_idx = &dev->req_alloc.ring.tailIdx;

    diag = wait_event_interruptible(dev->full_wait,
				    (VLINK_SESSION_IS_ABORTED(chan->vls) ||
				     (*new_tail_idx != tail_idx)));

    spin_lock(&adm_param_alloc->lock);
    vrpq_params_do_free(&adm_param_alloc->ring);
    spin_unlock(&adm_param_alloc->lock);

    if (diag) {
	return diag;
    }

    if (VLINK_SESSION_IS_ABORTED(chan->vls)) {
	return -ECHANBROKEN;
    }

    return 0;
}

    /*
     *
     */
    static inline VrpqParamInfo*
vrpq_param_alloc_fast (VrpqParamAlloc* alloc, VrpqSize size)
{
    VrpqParamRing*  ring = &alloc->ring;
    VrpqParamChunk* head;
    VrpqParamChunk* tail;
    VrpqParamChunk* limit_free;
    VrpqSize        chunk_size;
    VrpqParamChunk* chunk;
    VrpqParamChunk* chunk_next;

    spin_lock(&alloc->lock);

    head = ring->head;
    tail = ring->tail;

    if (head >= tail) {
	limit_free = ring->paramsLimit;
    } else {
	limit_free = tail;
    }

    chunk      = head;
    chunk_size = VRPQ_PARAM_CHUNK_SIZE(size);
    chunk_next = VRPQ_PARAM_CHUNK_NEXT(chunk, chunk_size);

    if (chunk_next < limit_free) {
	chunk->size      = chunk_size;
	chunk->flags.all = VRPQ_BYTE_FLAGS_INIT(1, 0, 0, 0);
	ring->head       = chunk_next;
	spin_unlock(&alloc->lock);
	return VRPQ_PARAM_CHUNK_TO_PINFO(chunk);
    }

    return NULL;
}

    /*
     *
     */
    static noinline VrpqParamInfo*
vrpq_param_alloc_slow (VrpqCltChan* chan, VrpqParamAlloc* alloc, VrpqSize size)
{
    VrpqParamRing*  ring = &alloc->ring;
    VrpqParamChunk* head;
    VrpqParamChunk* tail;
    VrpqSize        chunk_size;
    VrpqParamChunk* chunk;
    VrpqParamChunk* chunk_next;
    VrpqParamInfo*  pinfo;
    VrpqRingIdx     req_tail_idx;
    int             diag;

    for (;;) {

	req_tail_idx = chan->dev->req_alloc.ring.tailIdx;
	head         = ring->head;
	tail         = ring->tail;

	chunk_size   = VRPQ_PARAM_CHUNK_SIZE(size);

	if ((head >= tail) && (tail != ring->params)) {

	    chunk      = head;
	    chunk_next = VRPQ_PARAM_CHUNK_NEXT(chunk, chunk_size);

	    if (chunk_next == ring->paramsLimit) {
		chunk->size      = chunk_size;
		chunk->flags.all = VRPQ_BYTE_FLAGS_INIT(1, 0, 0, 0);
		ring->head       = ring->params;
		spin_unlock(&alloc->lock);
		return VRPQ_PARAM_CHUNK_TO_PINFO(chunk);
	    }

	    chunk->size      = (VrpqSize) ((nku8_f*)ring->paramsLimit -
					   (nku8_f*)head);
	    chunk->flags.all = 0;
	    ring->head       = ring->params;
	    pinfo            = vrpq_param_alloc_fast(alloc, size);
	    if (pinfo) {
		return pinfo;
	    }
	}

	spin_unlock(&alloc->lock);

	if (chunk_size >= ring->size / 2) {
	    return ERR_PTR(-EMSGSIZE);
	}

	VRPQ_PROF_INC(chan->dev, param_ring_full);

	if ((diag = vrpq_full_wait(chan, req_tail_idx)) != 0) {
	    return ERR_PTR(diag);
	}

	pinfo = vrpq_param_alloc_fast(alloc, size);
	if (pinfo) {
	    return pinfo;
	}
    }
}

    /*
     *
     */
    static inline VrpqParamInfo*
vrpq_param_alloc (VrpqCltChan* chan, VrpqParamAlloc* alloc, VrpqSize size)
{
    VrpqParamInfo* pinfo;

    pinfo = vrpq_param_alloc_fast(alloc, size);
    if (pinfo) {
	VRPQ_PROF_INC(chan->dev, param_alloc_fast);
	return pinfo;
    }
    VRPQ_PROF_INC(chan->dev, param_alloc_slow);
    return vrpq_param_alloc_slow(chan, alloc, size);
}

#ifdef CONFIG_VRPQ_PROFILE

    /*
     *
     */
    static noinline VrpqProcStatTbl*
vrpq_proc_stat_tbl_alloc (VrpqCltDev* dev, int write)
{
    VrpqProcStatTbl* tbl;
    unsigned int     version;

    if (!write) {
	return NULL;
    }

    version = dev->stats.proc_stat_version;

    spin_unlock(&dev->stats.proc_stat_lock);

    tbl = kzalloc(sizeof(VrpqProcStatTbl), GFP_KERNEL);

    spin_lock(&dev->stats.proc_stat_lock);

    if (dev->stats.proc_stat_version != version) {
	kfree(tbl);
	tbl = NULL;
    }

    return tbl;
}

    /*
     *
     */
    static noinline unsigned int*
vrpq_proc_stat_ref (VrpqCltDev* dev, VrpqProcId proc_id, int write)
{
    VrpqProcStatTbl* tbl;
    VrpqProcStatTbl* ntbl;
    unsigned int     idx;
    unsigned int     ror;

    tbl = dev->stats.proc_stat_dir;
    if (!tbl) {
	if ((tbl = vrpq_proc_stat_tbl_alloc(dev, write)) == NULL) {
	    return NULL;
	}
	dev->stats.proc_stat_dir = tbl;
	VRPQ_PROF_INC(dev, proc_stat_tbl);
   }

    for (ror = 24; ror > 0; ror -= 8) {

	idx  = (proc_id >> ror) & 0xff;
	ntbl = tbl->ent[idx].tbl;

	if (!ntbl) {
	    if ((ntbl = vrpq_proc_stat_tbl_alloc(dev, write)) == NULL) {
		return NULL;
	    }
	    tbl->ent[idx].tbl = ntbl;
	    VRPQ_PROF_INC(dev, proc_stat_tbl);
	}

	tbl = ntbl;
    }

    idx = proc_id & 0xff;
    return &tbl->ent[idx].count;
}

#define VRPQ_PROF_PROC(dev, proc_id)			\
    do {						\
	unsigned int *pcount;				\
	spin_lock(&(dev)->stats.proc_stat_lock);	\
	pcount = vrpq_proc_stat_ref(dev, proc_id, 1);	\
	if (pcount) (*pcount)++;			\
	spin_unlock(&(dev)->stats.proc_stat_lock);	\
    } while (0);

#define __PROF_IDX(v)	(offsetof(VrpqCltStats, v) / sizeof(atomic_t))

static struct {
    nku32_f id;
    nku32_f idx;
} vrpq_prof_map[] = {
    { VRPQ_PROF_ADM_CALLS,		__PROF_IDX(adm_calls)		     },
    { VRPQ_PROF_SESSION_CREATE,		__PROF_IDX(session_create)	     },
    { VRPQ_PROF_SESSION_DESTROY,	__PROF_IDX(session_destroy)	     },
    { VRPQ_PROF_CHAN_CREATE,		__PROF_IDX(chan_create)		     },
    { VRPQ_PROF_CHAN_DESTROY,		__PROF_IDX(chan_destroy)	     },
    { VRPQ_PROF_NOTIFY,			__PROF_IDX(notify)		     },
    { VRPQ_PROF_POST_MULTI_CALL,	__PROF_IDX(post_multi_call)	     },
    { VRPQ_PROF_POSTS_PER_CALL,		__PROF_IDX(posts_per_call)	     },
    { VRPQ_PROF_POSTS_PER_CALL_MAX,	__PROF_IDX(posts_per_call_max)	     },
    { VRPQ_PROF_POST_MULTI_NOTIFY,	__PROF_IDX(post_multi_notify)	     },
    { VRPQ_PROF_POSTS_PER_NOTIFY,	__PROF_IDX(posts_per_notify)	     },
    { VRPQ_PROF_POSTS_PER_NOTIFY_MAX,	__PROF_IDX(posts_per_notify_max)     },
    { VRPQ_PROF_POST_MULTI,		__PROF_IDX(post_multi)		     },
    { VRPQ_PROF_GROUPED_POSTS,		__PROF_IDX(grouped_posts)	     },
    { VRPQ_PROF_GROUPED_POSTS_MAX,	__PROF_IDX(grouped_posts_max)	     },
    { VRPQ_PROF_RESPONSES_INTR,		__PROF_IDX(resps_intr)		     },
    { VRPQ_PROF_PARAM_ALLOC_FAST,	__PROF_IDX(param_alloc_fast)	     },
    { VRPQ_PROF_PARAM_ALLOC_SLOW,	__PROF_IDX(param_alloc_slow)	     },
    { VRPQ_PROF_IN_PARAM_REF,		__PROF_IDX(in_param_ref)	     },
    { VRPQ_PROF_REQS_FREE,		__PROF_IDX(reqs_free)		     },
    { VRPQ_PROF_REQS_PER_FREE,		__PROF_IDX(reqs_per_free)	     },
    { VRPQ_PROF_REQ_RING_FULL,		__PROF_IDX(req_ring_full)	     },
    { VRPQ_PROF_PARAM_RING_FULL,	__PROF_IDX(param_ring_full)	     },
    { VRPQ_PROF_WAIT_FOR_SPACE_INTR,	__PROF_IDX(wait_for_space_intr)	     },
    { VRPQ_PROF_PROC_STAT_TBL,		__PROF_IDX(proc_stat_tbl)	     },
    { -1,				-1				     },
};

    /*
     *
     */
     static int
vrpq_prof_op (VrpqCltFile* vrpq_file, VrpqProfOp __user* uop, unsigned int sz)
{
    VrpqCltDev*       dev = vrpq_file->dev;
    VrpqProfOp        op;
    VrpqStat __user*  ustats;
    unsigned int      count;

    if (COPYIN(&op, uop, sizeof(VrpqProfOp))) {
	return -EFAULT;
    }

    count  = op.count;
    ustats = op.stats;

    switch (op.cmd) {

    case VRPQ_PROF_GEN_STAT_GET:
    {
	nku32_f      id;
	nku32_f      val;
	unsigned int i;

	while (count) {

	    if (get_user(id, &ustats->id)) {
		return -EFAULT;
	    }

	    for (i = 0;
		 (vrpq_prof_map[i].id != -1) && (vrpq_prof_map[i].id != id);
		 i++);

	    if (vrpq_prof_map[i].id == -1) {
		return -EINVAL;
	    }

	    val = atomic_read(&dev->stats.counts[vrpq_prof_map[i].idx]);

	    if (put_user(val, &ustats->val)) {
		return -EFAULT;
	    }

	    ustats++;
	    count--;

	}

	return 0;
    }

    case VRPQ_PROF_PROC_STAT_GET:
    {
	nku32_f       id;
	nku32_f       val;
	unsigned int *pval;

	while (count) {

	    if (get_user(id, &ustats->id)) {
		return -EFAULT;
	    }

	    spin_lock(&dev->stats.proc_stat_lock);

	    pval = vrpq_proc_stat_ref(dev, id, 0);
	    if (pval) {
		val = *pval;
	    } else {
		val = 0;
	    }

	    spin_unlock(&dev->stats.proc_stat_lock);

	    if (put_user(val, &ustats->val)) {
		return -EFAULT;
	    }

	    ustats++;
	    count--;

	}

	return 0;
    }

    case VRPQ_PROF_PROC_STAT_GET_ALL:
    {
	VrpqProcStatTbl* tbl1;
	VrpqProcStatTbl* tbl2;
	VrpqProcStatTbl* tbl3;
	VrpqProcStatTbl* tbl4;
	unsigned int     version;
	unsigned int     nversion;
	unsigned int     i1;
	unsigned int     i2;
	unsigned int     i3;
	unsigned int     i4;
	nku32_f          val;
	nku32_f          proc_id;
	int              avail = 0;

	spin_lock(&dev->stats.proc_stat_lock);
	tbl1    = dev->stats.proc_stat_dir;
	version = dev->stats.proc_stat_version;
	spin_unlock(&dev->stats.proc_stat_lock);

	if (!tbl1) {
	    return 0;
	}

	for (i1 = 0; i1 < 256; i1++) {

	    spin_lock(&dev->stats.proc_stat_lock);
	    if ((nversion = dev->stats.proc_stat_version) == version) {
		tbl2 = tbl1->ent[i1].tbl;
	    }
	    spin_unlock(&dev->stats.proc_stat_lock);

	    if (nversion != version) {
		break;
	    }
	    if (!tbl2) {
		continue;
	    }

	    for (i2 = 0; i2 < 256; i2++) {

		spin_lock(&dev->stats.proc_stat_lock);
		if ((nversion = dev->stats.proc_stat_version) == version) {
		    tbl3 = tbl2->ent[i2].tbl;
		}
		spin_unlock(&dev->stats.proc_stat_lock);

		if (nversion != version) {
		    break;
		}
		if (!tbl3) {
		    continue;
		}

		for (i3 = 0; i3 < 256; i3++) {

		    spin_lock(&dev->stats.proc_stat_lock);
		    if ((nversion = dev->stats.proc_stat_version) == version) {
			tbl4 = tbl3->ent[i3].tbl;
		    }
		    spin_unlock(&dev->stats.proc_stat_lock);

		    if (nversion != version) {
			break;
		    }
		    if (!tbl4) {
			continue;
		    }

		    for (i4 = 0; i4 < 256; i4++) {

			if ((nversion = dev->stats.proc_stat_version) ==
			    version) {
			    val = tbl4->ent[i4].count;
			}
			spin_unlock(&dev->stats.proc_stat_lock);

			if (nversion != version) {
			    break;
			}
			if (!val) {
			    continue;
			}

			if (count) {
			    proc_id = (i1 << 24 | i2 << 16 | i3 << 8 | i4);
			    if (put_user(proc_id, &ustats->id)) {
				return -EFAULT;
			    }
			    if (put_user(val, &ustats->val)) {
				return -EFAULT;
			    }
			    count--;
			}

			ustats++;
			avail++;
		    }
		}
	    }
	}

	return avail;
    }

    case VRPQ_PROF_RESET:
    {
	VrpqProcStatTbl* tbl1;
	VrpqProcStatTbl* tbl2;
	VrpqProcStatTbl* tbl3;
	VrpqProcStatTbl* tbl4;
	unsigned int     i;
	unsigned int     i1;
	unsigned int     i2;
	unsigned int     i3;

	for (i = 0; (vrpq_prof_map[i].id != -1); i++) {
	    if (vrpq_prof_map[i].id == VRPQ_PROF_PROC_STAT_TBL) {
		continue;
	    }
	    atomic_set(&dev->stats.counts[vrpq_prof_map[i].idx], 0);
	}

	spin_lock(&dev->stats.proc_stat_lock);

	tbl1 = dev->stats.proc_stat_dir;
	dev->stats.proc_stat_dir = NULL;
	dev->stats.proc_stat_version++;

	spin_unlock(&dev->stats.proc_stat_lock);

	if (!tbl1) {
	    return 0;
	}

	for (i1 = 0; i1 < 256; i1++) {
	    tbl2 = tbl1->ent[i1].tbl;
	    if (!tbl2) {
		continue;
	    }
	    for (i2 = 0; i2 < 256; i2++) {
		tbl3 = tbl2->ent[i2].tbl;
		if (!tbl3) {
		    continue;
		}
		for (i3 = 0; i3 < 256; i3++) {
		    tbl4 = tbl3->ent[i3].tbl;
		    if (!tbl4) {
			continue;
		    }
		    kfree(tbl4);
		    atomic_dec(&dev->stats.proc_stat_tbl);
		}
		kfree(tbl3);
		atomic_dec(&dev->stats.proc_stat_tbl);
	    }
 	    kfree(tbl2);
	    atomic_dec(&dev->stats.proc_stat_tbl);
	}
	kfree(tbl1);
	atomic_dec(&dev->stats.proc_stat_tbl);

	return 0;
    }

    }

    return -EINVAL;
}

    /*
     *
     */
    static void
vrpq_prof_init (VrpqCltDev* dev)
{
    spin_lock_init(&dev->stats.proc_stat_lock);
}

#else  /* CONFIG_VRPQ_PROFILE */

#define VRPQ_PROF_PROC(dev, proc_id)

    /*
     *
     */
     static int
vrpq_prof_op (VrpqCltFile* vrpq_file, VrpqProfOp __user* uop, unsigned int sz)
{
    return -ENOSYS;
}

#endif /* CONFIG_VRPQ_PROFILE */

    /*
     *
     */
    static noinline int
vrpq_req_send (VrpqCltChan* chan, VrpqProcId proc_id,
	       VrpqSize in_offset, VrpqCltCall* call)
{
    VrpqReqAlloc* alloc = &chan->dev->req_alloc;
    VrpqReqRing*  ring  = &alloc->ring;
    VrpqRingIdx   head_idx;
    VrpqRingIdx   tail_idx;
    VrpqRingIdx   mod_mask;
    VrpqReqMsg*   req;
    int           diag;

    spin_lock(&alloc->lock);

    VRPQ_PROF_PROC(chan->dev, proc_id);

    head_idx = ring->headIdx;
    tail_idx = ring->tailIdx;
    mod_mask = ring->modMask;

    while (VRPQ_REQ_RING_IS_FULL(head_idx, tail_idx, mod_mask)) {

	VRPQ_PROF_INC(chan->dev, req_ring_full);

	spin_unlock(&alloc->lock);
    
	if ((diag = vrpq_full_wait(chan, tail_idx)) != 0) {
	    return diag;
	}

	spin_lock(&alloc->lock);

	tail_idx = ring->tailIdx;
    }

    req                        = &ring->reqs[head_idx & mod_mask];
    req->msgId                 = head_idx;
    req->chanId                = chan->id;
    req->flags.all             = VRPQ_BYTE_FLAGS_INIT(1, 0, 0, 0);
    req->procReq.procId        = proc_id;
    req->procReq.inOffset      = in_offset;

    if (likely(!call)) {
	req->procReq.outOffset = 0;
	req->cookie            = 0;
    } else{
	req->flags.call        = 1;
	req->procReq.outOffset = call->out_offset;
	req->cookie            = call->idx;
	call->req_id           = req->msgId;
	call->rsp_id           = call->req_id - 1;
	call->req              = req;
    }

    ring->global->headIdx = ring->headIdx = ++head_idx;

    spin_unlock(&alloc->lock);

    return 0;
}

    /*
     *
     */
    static inline int
vrpq_req_post (VrpqCltChan* chan, VrpqProcId proc_id, VrpqSize in_offset)
{
    return vrpq_req_send(chan, proc_id, in_offset, NULL);
}

    /*
     *
     */
    static noinline VrpqCltCall*
vrpq_call_alloc_slow (VrpqCltChan* chan)
{
    VrpqCltCallAlloc* alloc = &chan->dev->call_alloc;
    VrpqCltCall*      call;
    unsigned int      idx;
    int               diag;

    spin_lock(&alloc->lock);

    if (alloc->nr < VRPQ_CALLS_MAX) {
	idx = alloc->nr++;

	spin_unlock(&alloc->lock);

	call = kzalloc(sizeof(VrpqCltCall), GFP_KERNEL);
	if (call) {
	    call->idx = idx;
	    init_waitqueue_head(&call->wait);
	    alloc->calls_dir[idx] = call;
	    return call;
	}

	spin_lock(&alloc->lock);

	alloc->nr--;
    }

    for (;;) {

	call = alloc->free;
	if (call) {
	    break;
	}

	spin_unlock(&alloc->lock);

	diag = wait_event_interruptible(alloc->full_wait,
					(VLINK_SESSION_IS_ABORTED(chan->vls) ||
					 alloc->free));
	if (diag) {
	    return ERR_PTR(diag);
	}

	if (VLINK_SESSION_IS_ABORTED(chan->vls)) {
	    return ERR_PTR(-ECHANBROKEN);
	}

	spin_lock(&alloc->lock);
    }

    alloc->free = call->next;

    spin_unlock(&alloc->lock);

    return call;
}

    /*
     *
     */
    static inline VrpqCltCall*
vrpq_call_alloc (VrpqCltChan* chan)
{
    VrpqCltCallAlloc* alloc = &chan->dev->call_alloc;
    VrpqCltCall*      call;

    spin_lock(&alloc->lock);

    call = alloc->free;
    if (call) {
	alloc->free = call->next;
    }

    spin_unlock(&alloc->lock);

    if (call) {
	return call;
    }
    return vrpq_call_alloc_slow(chan);
}

    /*
     *
     */
    static noinline void
vrpq_call_free (VrpqCltChan* chan, VrpqCltCall* call)
{
    VrpqCltCallAlloc* alloc = &chan->dev->call_alloc;
    unsigned int      was_not_full;

    spin_lock(&alloc->lock);

    was_not_full = (((unsigned long) alloc->free) |
		    (alloc->nr - VRPQ_CALLS_MAX));

    call->next   = alloc->free;
    alloc->free  = call;

    spin_unlock(&alloc->lock);

    if (!was_not_full) {
	wake_up_interruptible(&alloc->full_wait);
    }
}

    /*
     *
     */
    static inline void
vrpq_call_end (VrpqCltChan* chan, VrpqCltCall* call)
{
    call->req->flags.inuse = 0;
    call->req              = NULL;
    vrpq_call_free(chan, call);
}

    /*
     *
     */
    static noinline VrpqCltCall*
vrpq_req_call (VrpqCltChan* chan, VrpqProcId proc_id,
	       VrpqSize in_offset, VrpqSize out_offset)
{
    VrpqCltCall* call;
    int          diag;

    call = vrpq_call_alloc(chan);
    if (IS_ERR(call)) {
	return call;
    }

    call->out_offset = out_offset;

    diag = vrpq_req_send(chan, proc_id, in_offset, call);
    if (diag) {
	vrpq_call_free(chan, call);
	return ERR_PTR(diag);
    }

    vrpq_peer_notify(&chan->dev->peer);

    vrpq_reqs_free(chan->dev);

    diag = wait_event_interruptible(call->wait,
				    (VLINK_SESSION_IS_ABORTED(chan->vls) ||
				     call->rsp_id == call->req_id));

    call->req->cookie = 0;

    if (!diag && VLINK_SESSION_IS_ABORTED(chan->vls)) {
	diag = -ECHANBROKEN;
    }

    if (diag) {
	call->diag = diag;
    }

    return call;
}

    /*
     *
     */
    static void
vrpq_resps_handler (void* cookie, NkXIrq xirq)
{
    VrpqCltDev*   dev = (VrpqCltDev*) cookie;
    VrpqRspRing*  rsp_ring;
    VrpqRingIdx   head_idx;
    VrpqRingIdx   tail_idx;
    VrpqRingIdx   rsp_mod_mask;
    VrpqRspMsg*   resps;
    VrpqRspMsg*   rsp;
    VrpqReqRing*  req_ring;
    VrpqRingIdx   req_mod_mask;
    VrpqReqMsg*   reqs;
    VrpqReqMsg*   req;
    VrpqCltCall** calls_dir;
    VrpqCltCall*  call;
    unsigned int  call_idx;

    VRPQ_PROF_INC(dev, resps_intr);

    rsp_ring     = &dev->rsp_ring;
    head_idx     = rsp_ring->global->headIdx;
    tail_idx     = rsp_ring->tailIdx;
    rsp_mod_mask = rsp_ring->modMask;
    resps        = rsp_ring->resps;

    if (VRPQ_RSP_RING_IS_OVERFLOW(head_idx, tail_idx, rsp_mod_mask)) {
	return;
    }

    req_ring     = &dev->req_alloc.ring;
    req_mod_mask = req_ring->modMask;
    reqs         = req_ring->reqs;

    calls_dir    = dev->call_alloc.calls_dir;

    while (!VRPQ_RSP_RING_IS_EMPTY(head_idx, tail_idx)) {
	rsp           = &resps[tail_idx & rsp_mod_mask];
	req           = &reqs[rsp->reqIdx & req_mod_mask];
	call_idx      = req->cookie;
	if (call_idx >= VRPQ_CALLS_MAX) call_idx  = 0;
	call          = calls_dir[call_idx];
	call->diag    = rsp->retVal;
	call->rsp_id  = req->msgId;
	wake_up_interruptible(&call->wait);
	tail_idx++;
    }

    rsp_ring->global->tailIdx = rsp_ring->tailIdx = tail_idx;
}

    /*
     *
     */
    static void
vrpq_full_wait_handler (void* cookie, NkXIrq xirq)
{
    VrpqCltDev* dev = (VrpqCltDev*) cookie;

    VLINK_DTRACE(dev->gen_dev.vlink, "full wait handler called\n");

    VRPQ_PROF_INC(dev, wait_for_space_intr);

    vrpq_reqs_free(dev);
}

#define VRPQ_PARAM_OFF(p, o)		((void*)(((nku8_f*)(p)) + (o)))

    /*
     *
     */
    static noinline int
vrpq_ref_params_copyin (VrpqProcInfo* proc_info, VrpqParamInfo* param_info,
			unsigned int ref_count)
{
    VrpqSize      in_sz;
    VrpqSize      sz;
    VrpqSize      sza;
    VrpqSize      off;
    VrpqParamRef* ref;
    VrpqParamRef* eref;
    nku8_f*       dst;
    nku8_f*       end;
    void __user*  uptr;

    in_sz = param_info->post.inFullSize;
    end   = VRPQ_PARAM_OFF(param_info, in_sz);

    off   = param_info->post.inRefOffset & ~(VRPQ_PARAM_ALIGN_SIZE - 1);
    ref   = VRPQ_PARAM_OFF(param_info, off);

    off  += ref_count * sizeof(VrpqParamRef);
    eref  = VRPQ_PARAM_OFF(param_info, off);
    dst   = (nku8_f*) eref;

    if ((in_sz > proc_info->paramFullSize) || (off > in_sz)) {
	return -EINVAL;
    }

    do {

	uptr = ref->uptr;
	sz   = ref->size;
	sza  = VRPQ_ROUND_UP(sz, VRPQ_PARAM_ALIGN_SIZE, VrpqSize);

	if (sza > end - dst) {
	    return -EINVAL;
	}

	if (COPYIN(dst, uptr, sz)) {
	    return -EFAULT;
	}

	ref->offset = dst - (nku8_f*)ref;
	dst += sza;

    } while (++ref < eref);

    return 0;
}

    /*
     *
     */
    static inline void*
vrpq_ref_params_prepout (VrpqProcInfo* proc_info, VrpqParamInfo* param_info,
			 VrpqParamRef* ref, unsigned int ref_count)
{
    VrpqParamRef __user* uref;
    VrpqParamRef*        nref;
    VrpqParamRef*        eref;
    VrpqSize             ref_sz;
    VrpqSize             full_sz;
    VrpqSize             sz;
    VrpqSize             sza;
    VrpqSize             off_ref;
    VrpqSize             off_eref;
    void*                out;
    nku8_f*              dst;
    nku8_f*              end;

    off_ref  = param_info->call.inFullSize & ~(VRPQ_PARAM_ALIGN_SIZE - 1);
    nref     = VRPQ_PARAM_OFF(param_info, off_ref);
    out      = nref;

    if (ref_count == 0) {
	return out;
    }

    ref_sz   = ref_count * sizeof(VrpqParamRef);

    off_eref = off_ref + ref_sz;
    eref     = VRPQ_PARAM_OFF(param_info, off_eref);
    dst      = (nku8_f*) eref;

    full_sz  = proc_info->paramFullSize;
    end      = VRPQ_PARAM_OFF(param_info, full_sz);

    if (off_ref > full_sz || off_eref > full_sz) {
	return ERR_PTR(-EINVAL);
    }

    uref = VRPQ_PARAM_OFF(proc_info->paramInfo, param_info->call.outRefOffset);

    if (COPYIN(ref, uref, ref_sz)) {
	return ERR_PTR(-EFAULT);
    }

    do {

	sz   = ref->size;
	sza  = VRPQ_ROUND_UP(sz, VRPQ_PARAM_ALIGN_SIZE, VrpqSize);

	if (sza > end - dst) {
	    return ERR_PTR(-EINVAL);
	}

	nref->size   = sz;
	nref->offset = dst - (nku8_f*)nref;
	ref++;
	dst += sza;

    } while (++nref < eref);

    return out;
}

    /*
     *
     */
    static inline int
vrpq_ref_params_copyout (VrpqParamRef* out, VrpqParamRef* ref,
			 unsigned int ref_count)
{
    VrpqParamRef* eref = ref + ref_count;
    nku8_f*       src  = (nku8_f*) (out + ref_count);
    VrpqSize      sz;
    VrpqSize      sza;

    do {
	sz  = ref->size;
	sza = VRPQ_ROUND_UP(sz, VRPQ_PARAM_ALIGN_SIZE, VrpqSize);

	if (COPYOUT(ref->uptr, src, sz)) {
	    return -EFAULT;
	}

	src += sza;
    } while (++ref < eref);

    return 0;
}

    /*
     *
     */
    static noinline VrpqParamInfo*
vrpq_proc_multi (VrpqCltChan* chan, VrpqProcInfo* procs, unsigned int count)
{
    VrpqProcInfo*   proc_info;
    VrpqProcInfo*   procs_limit;
    VrpqParamAlloc* param_alloc;
    VrpqParamInfo*  param_info;
    VrpqSize        alloc_size;
    VrpqSize        cp_size;
    unsigned int    ref_count;
    void*           pmem_base;
    int             diag;

    param_alloc    = &chan->dev->usr_param_alloc;
    proc_info      = (VrpqProcInfo*)(((unsigned long)procs) & ~1);
    procs_limit    = procs + count - 1;
    pmem_base      = chan->session->pmem_map.kbase;

    do {

	alloc_size = proc_info->paramFullSize;
	cp_size    = proc_info->paramCopySize;

	if (cp_size > alloc_size) {
	    param_info = ERR_PTR(-EINVAL);
	    break;
	}

	param_info = vrpq_param_alloc(chan, param_alloc, alloc_size);
	if (IS_ERR(param_info)) {
	    break;
	}

	if (COPYIN(param_info, proc_info->paramInfo, cp_size)) {
	    diag = -EFAULT;
	    goto out_param_free;
	}

	ref_count = param_info->post.inRefCount;
	if (ref_count) {
	    diag = vrpq_ref_params_copyin(proc_info, param_info, ref_count);
	    if (diag) {
		goto out_param_free;
	    }
	    VRPQ_PROF_ATOMIC_INC(chan->dev, in_param_ref);
	}

	if (proc_info == procs_limit) {
	    break;
	}

	diag = vrpq_req_post(chan,
			     proc_info->procId,
			     VRPQ_PMEM_K2O(param_info->post.in, pmem_base));
	if (diag) {
	    goto out_param_free;
	}

    } while (++proc_info <= procs_limit);

    return param_info;

out_param_free:
    VRPQ_PARAM_FREE(param_info);
    vrpq_params_free(chan->dev);
    return ERR_PTR(diag);
}

#define VRPQ_POST_MULTI(chan, procs, count)				\
    vrpq_proc_multi(chan, (VrpqProcInfo*)(((unsigned long)procs) | 1), count)

#define VRPQ_POST_MULTI_CALL(chan, procs, count)			\
    vrpq_proc_multi(chan, procs, count)

    /*
     *
     */
     static int
vrpq_post_multi (VrpqCltFile*         vrpq_file,
		 VrpqProcInfo __user* uprocs,
		 unsigned int         count)
{
    VrpqCltChan*    chan = &vrpq_file->chan;
    VrpqProcInfo    procs[VRPQ_MULTI_PROCS_MAX];
    VrpqParamInfo*  param_info;
    int             diag;

    if (vrpq_file->type != VRPQ_FILE_CHAN) {
	return -EINVAL;
    }

    if (count == 0 || count > VRPQ_MULTI_PROCS_MAX) {
	return -EINVAL;
    }

    if (COPYIN(procs, uprocs, count * sizeof(VrpqProcInfo))) {
	return -EFAULT;
    }

    if (vlink_session_enter_and_test_alive(chan->vls)) {
	diag = -ECHANBROKEN;
	goto out_session_leave;
    }

    param_info = VRPQ_POST_MULTI(chan, procs, count);
    if (IS_ERR(param_info)) {
	diag = PTR_ERR(param_info);
	goto out_session_leave;
    }

    VRPQ_PROF_ATOMIC_INC(chan->dev, post_multi);
    VRPQ_PROF_ATOMIC_ADD(chan->dev, grouped_posts,     count - 1);
    VRPQ_PROF_ATOMIC_MAX(chan->dev, grouped_posts_max, count - 1);

    diag = 0;

out_session_leave:
    vlink_session_leave(chan->vls);
    return diag;
}

    /*
     *
     */
     static int
vrpq_post_multi_notify (VrpqCltFile*         vrpq_file,
			VrpqProcInfo __user* uprocs,
			unsigned int         count)
{
    VrpqCltChan*    chan = &vrpq_file->chan;
    VrpqProcInfo    procs[VRPQ_MULTI_PROCS_MAX];
    VrpqParamInfo*  param_info;
    int             diag;

    if (vrpq_file->type != VRPQ_FILE_CHAN) {
	return -EINVAL;
    }

    if (count == 0 || count > VRPQ_MULTI_PROCS_MAX) {
	return -EINVAL;
    }

    if (COPYIN(procs, uprocs, count * sizeof(VrpqProcInfo))) {
	return -EFAULT;
    }

    if (vlink_session_enter_and_test_alive(chan->vls)) {
	diag = -ECHANBROKEN;
	goto out_session_leave;
    }

    param_info = VRPQ_POST_MULTI(chan, procs, count);
    if (IS_ERR(param_info)) {
	diag = PTR_ERR(param_info);
	goto out_session_leave;
    }

    VRPQ_PROF_ATOMIC_INC(chan->dev, post_multi_notify);
    VRPQ_PROF_ATOMIC_ADD(chan->dev, posts_per_notify,     count - 1);
    VRPQ_PROF_ATOMIC_MAX(chan->dev, posts_per_notify_max, count - 1);

    vrpq_peer_notify(&chan->dev->peer);
    diag = 0;

out_session_leave:
    vlink_session_leave(chan->vls);
    return diag;
}

    /*
     *
     */
     static int
vrpq_post_multi_call (VrpqCltFile*         vrpq_file,
		      VrpqProcInfo __user* uprocs,
		      unsigned int         count)
{
    VrpqCltChan*         chan = &vrpq_file->chan;
    VrpqProcInfo         procs[VRPQ_MULTI_PROCS_MAX];
    VrpqParamRef         out_refs[VRPQ_OUT_REFS_MAX];
    VrpqProcInfo*        proc_info;
    VrpqParamInfo*       param_info;
    VrpqCltCall*         call;
    VrpqParamRef*        out;
    void*                pmem_base;
    unsigned int         ref_count;
    int                  diag;

    if (vrpq_file->type != VRPQ_FILE_CHAN) {
	return -EINVAL;
    }

    if (count == 0 || count > VRPQ_MULTI_PROCS_MAX) {
	return -EINVAL;
    }

    if (COPYIN(procs, uprocs, count * sizeof(VrpqProcInfo))) {
	return -EFAULT;
    }

    if (vlink_session_enter_and_test_alive(chan->vls)) {
	diag = -ECHANBROKEN;
	goto out_session_leave;
    }

    param_info = VRPQ_POST_MULTI_CALL(chan, procs, count);
    if (IS_ERR(param_info)) {
	diag = PTR_ERR(param_info);
	goto out_session_leave;
    }

    proc_info = &procs[count - 1];
    ref_count = param_info->call.outRefCount;
    if (ref_count > VRPQ_OUT_REFS_MAX) {
	diag = -EINVAL;
	goto out_param_free;
    }

    out = vrpq_ref_params_prepout(proc_info, param_info, out_refs, ref_count);
    if (IS_ERR(out)) {
	diag = PTR_ERR(out);
	goto out_param_free;
    }

    pmem_base = chan->session->pmem_map.kbase;

    call = vrpq_req_call(chan,
			 proc_info->procId,
			 VRPQ_PMEM_K2O(param_info->call.in, pmem_base),
			 VRPQ_PMEM_K2O(out, pmem_base));
    if (IS_ERR(call)) {
	diag = PTR_ERR(call);
	goto out_param_free;
    }

    diag = call->diag;
    if (!diag && ref_count) {
	diag = vrpq_ref_params_copyout(out, out_refs, ref_count);
	if (!diag) {
	    diag = call->diag;
	}
    }

    vrpq_call_end(chan, call);

    VRPQ_PROF_ATOMIC_INC(chan->dev, post_multi_call);
    VRPQ_PROF_ATOMIC_ADD(chan->dev, posts_per_call,     count - 1);
    VRPQ_PROF_ATOMIC_MAX(chan->dev, posts_per_call_max, count - 1);

out_session_leave:
    vlink_session_leave(chan->vls);
    return diag;

out_param_free:
    VRPQ_PARAM_FREE(param_info);
    vrpq_params_free(chan->dev);
    goto out_session_leave;
}

    /*
     *
     */
     static int
vrpq_notify (VrpqCltFile* vrpq_file, void* arg, unsigned int sz)
{
    VrpqCltChan* chan = &vrpq_file->chan;
    VrpqReqRing* ring;
    int          diag;

    if (vrpq_file->type != VRPQ_FILE_CHAN) {
	return -EINVAL;
    }

    if (vlink_session_enter_and_test_alive(chan->vls)) {
	diag = -ECHANBROKEN;
	goto out_session_leave;
    }

    ring = &chan->dev->req_alloc.ring;
    if (!VRPQ_REQ_RING_IS_EMPTY(ring->headIdx, ring->tailIdx)) {
	vrpq_peer_notify(&chan->dev->peer);
    }
    diag = 0;

    VRPQ_PROF_ATOMIC_INC(chan->dev, notify);

out_session_leave:
    vlink_session_leave(chan->vls);

    return diag;
}

    /*
     *
     */
    static noinline void
vrpq_clt_session_init (VrpqCltSession* session, VrpqSessionId id,
		       VrpqCltDev* dev)
{
    VrpqPmemMap* pmem_map = &session->pmem_map;

    memset(session, 0, sizeof(VrpqCltSession));

    session->id          = id;
    session->dev         = dev;

    pmem_map->ubase      = NULL;
    pmem_map->kbase      = dev->gen_dev.pmem_layout.pmemVaddr;
    pmem_map->k2u_offset = 0;

    atomic_set(&session->refcount, 1);
}

    /*
     *
     */
     static int
vrpq_clt_session_create (VrpqCltFile* vrpq_file, void* arg, unsigned int sz)
{
    Vlink*               vlink   = vrpq_file->dev->gen_dev.vlink;
    VrpqCltSession*      session = &vrpq_file->session;
    VrpqSessionCreateIn  in;
    VrpqSessionCreateOut out;
    int                  diag;

    if (down_interruptible(&vrpq_file->lock)) {
	return -ERESTARTSYS;
    }

    if (vrpq_file->type != VRPQ_FILE_NONE) {
	up(&vrpq_file->lock);
	return -EINVAL;
    }

    vrpq_clt_session_init(session, 0, vrpq_file->dev);

    do {

	diag = vlink_session_create(vlink,
				    session,
				    NULL,
				    &session->vls);
	if (diag) {
	    break;
	}

	in.appId = task_tgid_vnr(current);

	diag = VRPQ_ADM_CALL(session, VRPQ_CTRL_SESSION_CREATE, &in, &out);

	if (!diag && ((out.sessionId == 0) ||
		      (out.sessionId >= VRPQ_SESSION_MAX))) {
	    diag = -EINVAL;
	}

	vlink_session_leave(session->vls);

	if (diag) {
	    vlink_session_destroy(session->vls);
	}

    } while (diag == -ECHANBROKEN);

    if (!diag) {
	session->id     = out.sessionId;
	vrpq_file->type = VRPQ_FILE_SESSION;

	VRPQ_PROF_ATOMIC_INC(session->dev, session_create);

	VLINK_DTRACE(vlink,
		     "session #%d created on client side\n",
		     session->id);
    }

    up(&vrpq_file->lock);

    return diag;
}

    /*
     *
     */
    static noinline int
vrpq_clt_session_destroy (VrpqCltSession* session)
{
    VrpqSessionDestroyIn  in;
    VrpqSessionDestroyOut out;
    int                   diag = 0;

    if (vlink_session_enter_and_test_alive(session->vls) == 0) {
	in.sessionId = session->id;
	diag = VRPQ_ADM_CALL(session, VRPQ_CTRL_SESSION_DESTROY, &in, &out);
	vlink_session_leave(session->vls);
    }

    vlink_session_destroy(session->vls);

    VRPQ_PROF_ATOMIC_INC(session->dev, session_destroy);

    VLINK_DTRACE(session->dev->gen_dev.vlink,
		 "session #%d destroyed on client side\n",
		 session->id);

    kfree(VRPQ_CLT_SESSION_TO_FILE(session));

    return diag;
}

    /*
     *
     */
    static inline int
vrpq_clt_session_get (VrpqCltSession* session)
{
    if (!atomic_inc_not_zero(&session->refcount)) {
	return 1;
    }
    return 0;
}

    /*
     *
     */
    static inline int
vrpq_clt_session_put (VrpqCltSession* session)
{
    if (atomic_dec_and_test(&session->refcount)) {
	return vrpq_clt_session_destroy(session);
    }
    return 0;
}

    /*
     *
     */
    static noinline void
vrpq_clt_chan_init (VrpqCltChan* chan, VrpqChanId id, VrpqCltSession* session)
{
    memset(chan, 0, sizeof(VrpqCltChan));

    chan->id      = id;
    chan->dev     = session->dev;
    chan->session = session;
    chan->vls     = session->vls;
}

    /*
     *
     */
     static int
vrpq_clt_chan_create (VrpqCltFile* vrpq_file, unsigned int session_fd,
		      unsigned int sz)
{
    int               fput_needed;
    struct file*      filp;
    unsigned int      minor;
    unsigned int      major;
    VrpqCltDev*       dev;
    VrpqCltFile*      session_vrpq_file;
    VrpqCltSession*   session;
    VrpqCltChan*      chan = &vrpq_file->chan;
    VrpqChanCreateIn  in;
    VrpqChanCreateOut out;
    int               diag;

    if (down_interruptible(&vrpq_file->lock)) {
	return -ERESTARTSYS;
    }

    if (vrpq_file->type != VRPQ_FILE_NONE) {
	up(&vrpq_file->lock);
	return -EINVAL;
    }

    filp = fget_light(session_fd, &fput_needed);
    if (!filp) {
	diag = -EBADF;
	goto out_unlock;
    }

    major = imajor(filp->f_path.dentry->d_inode);
    minor = iminor(filp->f_path.dentry->d_inode);

    dev = (VrpqCltDev*) vrpq_dev_find(major, minor);
    if (dev != vrpq_file->dev) {
	diag = -EINVAL;
	goto out_fput;
    }

    session_vrpq_file = (VrpqCltFile*) filp->private_data;
    if (!session_vrpq_file) {
	diag = -EINVAL;
	goto out_fput;
    }

    if (session_vrpq_file->type != VRPQ_FILE_SESSION) {
	diag = -EINVAL;
	goto out_fput;
    }

    session = &session_vrpq_file->session;

    if (vrpq_clt_session_get(session)) {
	diag = -EINVAL;
	goto out_fput;
    }

    if (vlink_session_enter_and_test_alive(session->vls)) {
	diag = -ECHANBROKEN;
	goto out;
    }

    vrpq_clt_chan_init(chan, 0, session);

    in.sessionId = session->id;

    diag = VRPQ_ADM_CALL(session, VRPQ_CTRL_CHAN_CREATE, &in, &out);

    if (!diag &&  ((out.chanId == 0) || (out.chanId >= VRPQ_CHAN_MAX))) {
	diag = -EINVAL;
    }

out:
    vlink_session_leave(session->vls);

    if (diag) {
	vrpq_clt_session_put(session);
    } else {
	chan->id        = out.chanId;
	vrpq_file->type = VRPQ_FILE_CHAN;

	VRPQ_PROF_ATOMIC_INC(dev, chan_create);

	VLINK_DTRACE(dev->gen_dev.vlink,
		     "channel #%d created on client side\n",
		     chan->id);
    }

out_fput:
    fput_light(filp, fput_needed);
out_unlock:
    up(&vrpq_file->lock);
    return diag;
}

    /*
     *
     */
    static noinline int
vrpq_clt_chan_destroy (VrpqCltChan* chan)
{
    VrpqCltSession*    session = chan->session;
    VrpqChanDestroyIn  in;
    VrpqChanDestroyOut out;
    int                diag         = 0;
    int                diag_session = 0;

    if (vlink_session_enter_and_test_alive(session->vls) == 0) {
	in.chanId = chan->id;
	diag = VRPQ_ADM_CALL(session, VRPQ_CTRL_CHAN_DESTROY, &in, &out);
	vlink_session_leave(session->vls);
    }

    diag_session = vrpq_clt_session_put(chan->session);
    diag = diag ? diag : diag_session;

    VRPQ_PROF_ATOMIC_INC(chan->dev, chan_destroy);

    VLINK_DTRACE(chan->dev->gen_dev.vlink,
		 "channel #%d destroyed on client side\n",
		 chan->id);

    kfree(VRPQ_CLT_CHAN_TO_FILE(chan));

    return diag;
}

    /*
     *
     */
    static noinline int
vrpq_adm_call (VrpqCltSession* session, unsigned int func,
	       VrpqSize in_size, void* in, VrpqSize out_size, void* out)
{
    VrpqCltDev*     dev = session->dev;
    VrpqCltChan     chan;
    VrpqParamAlloc* param_alloc;
    VrpqParamInfo*  param_info;
    VrpqProcId      proc_id;
    VrpqSize        sz;
    VrpqSize        in_full_sz;
    void*           call_out;
    void*           call_in;
    VrpqCltCall*    call;
    void*           pmem_base;
    int             diag;

    if (down_interruptible(&dev->adm_sem)) {
	return -ERESTARTSYS;
    }

    vrpq_clt_chan_init(&chan, 0, session);

    param_alloc = &dev->adm_param_alloc;

    in_full_sz = VRPQ_ROUND_UP(sizeof(VrpqParamCallInfo) + in_size,
			       VRPQ_PARAM_ALIGN_SIZE, unsigned int);

    sz = in_full_sz + out_size;
    if (VRPQ_PARAM_CHUNK_SIZE(sz) > VRPQ_ADM_PARAM_CHUNK_SIZE) {
	diag = -EMSGSIZE;
	goto out_unlock;
    }

    param_info = vrpq_param_alloc(&chan, param_alloc, sz);
    if (IS_ERR(param_info)) {
	diag = PTR_ERR(param_info);
	goto out_unlock;
    }

    param_info->call.inRefOffset  = 0;
    param_info->call.inRefCount   = 0;
    param_info->call.inFullSize   = in_full_sz;
    param_info->call.outRefOffset = 0;
    param_info->call.outRefCount  = 0;

    call_in  = param_info->call.in;
    call_out = VRPQ_PARAM_OFF(param_info, in_full_sz);

    memcpy(call_in, in, in_size);

    proc_id   = VRPQ_PROC_ID(0, func);
    pmem_base = session->pmem_map.kbase;

    call = vrpq_req_call(&chan,
			 proc_id,
			 VRPQ_PMEM_K2O(call_in, pmem_base),
			 VRPQ_PMEM_K2O(call_out, pmem_base));
    if (IS_ERR(call)) {
	VRPQ_PARAM_FREE(param_info);
	diag = PTR_ERR(call); 
	goto out_req_free;
    }

    diag = call->diag;
    if (!diag) {
	memcpy(out, call_out, out_size);
    }

    vrpq_call_end(&chan, call);

    VRPQ_PROF_ATOMIC_INC(dev, adm_calls);

out_req_free:
    vrpq_reqs_free(dev);

    spin_lock(&param_alloc->lock);
    vrpq_params_do_free(&param_alloc->ring);
    spin_unlock(&param_alloc->lock);

out_unlock:
    up(&dev->adm_sem);

    return diag;
}

    /*
     *
     */
    static int
vrpq_clt_open (struct inode* inode, struct file* file)
{
    unsigned int minor = iminor(file->f_path.dentry->d_inode);
    unsigned int major = imajor(file->f_path.dentry->d_inode);
    VrpqDev*     vrpq_gendev;
    VrpqCltDev*  vrpq_dev;
    VrpqCltFile* vrpq_file;

    vrpq_gendev = vrpq_dev_find(major, minor);
    if (!vrpq_gendev) {
	return -ENXIO;
    }
    vrpq_dev = &vrpq_gendev->clt;

    vrpq_file = (VrpqCltFile*) kzalloc(sizeof(VrpqCltFile), GFP_KERNEL);
    if (!vrpq_file) {
	return -ENOMEM;
    }

    vrpq_file->type    = VRPQ_FILE_NONE;
    vrpq_file->dev     = vrpq_dev;
    sema_init(&vrpq_file->lock, 1);
    file->private_data = vrpq_file;

    return 0;
}

    /*
     *
     */
    static int
vrpq_clt_release (struct inode* inode, struct file* file)
{
    VrpqCltFile* vrpq_file = file->private_data;
    int          diag      = 0;

    if (down_interruptible(&vrpq_file->lock)) {
	return -ERESTARTSYS;
    }

    if (vrpq_file->type == VRPQ_FILE_SESSION) {
	diag = vrpq_clt_session_put(&vrpq_file->session);
    } else if (vrpq_file->type == VRPQ_FILE_CHAN) {
	diag = vrpq_clt_chan_destroy(&vrpq_file->chan);
    } else {
	kfree(vrpq_file);
    }

    up(&vrpq_file->lock);

    return diag;
}

    /*
     *
     */
static int vrpq_bad_ioctl (struct VrpqCltFile* vrpq_file, void* arg,
			   unsigned int sz)
{
    VLINK_ERROR(vrpq_file->dev->gen_dev.vlink, "invalid ioctl command\n");
    return -EINVAL;
}

    /*
     *
     */
static VrpqCltIoctl vrpq_clt_ioctl_ops[VRPQ_IOCTL_CMD_MAX] = {
    (VrpqCltIoctl)vrpq_clt_session_create,	/*  0 - SESSION_CREATE       */
    (VrpqCltIoctl)vrpq_clt_chan_create,		/*  1 - CHAN_CREATE          */
    (VrpqCltIoctl)vrpq_notify,			/*  2 - NOTIFY               */
    (VrpqCltIoctl)vrpq_post_multi,		/*  3 - POST_MULTI           */
    (VrpqCltIoctl)vrpq_post_multi_notify,	/*  4 - POST_MULTI_NOTIFY    */
    (VrpqCltIoctl)vrpq_post_multi_call,		/*  5 - POST_MULTI_CALL      */
    (VrpqCltIoctl)vrpq_bad_ioctl,		/*  6 - Unused               */
    (VrpqCltIoctl)vrpq_prof_op,			/*  7 - PROF_OP              */
    (VrpqCltIoctl)vrpq_bad_ioctl,		/*  8 - SESSION_ACCEPT       */
    (VrpqCltIoctl)vrpq_bad_ioctl,		/*  9 - CHAN_ACCEPT          */
    (VrpqCltIoctl)vrpq_bad_ioctl,		/* 10 - SHM_MAP              */
    (VrpqCltIoctl)vrpq_bad_ioctl,		/* 11 - CLIENT_ID_GET        */
    (VrpqCltIoctl)vrpq_bad_ioctl,		/* 12 - RECEIVE              */
    (VrpqCltIoctl)vrpq_bad_ioctl,		/* 13 - CALL_SET_REASON      */
    (VrpqCltIoctl)vrpq_bad_ioctl,		/* 14 - Unused               */
    (VrpqCltIoctl)vrpq_bad_ioctl,		/* 15 - Unused               */
};

    /*
     *
     */
    static int
vrpq_clt_ioctl (struct inode* inode, struct file* file,
		unsigned int cmd, unsigned long arg)
{
    VrpqCltFile* vrpq_file = file->private_data;
    unsigned int nr;
    unsigned int sz;

    nr = _IOC_NR(cmd) & (VRPQ_IOCTL_CMD_MAX - 1);
    sz = _IOC_SIZE(cmd);

    return vrpq_clt_ioctl_ops[nr](vrpq_file, (void*)arg, sz);
}

    /*
     *
     */
    static ssize_t
vrpq_clt_read (struct file* file, char __user* buf, size_t count, loff_t* ppos)
{
    return -EINVAL;
}

    /*
     *
     */
    static ssize_t
vrpq_clt_write (struct file* file, const char __user* buf,
	    size_t count, loff_t* ppos)
{
    return -EINVAL;
}

    /*
     *
     */
    static unsigned int
vrpq_clt_poll(struct file* file, poll_table* wait)
{
    return -ENOSYS;
}

    /*
     *
     */
static struct file_operations vrpq_clt_fops = {
    .owner	= THIS_MODULE,
    .open	= vrpq_clt_open,
    .read	= vrpq_clt_read,
    .write	= vrpq_clt_write,
    .ioctl      = vrpq_clt_ioctl,
    .release	= vrpq_clt_release,
    .poll	= vrpq_clt_poll,
};

    /*
     *
     */
    static int
vrpq_clt_vlink_abort (Vlink* vlink, void* cookie)
{
    VrpqCltDev*       dev        = (VrpqCltDev*) cookie;
    VrpqCltCallAlloc* call_alloc = &dev->call_alloc;
    VrpqCltCall*      call;
    unsigned int      i;

    VLINK_DTRACE(vlink, "vrpq_clt_vlink_abort called\n");

    for (i = 0; i < call_alloc->nr; i++) {
	call = call_alloc->calls_dir[i];
	if (call->req) {
	    wake_up_interruptible(&call->wait);
	}
    }

    wake_up_interruptible(&call_alloc->full_wait);
    wake_up_interruptible(&dev->full_wait);

    return 0;
}

    /*
     *
     */
    static int
vrpq_clt_vlink_reset (Vlink* vlink, void* cookie)
{
    VrpqCltDev*       dev             = (VrpqCltDev*) cookie;
    VrpqReqRing*      req_ring        = &dev->req_alloc.ring;
    VrpqRspRing*      rsp_ring        = &dev->rsp_ring;
    VrpqParamAlloc*   adm_param_alloc = &dev->adm_param_alloc;
    VrpqParamRing*    adm_param_ring  = &adm_param_alloc->ring;
    VrpqParamAlloc*   usr_param_alloc = &dev->usr_param_alloc;
    VrpqParamRing*    usr_param_ring  = &usr_param_alloc->ring;

    VLINK_DTRACE(vlink, "vrpq_clt_vlink_reset called\n");

    req_ring->headIdx          = 0;
    req_ring->tailIdx          = 0;

    req_ring->global->headIdx  = 0;
    req_ring->global->fullFlag = 0;

    rsp_ring->headIdx          = 0;
    rsp_ring->tailIdx          = 0;

    rsp_ring->global->tailIdx  = 0;

    adm_param_ring->head       = adm_param_ring->params;
    adm_param_ring->tail       = adm_param_ring->params;

    usr_param_ring->head       = usr_param_ring->params;
    usr_param_ring->tail       = usr_param_ring->params;

    dev->full_flag             = 0;

    return 0;
}

    /*
     *
     */
    static int
vrpq_clt_vlink_start (Vlink* vlink, void* cookie)
{
    VrpqCltDev* dev = (VrpqCltDev*) cookie;

    VLINK_DTRACE(vlink, "vrpq_clt_vlink_start called\n");

    nkops.nk_xirq_unmask(dev->gen_dev.cxirq);
    nkops.nk_xirq_unmask(dev->gen_dev.cxirq + 1);    

    return 0;
}

    /*
     *
     */
    static int
vrpq_clt_vlink_stop (Vlink* vlink, void* cookie)
{
    VrpqCltDev* dev = (VrpqCltDev*) cookie;

    VLINK_DTRACE(vlink, "vrpq_clt_vlink_stop called\n");

    nkops.nk_xirq_mask(dev->gen_dev.cxirq);
    nkops.nk_xirq_mask(dev->gen_dev.cxirq + 1);

    return 0;
}

    /*
     *
     */
    static int
vrpq_clt_vlink_cleanup (Vlink* vlink, void* cookie)
{
    VrpqCltDev*       dev        = (VrpqCltDev*) cookie;
    VrpqCltCallAlloc* call_alloc = &dev->call_alloc;
    unsigned int      i;

    VLINK_DTRACE(vlink, "vrpq_clt_vlink_cleanup called\n");

    dev->gen_dev.enabled = 0;

    if (call_alloc->calls_dir) {
	if (call_alloc->calls_dir[0]) {
	    kfree(call_alloc->calls_dir[0]);
	}
	for (i = VRPQ_CALLS_MIN; i < call_alloc->nr; i++) {
	    kfree(call_alloc->calls_dir[i]);
	}
	call_alloc->nr = 0;
	kfree(call_alloc->calls_dir);
	call_alloc->calls_dir = NULL;
    }

    vrpq_gen_dev_cleanup(&dev->gen_dev);

    return 0;
}

    /*
     *
     */
static VlinkOpDesc vrpq_clt_vlink_ops[] = {
    { VLINK_OP_RESET,   vrpq_clt_vlink_reset   },
    { VLINK_OP_START,   vrpq_clt_vlink_start   },
    { VLINK_OP_ABORT,   vrpq_clt_vlink_abort   },
    { VLINK_OP_STOP,    vrpq_clt_vlink_stop    },
    { VLINK_OP_CLEANUP, vrpq_clt_vlink_cleanup },
    { 0,                NULL                   },
};

#define VRPQ_PARAMS_LIMIT(r)	\
    VRPQ_PARAM_CHUNK_NEXT((r)->params, (r)->size)

    /*
     *
     */
    int
vrpq_clt_vlink_init (VrpqDrv* vrpq_drv, Vlink* vlink)
{
    VrpqCltDev*       dev             = &vrpq_drv->devs[vlink->unit].clt;
    VrpqReqAlloc*     req_alloc       = &dev->req_alloc;
    VrpqReqRing*      req_ring        = &req_alloc->ring;
    VrpqRspRing*      rsp_ring        = &dev->rsp_ring;
    VrpqParamAlloc*   adm_param_alloc = &dev->adm_param_alloc;
    VrpqParamRing*    adm_param_ring  = &adm_param_alloc->ring;
    VrpqParamAlloc*   usr_param_alloc = &dev->usr_param_alloc;
    VrpqParamRing*    usr_param_ring  = &usr_param_alloc->ring;
    VrpqPmemLayout*   pmem            = &dev->gen_dev.pmem_layout;
    VrpqCltCallAlloc* call_alloc      = &dev->call_alloc;
    VrpqCltCall*      calls_min;
    unsigned int      sz;
    int               diag;
    unsigned int      i;

    VLINK_DTRACE(vlink, "vrpq_clt_vlink_init called\n");

    dev->gen_dev.vrpq_drv = vrpq_drv;
    dev->gen_dev.vlink    = vlink;

    if ((diag = vrpq_gen_dev_init(&dev->gen_dev)) != 0) {
	vrpq_clt_vlink_cleanup(vlink, dev);
	return diag;
    }

    dev->peer.xirq_req          = dev->gen_dev.sxirq;
    dev->peer.osid              = dev->gen_dev.vlink->id_peer;

    req_ring->global            = pmem->reqRingGbl;
    req_ring->modMask           = pmem->reqCount - 1;
    req_ring->reqs              = pmem->reqs;

    rsp_ring->global            = pmem->rspRingGbl;
    rsp_ring->modMask           = pmem->rspCount - 1;
    rsp_ring->resps             = pmem->resps;

    adm_param_ring->size        = pmem->admParamSize;
    adm_param_ring->params      = pmem->admParams;
    adm_param_ring->paramsLimit = VRPQ_PARAMS_LIMIT(adm_param_ring);

    usr_param_ring->size        = pmem->usrParamSize;
    usr_param_ring->params      = pmem->usrParams;
    usr_param_ring->paramsLimit = VRPQ_PARAMS_LIMIT(usr_param_ring);

    sz = VRPQ_CALLS_MAX * sizeof(VrpqCltCall*);
    call_alloc->calls_dir = (VrpqCltCall**) kzalloc(sz, GFP_KERNEL);
    if (!call_alloc->calls_dir) {
	VLINK_ERROR(vlink, "cannot allocate calls directory (%d bytes)\n", sz);
	vrpq_clt_vlink_cleanup(vlink, dev);
	return -ENOMEM;
    }

    sz = VRPQ_CALLS_MIN * sizeof(VrpqCltCall);
    calls_min = (VrpqCltCall*) kzalloc(sz, GFP_KERNEL);
    if (!calls_min) {
	VLINK_ERROR(vlink, "cannot allocate %d call descriptors (%d bytes)\n",
		    VRPQ_CALLS_MIN, sz);
	vrpq_clt_vlink_cleanup(vlink, dev);
	return -ENOMEM;
    }

    call_alloc->calls_dir[0] = calls_min;
    for (i = 1; i < VRPQ_CALLS_MIN; i++) {
	call_alloc->calls_dir[i] = &calls_min[i];
	calls_min[i].next        = call_alloc->free;
	call_alloc->free         = &calls_min[i];
    }
    call_alloc->nr = i;
    for (; i < VRPQ_CALLS_MAX; i++) {
	call_alloc->calls_dir[i] = &calls_min[0];
    }
    for (i = 0; i < VRPQ_CALLS_MIN; i++) {
	call_alloc->calls_dir[i]->idx = i;
	init_waitqueue_head(&call_alloc->calls_dir[i]->wait);
    }

    spin_lock_init(&req_alloc->lock);
    spin_lock_init(&adm_param_alloc->lock);
    spin_lock_init(&usr_param_alloc->lock);
    spin_lock_init(&call_alloc->lock);

    init_waitqueue_head(&call_alloc->full_wait);
    init_waitqueue_head(&dev->full_wait);

    sema_init(&dev->adm_sem, VRPQ_ADM_CALLS_MAX);

    VRPQ_PROF_INIT(dev);

    diag = vrpq_xirq_attach(&dev->gen_dev, dev->gen_dev.cxirq,
			    vrpq_resps_handler);
    if (diag) {
	vrpq_clt_vlink_cleanup(vlink, dev);
	return diag;
    }

    diag = vrpq_xirq_attach(&dev->gen_dev, dev->gen_dev.cxirq + 1,
			    vrpq_full_wait_handler);
    if (diag) {
	vrpq_clt_vlink_cleanup(vlink, dev);
	return diag;
    }

    dev->gen_dev.enabled = 1;

    return 0;
}

    /*
     *
     */
    int
vrpq_clt_drv_init (VlinkDrv* parent_drv, VrpqDrv* vrpq_drv)
{
    int diag;

    vrpq_drv->parent_drv   = parent_drv;
    vrpq_drv->vops         = vrpq_clt_vlink_ops;
    vrpq_drv->fops         = &vrpq_clt_fops;
    vrpq_drv->chrdev_major = 0;
    vrpq_drv->class        = NULL;
    vrpq_drv->devs         = NULL;

    if ((diag = vrpq_gen_drv_init(vrpq_drv)) != 0) {
	vrpq_clt_drv_cleanup(vrpq_drv);
	return diag;
    }

    return 0;
}

    /*
     *
     */
    void
vrpq_clt_drv_cleanup (VrpqDrv* vrpq_drv)
{
    vrpq_gen_drv_cleanup(vrpq_drv);
}

    /*
     *
     */
    static int
vrpq_fe_module_init (void)
{
    return 0;
}

    /*
     *
     */
    static void
vrpq_fe_module_exit (void)
{

}

module_init(vrpq_fe_module_init);
module_exit(vrpq_fe_module_exit);
