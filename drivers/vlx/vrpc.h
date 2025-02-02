/*
 ****************************************************************
 *
 *  Component:	VirtualLogix VRPC driver interface
 *
 *  Copyright (C) 2010-2011, VirtualLogix. All Rights Reserved.
 *
 *  Contributor(s):
 *    Vladimir Grouzdev <vladimir.grouzdev@virtuallogix.com>
 *
 ****************************************************************
 */

#ifndef _VRPC_H_
#define _VRPC_H_

typedef unsigned int vrpc_size_t;

struct vrpc_t;

#define	VRPC_PMEM_BASE	1	/* first pmem_alloc ID available to driver */
#define	VRPC_PXIRQ_BASE	2	/* first pxirq_alloc ID available to driver */

   typedef void
(*vrpc_ready_t) (void* cookie);

   typedef vrpc_size_t
(*vrpc_call_t) (void* cookie, vrpc_size_t size);

    extern struct vrpc_t*
vrpc_server_lookup (const char* name, struct vrpc_t* last)
#if defined __GNUC__ && defined linux
    __attribute__((nonnull (1)))
    __must_check
#endif
    ;

    extern struct vrpc_t*
vrpc_client_lookup (const char* name, struct vrpc_t* last)
#if defined __GNUC__ && defined linux
    __attribute__((nonnull (1)))
    __must_check
#endif
    ;

    extern void
vrpc_release (struct vrpc_t* vrpc)
#if defined __GNUC__ && defined linux
    __attribute__((nonnull (1)))
#endif
    ;

    extern NkOsId
vrpc_peer_id (struct vrpc_t* vrpc)
#if defined __GNUC__ && defined linux
    __attribute__((nonnull (1)))
    __must_check
#endif
    ;

    extern NkPhAddr
vrpc_plink (struct vrpc_t* vrpc)
#if defined __GNUC__ && defined linux
    __attribute__((nonnull (1)))
    __must_check
#endif
    ;

    extern NkDevVlink*
vrpc_vlink (struct vrpc_t* vrpc)
#if defined __GNUC__ && defined linux
    __attribute__((nonnull (1)))
    __must_check
#endif
    ;

    extern void*
vrpc_data (struct vrpc_t* vrpc)
#if defined __GNUC__ && defined linux
    __attribute__((nonnull (1)))
    __must_check
#endif
    ;

    extern vrpc_size_t
vrpc_maxsize (struct vrpc_t* vrpc)
#if defined __GNUC__ && defined linux
    __attribute__((nonnull (1)))
    __must_check
#endif
    ;

    extern int
vrpc_server_open (struct vrpc_t* vrpc, vrpc_call_t call, void* cookie,
		  int direct)
#if defined __GNUC__ && defined linux
    __attribute__((nonnull (1,2)))
    __must_check
#endif
    ;

    extern int
vrpc_client_open (struct vrpc_t* vrpc, vrpc_ready_t ready, void* cookie)
#if defined __GNUC__ && defined linux
    __attribute__((nonnull (1)))
    __must_check
#endif
    ;

    extern int
vrpc_call (struct vrpc_t* vrpc, vrpc_size_t* size)
#if defined __GNUC__ && defined linux
    __attribute__((nonnull (1,2)))
    __must_check
#endif
    ;

    extern void
vrpc_close (struct vrpc_t* vrpc)
#if defined __GNUC__ && defined linux
    __attribute__((nonnull (1)))
#endif
    ;

    extern const char*
vrpc_info (struct vrpc_t* vrpc)
#if defined __GNUC__ && defined linux
    __attribute__((nonnull (1)))
#endif
    ;

#endif /* _VRPC_H_ */
