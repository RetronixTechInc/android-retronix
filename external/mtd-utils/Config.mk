# $(1): src directory
# $(2): output file
# $(3): label (if any)
# $(4): if true, add journal
#
# $(1): root directory
# $(2): system directory
# $(3): output file
MKUBIFS_FLAGS=-m 2048 -e 129024 -c 1996
UBINIZE_FLAGS=-m 2048 -p 128KiB -s 512

define build-userimage-ubifs-target
	@echo "Making ubifs"
	@NEWSHA=`.repo/repo/repo forall -c 'git log -1' | shasum | awk '{print $$1}'`; \
	       echo "VERSION := $${NEWSHA}" > $(TARGET_ROOT_OUT)/version.txt; \
	       .repo/repo/repo forall -c 'git log -1' >> $(TARGET_ROOT_OUT)/version.txt
	@ mkdir -p $(dir $(3))
	@ mkdir -p $(dir $(3))/ubi_root
	@ $(ACP) -d -p -r $(TARGET_ROOT_OUT)/* $(dir $(3))/ubi_root
	@ $(ACP) -d -p -r $(TARGET_OUT) $(dir $(3))/ubi_root/
	@ $(ACP) external/mtd-utils/ubinize.cfg $(dir $(3))
	@ $(ACP) -p $(UBINIZE) $(dir $(3))
	@ $(MKUBIFS) -r $(dir $(3))/ubi_root -o $(dir $(3))/android_beagle.ubifs $(MKUBIFS_FLAGS)
	@cd $(dir $(3)); ./ubinize -o `basename $(3)` $(UBINIZE_FLAGS) ubinize.cfg
	@rm -rf $(dir $(3))/ubi_root $(dir $(3))/android_beagle.ubifs $(dir $(3))/ubinize $(dir $(3))/ubinize.cfg
endef
