#ifndef P4RUNTIME_CONVERSIONS_H
#define P4RUNTIME_CONVERSIONS_H

// Import P4-related declarations after any other
#include "../common/ns_def.inc"

// Data structures
using ::P4_CONFIG_NAMESPACE_ID::P4Info;


std::string encode_param_value(uint16_t value, size_t bitwidth) {
  char lsb, msb;
  std::string res;

  // From most to least significant bits (msb|lsb), retrieve specific octet by shifting
  // as many positions to the right as the most significant it in the octect
  // (that is, shift(bitwidth - most_significant_bit_position); e.g.,
  // "msb" will shift 16 bits - 16 bits, "lsb" will shift 16 bits - 8 bits)
  msb = value & 0xFF;
  lsb = value >> 8;
  res.push_back(msb);
  if (lsb != 0) {
    res.push_back(lsb);
  }

  // Fill in the remaining bytes (computed as the expected "nbytes" - sizeof generated string)
  // with leading zeros in order to fit the full bitwidth expected per value
  size_t nbytes = (bitwidth + 7) / 8;
  int remaining_zeros = nbytes - res.size();
  std::string res_byte = "";
  while (remaining_zeros-- > 0) {
    msb = 0 & 0xFF;
    res_byte.push_back(msb);
    res = res_byte + res;
    res_byte = "";
  }

  return res;
}

uint16_t decode_param_value(const std::string value) {
  uint16_t res = 0;

  // From most to least significant bits (msb|lsb), compute value by shifting
  // as many positions to the left as the most significant it in the octect
  // (that is, shift(size(bitstring) - num_bit_from_right_to_left - 1); e.g.,
  // a bitstring "2" will be  shift like this:
  // value[0]: 0 << 6-0-1 == 0 << 5 == 000000
  // value[1]: 0 << 6-1-1 == 0 << 4 == 000000
  // value[2]: 0 << 6-2-1 == 0 << 3 == 000000
  // value[3]: 0 << 6-3-1 == 0 << 2 == 000000
  // value[4]: 1 << 6-4-1 == 1 << 1 == 000010
  // value[5]: 0 << 6-5-1 == 0 << 1 == 000000
  // summing up, the final value is: 000010 == 2
  for (int i = 0; i < value.size(); i++) {
    res += uint16_t(value[i]) << value.size()-i-1;
  }

  return res;
}

::PROTOBUF_NAMESPACE_ID::uint32 get_p4_table_id_from_name(
    ::P4_CONFIG_NAMESPACE_ID::P4Info p4info, std::string table_name) {
  for (::P4_CONFIG_NAMESPACE_ID::Table table : p4info.tables()) {
    if (table_name == table.preamble().name()) {
      return table.preamble().id();
    }
  }
  return 0L;
}

::PROTOBUF_NAMESPACE_ID::uint32 get_p4_action_id_from_name(
    ::P4_CONFIG_NAMESPACE_ID::P4Info p4info, std::string action_name) {
  for (::P4_CONFIG_NAMESPACE_ID::Action action : p4info.actions()) {
    if (action_name == action.preamble().name()) {
      return action.preamble().id();
    }
  }
  return 0L;
}

::PROTOBUF_NAMESPACE_ID::uint32 get_p4_indirect_counter_id_from_name(
  ::P4_CONFIG_NAMESPACE_ID::P4Info p4info, std::string counter_name) {
  for (::P4_CONFIG_NAMESPACE_ID::Counter counter : p4info.counters()) {
    if (counter_name == counter.preamble().name()) {
      return counter.preamble().id();
    }
  }
  return 0L;
}

std::list<::PROTOBUF_NAMESPACE_ID::uint32> get_p4_indirect_counter_ids(
    ::P4_CONFIG_NAMESPACE_ID::P4Info p4info) {
  std::list<uint32_t> indirect_counter_ids = std::list<uint32_t>();
  for (::P4_CONFIG_NAMESPACE_ID::Counter counter : p4info.counters()) {
    indirect_counter_ids.push_back(counter.preamble().id());
  }
  return indirect_counter_ids;
}

#include "../common/ns_undef.inc"
#endif