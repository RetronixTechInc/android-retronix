MKIMG = ../mkimage_imx8

CC ?= gcc
REVISION ?= A0
CFLAGS ?= -O2 -Wall -std=c99 -static
INCLUDE = ./lib

#define the F(Q)SPI header file
QSPI_HEADER = ../scripts/fspi_header
QSPI_PACKER = ../scripts/fspi_packer.sh
PAD_IMAGE = ../scripts/pad_image.sh

ifneq ($(wildcard /usr/bin/rename.ul),)
    RENAME = rename.ul
else
    RENAME = rename
endif


FORCE:

u-boot-hash.bin: u-boot.bin
	./$(MKIMG) -commit > head.hash
	@cat u-boot.bin head.hash > u-boot-hash.bin

u-boot-atf.bin: u-boot-hash.bin bl31.bin
	@cp bl31.bin u-boot-atf.bin
	@dd if=u-boot-hash.bin of=u-boot-atf.bin bs=1K seek=128

u-boot-atf.itb: u-boot-hash.bin bl31.bin
	./$(PAD_IMAGE) bl31.bin
	./$(PAD_IMAGE) u-boot-hash.bin
	./mkimage_fit_atf.sh > u-boot.its;
	./mkimage_uboot -E -p 0x3000 -f u-boot.its u-boot-atf.itb;
	@rm -f u-boot.its

u-boot-atf-container.img: bl31.bin u-boot-hash.bin
	if [ -f tee.bin ]; then \
		if [ $(shell echo $(ROLLBACK_INDEX_IN_CONTAINER)) ]; then \
			./$(MKIMG) -soc QX -sw_version $(ROLLBACK_INDEX_IN_CONTAINER) -rev B0 -c -ap bl31.bin a35 0x80000000 -ap u-boot-hash.bin a35 0x80020000 -ap tee.bin a35 0xFE000000 -out u-boot-atf-container.img; \
		else \
			./$(MKIMG) -soc QX -rev B0 -c -ap bl31.bin a35 0x80000000 -ap u-boot-hash.bin a35 0x80020000 -ap tee.bin a35 0xFE000000 -out u-boot-atf-container.img; \
		fi; \
	else \
	./$(MKIMG) -soc QX -rev B0 -c -ap bl31.bin a35 0x80000000 -ap u-boot-hash.bin a35 0x80020000 -out u-boot-atf-container.img; \
	fi

Image0: Image
	@dd if=Image of=Image0 bs=10M count=1
Image1: Image
	@dd if=Image of=Image1 bs=10M skip=1

.PHONY: clean nightly
clean:
	@rm -f $(MKIMG) $(DCD_CFG) .imx8qx_dcd.cfg.cfgtmp.d u-boot-atf-container.img Image0 Image1
	@rm -rf extracted_imgs
	@echo "imx8qx clean done"

flash: $(MKIMG) mx8qx-ahab-container.img scfw_tcm.bin u-boot-atf.bin
	./$(MKIMG) -soc QX -rev B0 -append mx8qx-ahab-container.img -c -scfw scfw_tcm.bin -ap u-boot-atf.bin a35 0x80000000 -out flash.bin

flash_flexspi: $(MKIMG) mx8qx-ahab-container.img scfw_tcm.bin u-boot-atf.bin $(QSPI_HEADER)
	./$(MKIMG) -soc QX -rev B0 -dev flexspi -append mx8qx-ahab-container.img -c -scfw scfw_tcm.bin -ap u-boot-atf.bin a35 0x80000000 -out flash.bin
	./$(QSPI_PACKER) $(QSPI_HEADER)

flash_nand: $(MKIMG) mx8qx-ahab-container.img scfw_tcm.bin u-boot-atf.bin
	./$(MKIMG) -soc QX -rev B0 -dev nand 16K -append mx8qx-ahab-container.img -c -scfw scfw_tcm.bin -ap u-boot-atf.bin a35 0x80000000 -out flash.bin
	./$(MKIMG) -soc QX -rev B0 -append mx8qx-ahab-container.img -c -scfw scfw_tcm.bin -ap u-boot-atf.bin a35 0x80000000 -out flash_fw.bin

flash_spl: $(MKIMG) mx8qx-ahab-container.img scfw_tcm.bin u-boot-spl.bin u-boot-atf-container.img
	./$(MKIMG) -soc QX -rev B0 -dcd skip -append mx8qx-ahab-container.img -c -scfw scfw_tcm.bin -ap u-boot-spl.bin a35 0x00100000 -out flash.bin
	@flashbin_size=`wc -c flash.bin | awk '{print $$1}'`; \
                   pad_cnt=$$(((flashbin_size + 0x400 - 1) / 0x400)); \
                   echo "append u-boot-atf-container.img at $$pad_cnt KB"; \
                   dd if=u-boot-atf-container.img of=flash.bin bs=1K seek=$$pad_cnt;

flash_spl_flexspi: $(MKIMG) mx8qx-ahab-container.img scfw_tcm.bin u-boot-spl.bin u-boot-atf-container.img
	./$(MKIMG) -soc QX -rev B0 -dcd skip -dev flexspi -append mx8qx-ahab-container.img -c -scfw scfw_tcm.bin -ap u-boot-spl.bin a35 0x00100000 -out flash.bin
	@flashbin_size=`wc -c flash.bin | awk '{print $$1}'`; \
                   pad_cnt=$$(((flashbin_size + 0x400 - 1) / 0x400)); \
                   echo "append u-boot-atf-container.img at $$pad_cnt KB"; \
                   dd if=u-boot-atf-container.img of=flash.bin bs=1K seek=$$pad_cnt;
	./$(QSPI_PACKER) $(QSPI_HEADER)

flash_spl_nand: $(MKIMG) mx8qx-ahab-container.img scfw_tcm.bin u-boot-spl.bin u-boot-atf-container.img
	./$(MKIMG) -soc QX -rev B0 -dcd skip -dev nand 16K -dcd skip -append mx8qx-ahab-container.img -c -scfw scfw_tcm.bin -ap u-boot-spl.bin a35 0x00100000 -out flash.bin
	@flashbin_size=`wc -c flash.bin | awk '{print $$1}'`; \
                   pad_cnt=$$(((flashbin_size + 0x4000 - 1) / 0x4000)); page=16;\
                   echo "append u-boot-atf-container.img at $$((pad_cnt * page))  a $$pad_cnt b $$page KB"; \
                   dd if=u-boot-atf-container.img of=flash.bin bs=1K seek=$$((pad_cnt * page))

flash_linux_m4: $(MKIMG) mx8qx-ahab-container.img scfw_tcm.bin u-boot-atf-container.img m4_image.bin u-boot-spl.bin
	./$(MKIMG) -soc QX -rev B0 -dcd skip -append mx8qx-ahab-container.img -c -flags 0x00200000 -scfw scfw_tcm.bin -ap u-boot-spl.bin a35 0x00100000 -p3 -m4 m4_image.bin 0 0x34FE0000 -out flash.bin
	cp flash.bin boot-spl-container.img
	@flashbin_size=`wc -c flash.bin | awk '{print $$1}'`; \
                   pad_cnt=$$(((flashbin_size + 0x400 - 1) / 0x400)); \
                   echo "append u-boot-atf-container.img at $$pad_cnt KB"; \
                   dd if=u-boot-atf-container.img of=flash.bin bs=1K seek=$$pad_cnt; \

flash_linux_m4_ddr: $(MKIMG) mx8qx-ahab-container.img scfw_tcm.bin u-boot-atf-container.img m4_image.bin u-boot-spl.bin
	./$(MKIMG) -soc QX -rev B0 -append mx8qx-ahab-container.img -c -flags 0x00200000 -scfw scfw_tcm.bin -ap u-boot-spl.bin a35 0x00100000 -p3 -m4 m4_image.bin 0 0x88000000 -out flash.bin
	cp flash.bin boot-spl-container.img
	@flashbin_size=`wc -c flash.bin | awk '{print $$1}'`; \
                   pad_cnt=$$(((flashbin_size + 0x400 - 1) / 0x400)); \
                   echo "append u-boot-atf-container.img at $$pad_cnt KB"; \
                   dd if=u-boot-atf-container.img of=flash.bin bs=1K seek=$$pad_cnt; \

flash_linux_m4_xip: $(MKIMG) mx8qx-ahab-container.img scfw_tcm.bin u-boot-atf-container.img m4_image.bin u-boot-spl.bin
	./$(MKIMG) -soc QX -rev B0 -dcd skip -append mx8qx-ahab-container.img -c -flags 0x00200000 -scfw scfw_tcm.bin -fileoff 0x80000 -p3 -m4 m4_image.bin 0 0x08081000 -fileoff 0x180000 -ap u-boot-spl.bin a35 0x00100000 -out flash.bin
	cp flash.bin boot-spl-container.img
	@flashbin_size=`wc -c flash.bin | awk '{print $$1}'`; \
                   pad_cnt=$$(((flashbin_size + 0x400 - 1) / 0x400)); \
                   echo "append u-boot-atf-container.img at $$pad_cnt KB"; \
                   dd if=u-boot-atf-container.img of=flash.bin bs=1K seek=$$pad_cnt; \
	./$(QSPI_PACKER) $(QSPI_HEADER)

flash_regression_linux_m4: $(MKIMG) mx8qx-ahab-container.img scfw_tcm.bin u-boot-atf.bin m4_image.bin
	./$(MKIMG) -soc QX -rev B0 -append mx8qx-ahab-container.img -c -flags 0x00200000 -scfw scfw_tcm.bin -ap u-boot-atf.bin a35 0x80000000 -p3 -m4 m4_image.bin 0 0x34FE0000 -out flash.bin

flash_regression_linux_m4_ddr: $(MKIMG) mx8qx-ahab-container.img scfw_tcm.bin u-boot-atf.bin m4_image.bin
	./$(MKIMG) -soc QX -rev B0 -append mx8qx-ahab-container.img -c -flags 0x00200000 -scfw scfw_tcm.bin -ap u-boot-atf.bin a35 0x80000000 -p3 -m4 m4_image.bin 0 0x88000000 -out flash.bin

flash_regression_linux_m4_xip : $(MKIMG) mx8qx-ahab-container.img scfw_tcm.bin u-boot-atf.bin m4_image.bin $(QSPI_HEADER)
	./$(MKIMG) -soc QX -rev B0 -dev flexspi -append mx8qx-ahab-container.img -c -flags 0x00200000 -scfw scfw_tcm.bin -fileoff 0x80000 -p3 -m4 m4_image.bin 0 0x08081000 -fileoff 0x180000 -ap u-boot-atf.bin a35 0x80000000 -out flash.bin
	./$(QSPI_PACKER) $(QSPI_HEADER)

flash_scfw: $(MKIMG) mx8qx-ahab-container.img scfw_tcm.bin
	./$(MKIMG) -soc QX -rev B0 -dcd skip -append mx8qx-ahab-container.img -c -scfw scfw_tcm.bin -out flash.bin

flash_kernel: $(MKIMG) Image fsl-imx8qxp-mek.dtb
	./$(MKIMG) -soc QX -rev B0 -c -ap Image a35 0x80280000 --data fsl-imx8qxp-mek.dtb 0x83000000 -out flash.bin

parse_container: $(MKIMG) flash.bin
	./$(MKIMG) -soc QX -rev B0 -parse flash.bin

extract: $(MKIMG) flash.bin
	./$(MKIMG) -soc QX -rev B0 -extract flash.bin


ifneq ($(wildcard scripts/misc.mak),)
$(info include misc.mak)
include scripts/misc.mak
endif

ifneq ($(wildcard scripts/m4.mak),)
$(info include m4.mak)
include scripts/m4.mak
endif

ifneq ($(wildcard scripts/android.mak),)
$(info include android.mak)
include scripts/android.mak
endif

ifneq ($(wildcard scripts/test.mak),)
$(info include test.mak)
include scripts/test.mak
endif

ifneq ($(wildcard scripts/autobuild.mak),)
$(info include autobuild.mak)
include scripts/autobuild.mak
endif

ifneq ($(wildcard scripts/rev_a.mak),)
$(info include rev_a.mak)
include scripts/rev_a.mak
endif

ifneq ($(wildcard scripts/alias.mak),)
$(info include alias.mak)
include scripts/alias.mak
endif
