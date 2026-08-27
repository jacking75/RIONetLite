#pragma once

#include "detail/rio_api.h"

#include <cstdint>
#include <deque>
#include <vector>

namespace rionetlite::test {

class FakeRioApi final : public detail::RioApi {
 public:
  bool Initialize(SOCKET) noexcept override {
    ++initialize_calls;
    return initialize_succeeds && initialize_calls != initialize_fail_on_call;
  }

  RIO_CQ CreateCompletionQueue(std::uint32_t,
                               PRIO_NOTIFICATION_COMPLETION) noexcept override {
    ++create_completion_queue_calls;
    if (create_completion_queue_calls == create_completion_queue_fail_on_call) {
      return RIO_INVALID_CQ;
    }
    return completion_queue;
  }

  void CloseCompletionQueue(RIO_CQ) noexcept override {
    ++close_completion_queue_calls;
  }

  ULONG DequeueCompletion(RIO_CQ, PRIORESULT results,
                          const ULONG result_count) noexcept override {
    if (corrupt_completion_queue) {
      return RIO_CORRUPT_CQ;
    }
    ULONG copied = 0;
    while (copied < result_count && !completions.empty()) {
      results[copied++] = completions.front();
      completions.pop_front();
    }
    return copied;
  }

  int Notify(RIO_CQ) noexcept override {
    ++notify_calls;
    return notify_result;
  }

  RIO_RQ CreateRequestQueue(SOCKET, ULONG, ULONG, ULONG, ULONG, RIO_CQ, RIO_CQ,
                            void*) noexcept override {
    ++create_request_queue_calls;
    if (create_request_queue_calls == create_request_queue_fail_on_call) {
      return RIO_INVALID_RQ;
    }
    return request_queue;
  }

  BOOL Receive(RIO_RQ, PRIO_BUF buffers, ULONG, DWORD,
               void* request_context) noexcept override {
    ++receive_calls;
    last_buffer = *buffers;
    last_request_context = request_context;
    return receive_succeeds && receive_calls != receive_fail_on_call ? TRUE
                                                                     : FALSE;
  }

  BOOL Send(RIO_RQ, PRIO_BUF buffers, ULONG, DWORD,
            void* request_context) noexcept override {
    ++send_calls;
    if (record_posted_buffers) {
      posted_buffers.push_back(*buffers);
    }
    last_request_context = request_context;
    return send_succeeds && send_calls != send_fail_on_call ? TRUE : FALSE;
  }

  RIO_BUFFERID RegisterBuffer(char*, DWORD) noexcept override {
    ++register_buffer_calls;
    return register_succeeds && register_buffer_calls != register_fail_on_call
               ? reinterpret_cast<RIO_BUFFERID>(1)
               : RIO_INVALID_BUFFERID;
  }

  void DeregisterBuffer(RIO_BUFFERID) noexcept override {
    ++deregister_buffer_calls;
  }

  bool initialize_succeeds = true;
  bool receive_succeeds = true;
  bool send_succeeds = true;
  bool register_succeeds = true;
  bool record_posted_buffers = true;
  bool corrupt_completion_queue = false;
  std::uint32_t initialize_fail_on_call = 0;
  std::uint32_t create_completion_queue_fail_on_call = 0;
  std::uint32_t create_request_queue_fail_on_call = 0;
  std::uint32_t receive_fail_on_call = 0;
  std::uint32_t send_fail_on_call = 0;
  std::uint32_t register_fail_on_call = 0;
  RIO_CQ completion_queue = reinterpret_cast<RIO_CQ>(1);
  RIO_RQ request_queue = reinterpret_cast<RIO_RQ>(1);
  std::uint32_t initialize_calls = 0;
  std::uint32_t create_completion_queue_calls = 0;
  std::uint32_t close_completion_queue_calls = 0;
  std::uint32_t create_request_queue_calls = 0;
  std::uint32_t receive_calls = 0;
  std::uint32_t send_calls = 0;
  std::uint32_t register_buffer_calls = 0;
  std::uint32_t deregister_buffer_calls = 0;
  std::uint32_t notify_calls = 0;
  int notify_result = ERROR_SUCCESS;
  RIO_BUF last_buffer{};
  void* last_request_context = nullptr;
  std::vector<RIO_BUF> posted_buffers;
  std::deque<RIORESULT> completions;
};

}  // namespace rionetlite::test
