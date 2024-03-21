HAB_ROOT :=$(PWD)

LINUXINCLUDE += -I$(HAB_ROOT)/include \
		-I$(HAB_ROOT)/include/uapi \
		-I$(HAB_ROOT)/ \
		-I$(HAB_ROOT)/hypervisor \
		-I$(HAB_ROOT)/os \
		-I$(KERNEL_SRC)/drivers/vhost \
                -I$(KERNEL_SRC)/include \
		-I$(KERNEL_SRC)/include/uapi/linux

EXTRA_CFLAGS += -DCONFIG_MSM_VHOST_HAB=1 \
		-DCONFIG_MSM_HAB_DEFAULT_VMID=2 \
                -Iinclude/linux \

obj-m += msm_hab.o

msm_hab-y += hab.o \
	hab_msg.o \
	hab_vchan.o \
	hab_pchan.o \
	hab_open.o \
	hab_mimex.o \
	hab_pipe.o \
	hab_parser.o \
        hab_linux.o \
	hab_stat.o

msm_hab-y += os/linux/khab.o \
	     os/linux/hab_mem_linux.o \
	     os/linux/khab_test.o 

#ifdef CONFIG_MSM_VHOST_HAB
msm_hab-y += hypervisor/virtio/hab_vhost.o
#endif

