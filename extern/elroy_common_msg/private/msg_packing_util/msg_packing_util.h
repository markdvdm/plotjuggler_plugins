/// @copyright Copyright 2021 Elroy Air Inc. All rights reserved.
/// @author phillip yee <phillip.yee@elroyair.com>
/// @note copied this file from `flight_software/main` commit
///       a079d3. When this is merged the flight_software version
///       (of this file) will need to be removed

#ifndef ELROY_COMMON_MSG_PRIVATE_MSG_PACKING_UTIL_MSG_PACKING_UTIL_H_
#define ELROY_COMMON_MSG_PRIVATE_MSG_PACKING_UTIL_MSG_PACKING_UTIL_H_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "msg_packing_util/endian.h"

namespace elroy_common_msg {

class MsgPackingUtil {
 public:
  /// templated function to pack src into dest
  /// @note assumes dest is start of src (index 0)
  /// @note the user must ensure `dest` is of appropriate length for
  ///       src type
  /// @see explicit template instantiation below class declaration
  /// @param[out] dest the destination array
  /// @param[in] src const value to put into dest
  /// @pre none
  /// @post none
  /// @return the number of bytes put into dest
  template <typename T, std::enable_if_t<!std::is_enum<T>::value, bool> = true>
  static size_t Pack(uint8_t* dest, const T src);

  template <typename EnumType, std::enable_if_t<std::is_enum<EnumType>::value, bool> = true>
  static size_t Pack(uint8_t* dest, const EnumType src) {
    return MsgPackingUtil::Pack(dest, static_cast<const typename std::underlying_type<EnumType>::type>(src));
  }

  /// template function to unpack src into dest
  /// @note assumes src is start of dest (index 0)
  /// @note the user must ensure `dest` is of appropriate length for
  ///       src type
  /// @see explicit template instantiation below class declaration
  /// @param[out] dest reference to output type
  /// @param[in] src the source array
  /// @pre none
  /// @post none
  /// @return the number of bytes read from src
  template <typename T, std::enable_if_t<!std::is_enum<T>::value, bool> = true>
  static size_t Unpack(T& dest, const uint8_t* src);

  template <typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
  static size_t Unpack(T& dest, const uint8_t* src) {
    // create temporary variable with underlying type of enumerate
    typename std::underlying_type<T>::type tmp_dest;
    // unpack
    size_t ret = MsgPackingUtil::Unpack(tmp_dest, src);
    // save off enumerate type
    dest = static_cast<T>(tmp_dest);
    return ret;
  }

 private:
  /// Template union for eight-byte data types
  template <typename T>
  union EightByteUnion {
    T data;
    uint8_t data_array[8];
  };

  /// Templated function to pack a eight-byte data type into
  /// a 8-bit array
  /// @note assumes dest[0] is beginning location
  /// @param[out] dest the 8-bit array to put src into
  /// @param[in] src the eight-byte data type
  /// @pre none
  /// @post none
  /// @return 8
  template <typename T>
  static size_t PackEightByteType(uint8_t* dest, const T src) {
    assert(nullptr != dest);
    EightByteUnion<T> to_array;

    to_array.data = htole(src);
    dest[0] = to_array.data_array[0];
    dest[1] = to_array.data_array[1];
    dest[2] = to_array.data_array[2];
    dest[3] = to_array.data_array[3];
    dest[4] = to_array.data_array[4];
    dest[5] = to_array.data_array[5];
    dest[6] = to_array.data_array[6];
    dest[7] = to_array.data_array[7];

    return 8;  /// size of 8 bytes is 8
  }

  /// Templated function to unpack a eight-byte data type
  /// from an 8-bit array
  /// @note assumes src[0] is the beginning of the eight-byte data
  /// @param[out] dest reference to eight byte destination
  /// @param[in] src the 8-bit array to pull
  /// @pre none
  /// @post none
  /// @return 8
  template <typename T>
  static size_t UnpackEightByteType(T& dest, const uint8_t* src) {
    assert(nullptr != src);
    EightByteUnion<T> from_array;

    from_array.data_array[0] = src[0];
    from_array.data_array[1] = src[1];
    from_array.data_array[2] = src[2];
    from_array.data_array[3] = src[3];
    from_array.data_array[4] = src[4];
    from_array.data_array[5] = src[5];
    from_array.data_array[6] = src[6];
    from_array.data_array[7] = src[7];
    dest = elroy_common_msg::letoh(from_array.data);

    return 8;  /// size of 8 bytes is 8
  }

  /// Template union for four-byte data types
  template <typename T>
  union FourByteUnion {
    T data;
    uint8_t data_array[4];
  };

  /// Templated function to pack a four-byte data type into
  /// a 8-bit array
  /// @note assumes dest[0] is beginning location
  /// @param[out] dest the 8-bit array to put src into
  /// @param[in] src the four-byte data type
  /// @pre none
  /// @post none
  /// @return 4
  template <typename T>
  static size_t PackFourByteType(uint8_t* dest, const T src) {
    assert(nullptr != dest);
    FourByteUnion<T> to_array;

    to_array.data = htole(src);
    dest[0] = to_array.data_array[0];
    dest[1] = to_array.data_array[1];
    dest[2] = to_array.data_array[2];
    dest[3] = to_array.data_array[3];

    return 4;
  }

  /// Templated function to unpack a four-byte data type
  /// from an 8-bit array
  /// @note assumes src[0] is the beginning of the four-byte data
  /// @param[out] dest reference to the four byte destination
  /// @param[in] src the 8-bit array to pull
  /// @pre none
  /// @post none
  /// @return 4
  template <typename T>
  static size_t UnpackFourByteType(T& dest, const uint8_t* src) {
    assert(nullptr != src);
    FourByteUnion<T> from_array;

    from_array.data_array[0] = src[0];
    from_array.data_array[1] = src[1];
    from_array.data_array[2] = src[2];
    from_array.data_array[3] = src[3];
    dest = elroy_common_msg::letoh(from_array.data);

    return 4;
  }

  /// Template union for two-byte data types
  template <typename T>
  union TwoByteUnion {
    T data;
    uint8_t data_array[2];
  };

  /// Templated function to pack a two-byte data type from
  /// an 8-bit array
  /// @note assumes dest[0] is the start of two-byte data
  /// @param[out] dest the 8-bit array to put src into
  /// @param[in] src the two-byte data type src
  /// @pre none
  /// @post none
  /// @return 2
  template <typename T>
  static size_t PackTwoByteType(uint8_t* dest, const T src) {
    assert(nullptr != dest);
    TwoByteUnion<T> to_array;

    to_array.data = htole(src);
    dest[0] = to_array.data_array[0];
    dest[1] = to_array.data_array[1];

    return 2;
  }

  /// Templated function to unpack a two-byte data type
  /// from an 8-bit array
  /// @note assumes src[0] is the start of two-byte data
  /// @param[out] dest reference the two-byte destination
  /// @param[in] src the 8-bit array to src from
  /// @pre none
  /// @post none
  /// @return 2
  template <typename T>
  static size_t UnpackTwoByteType(T& dest, const uint8_t* src) {
    assert(nullptr != src);
    TwoByteUnion<T> from_array;

    from_array.data_array[0] = src[0];
    from_array.data_array[1] = src[1];
    dest = elroy_common_msg::letoh(from_array.data);

    return 2;
  }

  /// Templated function to pack a one-byte data type
  /// from an 8-bit array.
  /// @note this function exists for consistency of calling functions,
  ///       it will look better with uniform calling
  /// @note assumes dest[0] is where one-byte data is
  /// @param[out] dest the 8-bit array to put src into
  /// @param[in] src the one-byte data type src
  /// @return 1
  template <typename T>
  static size_t PackOneByteType(uint8_t* dest, const T src) {
    assert(nullptr != dest);

    dest[0] = htole(src);

    return 1;
  }

  /// Templated function to unpack a one-byte data type
  /// from an 8-bit array
  /// @note this function exists for consistency of calling function,
  ///       it will look better with uniform calling.
  /// @note assumes src[0] is the start of the one-byte data
  /// @param[out] dest the one byte data type destination
  /// @param[in] src the 8-bit array src
  /// @return 1
  template <typename T>
  static size_t UnpackOneByteType(T& dest, const uint8_t* src) {
    assert(nullptr != src);

    dest = elroy_common_msg::letoh(src[0]);

    return 1;
  }

  /// Delete constructor and destructor, static helper object
  MsgPackingUtil() = delete;
  virtual ~MsgPackingUtil() = delete;
};

/// explicit template instantiation of double, float, and fix-width data types
/// inline required to not get multiple definition linking errors
template <>
inline size_t MsgPackingUtil::Pack<double>(uint8_t* dest, const double src) {
  return MsgPackingUtil::PackEightByteType<double>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Unpack<double>(double& dest, const uint8_t* src) {
  return MsgPackingUtil::UnpackEightByteType<double>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Pack<uint64_t>(uint8_t* dest, const uint64_t src) {
  return MsgPackingUtil::PackEightByteType<uint64_t>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Unpack<uint64_t>(uint64_t& dest, const uint8_t* src) {
  return MsgPackingUtil::UnpackEightByteType<uint64_t>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Pack<int64_t>(uint8_t* dest, const int64_t src) {
  return MsgPackingUtil::PackEightByteType<int64_t>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Unpack<int64_t>(int64_t& dest, const uint8_t* src) {
  return MsgPackingUtil::UnpackEightByteType<int64_t>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Pack<float>(uint8_t* dest, const float src) {
  return MsgPackingUtil::PackFourByteType<float>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Unpack<float>(float& dest, const uint8_t* src) {
  return MsgPackingUtil::UnpackFourByteType<float>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Pack<uint32_t>(uint8_t* dest, const uint32_t src) {
  return MsgPackingUtil::PackFourByteType<uint32_t>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Unpack<uint32_t>(uint32_t& dest, const uint8_t* src) {
  return MsgPackingUtil::UnpackFourByteType<uint32_t>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Pack<int32_t>(uint8_t* dest, const int32_t src) {
  return MsgPackingUtil::PackFourByteType<int32_t>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Unpack<int32_t>(int32_t& dest, const uint8_t* src) {
  return MsgPackingUtil::UnpackFourByteType<int32_t>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Pack<uint16_t>(uint8_t* dest, const uint16_t src) {
  return MsgPackingUtil::PackTwoByteType<uint16_t>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Unpack<uint16_t>(uint16_t& dest, const uint8_t* src) {
  return MsgPackingUtil::UnpackTwoByteType<uint16_t>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Pack<int16_t>(uint8_t* dest, const int16_t src) {
  return MsgPackingUtil::PackTwoByteType<int16_t>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Unpack<int16_t>(int16_t& dest, const uint8_t* src) {
  return MsgPackingUtil::UnpackTwoByteType<int16_t>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Pack<uint8_t>(uint8_t* dest, const uint8_t src) {
  return MsgPackingUtil::PackOneByteType<uint8_t>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Unpack<uint8_t>(uint8_t& dest, const uint8_t* src) {
  return MsgPackingUtil::UnpackOneByteType<uint8_t>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Pack<int8_t>(uint8_t* dest, const int8_t src) {
  return MsgPackingUtil::PackOneByteType<int8_t>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Unpack<int8_t>(int8_t& dest, const uint8_t* src) {
  return MsgPackingUtil::UnpackOneByteType<int8_t>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Pack<bool>(uint8_t* dest, const bool src) {
  return MsgPackingUtil::PackOneByteType<bool>(dest, src);
}

template <>
inline size_t MsgPackingUtil::Unpack<bool>(bool& dest, const uint8_t* src) {
  return MsgPackingUtil::UnpackOneByteType<bool>(dest, src);
}

}  // namespace elroy_common_msg

#endif  // ELROY_COMMON_MSG_PRIVATE_MSG_PACKING_UTIL_MSG_PACKING_UTIL_H_
