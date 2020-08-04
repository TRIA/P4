// To be imported by other header that imports the required classes

struct P4Parameter {
  uint32_t id;
  // Bitstring with enough capacity to express strings with variable sizes
  uint64_t value;
  size_t bitwidth;
};

struct P4Action {
  uint32_t action_id;
  // Flag to indicate whether the action is the default choice for a table
  bool default_action;
  std::list<P4Parameter> parameters;
};

// Using same IDs as in spec
enum P4MatchType {
  exact = ::P4_NAMESPACE_ID::FieldMatch::FieldMatchTypeCase::kExact,
  ternary = ::P4_NAMESPACE_ID::FieldMatch::FieldMatchTypeCase::kTernary,
  lpm = ::P4_NAMESPACE_ID::FieldMatch::FieldMatchTypeCase::kLpm,
  range = ::P4_NAMESPACE_ID::FieldMatch::FieldMatchTypeCase::kRange,
  optional = ::P4_NAMESPACE_ID::FieldMatch::FieldMatchTypeCase::kOptional,
  other = ::P4_NAMESPACE_ID::FieldMatch::FieldMatchTypeCase::kOther
};

struct P4Match {
  uint32_t field_id;
  P4MatchType type;
  // Bitstring with enough capacity to express strings with variable sizes
  uint64_t value;
  size_t bitwidth;
  // Only required for LPM
  int32_t lpm_prefix;
  // Only required for Ternary
  std::string ternary_mask;
  // Required for Range
  std::string range_low;
  std::string range_high;
};

struct P4TableEntry {
  uint32_t table_id;
  P4Action action;
  std::list<P4Match> matches;
  // Only available for Ternary, Range or Optional matches
  int32_t priority;
  // Only available for non-default Actions. Use nanoseconds
  int64_t timeout_ns;
};

// Direct counter
struct P4DirectCounterEntry {
  P4TableEntry table_entry;
  ::P4_NAMESPACE_ID::CounterData data;
};

// Indirect counter
struct P4CounterEntry {
  uint32_t counter_id;
  ::P4_NAMESPACE_ID::Index index;
  ::P4_NAMESPACE_ID::CounterData data;
};