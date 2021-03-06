/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * GPL HEADER START
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; If not, see
 * http://www.sun.com/software/products/lustre/docs/GPLv2.pdf
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *
 * GPL HEADER END
 */
/*
 * Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.
 * Use is subject to license terms.
 * Copyright (c) 2011 Whamcloud, Inc.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre is a trademark of Sun Microsystems, Inc.
 *
 * lustre/fld/fld_internal.h
 *
 * Author: Yury Umanets <umka@clusterfs.com>
 * Author: Tom WangDi <wangdi@clusterfs.com>
 */
#ifndef __FLD_INTERNAL_H
#define __FLD_INTERNAL_H

#include <lustre/lustre_idl.h>
#include <dt_object.h>

#include <libcfs/libcfs.h>
#include <lustre_req_layout.h>
#include <lustre_fld.h>

enum {
        LUSTRE_FLD_INIT = 1 << 0,
        LUSTRE_FLD_RUN  = 1 << 1
};

struct fld_stats {
        __u64   fst_count;
        __u64   fst_cache;
        __u64   fst_inflight;
};

typedef int (*fld_hash_func_t) (struct lu_client_fld *, __u64);

typedef struct lu_fld_target *
(*fld_scan_func_t) (struct lu_client_fld *, __u64);

struct lu_fld_hash {
        const char              *fh_name;
        fld_hash_func_t          fh_hash_func;
        fld_scan_func_t          fh_scan_func;
};

struct fld_cache_entry {
        cfs_list_t               fce_lru;
        cfs_list_t               fce_list;
        /**
         * fld cache entries are sorted on range->lsr_start field. */
        struct lu_seq_range      fce_range;
};

struct fld_cache {
        /**
         * Cache guard, protects fci_hash mostly because others immutable after
         * init is finished.
         */
        cfs_spinlock_t           fci_lock;

        /**
         * Cache shrink threshold */
        int                      fci_threshold;

        /**
         * Prefered number of cached entries */
        int                      fci_cache_size;

        /**
         * Current number of cached entries. Protected by \a fci_lock */
        int                      fci_cache_count;

        /**
         * LRU list fld entries. */
        cfs_list_t               fci_lru;

        /**
         * sorted fld entries. */
        cfs_list_t               fci_entries_head;

        /**
         * Cache statistics. */
        struct fld_stats         fci_stat;

        /**
         * Cache name used for debug and messages. */
        char                     fci_name[80];
};

enum fld_op {
        FLD_CREATE = 0,
        FLD_DELETE = 1,
        FLD_LOOKUP = 2
};

enum {
        /* 4M of FLD cache will not hurt client a lot. */
        FLD_SERVER_CACHE_SIZE      = (4 * 0x100000),

        /* 1M of FLD cache will not hurt client a lot. */
        FLD_CLIENT_CACHE_SIZE      = (1 * 0x100000)
};

enum {
        /* Cache threshold is 10 percent of size. */
        FLD_SERVER_CACHE_THRESHOLD = 10,

        /* Cache threshold is 10 percent of size. */
        FLD_CLIENT_CACHE_THRESHOLD = 10
};

extern struct lu_fld_hash fld_hash[];

#ifdef __KERNEL__

struct fld_thread_info {
        struct req_capsule *fti_pill;
        __u64               fti_key;
        struct lu_seq_range fti_rec;
        struct lu_seq_range fti_lrange;
        struct lu_seq_range fti_irange;
};


struct thandle *fld_trans_create(struct lu_server_fld *fld,
                                const struct lu_env *env);
int fld_trans_start(struct lu_server_fld *fld,
                    const struct lu_env *env, struct thandle *th);

void fld_trans_stop(struct lu_server_fld *fld,
                    const struct lu_env *env, struct thandle* th);

int fld_index_init(struct lu_server_fld *fld,
                   const struct lu_env *env,
                   struct dt_device *dt);

void fld_index_fini(struct lu_server_fld *fld,
                    const struct lu_env *env);

int fld_index_create(struct lu_server_fld *fld,
                     const struct lu_env *env,
                     const struct lu_seq_range *range,
                     struct thandle *th);

int fld_index_delete(struct lu_server_fld *fld,
                     const struct lu_env *env,
                     struct lu_seq_range *range,
                     struct thandle *th);

int fld_index_lookup(struct lu_server_fld *fld,
                     const struct lu_env *env,
                     seqno_t seq, struct lu_seq_range *range);

int fld_client_rpc(struct obd_export *exp,
                   struct lu_seq_range *range, __u32 fld_op);

#ifdef LPROCFS
extern struct lprocfs_vars fld_server_proc_list[];
extern struct lprocfs_vars fld_client_proc_list[];
#endif

#endif

struct fld_cache *fld_cache_init(const char *name,
                                 int cache_size, int cache_threshold);

void fld_cache_fini(struct fld_cache *cache);

void fld_cache_flush(struct fld_cache *cache);

void fld_cache_insert(struct fld_cache *cache,
                      const struct lu_seq_range *range);

void fld_cache_delete(struct fld_cache *cache,
                      const struct lu_seq_range *range);

int fld_cache_lookup(struct fld_cache *cache,
                     const seqno_t seq, struct lu_seq_range *range);

static inline const char *
fld_target_name(struct lu_fld_target *tar)
{
        if (tar->ft_srv != NULL)
                return tar->ft_srv->lsf_name;

        return (const char *)tar->ft_exp->exp_obd->obd_name;
}

extern cfs_proc_dir_entry_t *fld_type_proc_dir;

#endif /* __FLD_INTERNAL_H */
