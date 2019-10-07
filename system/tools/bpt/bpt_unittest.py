#!/usr/bin/python

# Copyright 2016, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


"""Unit-test for bpttool."""


import imp
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
bpttool = imp.load_source('bpttool', './bpttool')


class FakeGuidGenerator(object):
  """A GUID generator that dispenses predictable GUIDs.

  This is used in place of the usual GUID generator in bpttool which
  is generating random GUIDs as per RFC 4122.
  """

  def dispense_guid(self, partition_number):
    """Dispenses a new GUID."""

    uuid = '01234567-89ab-cdef-0123-%012x' % partition_number
    return uuid

class PatternPartition(object):
  """A partition image file containing a predictable pattern.

  This holds file data about a partition image file for binary pattern.
  testing.
  """
  def __init__(self, char='', file=None, partition_name=None, obj=None):
    self.char = char
    self.file = file
    self.partition_name = partition_name
    self.obj = obj

class RoundToMultipleTest(unittest.TestCase):
  """Unit tests for the RoundToMultiple() function."""

  def testRoundUp(self):
    """Checks that we round up correctly."""
    self.assertEqual(bpttool.RoundToMultiple(100, 10), 100)
    self.assertEqual(bpttool.RoundToMultiple(189, 10), 190)
    self.assertEqual(bpttool.RoundToMultiple(190, 10), 190)
    self.assertEqual(bpttool.RoundToMultiple(191, 10), 200)
    self.assertEqual(bpttool.RoundToMultiple(199, 10), 200)
    self.assertEqual(bpttool.RoundToMultiple(200, 10), 200)
    self.assertEqual(bpttool.RoundToMultiple(201, 10), 210)
    self.assertEqual(bpttool.RoundToMultiple(-18, 10), -10)
    self.assertEqual(bpttool.RoundToMultiple(-19, 10), -10)
    self.assertEqual(bpttool.RoundToMultiple(-20, 10), -20)
    self.assertEqual(bpttool.RoundToMultiple(-21, 10), -20)

  def testRoundDown(self):
    """Checks that we round down correctly."""
    self.assertEqual(bpttool.RoundToMultiple(100, 10, True), 100)
    self.assertEqual(bpttool.RoundToMultiple(189, 10, True), 180)
    self.assertEqual(bpttool.RoundToMultiple(190, 10, True), 190)
    self.assertEqual(bpttool.RoundToMultiple(191, 10, True), 190)
    self.assertEqual(bpttool.RoundToMultiple(199, 10, True), 190)
    self.assertEqual(bpttool.RoundToMultiple(200, 10, True), 200)
    self.assertEqual(bpttool.RoundToMultiple(201, 10, True), 200)
    self.assertEqual(bpttool.RoundToMultiple(-18, 10, True), -20)
    self.assertEqual(bpttool.RoundToMultiple(-19, 10, True), -20)
    self.assertEqual(bpttool.RoundToMultiple(-20, 10, True), -20)
    self.assertEqual(bpttool.RoundToMultiple(-21, 10, True), -30)


class ParseSizeTest(unittest.TestCase):
  """Unit tests for the ParseSize() function."""

  def testIntegers(self):
    """Checks parsing of integers."""
    self.assertEqual(bpttool.ParseSize(123), 123)
    self.assertEqual(bpttool.ParseSize(17179869184), 1<<34)
    self.assertEqual(bpttool.ParseSize(0x1000), 4096)

  def testRawNumbers(self):
    """Checks parsing of raw numbers."""
    self.assertEqual(bpttool.ParseSize('123'), 123)
    self.assertEqual(bpttool.ParseSize('17179869184'), 1<<34)
    self.assertEqual(bpttool.ParseSize('0x1000'), 4096)

  def testDecimalUnits(self):
    """Checks that decimal size units are interpreted correctly."""
    self.assertEqual(bpttool.ParseSize('1 kB'), pow(10, 3))
    self.assertEqual(bpttool.ParseSize('1 MB'), pow(10, 6))
    self.assertEqual(bpttool.ParseSize('1 GB'), pow(10, 9))
    self.assertEqual(bpttool.ParseSize('1 TB'), pow(10, 12))
    self.assertEqual(bpttool.ParseSize('1 PB'), pow(10, 15))

  def testBinaryUnits(self):
    """Checks that binary size units are interpreted correctly."""
    self.assertEqual(bpttool.ParseSize('1 KiB'), pow(2, 10))
    self.assertEqual(bpttool.ParseSize('1 MiB'), pow(2, 20))
    self.assertEqual(bpttool.ParseSize('1 GiB'), pow(2, 30))
    self.assertEqual(bpttool.ParseSize('1 TiB'), pow(2, 40))
    self.assertEqual(bpttool.ParseSize('1 PiB'), pow(2, 50))

  def testFloatAndUnits(self):
    """Checks that floating point numbers can be used with units."""
    self.assertEqual(bpttool.ParseSize('0.5 kB'), 500)
    self.assertEqual(bpttool.ParseSize('0.5 KiB'), 512)
    self.assertEqual(bpttool.ParseSize('0.5 GB'), 500000000)
    self.assertEqual(bpttool.ParseSize('0.5 GiB'), 536870912)
    self.assertEqual(bpttool.ParseSize('0.1 MiB'), 104858)

class MakeDiskImageTest(unittest.TestCase):
  """Unit tests for 'bpttool make_disk_image'."""

  def setUp(self):
    """Set-up method."""
    self.bpt = bpttool.Bpt()

  def _BinaryPattern(self, bpt_file_name, partition_patterns):
    """Checks that a binary pattern may be written to a specified partition.

    This checks individual partion image writes to portions of a disk.  Known
    patterns are written into certain partitions and are verified after each
    pattern has been written to.

    Arguments:
      bpt_file_name: File name of bpt JSON containing partition information.
      partition_patterns: List of tuples with each tuple having partition name
                          as the first argument, and character pattern as the
                          second argument.

    """
    bpt_file = open(bpt_file_name, 'r')
    partitions_string, _ = self.bpt.make_table([bpt_file])
    bpt_tmp = tempfile.NamedTemporaryFile()
    bpt_tmp.write(partitions_string)
    bpt_tmp.seek(0)
    partitions, _ = self.bpt._read_json([bpt_tmp])

    # Declare list of partition images to be written and compared on disk.
    pattern_images = [PatternPartition(
                      char=pp[1],
                      file=tempfile.NamedTemporaryFile(),
                      partition_name=pp[0])
                      for pp in partition_patterns]

    # Store partition object and write a known character pattern image.
    for pi in pattern_images:
      pi.obj = [p for p in partitions if str(p.label) == pi.partition_name][0]
      pi.file.write(bytearray(pi.char * int(pi.obj.size)))

    # Create the disk containing the partition filled with a known character
    # pattern, seek to it's position and compare it to the supposed pattern.
    with tempfile.NamedTemporaryFile() as generated_disk_image:
      bpt_tmp.seek(0)
      self.bpt.make_disk_image(generated_disk_image,
                               bpt_tmp,
                               [p.partition_name + ':' + p.file.name
                                for p in pattern_images])

      for pi in pattern_images:
        generated_disk_image.seek(pi.obj.offset)
        pi.file.seek(0)

        self.assertEqual(generated_disk_image.read(pi.obj.size),
                    pi.file.read())
        pi.file.close()

    bpt_file.close()
    bpt_tmp.close()

  def _LargeBinary(self, bpt_file_name):
    """Helper function to write large partition images to disk images.

    This is a simple call to make_disk_image, passing a large in an image
    which exceeds the it's size as specfied in the bpt file.

    Arguments:
      bpt_file_name: File name of bpt JSON containing partition information.

    """
    with open(bpt_file_name, 'r') as bpt_file, \
         tempfile.NamedTemporaryFile() as bpt_tmp, \
         tempfile.NamedTemporaryFile() as generated_disk_image, \
         tempfile.NamedTemporaryFile() as large_partition_image:
        partitions_string, _ = self.bpt.make_table([bpt_file])
        bpt_tmp.write(partitions_string)
        bpt_tmp.seek(0)
        partitions, _ = self.bpt._read_json([bpt_tmp])

        # Create the over-sized partition image.
        large_partition_image.write(bytearray('0' *
          int(1.1*partitions[0].size + 1)))

        bpt_tmp.seek(0)

        # Expect exception here.
        self.bpt.make_disk_image(generated_disk_image, bpt_tmp,
          [p.label + ':' + large_partition_image.name for p in partitions])

  def testBinaryPattern(self):
    """Checks patterns written to partitions on disk images."""
    self._BinaryPattern('test/pattern_partition_single.bpt', [('charlie', 'c')])
    self._BinaryPattern('test/pattern_partition_multi.bpt', [('alpha', 'a'),
                        ('beta', 'b')])

  def testExceedPartitionSize(self):
    """Checks that exceedingly large partition images are not accepted."""
    try:
      self._LargeBinary('test/pattern_partition_exceed_size.bpt')
    except bpttool.BptError as e:
      assert 'exceeds the partition size' in e.message


class MakeTableTest(unittest.TestCase):
  """Unit tests for 'bpttool make_table'."""

  def setUp(self):
    """Reset GUID generator for every test."""
    self.fake_guid_generator = FakeGuidGenerator()

  def _MakeTable(self, input_file_names,
                 expected_json_file_name,
                 expected_gpt_file_name=None,
                 disk_size=None,
                 disk_alignment=None,
                 disk_guid=None,
                 ab_suffixes=None):
    """Helper function to create partition tables.

    This is a simple wrapper for Bpt.make_table() that compares its
    output with an expected output when given a set of inputs.

    Arguments:
      input_file_names: A list of .bpt files to process.
      expected_json_file_name: File name of the file with expected JSON output.
      expected_gpt_file_name: File name of the file with expected binary
                              output or None.
      disk_size: if not None, the size of the disk to use.
      disk_alignment: if not None, the disk alignment to use.
      disk_guid: if not None, the disk GUID to use.
      ab_suffixes: if not None, a list of the A/B suffixes (as a
                   comma-separated string) to use.

    """

    inputs = []
    for file_name in input_file_names:
      inputs.append(open(file_name))

    bpt = bpttool.Bpt()
    (json_str, gpt_bin) = bpt.make_table(
        inputs,
        disk_size=disk_size,
        disk_alignment=disk_alignment,
        disk_guid=disk_guid,
        ab_suffixes=ab_suffixes,
        guid_generator=self.fake_guid_generator)
    self.assertEqual(json_str, open(expected_json_file_name).read())

    # Check that bpttool generates same JSON if being fed output it
    # just generated without passing any options (e.g. disk size,
    # alignment, suffixes). This verifies that we include all
    # necessary information in the generated JSON.
    (json_str2, _) = bpt.make_table(
        [open(expected_json_file_name, 'r')],
        guid_generator=self.fake_guid_generator)
    self.assertEqual(json_str, json_str2)

    if expected_gpt_file_name:
      self.assertEqual(gpt_bin, open(expected_gpt_file_name).read())

  def testBase(self):
    """Checks that the basic layout is as expected."""
    self._MakeTable(['test/base.bpt'],
                    'test/expected_json_base.bpt',
                    disk_size=bpttool.ParseSize('10 GiB'))

  def testSize(self):
    """Checks that disk size can be changed on the command-line."""
    self._MakeTable(['test/base.bpt'],
                    'test/expected_json_size.bpt',
                    disk_size=bpttool.ParseSize('20 GiB'))

  def testAlignment(self):
    """Checks that disk alignment can be changed on the command-line."""
    self._MakeTable(['test/base.bpt'],
                    'test/expected_json_alignment.bpt',
                    disk_size=bpttool.ParseSize('10 GiB'),
                    disk_alignment=1048576)

  def testDiskGuid(self):
    """Checks that disk GUID can be changed on the command-line."""
    self._MakeTable(['test/base.bpt'],
                    'test/expected_json_disk_guid.bpt',
                    disk_size=bpttool.ParseSize('10 GiB'),
                    disk_guid='01234567-89ab-cdef-0123-00000000002a')

  def testSuffixes(self):
    """Checks that A/B-suffixes can be changed on the command-line."""
    self._MakeTable(['test/base.bpt'],
                    'test/expected_json_suffixes.bpt',
                    disk_size=bpttool.ParseSize('10 GiB'),
                    ab_suffixes='-A,-B')

  def testStackedIgnore(self):
    """Checks that partitions can be ignored through stacking."""
    self._MakeTable(['test/base.bpt', 'test/ignore_userdata.bpt'],
                    'test/expected_json_stacked_ignore.bpt',
                    disk_size=bpttool.ParseSize('10 GiB'))

  def testStackedSize(self):
    """Checks that partition size can be changed through stacking."""
    self._MakeTable(['test/base.bpt', 'test/change_odm_size.bpt'],
                    'test/expected_json_stacked_size.bpt',
                    disk_size=bpttool.ParseSize('10 GiB'))

  def testStackedSettings(self):
    """Checks that settings can be changed through stacking."""
    self._MakeTable(['test/base.bpt', 'test/override_settings.bpt'],
                    'test/expected_json_stacked_override_settings.bpt')

  def testStackedNewPartition(self):
    """Checks that a new partition can be added through stacking."""
    self._MakeTable(['test/base.bpt', 'test/new_partition.bpt'],
                    'test/expected_json_stacked_new_partition.bpt',
                    disk_size=bpttool.ParseSize('10 GiB'))

  def testStackedChangeFlags(self):
    """Checks that we can change partition flags through stacking."""
    self._MakeTable(['test/base.bpt', 'test/change_userdata_flags.bpt'],
                    'test/expected_json_stacked_change_flags.bpt',
                    disk_size=bpttool.ParseSize('10 GiB'))

  def testStackedNewPartitionOnTop(self):
    """Checks that we can add a new partition only given the output JSON.

    This also tests that 'userdata' is shrunk (it has a size in the
    input file 'expected_json_base.bpt') to make room for the new
    partition. This works because the 'grow' attribute is preserved.
    """
    self._MakeTable(['test/expected_json_base.bpt',
                     'test/new_partition_on_top.bpt'],
                    'test/expected_json_stacked_new_partition_on_top.bpt')

  def testStackedSizeAB(self):
    """Checks that AB partition size can be changed given output JSON.

    This verifies that we un-expand A/B partitions when importing
    output JSON. E.g. the output JSON has system_a, system_b which is
    rewritten to just system as part of the import.
    """
    self._MakeTable(['test/expected_json_base.bpt',
                     'test/change_system_size.bpt'],
                    'test/expected_json_stacked_change_ab_size.bpt')

  def testPositionAttribute(self):
    """Checks that it's possible to influence partition order."""
    self._MakeTable(['test/base.bpt',
                     'test/positions.bpt'],
                    'test/expected_json_stacked_positions.bpt',
                    disk_size=bpttool.ParseSize('10 GiB'))

  def testBinaryOutput(self):
    """Checks binary partition table output.

    This verifies that we generate the binary partition tables
    correctly.
    """
    self._MakeTable(['test/expected_json_stacked_change_flags.bpt'],
                    'test/expected_json_stacked_change_flags.bpt',
                    'test/expected_json_stacked_change_flags.bin')

  def testFileWithSyntaxErrors(self):
    """Check that we catch errors in JSON files in a structured way."""
    try:
      self._MakeTable(['test/file_with_syntax_errors.bpt'],
                      'file_with_syntax_errors.bpt')
    except bpttool.BptParsingError as e:
      self.assertEqual(e.filename, 'test/file_with_syntax_errors.bpt')


class QueryPartitionTest(unittest.TestCase):
  """Unit tests for 'bpttool query_partition'."""

  def setUp(self):
    """Set-up method."""
    self.bpt = bpttool.Bpt()

  def testQuerySize(self):
    """Checks query for size."""
    self.assertEqual(
        self.bpt.query_partition(
            open('test/expected_json_stacked_change_flags.bpt'),
            'userdata', 'size', False),
        '7449042944')

  def testQueryOffset(self):
    """Checks query for offset."""
    self.assertEqual(
        self.bpt.query_partition(
            open('test/expected_json_stacked_change_flags.bpt'),
            'userdata', 'offset', False),
        '3288354816')

  def testQueryGuid(self):
    """Checks query for GUID."""
    self.assertEqual(
        self.bpt.query_partition(
            open('test/expected_json_stacked_change_flags.bpt'),
            'userdata', 'guid', False),
        '01234567-89ab-cdef-0123-000000000007')

  def testQueryTypeGuid(self):
    """Checks query for type GUID."""
    self.assertEqual(
        self.bpt.query_partition(
            open('test/expected_json_stacked_change_flags.bpt'),
            'userdata', 'type_guid', False),
        '0bb7e6ed-4424-49c0-9372-7fbab465ab4c')

  def testQueryFlags(self):
    """Checks query for flags."""
    self.assertEqual(
        self.bpt.query_partition(
            open('test/expected_json_stacked_change_flags.bpt'),
            'userdata', 'flags', False),
        '0x0420000000000000')

  def testQuerySizeCollapse(self):
    """Checks query for size when collapsing A/B partitions."""
    self.assertEqual(
        self.bpt.query_partition(
            open('test/expected_json_stacked_change_flags.bpt'),
            'odm', 'size', True),
        '1073741824')


if __name__ == '__main__':
  unittest.main()
