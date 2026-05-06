// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/of_device.h>
#include "hab.h"
#include "hab_virq.h"

unsigned int get_refcnt(struct kref ref)
{
	return kref_read(&ref);
}

static int hab_open(struct inode *inodep, struct file *filep)
{
	int result = 0;
	struct uhab_context *ctx;

	ctx = hab_ctx_alloc(0);

	if (ctx == NULL) {
		pr_err("hab_ctx_alloc failed\n");
		filep->private_data = NULL;
		return -ENOMEM;
	}

	ctx->owner = task_pid_nr(current);
	filep->private_data = ctx;
	pr_debug("ctx owner %d refcnt %d\n", ctx->owner,
			get_refcnt(ctx->refcount));

	return result;
}

static int hab_release(struct inode *inodep, struct file *filep)
{
	struct uhab_context *ctx = filep->private_data;
	struct virtual_channel *vchan, *tmp;
	struct hab_open_node *node;

	if (ctx == NULL)
		return 0;

	pr_debug("inode %pK, filep %pK ctx %pK\n", inodep, filep, ctx);

	write_lock(&ctx->ctx_lock);
	/* notify remote side on vchan closing */
	list_for_each_entry_safe(vchan, tmp, &ctx->vchannels, node) {
		/* local close starts */
		vchan->closed = 1;

		list_del(&vchan->node); /* vchan is not in this ctx anymore */
		ctx->vcnt--;

		write_unlock(&ctx->ctx_lock);
		hab_vchan_stop_notify(vchan);
		hab_vchan_put(vchan); /* there is a lock inside */
		write_lock(&ctx->ctx_lock);
	}

	/* notify remote side on pending open */
	list_for_each_entry(node, &ctx->pending_open, node) {
		/* no touch to the list itself. it is allocated on the stack */
		if (hab_open_cancel_notify(&node->request) != 0)
			pr_err("failed to send open cancel vcid %x subid %d openid %d pchan %s\n",
					node->request.xdata.vchan_id,
					node->request.xdata.sub_id,
					node->request.xdata.open_id,
					node->request.pchan->habdev->name);
	}
	write_unlock(&ctx->ctx_lock);

	hab_ctx_put(ctx);
	filep->private_data = NULL;

	return 0;
}

static long hab_copy_data(struct hab_message *msg, struct hab_recv *recv_param)
{
	long ret = 0;
	int i = 0;
	void **scatter_buf = (void **)msg->data;
	uint64_t dest = 0U;

	if (unlikely(msg->scatter)) {
		/* The maximum size of msg is limited in hab_msg_alloc */
		for (i = 0; (unsigned long)i < msg->sizebytes / PAGE_SIZE; i++) {
			dest = (uint64_t)(recv_param->data) + (uint64_t)((unsigned long)i * PAGE_SIZE);
			if (copy_to_user((void __user *)dest,
					scatter_buf[i],
					PAGE_SIZE) != 0U) {
				pr_err("copy_to_user failed: vc=%x size=%d\n",
				recv_param->vcid, (int)msg->sizebytes);
				recv_param->sizebytes = 0;
				ret = -EFAULT;
				break;
			}
		}
		if ((ret != -EFAULT) && ((msg->sizebytes % PAGE_SIZE) != 0U)) {
			dest = (uint64_t)(recv_param->data) + (uint64_t)((unsigned long)i * PAGE_SIZE);
			if (copy_to_user((void __user *)dest,
					scatter_buf[i],
					msg->sizebytes % PAGE_SIZE) != 0U) {
				pr_err("copy_to_user failed: vc=%x size=%d\n",
				recv_param->vcid, (int)msg->sizebytes);
				recv_param->sizebytes = 0;
				ret = -EFAULT;
			}
		}
	} else {
		if (copy_to_user((void __user *)recv_param->data,
				msg->data,
				msg->sizebytes) != 0U) {
			pr_err("copy_to_user failed: vc=%x size=%d\n",
			recv_param->vcid, (int)msg->sizebytes);
			recv_param->sizebytes = 0;
			ret = -EFAULT;
		}
	}

	return ret;
}

static long hab_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	struct uhab_context *ctx = (struct uhab_context *)filep->private_data;
	struct hab_open *open_param;
	struct hab_close *close_param;
	struct hab_recv *recv_param;
	struct hab_send *send_param;
	struct hab_info *info_param;
	struct hab_message *msg = NULL;
	void *send_data;
	unsigned char data[256] = { 0 };
	long ret = 0;
	char names[30] = { 0 };

	if (_IOC_SIZE(cmd) && (cmd & IOC_INOUT)) {
		if (_IOC_SIZE(cmd) > sizeof(data))
			return -EINVAL;

		if ((cmd & IOC_IN) != 0U)
			if (copy_from_user(data, (void __user *)arg, _IOC_SIZE(cmd))) {
				pr_err("copy_from_user failed cmd=%x size=%d\n",
					cmd, _IOC_SIZE(cmd));
				return -EFAULT;
			}
	}

	switch (cmd) {
	case IOCTL_HAB_VC_OPEN:
		open_param = (struct hab_open *)data;
		ret = hab_vchan_open(ctx, open_param->mmid,
			&open_param->vcid,
			(int32_t)open_param->timeout,
			open_param->flags);
		break;
	case IOCTL_HAB_VC_CLOSE:
		close_param = (struct hab_close *)data;
		ret = hab_vchan_close(ctx, close_param->vcid);
		break;
	case IOCTL_HAB_SEND:
		send_param = (struct hab_send *)data;
		if (send_param->sizebytes > (uint32_t)(HAB_HEADER_SIZE_MAX)) {
			ret = -EINVAL;
			break;
		}

		send_data = kzalloc(send_param->sizebytes, GFP_KERNEL);
		if (send_data == NULL) {
			ret = -ENOMEM;
			break;
		}

		if (copy_from_user(send_data, (void __user *)send_param->data,
				send_param->sizebytes) != 0U) {
			ret = -EFAULT;
		} else {
			ret = hab_vchan_send(ctx, send_param->vcid,
						send_param->sizebytes,
						send_data,
						send_param->flags);
		}
		kfree(send_data);
		break;
	case IOCTL_HAB_RECV:
		recv_param = (struct hab_recv *)data;
		if (recv_param->data == NULL) {
			ret = -EINVAL;
			break;
		}

		ret = hab_vchan_recv(ctx, &msg, recv_param->vcid,
				&recv_param->sizebytes, recv_param->timeout,
				recv_param->flags);

		if (msg != NULL) {
			ret = hab_copy_data(msg, recv_param);
			hab_msg_free(msg);
		}

		break;
	case IOCTL_HAB_VC_EXPORT:
		ret = hab_mem_export(ctx, (struct hab_export *)data, 0);
		break;
	case IOCTL_HAB_VC_IMPORT:
		ret = hab_mem_import(ctx, (struct hab_import *)data, 0);
		break;
	case IOCTL_HAB_VC_UNEXPORT:
		ret = hab_mem_unexport(ctx, (struct hab_unexport *)data, 0);
		break;
	case IOCTL_HAB_VC_UNIMPORT:
		ret = hab_mem_unimport(ctx, (struct hab_unimport *)data, 0);
		break;
	case IOCTL_HAB_VC_QUERY:
		info_param = (struct hab_info *)data;
		if ((info_param->names == 0U) || (info_param->namesize == 0U) ||
			info_param->namesize > sizeof(names)) {
			pr_err("wrong param for vm info vcid %X, names %llX, sz %d\n",
					info_param->vcid, info_param->names,
					info_param->namesize);
			ret = -EINVAL;
			break;
		}
		ret = hab_vchan_query(ctx, info_param->vcid,
				(uint64_t *)&info_param->ids,
				 names, info_param->namesize, 0);
		if (ret == 0) {
			if (copy_to_user((void __user *)info_param->names,
						 names,
						 info_param->namesize) != 0U) {
				pr_err("copy_to_user failed: vc=%x size=%d\n",
						info_param->vcid,
						info_param->namesize*2);
				info_param->namesize = 0;
				ret = -EFAULT;
			}
		}
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	if ((ret != -ENOIOCTLCMD) && _IOC_SIZE(cmd) && (cmd & IOC_OUT))
		if (copy_to_user((void __user *) arg, data, _IOC_SIZE(cmd))) {
			pr_err("copy_to_user failed: cmd=%x\n", cmd);
			ret = -EFAULT;
		}

	return ret;
}

static long hab_compat_ioctl(struct file *filep, unsigned int cmd,
	unsigned long arg)
{
	return hab_ioctl(filep, cmd, arg);
}

static const struct file_operations hab_fops = {
	.owner = THIS_MODULE,
	.open = hab_open,
	.release = hab_release,
	.mmap = habmem_imp_hyp_mmap,
	.unlocked_ioctl = hab_ioctl,
	.compat_ioctl = hab_compat_ioctl
};

/*
 * These map sg functions are pass through because the memory backing the
 * sg list is already accessible to the kernel as they come from a the
 * dedicated shared vm pool
 */

static int hab_map_sg(struct device *dev, struct scatterlist *sgl,
	int nelems, enum dma_data_direction dir,
	unsigned long attrs)
{
	/* return nelems directly */
	return nelems;
}

static void hab_unmap_sg(struct device *dev,
	struct scatterlist *sgl, int nelems,
	enum dma_data_direction dir,
	unsigned long attrs)
{
	/*Do nothing */
}

static const struct dma_map_ops hab_dma_ops = {
	.map_sg		= hab_map_sg,
	.unmap_sg	= hab_unmap_sg,
};

static int hab_power_down_callback(
		struct notifier_block *nfb, unsigned long action, void *data)
{

	switch (action) {
	case SYS_DOWN:
	case SYS_HALT:
	case SYS_POWER_OFF:
		pr_debug("reboot called %ld\n", action);
		hab_hypervisor_unregister(); /* only for single VM guest */
		break;
	default:
		pr_debug("unspported action: %ld\n", action);
		break;
	}
	pr_debug("reboot called %ld done\n", action);
	return NOTIFY_DONE;
}

static struct notifier_block hab_reboot_notifier = {
	.notifier_call = hab_power_down_callback,
};

static void reclaim_cleanup(struct work_struct *reclaim_work)
{
	struct export_desc *export = NULL, *exp_tmp = NULL;
	struct export_desc_super *exp_super = NULL;
	struct physical_channel *pchan = NULL;
	LIST_HEAD(free_list);

	pr_debug("reclaim worker called\n");
	spin_lock(&hab_driver.reclaim_lock);
	list_for_each_entry_safe(export, exp_tmp, &hab_driver.reclaim_list, node) {
		exp_super = container_of(export, struct export_desc_super, exp);
		if (exp_super->remote_imported == 0U)
			list_move(&export->node, &free_list);
	}
	spin_unlock(&hab_driver.reclaim_lock);

	list_for_each_entry_safe(export, exp_tmp, &free_list, node) {
		list_del(&export->node);
		exp_super = container_of(export, struct export_desc_super, exp);
		pchan = export->pchan;
		spin_lock_bh(&pchan->expid_lock);
		(void)idr_remove(&pchan->expid_idr, export->export_id);
		spin_unlock_bh(&pchan->expid_lock);
		pr_info("cleanup exp id %u from %s\n", export->export_id, pchan->name);
		habmem_export_put(exp_super);
	}
}

void hab_rb_init(struct rb_root *root)
{
	*root = RB_ROOT;
}

struct export_desc_super *hab_rb_exp_find(struct rb_root *root, struct export_desc_super *key)
{
	struct rb_node *node = root->rb_node;
	struct export_desc_super *exp_super;

	while (node != NULL) {
		exp_super = rb_entry(node, struct export_desc_super, node);
		if (key->exp.export_id < exp_super->exp.export_id)
			node = node->rb_left;
		else if (key->exp.export_id > exp_super->exp.export_id)
			node = node->rb_right;
		else {
			if ((uint64_t)(key->exp.pchan) < (uint64_t)(exp_super->exp.pchan))
				node = node->rb_left;
			else if ((uint64_t)(key->exp.pchan) > (uint64_t)(exp_super->exp.pchan))
				node = node->rb_right;
			else
				return exp_super;
		}
	}

	return NULL;
}

struct export_desc_super *hab_rb_exp_insert(struct rb_root *root, struct export_desc_super *exp_super)
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;

	while (*new != NULL) {
		struct export_desc_super *this = rb_entry(*new, struct export_desc_super, node);
		parent = *new;
		if (exp_super->exp.export_id < this->exp.export_id)
			new = &((*new)->rb_left);
		else if (exp_super->exp.export_id > this->exp.export_id)
			new = &((*new)->rb_right);
		else {
			if ((uint64_t)(exp_super->exp.pchan) < (uint64_t)(this->exp.pchan))
				new = &((*new)->rb_left);
			else if ((uint64_t)(exp_super->exp.pchan) > (uint64_t)(this->exp.pchan))
				new = &((*new)->rb_right);
			else
				/* should not found the target key before insert */
				return this;
		}
	}

	rb_link_node(&exp_super->node, parent, new);
	rb_insert_color(&exp_super->node, root);

	return NULL;
}

int hab_create_cdev_node(int index)
{
	int result;
	dev_t dev_no;

	cdev_init(&(hab_driver.cdev[index]), &hab_fops);

	hab_driver.cdev[index].owner = THIS_MODULE;

	dev_no = MKDEV(hab_driver.major, index);

	result = cdev_add(&(hab_driver.cdev[index]), dev_no, 1);
	if (result) {
		pr_err("cdev_add failed for index %d : %d\n", index, result);
		return result;
	}

	hab_driver.dev[index] = device_create(hab_driver.class, NULL,
			dev_no, &hab_driver, "hab");

	if (IS_ERR_OR_NULL(hab_driver.dev[index])) {
		result = PTR_ERR(hab_driver.dev[index]);
		pr_err("index %d device_create for /dev/hab failed: %d\n",
				index,result);
		cdev_del(&hab_driver.cdev[index]);
		hab_driver.dev[index] = NULL;
		return result;
	}

	pr_debug("create char device for /dev/hab successful\n");
	return 0;
}

/* This flag is used to create 2 Nodes
 * 1. /dev/hab node with which MM functionality will work
 * 2. /dev/hab-virq with which Virtual IRQ functionality will work.
 * seperate node for Virtual IRQ is needed to have a separate ioctls
 * for virq feature and also to have a seperate context when called open
 * ( on /dev/hab-virq). This is done to align with Safety/Security requirement
 * to have an access control on the nodes when called from userspace
 */
#define CDEV_NUM_MAX (2)
static int __init hab_init(void)
{
	int result;
	dev_t dev;
	struct device *device = NULL;
	dev_t dev_no;

	result = alloc_chrdev_region(&dev_no, 0, CDEV_NUM_MAX, "hab");
	if (result < 0) {
		pr_err("alloc_chrdev_region failed: %d\n", result);
		return result;
	}

	hab_driver.major = MAJOR(dev_no);

	hab_driver.dev = kzalloc(sizeof(struct device *) * CDEV_NUM_MAX, GFP_KERNEL);
	if (!hab_driver.dev) {
		unregister_chrdev_region(hab_driver.major, CDEV_NUM_MAX);
		result = -EFAULT;
		return result;
	}

	hab_driver.cdev = kzalloc(sizeof(struct cdev) * CDEV_NUM_MAX, GFP_KERNEL);
	if (!hab_driver.cdev) {
		kfree(hab_driver.dev);
		unregister_chrdev_region(hab_driver.major, CDEV_NUM_MAX);
		result = -EFAULT;
		return result;
	}

	hab_driver.class = class_create("hab");
	if (IS_ERR(hab_driver.class)) {
		result = PTR_ERR(hab_driver.class);
		pr_err("class_create failed: %d\n", result);
		goto exit;
	}

	result = register_reboot_notifier(&hab_reboot_notifier);
	if (result != 0)
		pr_err("failed to register reboot notifier %d\n", result);

	INIT_WORK(&hab_driver.reclaim_work, reclaim_cleanup);

	/* read in hab config, then configure pchans */
	result = do_hab_parse();
	if (result) {
		if (!IS_ERR_OR_NULL(hab_driver.class))
			class_destroy(hab_driver.class);
		goto exit;
	}

	/* Create /dev/hab, index 0 means /dev/hab */
	result = hab_create_cdev_node(0);
	if (result) {
		if (!IS_ERR_OR_NULL(hab_driver.class))
			class_destroy(hab_driver.class);
		goto exit;
	}

	/* Create /dev/hab-virq, index 1 means /dev/hab-virq */
	result = hab_create_virq_cdev_node(1);
	if (result) {
		pr_err("virq node failed result %d cont with hab init\n", result);
		result = 0;
	} else {
		hab_driver.kvirq_ctx = virq_hab_ctx_alloc(1);
                if (hab_driver.kvirq_ctx == NULL)
			pr_err("hab_virq_ctx alloc failed\n");
	}

	if (result == 0) {
		hab_driver.kctx = hab_ctx_alloc(1);
		if (hab_driver.kctx == NULL) {
			pr_err("hab_ctx_alloc failed\n");
			result = -ENOMEM;
			hab_hypervisor_unregister();
			if (hab_driver.kvirq_ctx != NULL)
				virq_hab_ctx_put(hab_driver.kvirq_ctx);
			if (!IS_ERR_OR_NULL(hab_driver.class))
				class_destroy(hab_driver.class);
			goto exit;
		} else {
			/* First, try to configure system dma_ops
			 * Index 0 dev[0] , set it for /dev/hab
			 */
			result = dma_coerce_mask_and_coherent(
					hab_driver.dev[0],
					DMA_BIT_MASK(64));

			/* System dma_ops failed, fallback to dma_ops of hab
			 * Since dma_ops needed for /dev/hab node ,
			 * passing index 0 for the same
			 */
			if (result != 0) {
				pr_warn("config system dma_ops failed %d, fallback to hab\n",
						result);
				hab_driver.dev[0]->bus = NULL;
				set_dma_ops(hab_driver.dev[0], &hab_dma_ops);
			}
		}
	}

	(void)hab_stat_init(&hab_driver);
	return result;

exit:
	kfree(hab_driver.cdev);
	kfree(hab_driver.dev);
	unregister_chrdev_region(hab_driver.major, CDEV_NUM_MAX);
	pr_err("Error in hab init, result %d\n", result);
	return result;
}

static void __exit hab_exit(void)
{
	hab_hypervisor_unregister();
	(void)hab_stat_deinit(&hab_driver);
	hab_ctx_put(hab_driver.kctx);

	if (hab_driver.kvirq_ctx != NULL)
		virq_hab_ctx_put(hab_driver.kvirq_ctx);

	for (int i = 0; i < CDEV_NUM_MAX; i++) {
		if (!IS_ERR_OR_NULL(hab_driver.dev[i])) {
			device_destroy(hab_driver.class, MKDEV(hab_driver.major, i));
			cdev_del(&hab_driver.cdev[i]);
		}
	}
	class_destroy(hab_driver.class);
	unregister_chrdev_region(hab_driver.major, CDEV_NUM_MAX);
	(void)unregister_reboot_notifier(&hab_reboot_notifier);
	pr_info("hab exit called\n");
}

module_init(hab_init);
module_exit(hab_exit);

MODULE_DESCRIPTION("Hypervisor abstraction layer");
MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS(DMA_BUF);
