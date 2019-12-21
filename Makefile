module_name = s2fs
obj-m+=s2fs.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
mount:
	sudo insmod $(module_name).ko
	sudo mount -t $(module_name) nodev mnt
remove:
	-sudo rmmod $(module_name)