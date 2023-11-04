obj-m += lkm_device.o
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean

tests: all
	python3 -m tests BasicTestCases -f

tests-extra: all
	python3 -m tests -f
