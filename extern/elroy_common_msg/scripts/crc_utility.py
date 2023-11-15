#!/usr/bin/env python3

import argparse
from ast import arguments
from mimetypes import init
import crcmod
import pytest

'''
@class 
'''
class CrcUtility(object):
  def __init__(self, poly, initial_crc=0xFFFFFFFF, xor_output=0xFFFFFFFF, reverse_bits=False):
    self._poly = poly
    self._initial_crc = initial_crc
    self._xor_output=xor_output
    self._reverse_bits = reverse_bits
    init_Crc = 0xFFFFFFFF
    if initial_crc == 0xFFFFFFFF:
      # flip initCRC because of crcmod implementation
      init_Crc = 0
    self._crcmod_crc = crcmod.Crc(poly=poly, initCrc=init_Crc, rev=reverse_bits, xorOut=xor_output, initialize=True)
    self._crcmod_crc_func = crcmod.mkCrcFun(poly=poly, initCrc=init_Crc, xorOut=xor_output, rev=reverse_bits)
    self._crc_size_in_bytes  = 32/8

  def __str__(self):
    me = []
    me.append('poly = 0x%X' % self._poly)
    me.append('crc size in bytes = %s' % self._crc_size_in_bytes)
    me.append('reverse bits = %s' % self._reverse_bits)
    fmt = '0x%%0%dX' % (self._crc_size_in_bytes * 2)
    me.append('initial crc = %s' % (fmt % self._initial_crc))
    me.append('XOR output = %s' % (fmt % self._initial_crc))
    crcmod_str = str(self._crcmod_crc).replace('\n', '\n\t')
    me.append('crcmod CRC object: \n\t%s' % crcmod_str)
    return '\n'.join(me)

  def calculate_crc(self, table, init_crc, byte_array):
    """"
    @brief calculates the CRC of the byte_array given table and init_crc
    @param[in] table the crc lookup table
    @param[in] init_crc the initialized crc
    @param[in] byte_array
    """
    crc = init_crc
    for b in byte_array:
      pos = (((crc >> 24) ^ b)) & 0xFF
      crc = ((crc << 8) ^ table[pos]) & 0xFFFFFFFF
    return (crc ^ self._xor_output) & 0xFFFFFFFF

  def calculate_crc_sarwate(self, poly, init_crc):
    """
    @brief calculates the CRC per sarwate algorithm
    @see https://create.stephan-brumme.com/crc32/#sarwate
    @note this is a modified version of the sarwate algorithm
          for 32-bit CRC, which is adding a 31-bit right shift
          to compare the crc's MSbit to 1 and extend it to all `F`s
          (per -int()) to get the poly.
    """
    crc = init_crc << 24
    for bit in range(8):
      crc = (crc << 1) ^ (-int((crc >> 31) & 1) & poly)
    return crc

  def calculate_crc_stackoverflow(self, poly, init_crc):
    """
    @brief calculates the CRC per the stack overflow reference but
           modified to not be reversed (i.e. left shift)
    @see https://stackoverflow.com/questions/26049150/calculate-a-32-bit-crc-lookup-table-in-c-c
    @note this is the slower if-conditional calculation, which the sarwate
          algorithm optimizes
    @note the original implementation is for reversed, CRC CRC held in LSbit,
          whereas we wish to do MSbit
          for more info see section 7.3.1 Reversed CRC lookup table & calculation
          of reciprocal CRC
    """
    crc = (init_crc << 24) & 0xFFFFFFFF
    for bit in range(8):
      if (crc &  0x80000000):
        crc = ((crc << 1) ^ poly) & 0xFFFFFFFF
      else:
        crc = (crc << 1) & 0xFFFFFFFF
    return crc

  def calculate_crc_usf(self, poly, init_crc):
    """
    @brief calculates the CRC per USF reference
    @see  https://cse.usf.edu/~kchriste/tools/crc32.c
    @note this uses MSB of CRC
    """
    crc = (init_crc << 24) & 0xFFFFFFFF
    for bit in range(8):
      if (crc & 0x80000000):
        crc = ((crc << 1) ^ poly) & 0xFFFFFFFF
      else:
        crc = (crc << 1) & 0xFFFFFFFF
    return crc & 0xFFFFFFFF

  def calculate_crc_sunshine2k(self, poly, init_crc):
    """
    @brief calculates the CRC per sunshine2k.de reference
    @see http://www.sunshine2k.de/articles/coding/crc/understanding_crc.html#ch6
    """
    crc = (init_crc << 24) & 0xFFFFFFFF
    for bit in range(8):
      if ((crc & 0x80000000) != 0):
        crc = ((crc << 1) ^ poly) & 0xFFFFFFFF
      else:
        crc <<= 1
    return crc & 0xFFFFFFFF

  def gen_table(self, init_crc, crc_calc_type: str):
    """
    @brief generates 32-bit CRC table (len 256) per crc_calc_type
    @param[in] poly the polynomial
    @param[in] init_crc the initial crc value
    @param[in] crc_calc_type the crc calculation type, expecting:
               [sarwate, usf, stackoverflow, usf, crcmod, sunshine2k], uses class function
               calculate_crc_<crc_calc_type>
    @note all but crcmod are handwritten crc calculations. Crcmod uses the
          python module crcmod
    """
    expected_crcs = ["sarwate", "stackoverflow", "usf", "crcmod", "sunshine2k"]
    
    if crc_calc_type not in expected_crcs:
      raise KeyError('%s is not an expected CRC calculation type: %s' % (crc_calc_type, expected_crcs))

    if crc_calc_type == "crcmod":
      crc_table = []
      # convert entries to hex
      for i in range(0, len(self._crcmod_crc.table)):
        crc_table.append(self._crcmod_crc.table[i])
      return crc_table
    else:  
      crc_func = getattr(self, "calculate_crc_%s" % crc_calc_type)
      crc_table = []
      for i in range(256):
        crc_table.append(crc_func(self._poly, i))
      return crc_table

def crcmod_test(crc_util): 
  """
  @brief uses crcmod function to calculate CRC of the same buffer defined
         in ./scripts/templates/cpp/msg_handling_ut.jinja2
  @param[in] crc_util 
  """
  buf = bytearray([
    0x0E, 0x00, 0x90, 0x9E, 0x12, 0x26, 0x00, 0x00, 0xF0, 0x41,
    0x00, 0x00, 0xA0, 0x41, 0x00, 0x00, 0x04, 0x42, 0x00, 0x00,
    0x60, 0x42, 0x00, 0x00, 0xEC, 0x41, 0xCD, 0xCC, 0xF8, 0x41,
    0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x0C, 0x42, 0x00, 0x00,
    0xC8, 0x42, 0x00, 0x00, 0xCA, 0x42, 0x00, 0x00, 0x48, 0x43,
    0x00, 0x00, 0x52, 0x43, 0x00, 0x00, 0x80, 0x3F, 0x9A, 0x99,
    0x99, 0x3F, 0x00, 0x00, 0x94, 0x41, 0x66, 0x66, 0x3E, 0x41,
    0x00, 0x00, 0x48, 0x41, 0x33, 0x33, 0x23, 0x41, 0x00, 0x00,
    0x60, 0x41, 0x00, 0x00, 0x80, 0x41, 0x00, 0x00, 0x88, 0x41,
    0x00, 0x00, 0x90, 0x41, 0x00, 0x00, 0x98, 0x41
  ])

  expected_crc = 0x1CF92F07
  crc = hex(crc_util._crcmod_crc_func(buf)).upper().replace("X", "x")

  assert int(crc, base=16) == expected_crc
  assert int(crc, base=16)+1 != expected_crc

def test_crcs(crc_util): 
  # test print for __str__
  print("Crc Util %s" % crc_util)

  init_crc = 0xFFFFFFFF

  tables = {
    "usf" : {
      "table" : None,
      "crc" : None
    },
    "sarwate" : {
      "table" : None,
      "crc" : None
    },
    "stackoverflow" : {
      "table" : None,
      "crc" : None
    },
    "crcmod" : {
      "table" : None,
      "crc" : None
    },
    "sunshine2k" : {
      "table" : None,
      "crc" : None
    }
  }

  # calculate tables
  for k in tables:
    table = crc_util.gen_table(init_crc, k)
    if len(table) != 256:
      print("Error! Expecting table size of 255 but got %d for %s's table" % (len(table), k))
    tables[k]["table"] = table

  tables_ok = True
  # check for consistency
  for k in tables:
    table = tables[k]["table"]
    for other_key in tables:
      # skip this k(ey)
      if other_key == k:
        continue
      other_table = tables[k]["table"]
      if (table != other_table):
        tables_ok = False
        print("Error! Table for %s and %s do not match!\n" % (k, other_key))


  if tables_ok == True:
    print("\nAll generated tables were consistent!\n")

  buf = bytearray([
    0x0E, 0x00, 0x90, 0x9E, 0x12, 0x26, 0x00, 0x00, 0xF0, 0x41,
    0x00, 0x00, 0xA0, 0x41, 0x00, 0x00, 0x04, 0x42, 0x00, 0x00,
    0x60, 0x42, 0x00, 0x00, 0xEC, 0x41, 0xCD, 0xCC, 0xF8, 0x41,
    0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x0C, 0x42, 0x00, 0x00,
    0xC8, 0x42, 0x00, 0x00, 0xCA, 0x42, 0x00, 0x00, 0x48, 0x43,
    0x00, 0x00, 0x52, 0x43, 0x00, 0x00, 0x80, 0x3F, 0x9A, 0x99,
    0x99, 0x3F, 0x00, 0x00, 0x94, 0x41, 0x66, 0x66, 0x3E, 0x41,
    0x00, 0x00, 0x48, 0x41, 0x33, 0x33, 0x23, 0x41, 0x00, 0x00,
    0x60, 0x41, 0x00, 0x00, 0x80, 0x41, 0x00, 0x00, 0x88, 0x41,
    0x00, 0x00, 0x90, 0x41, 0x00, 0x00, 0x98, 0x41
  ])

  # calculate CRC of bytearray from all tables
  for k in tables:
    tables[k]["crc"] = crc_util.calculate_crc(tables[k]["table"], init_crc, buf)

  # make sure crc's are all good
  crcs_ok = True
  for k in tables:
    crc = tables[k]["crc"]
    for other_key in tables:
      if k == other_key:
        continue
      other_crc = tables[other_key]["crc"]
      if crc != other_crc:
        crcs_ok = False
        print("Error! %s crc (0x%X) does not match %s (0x%X)" % (k, crc, other_key, other_crc))

  if crcs_ok is True:
    print("All CRC calculations are consistent")


def IsPolyValid(poly):
  # make sure hex
  if "0x" not in poly:
    print("Invalid Polynimal formatting, expecting Hex format")
    return False
  # check for leading 1
  if "1" != poly[2]:
    print("Invalid Polynomial formatting, expecting leading 1 (i.e. 0x1F1922815)")
    return False
  # make sure poly length is good
  if len(poly) != len("0x1f1922815"):
    print("Invalid Polynomial formatting, expecting 32-bit polynomial w/ explicit + 1")
    return False
  return True

def GenerateCpp(crc_util):
  """
  @brief generates CPP lookup table and writes it to templates/cpp/c_crc_calculator.h
  @note replaces that files "CRC TABLE PLACEHOLDER" 
  """
  poly = hex(crc_util._poly).upper().replace("X", "x")
  print("Generating table for poly %s\n" % poly)
  table = crc_util.gen_table(0xFFFFFFFF, "crcmod")
  cpp_table = '\t/// @brief this table was generated with elroy_common_msg/scripts/crc_util.py via:\n'\
              '\t///        crc_util.py --poly ' + poly + ' --gen-table\n'\
              '\t/// @note crc_util defaults to use crcmod lookup table generation\n'\
              '\t/// @note crc32 lookup table for polynomial ' + poly + '\n'\
              '\tstatic constexpr uint32_t kCrc32LookupTable_[256] = {\n'
  for i in range(0, len(table), 8):
    cpp_table += '\t\t0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X' % (table[i], table[i+1], table[i+2], table[i+3], table[i+4], table[i+5], table[i+6], table[i+7])
    if i == (len(table) - 8):
      cpp_table += '};\n'
    else:
      cpp_table += ',\n'

  print("----------------------[COPY BELOW]----------------------")  
  print(cpp_table)
  print("----------------------[COPY ABOVE]----------------------")  

if __name__ == "__main__":

  parser = argparse.ArgumentParser(description="CRC Utility")
  parser.add_argument('--poly', type=str, dest='poly', required=True, help="The 32-bit polynomial in hexadecimal explicit + 1 notation (i.e. most-significant 1)!")
  parser.add_argument('--gen-table', action='store_true', default=False, help="Generate 32-bit lookup table for --poly and updates `templates/cpp/c_crc_calculator.h`. Defaults to use crcmod generated table")
  parser.add_argument('--test', action='store_true', required=False, default=False, help="Execute test that compares various CRC methods")
  parser.add_argument('--crcmod-test', action='store_true', required=False, default=False, help="Calculate crc for identical array defined in ./scripts/templates/cpp/msg_handling_ut.jinja2 using crcmod")


  args = parser.parse_args()

  crc_util = CrcUtility(int(args.poly, base=16))

  if IsPolyValid(args.poly) is False:
    print("Error! Invalid Poly")

  if args.gen_table is True:
    GenerateCpp(crc_util)

  if args.test is True:
    test_crcs(crc_util)

  if args.crcmod_test is True:
    crcmod_test(crc_util)