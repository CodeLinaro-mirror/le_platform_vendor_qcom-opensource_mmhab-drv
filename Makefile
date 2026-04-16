# SPDX-License-Identifier: GPL-2.0

HAB_ROOT=$(M)

INSTALL_HDR=$(shell mkdir -p $(HDR_INSTAL_PATH)/linux; \
 			 cd $(KERNEL_SRC);  \
 			 $(KERNEL_SRC)/scripts/headers_install.sh \
 			 $(PWD)/include/uapi/linux/habmmid.h $(HDR_INSTAL_PATH)/linux/habmmid.h; \
 			 $(KERNEL_SRC)/scripts/headers_install.sh \
 			 $(PWD)/include/uapi/linux/hab_ioctl.h $(HDR_INSTAL_PATH)/linux/hab_ioctl.h; \
 			 [ -f $(HDR_INSTAL_PATH)/linux/habmmid.h ] && [ -f $(HDR_INSTAL_PATH)/linux/hab_ioctl.h ] && echo "passed" || echo "failed")

all: clean modules

modules:
	$(MAKE) -C "$(KERNEL_SRC)" "M=$(M)" modules

modules_install:
	$(MAKE) -C "$(KERNEL_SRC)" "M=$(M)" modules_install

clean:
	$(MAKE) -C "$(KERNEL_SRC)" "M=$(M)" clean

headers_install:
	@echo "header installing $(INSTALL_HDR)"
