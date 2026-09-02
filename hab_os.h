/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef __HAB_OS_H
#define __HAB_OS_H

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "hab:%s:%d " fmt, __func__, __LINE__

#include <linux/types.h>

#include <linux/habmm.h>
#include <linux/hab_ioctl.h>

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rbtree.h>
#include <linux/idr.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/dma-map-ops.h>
#include <linux/jiffies.h>
#include <linux/reboot.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/devcoredump.h>
#include <linux/freezer.h>
#include <linux/interval_tree.h>
#ifdef CONFIG_MSM_VHOST_HAB
#include <linux/gunyah/gh_vm_addr_translation.h>
#endif

void hab_rb_init(struct rb_root *root);

#define HAB_RB_PROTOTYPE(rb_name, type, node, cmp)	\
struct export_desc_super *hab_rb_exp_insert(struct rb_root *root, struct export_desc_super *exp_super); \
struct export_desc_super *hab_rb_exp_find(struct rb_root *root, struct export_desc_super *key);

#define hab_rb_remove(root, pos) rb_erase(&(pos)->node, root)
#define hab_rb_min(root, type, node) rb_entry_safe(rb_first(root), type, node)
#define hab_rb_max(root, type, node) rb_entry_safe(rb_last(root), type, node)
#define hab_rb_for_each_entry(pos, n, head, member)	\
	rbtree_postorder_for_each_entry_safe(pos, n, head, member)
#define HAB_RB_ENTRY struct rb_node
#define HAB_RB_ROOT struct rb_root

#if defined(CONFIG_MSM_VHOST_HAB) || defined(CONFIG_MSM_VIRTIO_HAB)
#include <asm/arch_timer.h>
static inline unsigned long long msm_timer_get_sclk_ticks(void)
{
	return __arch_counter_get_cntpct();
}
#elif IS_ENABLED(CONFIG_MSM_BOOT_TIME_MARKER)
#include <soc/qcom/boot_stats.h>
#else
static inline unsigned long long msm_timer_get_sclk_ticks(void)
{
	return 0;
}
#endif

#ifdef CONFIG_MSM_VHOST_HAB
#define spin_lock_bh spin_lock
#define spin_unlock_bh spin_unlock

#define write_lock_bh write_lock
#define write_unlock_bh write_unlock

#define read_lock_bh read_lock
#define read_unlock_bh read_unlock
#endif

#ifdef CONFIG_MSM_VHOST_HAB
int hab_vm_addr_translate(struct vm_addr_rgn_table *gvm_addr_rgn_tbl,
                          struct vm_addr_rgn_table *pvm_addr_rgn_table,
                          int hab_vmid, bool *output_in_pvm_addr_rgn_tbl);
struct vm_addr_rgn_table *hab_vm_addr_rgn_table_alloc(unsigned int nents);
void hab_vm_addr_rgn_table_free(struct vm_addr_rgn_table *vm_ipa);
#else
struct vm_addr_rgn_table {
    char _unused;
};

static inline int hab_vm_addr_translate(struct vm_addr_rgn_table *gvm_addr_rgn_tbl,
                                 struct vm_addr_rgn_table *pvm_addr_rgn_table,
                                 int hab_vmid, bool *output_in_pvm_addr_rgn_tbl)
{
	return 0;
}

static inline struct vm_addr_rgn_table *hab_vm_addr_rgn_table_alloc(unsigned int nents)
{
    return NULL;
}

static inline void hab_vm_addr_rgn_table_free(struct vm_addr_rgn_table *vm_ipa) {};
#endif

#endif /*__HAB_OS_H*/
