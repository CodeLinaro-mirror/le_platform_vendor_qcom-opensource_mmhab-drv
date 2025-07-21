HAB_ROOT :=$(PWD)

INC_DIR := $(GUNYAH_DRIVERS_SYSROOT_INCDIR)

LINUXINCLUDE += -I$(HAB_ROOT)/include \
		-I$(HAB_ROOT)/include/uapi \
		-I$(HAB_ROOT)/ \
		-I$(HAB_ROOT)/hypervisor \
		-I$(HAB_ROOT)/os \
		-I$(KERNEL_SRC)/drivers/vhost \
                -I$(KERNEL_SRC)/include \
		-I$(KERNEL_SRC)/include/uapi/linux

EXTRA_CFLAGS += -DCONFIG_MSM_VHOST_HAB=1 \
		-DCONFIG_GH_DBL=1 \
		-DCONFIG_MSM_VIRQ_HAB=1 \
		-DCONFIG_MSM_HAB_DEFAULT_VMID=2 \
                -Iinclude/linux \
		-I$(INC_DIR) \

obj-m += msm_hab.o

msm_hab-y += hab.o \
	hab_msg.o \
	hab_vchan.o \
	hab_pchan.o \
	hab_open.o \
	hab_mimex.o \
	hab_pipe.o \
	hab_parser.o \
	hab_virq.o \
	hab_linux.o \
	hab_stat.o

msm_hab-y += os/linux/khab.o \
	     os/linux/hab_mem_linux.o \
	     os/linux/khab_test.o

#ifdef CONFIG_MSM_VIRQ_HAB
msm_hab-y += hypervisor/virtio/hab_virq_hgy.o
#endif
#ifdef CONFIG_MSM_VHOST_HAB
msm_hab-y += hypervisor/virtio/hab_vhost.o
#endif
