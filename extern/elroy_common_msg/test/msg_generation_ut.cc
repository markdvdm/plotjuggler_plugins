/// @copyright Copyright 2022 Elroy Air Inc. All rights reserved.
/// @author Matt Michini <matt@elroyair.com>

// External library includes
#include "gmock/gmock.h"
#include "gtest/gtest.h"

// Elroy includes
#include "elroy_common_msg/enums/message_id.h"
#include "elroy_common_msg/test_msg_default_enums_message.h"
#include "elroy_common_msg/test_msg_single_field_message.h"

constexpr size_t kCrcSizeBytes = 4;

/// @brief This templated helper function tests both serialization and deserialization. Given the known raw byte array
/// 'msg_bytes' for the message, and a 'msg_struct' containing the struct contents of the message, this function
/// serializes the struct into a raw byte array (and vice versa) and asserts that the resulting data structures match.
/// @note It is best to only call this method once per test case - that way it's easier to see which line of the test
// case has failed.
/// @param[in] msg_bytes The expected (correct) byte array if 'msg_struct' is properly serialized.
/// @param[in] msg_struct The expected (correct) message struct if 'msg_bytes' is properly deserialized.
/// @tparam MessageType The type of the actual elroy_common_msg message. This type will correspond to StructType.
/// @tparam StructType The type of the underlying data struct for the elroy_common_msg.
template <typename MessageType, typename StructType>
void TestSerialization(const std::vector<uint8_t>& msg_bytes, const StructType& msg_struct) {
  // Create a new message object and write the contents of msg_struct into it
  MessageType msg;
  msg.Write(msg_struct);

  // Do some size checks
  ASSERT_GT(msg.GetPackedSize(), kCrcSizeBytes);
  ASSERT_EQ(msg_bytes.size(), msg.GetPackedSize());
  ASSERT_EQ(msg_bytes.size() - kCrcSizeBytes, msg.GetPackedSizeWithoutCrc());

  // Attempt to serialize the contents of 'msg' into a byte array, and compare that to the input 'msg_bytes'
  std::vector<uint8_t> serialized_msg;
  serialized_msg.resize(msg.GetPackedSize());
  msg.Pack(serialized_msg.data(), msg.GetPackedSize());

  ASSERT_THAT(serialized_msg, ::testing::ContainerEq(msg_bytes));

  // Now attempt to decode a message from the input byte array 'msg_bytes', and ensure that the resulting struct matches
  // the input struct
  MessageType msg_deserialized;
  const auto bytes_unpacked = msg_deserialized.Unpack(msg_bytes.data(), msg_bytes.size());
  ASSERT_EQ(msg_bytes.size(), bytes_unpacked);

  StructType msg_struct_deserialized;
  msg_deserialized.Read(msg_struct_deserialized);
  ASSERT_EQ(msg_struct, msg_struct_deserialized);
}

// Helper function to simply print the serialized byte array to the screen. This is helpful if you're writing new unit
// tests, for example.
void PrintSerializedMessage(const elroy_common_msg::BusObjectMessage& msg) {
  std::vector<uint8_t> serialized_msg;
  serialized_msg.resize(msg.GetPackedSize());
  msg.Pack(serialized_msg.data(), msg.GetPackedSize());

  for (const auto& b : serialized_msg) {
    printf("0x%02X, ", b);
  }
  printf("\n");
}

TEST(MsgGenerationTest, TestMsgDefaultEnumsMessageDefaultValues) {
  elroy_common_msg::test_messages::TestMsgDefaultEnums msg_struct;

  std::vector<uint8_t> msg_bytes{0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x01, 0x01, 0x17, 0xDB, 0xD2, 0xD0};

  TestSerialization<elroy_common_msg::test_messages::TestMsgDefaultEnumsMessage>(msg_bytes, msg_struct);
}

TEST(MsgGenerationTest, TestMsgDefaultEnumsMessageModifiedValues) {
  elroy_common_msg::test_messages::TestMsgDefaultEnums msg_struct;
  msg_struct.enum1_ = elroy_common_msg::TestEnum::ThirdEnumValue;
  msg_struct.struct1_.enum1_ = elroy_common_msg::TestEnum::SecondEnumValue;

  std::vector<uint8_t> msg_bytes{0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x02, 0x03, 0x86, 0xF2, 0x6E, 0x13};

  TestSerialization<elroy_common_msg::test_messages::TestMsgDefaultEnumsMessage>(msg_bytes, msg_struct);
}

TEST(MsgGenerationTest, TestMsgSingleFieldMessageDefaultValues) {
  elroy_common_msg::test_messages::TestMsgSingleField msg_struct;

  std::vector<uint8_t> msg_bytes{0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x63, 0x13, 0x4F, 0x40, 0x26};

  TestSerialization<elroy_common_msg::test_messages::TestMsgSingleFieldMessage>(msg_bytes, msg_struct);
}

TEST(MsgGenerationTest, TestMsgSingleFieldMessageModifiedValues) {
  elroy_common_msg::test_messages::TestMsgSingleField msg_struct;
  msg_struct.uint8_field_ = 100;

  std::vector<uint8_t> msg_bytes{0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x64, 0x47, 0xEF, 0x08, 0xE0};

  TestSerialization<elroy_common_msg::test_messages::TestMsgSingleFieldMessage>(msg_bytes, msg_struct);
}

TEST(MsgGenerationTest, GcsMessageIds) {
  std::string str(
      "I've noticed that the above message ID has changed - this may affect GCS <> clio communication and cause the "
      "clios to be unable to be placed in bootloader mode, which will make re-programming them difficult. Have you "
      "added new messages to the schema file before these messages? If so consider moving your new messages to the "
      "bottom of the schema file.");
  EXPECT_EQ(4, (int)elroy_common_msg::MessageId::GcsCommand) << str;
  EXPECT_EQ(5, (int)elroy_common_msg::MessageId::FlightComputerReboot) << str;
}
