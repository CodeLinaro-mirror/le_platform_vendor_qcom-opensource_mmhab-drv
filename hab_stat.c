// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "hab.h"
#include "hab_grantable.h"
#include "hab_virq.h"

#define MAX_LINE_SIZE 128

int hab_stat_init(struct hab_driver *driver)
{
	return hab_stat_init_sub(driver);
}

int hab_stat_deinit(struct hab_driver *driver)
{
	return hab_stat_deinit_sub(driver);
}

/*
 * If all goes well the return value is the formated print and concatenated
 * original dest string.
 */
int hab_stat_buffer_print(char *dest,
		int dest_size, const char *fmt, ...)
{
	va_list args;
	char line[MAX_LINE_SIZE];
	int ret;

	va_start(args, fmt);
	ret = vsnprintf(line, sizeof(line), fmt, args);
	va_end(args);
	if (ret > 0)
		ret = (int)strlcat(dest, line, (uint32_t)dest_size);
	return ret;
}

int hab_stat_show_vchan(struct hab_driver *driver,
		char *buf, int size)
{
	int i, ret = 0;

	(void)strscpy(buf, "", (uint32_t)size);
	for (i = 0; i < driver->ndevices; i++) {
		struct hab_device *dev = &driver->devp[i];
		struct physical_channel *pchan;
		struct virtual_channel *vc;

		read_lock_bh(&dev->pchan_lock);
		list_for_each_entry(pchan, &dev->pchannels, node) {
			if (pchan->vcnt == 0)
				continue;

			ret = hab_stat_buffer_print(buf, size,
				"nm %s r %d lc %d rm %d sq_t %d sq_r %d st 0x%x vn %d:\n",
				pchan->name, pchan->is_be, pchan->vmid_local,
				pchan->vmid_remote, pchan->sequence_tx,
				pchan->sequence_rx, pchan->status, pchan->vcnt);

			read_lock(&pchan->vchans_lock);
			list_for_each_entry(vc, &pchan->vchannels, pnode) {
				ret = hab_stat_buffer_print(buf, size,
					"%08X(%d:%d:%lu:%lu:%d) ", vc->id,
					get_refcnt(vc->refcount),
					vc->otherend_closed,
					(unsigned long)vc->tx_cnt,
					(unsigned long)vc->rx_cnt,
					vc->rx_inflight);
			}
			ret = hab_stat_buffer_print(buf, size, "\n");
			read_unlock(&pchan->vchans_lock);
		}
		read_unlock_bh(&dev->pchan_lock);
	}

	return ret;
}

int hab_stat_show_ctx(struct hab_driver *driver,
		char *buf, int size)
{
	int ret = 0;
	struct uhab_context *ctx;
	struct virq_uhab_context *virq_ctx;

	(void)strscpy(buf, "", (uint32_t)size);

	spin_lock_bh(&hab_driver.drvlock);
	ret = hab_stat_buffer_print(buf, size,
			"Total contexts %d\n",
			driver->ctx_cnt);
	list_for_each_entry(ctx, &hab_driver.uctx_list, node) {
		ret = hab_stat_buffer_print(buf, size,
			"ctx %d K %d close %d vc %d exp %d imp %d open %d ref %d\n",
			ctx->owner, ctx->kernel, ctx->closing,
			ctx->vcnt, ctx->export_total,
			ctx->import_total, ctx->pending_cnt,
			get_refcnt(ctx->refcount));
	}

	ret = hab_stat_buffer_print(buf, size,
			"Total virq contexts %d\n",
			driver->virq_ctx_cnt);
	list_for_each_entry(virq_ctx, &hab_driver.virq_uctx_list, node) {
		ret = hab_stat_buffer_print(buf, size,
		"ctx %d K %d virq %d ref %d\n",
			virq_ctx->owner, virq_ctx->kernel,
			virq_ctx->virq_total,
			get_refcnt(virq_ctx->refcount));
	}
	spin_unlock_bh(&hab_driver.drvlock);

	return ret;
}

static uint32_t get_pft_tbl_total_size(struct compressed_pfns *pfn_table)
{
	int i;
	uint32_t total_size = 0U;

	for (i = 0; i < pfn_table->nregions; i++)
		total_size += pfn_table->region[i].size * (uint32_t)PAGE_SIZE;
	return total_size;
}

static int print_ctx_total_expimp(struct uhab_context *ctx,
		char *buf, int size)
{
	struct compressed_pfns *pfn_table = NULL;
	int exp_cnt = 0, imp_cnt = 0;
	struct export_desc *export = NULL;
	struct export_desc_super *exp_super, *exp_super_tmp;
	uint32_t imp_total = 0, exim_size = 0U, exp_total = 0U;
	int ret = 0;

	read_lock(&ctx->exp_lock);
	list_for_each_entry(export, &ctx->exp_whse, node) {
		pfn_table =	(struct compressed_pfns *)export->payload;
		exim_size = get_pft_tbl_total_size(pfn_table);
		exp_total += exim_size;
		exp_cnt++;
	}
	read_unlock(&ctx->exp_lock);

	spin_lock_bh(&ctx->imp_lock);
	hab_rb_for_each_entry(exp_super, exp_super_tmp, &ctx->imp_whse, node) {
		export = &exp_super->exp;
		if (habmm_imp_hyp_map_check(ctx->import_ctx, export) != 0) {
			pfn_table =	(struct compressed_pfns *)export->payload;
			exim_size = get_pft_tbl_total_size(pfn_table);
			imp_total += exim_size;
			imp_cnt++;
		}
	}
	spin_unlock_bh(&ctx->imp_lock);

	if (exp_cnt != 0 || exp_total != 0U || imp_cnt != 0 || imp_total != 0U)
		ret = hab_stat_buffer_print(buf, size,
				"ctx %d exp %d size %d imp %d size %u\n",
				ctx->owner, exp_cnt, exp_total,
				imp_cnt, imp_total);
	else
		return 0;

	read_lock(&ctx->exp_lock);
	ret = hab_stat_buffer_print(buf, size, "export[expid:vcid:size]: ");
	list_for_each_entry(export, &ctx->exp_whse, node) {
		pfn_table =	(struct compressed_pfns *)export->payload;
		exim_size = get_pft_tbl_total_size(pfn_table);
		ret = hab_stat_buffer_print(buf, size,
			"[%d:%x:%d] ", export->export_id,
			export->vcid_local, exim_size);
	}
	ret = hab_stat_buffer_print(buf, size, "\n");
	read_unlock(&ctx->exp_lock);

	spin_lock_bh(&ctx->imp_lock);
	ret = hab_stat_buffer_print(buf, size, "import[expid:vcid:size]: ");
	hab_rb_for_each_entry(exp_super, exp_super_tmp, &ctx->imp_whse, node) {
		export = &exp_super->exp;
		if (habmm_imp_hyp_map_check(ctx->import_ctx, export) != 0) {
			pfn_table =	(struct compressed_pfns *)export->payload;
			exim_size = get_pft_tbl_total_size(pfn_table);
			ret = hab_stat_buffer_print(buf, size,
				"[%d:%x:%d] ", export->export_id,
				export->vcid_local, exim_size);
		}
	}
	ret = hab_stat_buffer_print(buf, size, "\n");
	spin_unlock_bh(&ctx->imp_lock);

	return ret;
}

int hab_stat_show_expimp(struct hab_driver *driver,
		int pid, char *buf, int size)
{
	struct uhab_context *ctx = NULL;
	int ret = 0;
	struct virtual_channel *vchan = NULL;
	uint32_t mmid = 0;
	struct physical_channel *pchans[HABCFG_MMID_NUM];
	int pchan_count = 0;

	(void)driver;

	(void)strscpy(buf, "", (uint32_t)size);

	spin_lock_bh(&hab_driver.drvlock);
	list_for_each_entry(ctx, &hab_driver.uctx_list, node) {
		if (pid == ctx->owner) {
			ret = print_ctx_total_expimp(ctx, buf, size);

			list_for_each_entry(vchan, &ctx->vchannels, node) {
				if (vchan->pchan->habdev->id != mmid) {
					mmid = vchan->pchan->habdev->id;
					pchans[pchan_count++] = vchan->pchan;
					if (pchan_count >= HABCFG_MMID_NUM)
						break;
				}
			}
			break;
		}
	}
	spin_unlock_bh(&hab_driver.drvlock);

	/* print pchannel status, drvlock is not required */
	if (pchan_count > 0)
		ret = hab_stat_log(pchans, pchan_count, buf, size);

	return ret;
}

int hab_stat_show_reclaim(struct hab_driver *driver, char *buf, int size)
{
	struct export_desc *export = NULL;
	struct compressed_pfns *pfn_table = NULL;
	uint32_t exim_size = 0U, total_size = 0U;
	size_t total_num = 0U;

	(void)strscpy(buf, "", (uint32_t)size);
	(void)hab_stat_buffer_print(buf, size, "export[expid:vcid:size:pchan]:\n");

	spin_lock(&hab_driver.reclaim_lock);
	list_for_each_entry(export, &hab_driver.reclaim_list, node) {
		pfn_table = (struct compressed_pfns *)export->payload;
		exim_size = get_pft_tbl_total_size(pfn_table);
		total_size += exim_size;
		total_num++;
		(void)hab_stat_buffer_print(buf, size, "[%d:%x:%d:%s] ",
			export->export_id,
			export->vcid_local,
			exim_size,
			export->pchan->name);
		(void)hab_stat_buffer_print(buf, size, "\n");
	}
	spin_unlock(&hab_driver.reclaim_lock);

	return hab_stat_buffer_print(buf, size, "total: %u, size %u\n", total_num, total_size);
}

int hab_stat_show_virq(struct hab_driver *driver, char *buf, int size)
{
	int ret = 0;
	struct virq_uhab_context *ctx;
	struct hvirq_dbl *dbl = NULL;

	ret = strscpy(buf, "", size);

	spin_lock_bh(&hab_driver.drvlock);
	list_for_each_entry(ctx, &hab_driver.virq_uctx_list, node) {
		list_for_each_entry(dbl, &ctx->virq, node) {
			ret = hab_stat_buffer_print(buf, size,
				"ctx %d virq rx regd %d send %d lbl %d\n",
				ctx->owner,  ctx->virq_total,
				dbl->virq_send, dbl->virtirq_label);
		}
	}
	spin_unlock_bh(&hab_driver.drvlock);

	return ret;
}

#define HAB_PIPE_DUMP_FILE_NAME "/sdcard/habpipe-"
#define HAB_PIPE_DUMP_FILE_EXT ".dat"

#define HAB_PIPEDUMP_SIZE (768*1024*4)
static char *filp;
static int pipedump_idx;

int dump_hab_open(void)
{
	int rc = 0;
	char file_path[256];
	char file_time[100];

	rc = dump_hab_get_file_name(file_time, (int)sizeof(file_time));
	(void)strscpy(file_path, HAB_PIPE_DUMP_FILE_NAME, sizeof(file_path));
	(void)strlcat(file_path, file_time, sizeof(file_path));
	(void)strlcat(file_path, HAB_PIPE_DUMP_FILE_EXT, sizeof(file_path));

	filp = vmalloc((uint32_t)HAB_PIPEDUMP_SIZE);
	if (IS_ERR(filp)) {
		rc = (int)PTR_ERR(filp);
		pr_err("failed to create pipe dump buffer rc %d\n", rc);
		filp = NULL;
	} else {
		pr_info("hab pipe dump buffer opened %s\n", file_path);
		pipedump_idx = 0;
		(void)dump_hab_buf(file_path, strlen(file_path)); /* id first */
	}
	return rc;
}

void dump_hab_close(void)
{
	pr_info("pipe dump content size %d completed\n", pipedump_idx);
	/* transfer buffer ownership to devcoredump */
	filp = NULL;
	pipedump_idx = 0;
}

int dump_hab_buf(void *buf, int size)
{
	if (buf == NULL || size == 0 || size > HAB_PIPEDUMP_SIZE - pipedump_idx) {
		pr_err("wrong parameters buf %pK size %d allowed %d\n",
			 buf, size, HAB_PIPEDUMP_SIZE - pipedump_idx);
		return 0;
	}

	(void)memcpy(&filp[pipedump_idx], buf, size);
	pipedump_idx += size;
	return size;
}

void dump_hab(uint32_t mmid)
{
	struct physical_channel *pchan = NULL;
	int i = 0;
	char str[8] = {'#', '#', '#', '#', '#', '#', '#', '#'};

	(void)dump_hab_open();
	for (i = 0; i < hab_driver.ndevices; i++) {
		struct hab_device *habdev = &hab_driver.devp[i];

		if (habdev->id == mmid) {
			list_for_each_entry(pchan, &habdev->pchannels, node) {
				if (pchan->vcnt > 0) {
					pr_info("***** dump pchan %s vcnt %d *****\n",
						pchan->name, pchan->vcnt);
					hab_pipe_read_dump(pchan);
					break;
				}
			}
			(void)dump_hab_buf(str, 8); /* separator */
		}
	}
	dev_coredumpv(hab_driver.dev[0], filp, (uint32_t)pipedump_idx, GFP_KERNEL);
	dump_hab_close();
}
