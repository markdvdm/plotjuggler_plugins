/// @note this file was copied from allocortec. When we stop
///       paying for their license we'll need to blast it.
/// @copyright 2018 allocortech inc. all rights reserved

#ifndef ELROY_COMMON_MSG_PRIVATE_MSG_PACKING_UTIL_ENDIAN_H_
#define ELROY_COMMON_MSG_PRIVATE_MSG_PACKING_UTIL ENDIAN_H_

#include <cstdint>
#include <type_traits>

namespace elroy_common_msg {

/// @{
/// Size sensitive byte swap operations
template <typename T>
inline typename std::enable_if<(std::is_integral<T>::value || std::is_enum<T>::value) && (sizeof(T) == 1), T>::type
bswap(T v) noexcept {
  return v;
}

template <typename T>
inline typename std::enable_if<(std::is_integral<T>::value || std::is_enum<T>::value) && (sizeof(T) == 2), T>::type
bswap(T v) noexcept {
  union vp {
    uint16_t u;
    T v;
  } *temp = reinterpret_cast<vp *>(&v);
  temp->u = __builtin_bswap16(temp->u);
  return temp->v;
}

template <typename T>
inline typename std::enable_if<(std::is_arithmetic<T>::value || std::is_enum<T>::value) && (sizeof(T) == 4), T>::type
bswap(T v) noexcept {
  union vp {
    uint32_t u;
    T v;
  } *temp = reinterpret_cast<vp *>(&v);
  temp->u = __builtin_bswap32(temp->u);
  return temp->v;
}

template <typename T>
inline typename std::enable_if<(std::is_arithmetic<T>::value || std::is_enum<T>::value) && (sizeof(T) == 8), T>::type
bswap(T v) noexcept {
  union vp {
    uint64_t u;
    T v;
  } *temp = reinterpret_cast<vp *>(&v);
  temp->u = __builtin_bswap64(temp->u);
  return temp->v;
}

// Forward declare the byte swap implementations for any other non-integral, non-enum or big ass
// values. These types will need to be implemented on a case-by-case basis, but need a declaration
// for compile time function of the endian templates below.
template <typename T>
typename std::enable_if<!(std::is_arithmetic<T>::value || std::is_enum<T>::value) || (sizeof(T) > 8), T>::type bswap(
    T &v) noexcept;

/// @}

#if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
// "Network" byte order

/// Convert a value from big endian to host byte order
template <typename T>
inline T betoh(const T v) noexcept {
  return v;
}
/// Convert a value from host to big endian byte order
template <typename T>
inline T htobe(const T v) noexcept {
  return v;
}

/// Convert a value from little endian to host byte order
template <typename T>
inline T letoh(const T v) noexcept {
  return bswap(v);
}
/// Convert a value from host to little endian byte order
template <typename T>
inline T htole(const T v) noexcept {
  return bswap(v);
}

#elif (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
// "x86" byte order

/// Convert a value from big endian to host byte order
template <typename T>
inline T betoh(const T v) noexcept {
  return bswap(v);
}
/// Convert a value from host to big endian byte order
template <typename T>
inline T htobe(const T v) noexcept {
  return bswap(v);
}

/// Convert a value from little endian to host byte order
template <typename T>
inline T letoh(const T v) noexcept {
  return v;
}
/// Convert a value from host to little endian byte order
template <typename T>
inline T htole(const T v) noexcept {
  return v;
}

#else
#error Platform byte order unhandled
#endif

/// Convert a value from network to host byte order
template <typename T>
inline T ntoh(const T v) noexcept {
  return betoh(v);
}
/// Convert a value from host to network byte order
template <typename T>
inline T hton(const T v) noexcept {
  return htobe(v);
}

} /* namespace elroy_common_msg */

#endif  // ELROY_COMMON_MSG_PRIVATE_MSG_PACKING_UTIL ENDIAN_H_