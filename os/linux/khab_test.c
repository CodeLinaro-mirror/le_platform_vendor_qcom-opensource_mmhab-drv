// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include "hab.h"
#include <linux/rtc.h>

static struct kobject *hab_kobject;

static int vchan_stat;
static int context_stat;
static int pid_stat;

static ssize_t vchan_show(struct kobject *kobj, struct kobj_attribute *attr,
						char *buf)
{
	return hab_stat_show_vchan(&hab_driver, buf, PAGE_SIZE);
}

static ssize_t vchan_store(struct kobject *kobj, struct kobj_attribute *attr,
						const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%du", &vchan_stat);
	if (ret < 1) {
		pr_err("failed to read anything from input %d\n", ret);
		return 0;
	} else
		return count;
}

static ssize_t ctx_show(struct kobject *kobj, struct kobj_attribute *attr,
						char *buf)
{
	return hab_stat_show_ctx(&hab_driver, buf, PAGE_SIZE);
}

static ssize_t ctx_store(struct kobject *kobj, struct kobj_attribute *attr,
						const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%du", &context_stat);
	if (ret < 1) {
		pr_err("failed to read anything from input %d\n", ret);
		return 0;
	} else
		return count;
}

static ssize_t expimp_show(struct kobject *kobj, struct kobj_attribute *attr,
						char *buf)
{
	return hab_stat_show_expimp(&hab_driver, pid_stat, buf, PAGE_SIZE);
}

static ssize_t expimp_store(struct kobject *kobj, struct kobj_attribute *attr,
						const char *buf, size_t count)
{
	int ret = -1;
	char str[36] = {0};
	struct uhab_context *ctx = NULL;
	struct virtual_channel *vchan = NULL;

	if (buf) {
		ret = sscanf(buf, "%35s", str);
		if (ret < 1) {
			pr_err("failed to read anything from input %d\n", ret);
			return -EINVAL;
		}
	} else
		return -EINVAL;

	if (strnlen(str, strlen("dump_pipe")) == strlen("dump_pipe") &&
		strcmp(str, "dump_pipe") == 0) {
		/* string terminator is ignored */
		list_for_each_entry(ctx, &hab_driver.uctx_list, node) {
			if (ctx->owner == pid_stat) {
				vchan = list_first_entry(&ctx->vchannels,
					struct virtual_channel, node);
				if (vchan) {
					dump_hab_wq(vchan->pchan); /* user context */
					break;
				}
			}
		}
		return count;
	}

	ret = sscanf(buf, "%du", &pid_stat);
	if (ret < 1)
		pr_err("failed to read anything from input %d\n", ret);
	else
		return count; /* good result stored */
	return -EEXIST;
}

static struct kobj_attribute vchan_attribute = __ATTR(vchan_stat, 0660,
								vchan_show,
								vchan_store);

static struct kobj_attribute ctx_attribute = __ATTR(context_stat, 0660,
								ctx_show,
								ctx_store);

static struct kobj_attribute expimp_attribute = __ATTR(pid_stat, 0660,
								expimp_show,
								expimp_store);

int hab_stat_init_sub(struct hab_driver *driver)
{
	int result;

	hab_kobject = kobject_create_and_add("hab", kernel_kobj);
	if (!hab_kobject)
		return -ENOMEM;

	result = sysfs_create_file(hab_kobject, &vchan_attribute.attr);
	if (result)
		pr_debug("cannot add vchan in /sys/kernel/hab %d\n", result);

	result = sysfs_create_file(hab_kobject, &ctx_attribute.attr);
	if (result)
		pr_debug("cannot add ctx in /sys/kernel/hab %d\n", result);

	result = sysfs_create_file(hab_kobject, &expimp_attribute.attr);
	if (result)
		pr_debug("cannot add expimp in /sys/kernel/hab %d\n", result);

	return result;
}

int hab_stat_deinit_sub(struct hab_driver *driver)
{
	sysfs_remove_file(hab_kobject, &vchan_attribute.attr);
	sysfs_remove_file(hab_kobject, &ctx_attribute.attr);
	sysfs_remove_file(hab_kobject, &expimp_attribute.attr);
	kobject_put(hab_kobject);

	return 0;
}

int dump_hab_get_file_name(char *file_time, int ft_size)
{
	struct timespec64 time = {0};
	unsigned long local_time;
	struct rtc_time tm;

	ktime_get_real_ts64(&time);
	local_time = (unsigned long)(time.tv_sec - sys_tz.tz_minuteswest * 60);
	rtc_time64_to_tm(local_time, &tm);

	snprintf(file_time, ft_size, "%04d_%02d_%02d-%02d_%02d_%02d",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
		tm.tm_min, tm.tm_sec);

	return 0;
}

