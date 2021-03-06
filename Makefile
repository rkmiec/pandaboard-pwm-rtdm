MODULES = pwm

KSRC ?= /lib/modules/$(shell uname -r)/build

OBJS     := ${patsubst %, %.o, $(MODULES)}
CLEANMOD := ${patsubst %, .%*, $(MODULES)}
PWD      := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)

obj-m        := $(OBJS)
EXTRA_CFLAGS := -I$(KSRC)/include/xenomai 
EXTRA_CFLAGS += -I$(KSRC)/include/xenomai/posix $(ADD_CFLAGS)

all::
	$(MAKE) -C $(KSRC) M=$(PWD) modules

clean::
	$(RM) $(CLEANMOD) *.o *.ko *.mod.c Module*.symvers 
	$(RM) Module.markers modules.order
	$(RM) -R .tmp*
