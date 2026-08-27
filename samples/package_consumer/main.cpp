#include <rionetlite/server.h>

class Handler final : public rionetlite::EventHandler {
 public:
  void OnConnected(rionetlite::SessionId) override {}
  std::size_t OnData(rionetlite::SessionId,
                     std::span<const std::byte> data) override {
    return data.size();
  }
  void OnClosed(rionetlite::SessionId, rionetlite::CloseReason) override {}
};

int main() {
  Handler handler;
  rionetlite::ServerConfig config;
  config.port = 32452;
  auto server = rionetlite::Server::Create(config, handler);
  if (!server || !server->Start()) {
    return 1;
  }
  return server->Stop() ? 0 : 1;
}
