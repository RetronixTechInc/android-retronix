

ifeq ($(PLATFORM), IMX8MM)
all:
	make -f Makefile_G1G2
	make -f Makefile_H1
install:
	make -f Makefile_H1 install
	make -f Makefile_G1G2 install
clean:
	make -f Makefile_G1G2 clean
	make -f Makefile_H1 clean
else
all:
	make -f Makefile_G1G2
install:
	make -f Makefile_G1G2 install
clean:
	make -f Makefile_G1G2 clean
endif

