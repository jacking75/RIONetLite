#pragma once

#include "define.h"
#include "i_net_event.h"

#include <cstdint>
#include <memory>

namespace rionetlite {

// Compatibility API. New code should include <rionetlite/server.h> and use
// rionetlite::Server.
class ServerNetManager {
 public:
  ServerNetManager();
  ~ServerNetManager();

  ServerNetManager(const ServerNetManager&) = delete;
  ServerNetManager& operator=(const ServerNetManager&) = delete;
  ServerNetManager(ServerNetManager&&) noexcept;
  ServerNetManager& operator=(ServerNetManager&&) noexcept;

  NetErrorCode Initialize(NetConfig net_cfg, INetEventManager* p_net_event_mgr);
  void Start();
  void End();

  NetErrorCode SendData(std::int32_t session_index, std::uint64_t session_uid,
                        std::uint32_t data_size, const char* p_data);
  void ForcedClose(std::int32_t session_index, std::uint64_t session_uid);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace rionetlite
