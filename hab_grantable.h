/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 */
#ifndef __HAB_GRANTABLE_H
#define __HAB_GRANTABLE_H

/* Grantable should be common between exporter and importer */
struct grantable {
	unsigned long pfn;
};

struct compressed_pfns {
	unsigned long first_pfn;
	int nregions;
	struct region {
		unsigned int size; /* number of the pages in current region */
		/*
		 * gap(# of pfn) between the last page of current region and
		 * the first page of the next region. The gap can be a negative value.
		 *
		 * for instance, pfns in region[0]:
		 * starting pfn: first_pfn
		 * end pfn: first_pfn + region[0].size - 1
		 *
		 * pfns in region[1]:
		 * starting pfn: first_pfn + region[0].size - 1 + space
		 * end pfn: above pfn + region[1].size - 1
		 */
		int space;
	} region[];
};
#endif /* __HAB_GRANTABLE_H */
