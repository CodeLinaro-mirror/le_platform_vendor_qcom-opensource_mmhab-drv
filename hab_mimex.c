// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include "hab.h"
#include "hab_grantable.h"

static int hab_import_ack_find(struct uhab_context *ctx,
	struct hab_import_ack *expect_ack, struct virtual_channel *vchan, uint32_t *scan_imp_whse)
{
	int ret = 0;
	struct hab_import_ack_recvd *ack_recvd = NULL, *tmp = NULL;

	spin_lock_bh(&ctx->impq_lock);

	list_for_each_entry_safe(ack_recvd, tmp, &ctx->imp_rxq, node) {
		if (ack_recvd->ack.export_id == expect_ack->export_id &&
		  ack_recvd->ack.vcid_local == expect_ack->vcid_local &&
		  ack_recvd->ack.vcid_remote == expect_ack->vcid_remote) {
			list_del(&ack_recvd->node);
			*scan_imp_whse = ack_recvd->ack.imp_whse_added;
			kfree(ack_recvd);
			ret = 1;
			break;
		}
		ack_recvd->age++;
		if (ack_recvd->age > Q_AGE_THRESHOLD) {
			list_del(&ack_recvd->node);
			kfree(ack_recvd);
		}
	}

	if (!ret && vchan->otherend_closed) {
		pr_info("no expected imp ack, but vchan %x is remotely closed\n", vchan->id);
		ret = 1;
	}

	spin_unlock_bh(&ctx->impq_lock);

	return ret;
}

static int hab_import_ack_wait(struct uhab_context *ctx,
	struct hab_import_ack *import_ack, struct virtual_channel *vchan, uint32_t *scan_imp_whse)
{
	int ret;

	ret = wait_event_interruptible_timeout(ctx->imp_wq,
		hab_import_ack_find(ctx, import_ack, vchan, scan_imp_whse),
		HAB_HS_TIMEOUT);

	if (!ret || (ret == -ERESTARTSYS))
		ret = -EAGAIN;
	else if (vchan->otherend_closed)
		ret = -ENODEV;
	else if (ret > 0)
		ret = 0;

	return ret;
}

/*
 * use physical channel to send export parcel

 * local                      remote
 * send(export)        -->    IRQ store to export warehouse
 * wait(export ack)   <--     send(export ack)

 * the actual data consists the following 3 parts listed in order
 * 1. header (uint32_t) vcid|type|size
 * 2. export parcel (full struct)
 * 3. full contents in export->pdata
 */


static int hab_export_ack_find(struct uhab_context *ctx,
	struct hab_export_ack *expect_ack, struct virtual_channel *vchan)
{
	int ret = 0;
	struct hab_export_ack_recvd *ack_recvd, *tmp;

	spin_lock_bh(&ctx->expq_lock);

	list_for_each_entry_safe(ack_recvd, tmp, &ctx->exp_rxq, node) {
		if ((ack_recvd->ack.export_id == expect_ack->export_id &&
			ack_recvd->ack.vcid_local == expect_ack->vcid_local &&
			ack_recvd->ack.vcid_remote == expect_ack->vcid_remote)
			|| vchan->otherend_closed) {
			list_del(&ack_recvd->node);
			kfree(ack_recvd);
			ret = 1;
			break;
		}
		ack_recvd->age++;
		if (ack_recvd->age > Q_AGE_THRESHOLD) {
			list_del(&ack_recvd->node);
			kfree(ack_recvd);
		}
	}

	spin_unlock_bh(&ctx->expq_lock);

	return ret;
}

static int hab_export_ack_wait(struct uhab_context *ctx,
	struct hab_export_ack *expect_ack, struct virtual_channel *vchan)
{
	int ret;

	ret = wait_event_interruptible_timeout(ctx->exp_wq,
		hab_export_ack_find(ctx, expect_ack, vchan),
		HAB_HS_TIMEOUT);
	if (!ret || (ret == -ERESTARTSYS))
		ret = -EAGAIN;
	else if (vchan->otherend_closed)
		ret = -ENODEV;
	else
		if (ret > 0)
			ret = 0;

	return ret;
}

/*
 * Get id from free list first. if not available, new id is generated.
 * Once generated it will not be erased
 * assumptions: no handshake or memory map/unmap in this helper function
 */
struct export_desc_super *habmem_add_export(
		struct virtual_channel *vchan,
		int sizebytes,
		uint32_t flags)
{
	struct export_desc *export = NULL;
	struct export_desc_super *exp_super = NULL;

	if (!vchan || !sizebytes)
		return NULL;

	exp_super = vzalloc(sizebytes);
	if (!exp_super)
		return NULL;

	export = &exp_super->exp;
	idr_preload(GFP_KERNEL);
	spin_lock_bh(&vchan->pchan->expid_lock);
	/* using cyclic way to match with BE side */
	export->export_id =
		idr_alloc_cyclic(&vchan->pchan->expid_idr, export, 1, 0, GFP_NOWAIT);
	spin_unlock_bh(&vchan->pchan->expid_lock);
	idr_preload_end();

	export->readonly = flags;
	export->vcid_local = vchan->id;
	export->vcid_remote = vchan->otherend_id;
	export->domid_local = vchan->pchan->vmid_local;
	export->domid_remote = vchan->pchan->vmid_remote;

	/*
	 * In new protocol, exp_desc will not be sent to remote during hab export.
	 * Below pointers are required for local usage and will be removed before sending.
	 */
	if (vchan->pchan->mem_proto == 1) {
		export->vchan = vchan;
		export->ctx = vchan->ctx;
		export->pchan = vchan->pchan;
	}

	return exp_super;
}

void habmem_remove_export(struct export_desc *export)
{
	struct uhab_context *ctx = NULL;
	struct export_desc_super *exp_super =
			container_of(export,
				struct export_desc_super,
				exp);

	if (!export || !export->ctx) {
		if (export)
			pr_err("invalid info in exp %pK ctx %pK\n",
			   export, export->ctx);
		else
			pr_err("invalid exp\n");
		return;
	}

	ctx = export->ctx;
	write_lock(&ctx->exp_lock);
	ctx->export_total--;
	write_unlock(&ctx->exp_lock);
	export->ctx = NULL;

	habmem_export_put(exp_super);
}

static void habmem_export_destroy(struct kref *refcount)
{
	struct export_desc_super *exp_super =
			container_of(
				refcount,
				struct export_desc_super,
				refcount);

	if (!exp_super) {
		pr_err("invalid exp_super\n");
		return;
	}

	(void)habmem_exp_release(exp_super);
	vfree(exp_super);
}

/*
 * store the parcel to the warehouse, then send the parcel to remote side
 * both exporter composed export descriptor and the grantrefids are sent
 * as one msg to the importer side
 */
static int habmem_export_vchan(struct uhab_context *ctx,
		struct virtual_channel *vchan,
		int payload_size,
		uint32_t flags,
		uint32_t export_id)
{
	int ret;
	struct export_desc *export = NULL;
	struct export_desc_super *exp_super = NULL;
	uint32_t sizebytes = sizeof(*export) + payload_size;
	struct hab_export_ack expected_ack = {0};
	struct hab_header header = HAB_HEADER_INITIALIZER;

	if (sizebytes > (uint32_t)HAB_HEADER_SIZE_MAX) {
		pr_err("exp message too large, %u bytes, max is %d\n",
			sizebytes, HAB_HEADER_SIZE_MAX);
		return -EINVAL;
	}

	spin_lock_bh(&vchan->pchan->expid_lock);
	export = idr_find(&vchan->pchan->expid_idr, export_id);
	spin_unlock_bh(&vchan->pchan->expid_lock);
	if (!export) {
		pr_err("export vchan failed: exp_id %d, pchan %s\n",
				export_id, vchan->pchan->name);
		return -EINVAL;
	}

	pr_debug("sizebytes including exp_desc: %u = %zu + %d\n",
		sizebytes, sizeof(*export), payload_size);

	/* exp_desc will not be sent to remote during export in new protocol */
	if (vchan->pchan->mem_proto == 0) {
		HAB_HEADER_SET_SIZE(header, sizebytes);
		HAB_HEADER_SET_TYPE(header, HAB_PAYLOAD_TYPE_EXPORT);
		HAB_HEADER_SET_ID(header, vchan->otherend_id);
		HAB_HEADER_SET_SESSION_ID(header, vchan->session_id);
		ret = physical_channel_send(vchan->pchan, &header, export);

		if (ret != 0) {
			pr_err("failed to send imp msg %d, exp_id %d, vcid %x\n",
				ret, export_id, vchan->id);
			return ret;
		}

		expected_ack.export_id = export->export_id;
		expected_ack.vcid_local = export->vcid_local;
		expected_ack.vcid_remote = export->vcid_remote;
		ret = hab_export_ack_wait(ctx, &expected_ack, vchan);
		if (ret != 0) {
			pr_err("failed to receive remote export ack %d on vc %x\n",
					ret, vchan->id);
			return ret;
		}

		export->pchan = vchan->pchan;
		export->vchan = vchan;
		export->ctx = ctx;
	}

	write_lock(&ctx->exp_lock);
	ctx->export_total++;
	list_add_tail(&export->node, &ctx->exp_whse);
	write_unlock(&ctx->exp_lock);

	exp_super = container_of(export, struct export_desc_super, exp);
	WRITE_ONCE(exp_super->exp_state, HAB_EXP_SUCCESS);

	return ret;
}

void habmem_export_get(struct export_desc_super *exp_super)
{
	kref_get(&exp_super->refcount);
}

void habmem_export_put(struct export_desc_super *exp_super)
{
	(void)kref_put(&exp_super->refcount, habmem_export_destroy);
}

int hab_mem_export(struct uhab_context *ctx,
		struct hab_export *param,
		int kernel)
{
	int ret = 0;
	unsigned int payload_size = 0;
	uint32_t export_id = 0;
	struct virtual_channel *vchan;
	int page_count;
	int compressed = 0;

	if (!ctx || !param || !param->sizebytes
		|| ((param->sizebytes % PAGE_SIZE) != 0)
		|| (!param->buffer && !(HABMM_EXPIMP_FLAGS_FD & param->flags))
		)
		return -EINVAL;

	param->exportid = 0;
	vchan = hab_get_vchan_fromvcid(param->vcid, ctx, 0);
	if (!vchan || !vchan->pchan) {
		ret = -ENODEV;
		goto err;
	}

	page_count = param->sizebytes/PAGE_SIZE;
	if (kernel) {
		ret = habmem_hyp_grant(vchan,
			(unsigned long)param->buffer,
			page_count,
			param->flags,
			vchan->pchan->dom_id,
			&compressed,
			&payload_size,
			&export_id);
	} else {
		ret = habmem_hyp_grant_user(vchan,
			(unsigned long)param->buffer,
			page_count,
			param->flags,
			vchan->pchan->dom_id,
			&compressed,
			&payload_size,
			&export_id);
	}
	if (ret < 0) {
		pr_err("habmem_hyp_grant vc %x failed size=%d ret=%d\n",
			   param->vcid, payload_size, ret);
		goto err;
	}

	ret = habmem_export_vchan(ctx,
		vchan,
		payload_size,
		param->flags,
		export_id);

	param->exportid = export_id;
err:
	if (vchan)
		hab_vchan_put(vchan);
	return ret;
}

int hab_mem_unexport(struct uhab_context *ctx,
		struct hab_unexport *param,
		int kernel)
{
	int ret = 0;
	struct export_desc *export = NULL;
	struct export_desc_super *exp_super = NULL;
	struct virtual_channel *vchan;

	if (!ctx || !param)
		return -EINVAL;

	/* refcnt on the access */
	vchan = hab_get_vchan_fromvcid(param->vcid, ctx, 1);
	if (!vchan || !vchan->pchan) {
		ret = -ENODEV;
		goto err_novchan;
	}

	spin_lock_bh(&vchan->pchan->expid_lock);
	export = idr_find(&vchan->pchan->expid_idr, param->exportid);
	if (!export) {
		spin_unlock_bh(&vchan->pchan->expid_lock);
		pr_err("unexp fail, cannot find exp id %d\n", param->exportid);
		ret = -EINVAL;
		goto err_novchan;
	}

	exp_super = container_of(export, struct export_desc_super, exp);
	if (exp_super->exp_state == HAB_EXP_SUCCESS &&
			export->ctx == ctx &&
			exp_super->remote_imported == 0)
		(void)idr_remove(&vchan->pchan->expid_idr, param->exportid);
	else {
		ret = exp_super->remote_imported == 0 ? -EINVAL : -EBUSY;
		pr_err("unexp exp id %d fail, exp state %d, remote imp %d\n",
				param->exportid, exp_super->exp_state, exp_super->remote_imported);
		spin_unlock_bh(&vchan->pchan->expid_lock);
		goto err_novchan;
	}
	spin_unlock_bh(&vchan->pchan->expid_lock);

	/* TODO: hab stat is not accurate after idr_remove and before list_del here */
	write_lock(&ctx->exp_lock);
	list_del(&export->node);
	write_unlock(&ctx->exp_lock);

	ret = habmem_hyp_revoke(export->payload, export->payload_count);
	if (ret) {
		/* unrecoverable scenario*/
		pr_err("Error found in revoke grant with ret %d\n", ret);
		goto err_novchan;
	}
	habmem_remove_export(export);

err_novchan:
	if (vchan)
		hab_vchan_put(vchan);

	return ret;
}

int hab_mem_import(struct uhab_context *ctx,
		struct hab_import *param,
		int kernel)
{
	int ret = 0, found = 0;
	struct export_desc *export = NULL;
	struct export_desc_super *exp_super = NULL, key = {0};
	struct virtual_channel *vchan = NULL;
	struct hab_header header = HAB_HEADER_INITIALIZER;
	struct hab_import_ack expected_ack = {0};
	struct hab_import_data imp_data = {0};
	uint32_t scan_imp_whse = 0U;

	if (!ctx || !param)
		return -EINVAL;

	if ((param->sizebytes % PAGE_SIZE) != 0) {
		pr_err("request imp size %ld is not page aligned on vc %x\n",
			param->sizebytes, param->vcid);
		return -EINVAL;
	}

	vchan = hab_get_vchan_fromvcid(param->vcid, ctx, 0);
	if (!vchan || !vchan->pchan) {
		ret = -ENODEV;
		goto err_imp;
	}

	if (vchan->pchan->mem_proto == 1) {
		/* send import sync message to the remote side */
		imp_data.exp_id = param->exportid;
		imp_data.page_cnt = param->sizebytes >> PAGE_SHIFT;
		HAB_HEADER_SET_SIZE(header, sizeof(struct hab_import_data));
		HAB_HEADER_SET_TYPE(header, HAB_PAYLOAD_TYPE_IMPORT);
		HAB_HEADER_SET_ID(header, vchan->otherend_id);
		HAB_HEADER_SET_SESSION_ID(header, vchan->session_id);
		ret = physical_channel_send(vchan->pchan, &header, &imp_data);

		if (ret != 0) {
			pr_err("failed to send imp msg %d, exp_id %d, vcid %x\n",
				ret,
				param->exportid,
				vchan->id);
			goto err_imp;
		}

		expected_ack.export_id = param->exportid;
		expected_ack.vcid_local = vchan->id;
		expected_ack.vcid_remote = vchan->otherend_id;
		ret = hab_import_ack_wait(ctx, &expected_ack, vchan, &scan_imp_whse);
		if (ret != 0) {
			pr_err("failed to receive remote import ack %d on vc %x\n", ret, vchan->id);
			goto err_imp;
		}

		if (!scan_imp_whse) {
			ret = -EINVAL;
			pr_err("imp_ack_fail msg recv on vc %x\n", vchan->id);
			goto err_imp;
		}
	}

	key.exp.export_id = param->exportid;
	key.exp.pchan = vchan->pchan;
	spin_lock_bh(&ctx->imp_lock);
	exp_super = hab_rb_exp_find(&ctx->imp_whse, &key);
	if (exp_super) {
		/* not allowed to import one exp desc more than once */
		if (exp_super->import_state == EXP_DESC_IMPORTED
			|| exp_super->import_state == EXP_DESC_IMPORTING) {
			export = &exp_super->exp;
			pr_err("vc %x not allowed to import one expid %u more than once\n",
					vchan->id, export->export_id);
			spin_unlock_bh(&ctx->imp_lock);
			ret = -EINVAL;
			goto err_imp;
		}
		/*
		 * set the flag to avoid another thread getting the exp desc again
		 * and must be before unlock, otherwise it is no use.
		 */
		exp_super->import_state = EXP_DESC_IMPORTING;
		found = 1;
	} else {
		spin_unlock_bh(&ctx->imp_lock);
		pr_err("Fail to get export descriptor from export id %d vcid %x\n",
			param->exportid, vchan->id);
		ret = -ENODEV;
		goto err_imp;
	}
	spin_unlock_bh(&ctx->imp_lock);

	export = &exp_super->exp;
	if ((export->payload_count << PAGE_SHIFT) != param->sizebytes) {
		pr_err("input size %d don't match buffer size %d\n",
			param->sizebytes, export->payload_count << PAGE_SHIFT);
		ret = -EINVAL;
		exp_super->import_state = EXP_DESC_INIT;
		goto err_imp;
	}

	ret = habmem_imp_hyp_map(ctx->import_ctx, param, export, kernel);

	if (ret) {
		pr_err("Import fail ret:%d pcnt:%d rem:%d 1st_ref:0x%X\n",
			ret, export->payload_count,
			export->domid_local, *((uint32_t *)export->payload));
		exp_super->import_state = EXP_DESC_INIT;
		goto err_imp;
	}

	export->import_index = param->index;
	export->kva = kernel ? (void *)param->kva : NULL;
	exp_super->import_state = EXP_DESC_IMPORTED;

err_imp:
	if (vchan) {
		if ((vchan->pchan != NULL) &&
			(vchan->pchan->mem_proto == 1) &&
			(found == 1) &&
			(ret != 0)) {
			/* dma_buf create failure, rollback required */
			hab_send_unimport_msg(vchan, export->export_id);

			spin_lock_bh(&ctx->imp_lock);
			hab_rb_remove(&ctx->imp_whse, exp_super);
			ctx->import_total--;
			spin_unlock_bh(&ctx->imp_lock);

			kfree(exp_super);
		}
		hab_vchan_put(vchan);
	}

	return ret;
}

int hab_mem_unimport(struct uhab_context *ctx,
		struct hab_unimport *param,
		int kernel)
{
	int ret = 0, found = 0;
	struct export_desc *export = NULL;
	struct export_desc_super *exp_super = NULL, key = {0};
	struct virtual_channel *vchan;

	if (!ctx || !param)
		return -EINVAL;

	vchan = hab_get_vchan_fromvcid(param->vcid, ctx, 1);
	if (!vchan || !vchan->pchan) {
		if (vchan)
			hab_vchan_put(vchan);
		return -ENODEV;
	}

	key.exp.export_id = param->exportid;
	key.exp.pchan = vchan->pchan;
	spin_lock_bh(&ctx->imp_lock);
	exp_super = hab_rb_exp_find(&ctx->imp_whse, &key);
	if (exp_super) {
		/* only successfully imported export desc could be found and released */
		if (exp_super->import_state == EXP_DESC_IMPORTED) {
			hab_rb_remove(&ctx->imp_whse, exp_super);
			ctx->import_total--;
			found = 1;
		} else
			pr_err("vc %x exp id:%u status:%d is found, invalid to unimport\n",
					vchan->id, exp_super->exp.export_id, exp_super->import_state);
	}
	spin_unlock_bh(&ctx->imp_lock);

	if (!found) {
		ret = -EINVAL;
		pr_err("exp id %u unavailable on vc %x\n", param->exportid, vchan->id);
	} else {
		export = &exp_super->exp;
		ret = habmm_imp_hyp_unmap(ctx->import_ctx, export, kernel);
		if (ret) {
			pr_err("unmap fail id:%d pcnt:%d vcid:%d\n",
			export->export_id, export->payload_count, export->vcid_remote);
		}
		param->kva = (uint64_t)export->kva;
		if (vchan->pchan->mem_proto == 1)
			hab_send_unimport_msg(vchan, export->export_id);
		kfree(exp_super);
	}

	if (vchan)
		hab_vchan_put(vchan);

	return ret;
}
