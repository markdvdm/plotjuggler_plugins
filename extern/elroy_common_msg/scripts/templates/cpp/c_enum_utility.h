/// @copyright Copyright 2022 Elroy Air Inc. All rights reserved.
/// @author elroy_common_msg autogeneration

#ifndef ELROY_COMMON_MSG_UTILITY_C_ENUM_UTILITY_H_
#define ELROY_COMMON_MSG_UTILITY_C_ENUM_UTILITY_H_

#include <string>
#include "yaml-cpp/yaml.h"
#include <atomic>
#include <iostream>

namespace elroy_common_msg {

/// @class
/// a utility class to access the enumerates manifest for
/// easier API of `enum_to_string` and `enum_to_value`
/// conversions
/// @note assumes latest `yaml-cpp` (v0.6); what comes with
///       integration_sim's gazebo (at the time of this commit)
/// @note to integrate into your project:
///       1. ensure you have `yaml-cpp` source:
///          https://github.com/jbeder/yaml-cpp
///       2. ensure `extern/elroy_common_msg/generated/include/`
///          is apart of your path (if you are using ECM already,
///          it likely is)
struct SEnumUtility {
  YAML::Node node_;
  std::atomic_bool initialized_{false};

  void init(const std::string &path) {
    node_ = YAML::LoadFile(path);
    initialized_ = true;
  }
};

/// @note helper structs to make API of EnumTo easier:
///       elroy_common_msg::EumToValue("Mode", "PowerplantSafe");
static SEnumUtility k_enum_to_value_;
static SEnumUtility k_enum_to_string_;

/// @brief Returns the enum_name::enumerator integer value
/// @param enum_name the enum name
/// @param enumerator the enumerator name (as a string)
/// @return 0 (Unknown) if the enum_name::enumerator combo is
///         not a valid key, else the equivalent integer value.
__attribute__((unused)) static uint32_t EnumToValue(const std::string &enum_name,
                            const std::string &enumerator) {
  if (false == k_enum_to_value_.initialized_) {
    k_enum_to_value_.init("extern/elroy_common_msg/generated/utility/enum_to_value.yaml");
  }
  uint32_t ret = 0;
  try {
    ret = k_enum_to_value_.node_[enum_name][enumerator].as<uint32_t>();
  } catch (const YAML::Exception& e) {
    std::cerr << "elroy_common_msg::CEnumUtility Error: \n\t "
      << e.what() << std::endl;
  }
  return ret;
}

/// @brief Returns the string version of the enum_name::enumerator_value
/// @param enum_name the enumerator's name
/// @param enumerator_value the enumerator's integer value
/// @return The enumerator_value as its equivalent string, Unknown if
///         the (enum_name, enumerator_value) is not a valid key
__attribute__((unused)) static std::string EnumToString(const std::string &enum_name,
                                const uint32_t enumerator_value) {
  if (false == k_enum_to_string_.initialized_) {
    k_enum_to_string_.init("extern/elroy_common_msg/generated/utility/enum_to_string.yaml");
  }
  std::string ret = "Unknown";
  try {
    ret = k_enum_to_string_.node_[enum_name][enumerator_value].as<std::string>();
  } catch (const YAML::Exception& e) {
    std::cerr << "elroy_common_msg::CEnumUtility Error: \n\t "
      << e.what() << std::endl;
  }
  return ret;
}

}  // namespace elroy_common_msg

#endif  // ELROY_COMMON_MSG_UTILITY_C_ENUM_UTILITY_H_