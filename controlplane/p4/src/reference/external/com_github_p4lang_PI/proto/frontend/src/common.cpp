/* Copyright 2013-present Barefoot Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Antonin Bas (antonin@barefootnetworks.com)
 *
 */

#include "common.h"

#include <string>

namespace pi {

namespace fe {

namespace proto {

namespace common {

namespace {

// count leading zeros in byte
uint8_t clz(uint8_t byte) {
  if (byte == 0) {
    std::cout << "common::count_leading_zeros - byte == 0" << std::endl;
  } else {
    std::cout << "common::count_leading_zeros -> byte (str repr) == \"" << static_cast<std::uint8_t>(byte) << "\"" << std::endl;
  }
  static constexpr uint8_t clz_table[16] =
      {4, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
  uint8_t half_byte_hi = byte >> 4;
  std::cout << "common::count_leading_zeros -> half_byte_hi = " << static_cast<std::uint8_t>(half_byte_hi) << std::endl;
  std::cout << "common::count_leading_zeros -> half_byte_hi (1) = " << static_cast<std::uint8_t>(byte >> 4) << std::endl;
//   std::cout << "common::count_leading_zeros -> half_byte_hi (2) = " << static_cast<std::uint8_t>(0 >> 4) << std::endl;
  std::cout << "common::count_leading_zeros -> half_byte_hi (3) = " << static_cast<std::uint8_t>(byte / 16) << std::endl;
//   std::cout << "common::count_leading_zeros -> half_byte_hi (4) = " << static_cast<std::uint8_t>(0 / 16) << std::endl;
  uint8_t half_byte_lo = byte & 0x0f;
  std::cout << "common::count_leading_zeros -> half_byte_lo = " << static_cast<std::uint8_t>(half_byte_lo) << std::endl;
  std::cout << "common::count_leading_zeros -> clz_table[half_byte_lo] = " << static_cast<std::uint8_t>(clz_table[half_byte_lo]) << std::endl;
  std::cout << "common::count_leading_zeros -> clz_table[half_byte_hi] = " << static_cast<std::uint8_t>(clz_table[half_byte_hi]) << std::endl;
  return (half_byte_hi == 0) ?
      (4 + clz_table[half_byte_lo]) : clz_table[half_byte_hi];
}

// count trailing zeros in byte
uint8_t ctz(uint8_t b) {
  static constexpr uint8_t ctz_table[16] =
      {4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0};
  uint8_t half_byte_hi = b >> 4;
  uint8_t half_byte_lo = b & 0x0f;
  return (half_byte_lo == 0) ?
      (4 + ctz_table[half_byte_hi]) : ctz_table[half_byte_lo];
}

}  // namespace

Code check_proto_bytestring(const std::string &str, size_t nbits) {
  std::cout << "common::check_proto_bytestring -> str = " << str << std::endl;
  std::cout << "common::check_proto_bytestring -> nbits = " << nbits << std::endl;
  size_t nbytes = (nbits + 7) / 8;
  // BEGIN
  std::cout << "common::check_proto_bytestring -> expected sizes: str.size() == nbytes" << std::endl;
  std::cout << "common::check_proto_bytestring -> str.size() = " << str.size() << std::endl;
  std::cout << "common::check_proto_bytestring -> nbytes = " << nbytes << std::endl;
  // END
  if (str.size() != nbytes) return Code::INVALID_ARGUMENT;
  size_t zero_nbits = (nbytes * 8) - nbits;
  std::cout << "common::check_proto_bytestring -> str[0] = " << str[0] << std::endl;
  std::cout << "common::check_proto_bytestring -> byte to count_leading_zeros = " << static_cast<std::uint8_t>(str[0]) << std::endl;
  std::cout << "common::check_proto_bytestring -> -----------------" << std::endl;
  auto not_zero_pos = static_cast<size_t>(clz(static_cast<uint8_t>(str[0])));
//   std::cout << "common::check_proto_bytestring -> -----------------" << std::endl;
//   not_zero_pos = static_cast<size_t>(clz(static_cast<uint8_t>(0)));
  std::cout << "common::check_proto_bytestring -> -----------------" << std::endl;
  // BEGIN
  std::cout << "common::check_proto_bytestring -> expected leading zeros: not_zero_pos >= zero_nbits" << std::endl;
  std::cout << "common::check_proto_bytestring -> not_zero_pos = " << not_zero_pos << std::endl;
  std::cout << "common::check_proto_bytestring -> zero_nbits = " << zero_nbits << std::endl;
  // END
  if (not_zero_pos < zero_nbits) return Code::INVALID_ARGUMENT;
  return Code::OK;
}

bool check_prefix_trailing_zeros(const std::string &str, int pLen) {
  size_t bitwidth = str.size() * 8;
  // must be guaranteed by caller
  assert(pLen >= 0 && static_cast<size_t>(pLen) <= bitwidth);
  size_t trailing_zeros = bitwidth - pLen;
  size_t pos = str.size() - 1;
  for (; trailing_zeros >= 8; trailing_zeros -= 8) {
    if (str[pos] != 0) return false;
    pos--;
  }
  return (trailing_zeros == 0) ||
      (ctz(static_cast<uint8_t>(str[pos])) >= trailing_zeros);
}

std::string range_default_lo(size_t nbits) {
  size_t nbytes = (nbits + 7) / 8;
  return std::string(nbytes, '\x00');
}

std::string range_default_hi(size_t nbits) {
  size_t nbytes = (nbits + 7) / 8;
  std::string hi(nbytes, '\xff');
  size_t zero_nbits = (nbytes * 8) - nbits;
  uint8_t mask = 0xff >> zero_nbits;
  hi[0] &= static_cast<char>(mask);
  assert(check_proto_bytestring(hi, nbits) == Code::OK);
  return hi;
}

}  // namespace common

}  // namespace proto

}  // namespace fe

}  // namespace pi