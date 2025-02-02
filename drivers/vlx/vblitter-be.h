/*
 ****************************************************************
 *
 * Copyright (C) 2010 VirtualLogix inc. All Rights Reserved.
 *
 ****************************************************************
 */

#ifndef _VBLITTER_BE_H_
#define _VBLITTER_BE_H_

#include <vlx/vblitter_common.h>
#include "vpmem.h"

typedef unsigned long vblitter_hw_handle_t;

typedef struct vblitter_hw_ops {
    unsigned int  version;
    int (*open)   (unsigned int hw_id, vpmem_handle_t vpmem, vblitter_hw_handle_t* hw_handle);
    int (*release)(vblitter_hw_handle_t hw_handle);
    int (*blit)   (vblitter_hw_handle_t hw_handle, vblit_blit_req_t* req);
} vblitter_hw_ops_t;

#define VBLITTER_HW_OPS_VERSION 1

// Hardware blitter driver entry point
extern int vblitter_hw_ops_init (vblitter_hw_ops_t** hw_ops);

// Helper functions
unsigned int vblitter_get_format_bpp    (unsigned int fmt);
const char*  vblitter_get_format_str    (unsigned int fmt);
char*        vblitter_get_transform_str (unsigned int flags);

#endif // _VBLITTER_BE_H_
