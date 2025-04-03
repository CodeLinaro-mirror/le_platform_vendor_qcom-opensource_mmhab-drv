/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2016-2018, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef _HAB_IOCTL_H
#define _HAB_IOCTL_H

#include <linux/types.h>

struct hab_send {
	__u64 data;
	__s32 vcid;
	__u32 sizebytes;
	__u32 flags;
};

struct hab_recv {
	__u64 data;
	__s32 vcid;
	__u32 sizebytes;
	__u32 timeout;
	__u32 flags;
};

struct hab_open {
	__s32 vcid;
	__u32 mmid;
	__u32 timeout;
	__u32 flags;
};

struct hab_close {
	__s32 vcid;
	__u32 flags;
};

struct hab_export {
	__u64 buffer;
	__s32 vcid;
	__u32 sizebytes;
	__u32 exportid;
	__u32 flags;
};

struct hab_import {
	__u64 index;
	__u64 kva;
	__s32 vcid;
	__u32 sizebytes;
	__u32 exportid;
	__u32 flags;
};

struct hab_unexport {
	__s32 vcid;
	__u32 exportid;
	__u32 flags;
};


struct hab_unimport {
	__s32 vcid;
	__u32 exportid;
	__u64 kva;
	__u32 flags;
};

struct hab_info {
	__s32 vcid;
	__u64 ids; /* high part remote; low part local */
	__u64 names;
	__u32 namesize; /* single name length */
	__u32 flags;
};

struct vhost_hab_config {
	__u8 vm_name[32];
};

struct vhost_config {
        __u32 offset;
        __u32 size;
        __u32 flags;
        __u8 *data;
};

#define HAB_IOC_TYPE 0x0AU

#define IOCTL_HAB_SEND \
	_IOW(HAB_IOC_TYPE, 0x2U, struct hab_send)

#define IOCTL_HAB_RECV \
	_IOWR(HAB_IOC_TYPE, 0x3U, struct hab_recv)

#define IOCTL_HAB_VC_OPEN \
	_IOWR(HAB_IOC_TYPE, 0x4U, struct hab_open)

#define IOCTL_HAB_VC_CLOSE \
	_IOW(HAB_IOC_TYPE, 0x5U, struct hab_close)

#define IOCTL_HAB_VC_EXPORT \
	_IOWR(HAB_IOC_TYPE, 0x6U, struct hab_export)

#define IOCTL_HAB_VC_IMPORT \
	_IOWR(HAB_IOC_TYPE, 0x7U, struct hab_import)

#define IOCTL_HAB_VC_UNEXPORT \
	_IOW(HAB_IOC_TYPE, 0x8U, struct hab_unexport)

#define IOCTL_HAB_VC_UNIMPORT \
	_IOW(HAB_IOC_TYPE, 0x9U, struct hab_unimport)

#define IOCTL_HAB_VC_QUERY \
	_IOWR(HAB_IOC_TYPE, 0xAU, struct hab_info)

#define VHOST_SET_CONFIG _IOW(VHOST_VIRTIO, 0x70U, struct vhost_config)

#endif /* _HAB_IOCTL_H */
