/// @copyright Copyright 2021 Elroy Air Inc. All rights reserved.
/// @author Phillip Yee <phillip.yee@elroyair.com>

#include "msg_packing_util/msg_packing_util.h"

#include "gtest/gtest.h"

using elroy_common_msg::MsgPackingUtil;

TEST(MsgPackingUtil, EightByteTests) {
  double packed_double = 1234.5678F;
  double unpacked_double = 0;
  uint8_t dest[8] = {0};

  MsgPackingUtil::Pack<double>(dest, packed_double);
  MsgPackingUtil::Unpack<double>(unpacked_double, dest);

  ASSERT_DOUBLE_EQ(unpacked_double, packed_double);

  uint64_t packed_uint64 = 123456;
  uint64_t unpacked_uint64 = 0;

  MsgPackingUtil::Pack<uint64_t>(dest, packed_uint64);
  MsgPackingUtil::Unpack<uint64_t>(unpacked_uint64, dest);

  ASSERT_EQ(unpacked_uint64, packed_uint64);
}

TEST(MsgPackingUtil, FourByteTests) {
  float packed_float = 123.45F;
  float unpacked_float = 0.0F;
  uint8_t dest[4] = {0};

  MsgPackingUtil::Pack<float>(dest, packed_float);
  MsgPackingUtil::Unpack<float>(unpacked_float, dest);

  ASSERT_FLOAT_EQ(unpacked_float, packed_float);

  uint32_t packed_uint32 = 5678;
  uint32_t unpacked_uint32 = 0;

  MsgPackingUtil::Pack<uint32_t>(dest, packed_uint32);
  MsgPackingUtil::Unpack<uint32_t>(unpacked_uint32, dest);

  ASSERT_EQ(unpacked_uint32, packed_uint32);
}

TEST(MsgPackingUtil, TwoByteTests) {
  uint16_t packed_uint16 = 1234;
  uint16_t unpacked_uint16 = 0;
  uint8_t dest[2] = {0};

  MsgPackingUtil::Pack<uint16_t>(dest, packed_uint16);
  MsgPackingUtil::Unpack<uint16_t>(unpacked_uint16, dest);

  ASSERT_EQ(unpacked_uint16, packed_uint16);
}

TEST(MsgPackingUtil, OneByteTests) {
  uint8_t packed_uint8 = 0xDA;
  uint8_t unpacked_uint8 = 0;
  uint8_t dest[1] = {0};

  MsgPackingUtil::Pack(dest, packed_uint8);
  MsgPackingUtil::Unpack(unpacked_uint8, dest);

  ASSERT_EQ(unpacked_uint8, packed_uint8);
}
