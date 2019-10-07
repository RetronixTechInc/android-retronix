flash_b0_xen_uboot: u-boot-hash.bin
	./$(MKIMG) -soc QM -rev B0 -c -ap u-boot-hash.bin a53 0x81080000 -out u-boot-xen-container.img \

flash_b0_spl_container_m4_1_trusty: $(MKIMG) mx8qm-ahab-container.img scfw_tcm.bin u-boot-atf-container.img m4_1_image.bin tee.bin u-boot-spl.bin
	./$(MKIMG) -soc QM -rev B0 -append mx8qm-ahab-container.img -c -flags 0x01200000 -scfw scfw_tcm.bin -p4 -m4 m4_1_image.bin 1 0x88800000 -ap u-boot-spl.bin a53 0x00100000 -out flash.bin
	cp flash.bin boot-spl-container.img
	@flashbin_size=`wc -c flash.bin | awk '{print $$1}'`; \
                   pad_cnt=$$(((flashbin_size + 0x400 - 1) / 0x400)); \
                   echo "append u-boot-atf-container.img at $$pad_cnt KB"; \
                   dd if=u-boot-atf-container.img of=flash.bin bs=1K seek=$$pad_cnt; \

flash_b0_spl_container_m4_1: $(MKIMG) mx8qm-ahab-container.img scfw_tcm.bin u-boot-atf-container.img m4_1_image.bin u-boot-spl.bin
	./$(MKIMG) -soc QM -rev B0 -append mx8qm-ahab-container.img -c -flags 0x01200000 -scfw scfw_tcm.bin -p4 -m4 m4_1_image.bin 1 0x88800000 -ap u-boot-spl.bin a53 0x00100000 -out flash.bin
	cp flash.bin boot-spl-container.img
	@flashbin_size=`wc -c flash.bin | awk '{print $$1}'`; \
                   pad_cnt=$$(((flashbin_size + 0x400 - 1) / 0x400)); \
                   echo "append u-boot-atf-container.img at $$pad_cnt KB"; \
                   dd if=u-boot-atf-container.img of=flash.bin bs=1K seek=$$pad_cnt; \

flash_b0_spl_container_trusty: $(MKIMG) mx8qm-ahab-container.img scfw_tcm.bin u-boot-atf-container.img tee.bin u-boot-spl.bin
	./$(MKIMG) -soc QM -rev B0 -dcd skip -append mx8qm-ahab-container.img -c -scfw scfw_tcm.bin -ap u-boot-spl.bin a53 0x00100000 -out flash.bin
	cp flash.bin boot-spl-container.img
	@flashbin_size=`wc -c flash.bin | awk '{print $$1}'`; \
                   pad_cnt=$$(((flashbin_size + 0x400 - 1) / 0x400)); \
                   echo "append u-boot-atf-container.img at $$pad_cnt KB"; \
                   dd if=u-boot-atf-container.img of=flash.bin bs=1K seek=$$pad_cnt; \
