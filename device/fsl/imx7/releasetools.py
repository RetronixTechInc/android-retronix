# Copyright (C) 2012 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Emit extra commands needed for Group during OTA installation
(installing the bootloader)."""

import common

def FullOTA_InstallEnd_Ext4(info):
  try:
    bootloader_bin = info.input_zip.read("RADIO/bootloader.img")
  except KeyError:
    print "no bootloader.raw in target_files; skipping install"
  else:
    WriteExt4Bootloader(info, bootloader_bin)

def FullOTA_InstallEnd_Ubifs(info):
  try:
    bootloader_bin = info.input_zip.read("RADIO/bootloader.img")
  except KeyError:
    print "no bootloader.raw in target_files; skipping install"
  else:
    WriteUbifsBootloader(info, bootloader_bin)

def IncrementalOTA_InstallEnd_Ext4(info):
  try:
    target_bootloader_bin = info.target_zip.read("RADIO/bootloader.img")
    try:
      source_bootloader_bin = info.source_zip.read("RADIO/bootloader.img")
    except KeyError:
      source_bootloader_bin = None

    if source_bootloader_bin == target_bootloader_bin:
      print "bootloader unchanged; skipping"
    else:
      WriteExt4Bootloader(info, target_bootloader_bin)
  except KeyError:
    print "no bootloader.img in target target_files; skipping install"

def IncrementalOTA_InstallEnd_Ubifs(info):
  try:
    target_bootloader_bin = info.target_zip.read("RADIO/bootloader.img")
    try:
      source_bootloader_bin = info.source_zip.read("RADIO/bootloader.img")
    except KeyError:
      source_bootloader_bin = None

    if source_bootloader_bin == target_bootloader_bin:
      print "bootloader unchanged; skipping"
    else:
      WriteUbifsBootloader(info, target_bootloader_bin)
  except KeyError:
    print "no bootloader.img in target target_files; skipping install"

def WriteExt4Bootloader(info, bootloader_bin):
  common.ZipWriteStr(info.output_zip, "bootloader.img", bootloader_bin)
  fstab = info.info_dict["fstab"]

  info.script.Print("Writing bootloader...")
  info.script.AppendExtra('''package_extract_bootloader("bootloader.img", "%s");''' %
                          (fstab["/bootloader"].device,))

def WriteUbifsBootloader(info, bootloader_bin):
  common.ZipWriteStr(info.output_zip, "bootloader.img", bootloader_bin)
  fstab = info.info_dict["fstab"]

  info.script.Print("Writing bootloader...")
  info.script.AppendExtra('''package_extract_file("bootloader.img", "%s");''' %
                          "/tmp/bootloader.imx")
  info.script.AppendExtra('''write_raw_bootloader_image("/tmp/bootloader.imx", "bootloader");''' )
  info.script.AppendExtra('''delete("/tmp/bootloader.imx");''' )
 
