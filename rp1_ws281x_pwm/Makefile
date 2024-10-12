MOD = rp1_ws281x_pwm
KPATH :=/lib/modules/$(shell uname -r)/build
PWD :=$(shell pwd)

obj-m = rp1_ws281x_pwm.o

all:
	$(MAKE) -C $(KPATH) M=$(PWD) modules

clean:
	$(MAKE) -C $(KPATH) M=$(PWD) clean

install:
	$(MAKE) -C $(KPATH) M=$(PWD) install

