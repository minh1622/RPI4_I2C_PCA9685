#Bien dich cheo cho raspberry pi
obj-m += driver_servo.o

KDIR = /lib/modules/$(shell uname -r)/build
MYY_KERNEL_DIR = /home/minh/linux

all:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- M=$(shell pwd) -C $(MYY_KERNEL_DIR) modules

clean:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- M=$(shell pwd) -C $(MYY_KERNEL_DIR) clean
