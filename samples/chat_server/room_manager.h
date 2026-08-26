#pragma once

#include "error_code.h"
#include "room.h"

#include <cstdint>

class RoomManager {
 public:
  RoomManager() = default;
  ~RoomManager() = default;

  std::function<int(const std::int32_t, const std::uint64_t,
                    const std::uint32_t, const char*)>
      SendPacketFunc;

  void Init(const std::int32_t begin_room_number,
            const std::int32_t max_room_count,
            const std::int32_t max_room_client_count) {
    begin_room_number_ = begin_room_number;
    max_room_count_ = max_room_count;
    end_room_number_ = begin_room_number + max_room_count;

    room_list_ = std::vector<Room*>(max_room_count);

    for (auto i = 0; i < max_room_count; i++) {
      room_list_[i] = new Room();
      room_list_[i]->SendPacketFunc = SendPacketFunc;
      room_list_[i]->Init((i + begin_room_number), max_room_client_count);
    }
  }

  std::int32_t GetMaxRoomCount() { return max_room_count_; }

  std::uint16_t EnterClient(std::int32_t room_number, ClientSession* p_client) {
    auto pRoom = GetRoomByNumber(room_number);
    if (pRoom == nullptr) {
      return (std::uint16_t)ErrorCode::kROOM_INVALID_INDEX;
    }

    return pRoom->EnterClient(p_client);
  }

  std::uint16_t LeaveClient(std::int32_t room_number, ClientSession* p_client) {
    auto pRoom = GetRoomByNumber(room_number);
    if (pRoom == nullptr) {
      return (std::uint16_t)ErrorCode::kROOM_INVALID_INDEX;
    }

    p_client->SetDomainState(ClientSession::DomainState::kLOGIN);
    pRoom->LeaveClient(p_client);
    return (std::uint16_t)ErrorCode::kNONE;
  }

  Room* GetRoomByNumber(std::int32_t number) {
    if (number < begin_room_number_ || number >= end_room_number_) {
      return nullptr;
    }

    auto index = (number - begin_room_number_);
    return room_list_[index];
  }

 private:
  std::vector<Room*> room_list_;
  std::int32_t begin_room_number_ = 0;
  std::int32_t end_room_number_ = 0;
  std::int32_t max_room_count_ = 0;
};
