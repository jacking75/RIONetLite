#include "chat_server_content.h"
#include "chat_server.h"
#include "logger.h"

#include "packet.h"

#include <format>
#include <thread>

bool ChatServerContent::Init(rionetlite::ServerNetManager* p_net_mgr) {
  pref_net_mgr_ = p_net_mgr;

  // TODO 설정 정보에서 가져오기
  const int max_session_count = 12;
  const int max_msg_count_per_session = 8;

  client_session_mgr_.Init(max_session_count);

  room_mgr_.Init(0, 12, 4);

  packet_mgr_.Init(&client_session_mgr_, &room_mgr_);

  SetSendFunction();

  net_event_msg_pool_.Init(max_session_count, max_msg_count_per_session);

  if (InitNetMsgQueue() == false) {
    return false;
  }

  return true;
}

void ChatServerContent::Start() {
  Logger::WriteLog((int)rionetlite::LogLevel::kINFO,
                   "ChatServerContent::Start");

  is_run_ = true;

  work_thread_ = std::thread([&]() { Update(); });
}

void ChatServerContent::Stop() {
  Logger::WriteLog((int)rionetlite::LogLevel::kINFO,
                   "ChatServerContent::Stop - Start");

  is_run_ = false;

  if (msg_queue_iocp_ != INVALID_HANDLE_VALUE) {
    CloseHandle(msg_queue_iocp_);
  }

  if (work_thread_.joinable()) {
    work_thread_.join();
  }

  Logger::WriteLog((int)rionetlite::LogLevel::kINFO,
                   "ChatServerContent::Stop - End");
}

void ChatServerContent::Update() {
  Logger::WriteLog((int)rionetlite::LogLevel::kINFO,
                   "ChatServerContent::Update - Start");

  int fail_count = 0;

  while (is_run_) {
    NetEventMsg* p_msg = nullptr;
    DWORD dummy1 = 0;
    DWORD dummy2 = 0;
    auto wait_time = INFINITE;

    if (auto result = GetQueuedCompletionStatus(
            msg_queue_iocp_, &dummy1, (PULONG_PTR)&dummy2,
            reinterpret_cast<LPOVERLAPPED*>(&p_msg), wait_time);
        result == false) {
      ++fail_count;

      if (fail_count < 3) {
        Logger::WriteLog(
            (int)rionetlite::LogLevel::kERROR,
            "ChatServerContent::Update - fail GetQueuedCompletionStatus");
        break;
      }
      continue;
    } else {
      fail_count = 0;
    }

    switch (p_msg->event_type_) {
      case NetEventType::kCONNECTION: {
        Logger::WriteLog((int)rionetlite::LogLevel::kDEBUG,
                         "ChatServerContent::Update - kCONNECTION");

        auto p_session =
            client_session_mgr_.GetClientByConnIdx(p_msg->session_index_);
        p_session->Connected(p_msg->uid_);
      } break;
      case NetEventType::kCLOSE: {
        Logger::WriteLog((int)rionetlite::LogLevel::kDEBUG,
                         "ChatServerContent::Update - kCLOSE");
        net_event_msg_pool_.ClearSession(p_msg->session_index_);
      } break;
      case NetEventType::kONRECV: {
        Logger::WriteLog((int)rionetlite::LogLevel::kDEBUG,
                         "ChatServerContent::Update - kONRECV");
        packet_mgr_.ProcessRecvPacket(p_msg->session_index_, p_msg->uid_,
                                      p_msg->data_size_, p_msg->p_data_);
      } break;
      default:
        Logger::WriteLog((int)rionetlite::LogLevel::kERROR,
                         "ChatServerContent::Update - Unknown net event");
    }
  }

  Logger::WriteLog((int)rionetlite::LogLevel::kINFO,
                   "ChatServerContent::Update - End");
}

void ChatServerContent::OnConnect(const std::int32_t index,
                                  const std::uint64_t uid) {
  Logger::WriteLog(
      (int)rionetlite::LogLevel::kDEBUG,
      std::format("[OnConnect] - SessionIndex({0}), UniqueId({1})", index, uid)
          .c_str());

  auto p_event_msg = net_event_msg_pool_.Alloc(index);
  p_event_msg->uid_ = uid;
  p_event_msg->event_type_ = NetEventType::kCONNECTION;

  PostNetMessage(p_event_msg);
}

void ChatServerContent::OnClose(const std::int32_t index,
                                const std::uint64_t uid,
                                const rionetlite::DisconnectReason reason) {
  Logger::WriteLog(
      (int)rionetlite::LogLevel::kDEBUG,
      std::format("[OnClose] - SessionIndex({0}), Reason({1})", index,
                  rionetlite::DisconnectReasonStr[(int)reason])
          .c_str());

  auto p_event_msg = net_event_msg_pool_.Alloc(index);
  p_event_msg->uid_ = uid;
  p_event_msg->event_type_ = NetEventType::kCLOSE;

  PostNetMessage(p_event_msg);
}

std::uint32_t ChatServerContent::OnReceive(
    const std::int32_t index, [[maybe_unused]] const std::uint64_t uid,
    const std::uint32_t data_size, char* p_data) {
  // printf("[OnReceive] - SessionIndex(%d), size(%d)\n", index_,
  // (int)dataSize_); auto usedDataSize = ProcessPacket(index, uid, data_size,
  // p_data); return usedDataSize;
  const int max_packet_size = 1024;  // TODO config의 값을 사용하도록 변경 필요
  auto readPos = 0;
  auto remainSize = data_size;

  while (remainSize >= PACKET_HEADER_SIZE) {
    auto [p_msg, err] = GenerateNetEventMsg(index, remainSize, &p_data[readPos],
                                            max_packet_size);

    if (p_msg == nullptr) {
      if (err != ErrorCode::kNONE) {
        Logger::WriteLog(
            (int)rionetlite::LogLevel::kERROR,
            std::format("[OnReceive] - SessionIndex({0}), error({1})", index,
                        (int)err)
                .c_str());
      }
      break;
    }

    p_msg->uid_ = uid;
    PostNetMessage(p_msg);

    readPos += (int)p_msg->data_size_;
    remainSize -= (int)p_msg->data_size_;
  }

  return readPos;
}

std::tuple<NetEventMsg*, ErrorCode> ChatServerContent::GenerateNetEventMsg(
    const int32_t session_index, const int packet_data_size,
    char* p_packet_data, const int max_packet_size) {
  auto pPktHeader = (PacketHeader*)p_packet_data;

  if (max_packet_size < pPktHeader->length_) {
    return {nullptr, ErrorCode::kPACKET_SIZE_TOO_LARGE};
  }

  if (packet_data_size < pPktHeader->length_) {
    return {nullptr, ErrorCode::kNONE};
  }

  auto p_event_msg = net_event_msg_pool_.Alloc(session_index);
  p_event_msg->event_type_ = NetEventType::kONRECV;
  p_event_msg->data_size_ = pPktHeader->length_;
  p_event_msg->p_data_ = p_packet_data;
  return {p_event_msg, ErrorCode::kNONE};
}

BOOL ChatServerContent::PostNetMessage(NetEventMsg* pEvnet_) {
  // 오류는 절대 없다고 가정한다.
  // Boost.Asio의 경우 다른 식으로 오류에 대해 방어하고 있다. 참고하자
  // https://www.boost.org/doc/libs/1_67_0/boost/asio/detail/impl/win_iocp_io_context.ipp

  return PostQueuedCompletionStatus(msg_queue_iocp_, 0, NULL,
                                    reinterpret_cast<LPOVERLAPPED>(pEvnet_));
}

void ChatServerContent::SetSendFunction() {
  auto send_func =
      [&](const std::int32_t session_index, const std::uint64_t session_uid,
          const std::uint32_t data_size, const char* p_data) -> int {
    return (int)pref_net_mgr_->SendData(session_index, session_uid, data_size,
                                        p_data);
  };

  room_mgr_.SendPacketFunc = send_func;
  packet_mgr_.SendPacketFunc = send_func;
}
