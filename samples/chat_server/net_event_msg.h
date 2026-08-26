#pragma once

#include <cstdint>

enum class NetEventType : std::int8_t {
  kNONE = 0,
  kCONNECTION,
  kCLOSE,
  kONRECV
};

struct NetEventMsg {
  int session_index_ = -1;
  uint64_t uid_ = 0;
  NetEventType event_type_ = NetEventType::kNONE;
  uint16_t data_size_ = 0;
  char* p_data_ = nullptr;
};