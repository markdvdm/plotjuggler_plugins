/// @copyright Copyright 2022 Elroy Air Inc. All rights reserved.
/// @author Matt Michini <matt@elroyair.com>

// External library includes
#include "gmock/gmock.h"
#include "gtest/gtest.h"

// Elroy includes
#include "elroy_common_msg/structs/test_struct_array.h"
#include "elroy_common_msg/structs/test_struct_default_enums.h"
#include "elroy_common_msg/structs/test_struct_inheritance_a.h"
#include "elroy_common_msg/structs/test_struct_inheritance_b.h"
#include "elroy_common_msg/structs/test_struct_inheritance_c.h"
#include "elroy_common_msg/structs/test_struct_single_value.h"

TEST(StructGenerationTest, DefaultEnums) {
  elroy_common_msg::TestStructDefaultEnums s;
  ASSERT_EQ(elroy_common_msg::TestEnum::FirstEnumValue, s.enum1_);
}

TEST(StructGenerationTest, StructEqualityBasic) {
  elroy_common_msg::TestStructSingleValue s1;
  elroy_common_msg::TestStructSingleValue s2;

  ASSERT_EQ(s1, s2);
  ASSERT_TRUE(s1 == s2);
  ASSERT_FALSE(s1 != s2);

  s1.some_val_ = 1;
  s2.some_val_ = 0;

  ASSERT_NE(s1, s2);
  ASSERT_FALSE(s1 == s2);
  ASSERT_TRUE(s1 != s2);
}

TEST(StructGenerationTest, StructEqualityArray) {
  elroy_common_msg::TestStructArray s1;
  elroy_common_msg::TestStructArray s2;

  ASSERT_EQ(s1, s2);

  s1.some_array_[2] = 1;
  s2.some_array_[2] = 0;

  ASSERT_NE(s1, s2);

  s1.some_array_[2] = 1;
  s2.some_array_[2] = 1;

  ASSERT_EQ(s1, s2);
}

TEST(StructGenerationTest, StructEqualityDeepInheritance) {
  // This test ensures that structs with deep inheritance properly compare even when values defined in the base classes
  // are modified.

  elroy_common_msg::TestStructInheritanceC s1;
  elroy_common_msg::TestStructInheritanceC s2;

  ASSERT_EQ(s1, s2);

  // Modify a value defined in TestStructInheritanceC (then reset it)
  s1.float_value_ = 99.999;
  ASSERT_NE(s1, s2);
  s1 = elroy_common_msg::TestStructInheritanceC{};
  ASSERT_EQ(s1, s2);

  // Modify a value defined in TestStructInheritanceB (then reset it)
  s1.struct_array_[1].some_val_ = 101;
  ASSERT_NE(s1, s2);
  s1 = elroy_common_msg::TestStructInheritanceC{};
  ASSERT_EQ(s1, s2);

  // Modify a value defined in TestStructInheritanceA (then reset it)
  s1.pears_[2] = 555.5;
  ASSERT_NE(s1, s2);
  s1 = elroy_common_msg::TestStructInheritanceC{};
  ASSERT_EQ(s1, s2);
}
