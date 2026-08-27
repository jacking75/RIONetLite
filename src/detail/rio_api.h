#pragma once

#include <WinSock2.h>

#include <MSWSock.h>

#include <cstdint>

namespace rionetlite::detail {

class RioApi {
 public:
  virtual ~RioApi() = default;

  virtual bool Initialize(SOCKET socket) noexcept = 0;
  virtual RIO_CQ CreateCompletionQueue(
      std::uint32_t queue_size,
      PRIO_NOTIFICATION_COMPLETION notification) noexcept = 0;
  virtual void CloseCompletionQueue(RIO_CQ queue) noexcept = 0;
  virtual ULONG DequeueCompletion(RIO_CQ queue, PRIORESULT results,
                                  ULONG result_count) noexcept = 0;
  virtual int Notify(RIO_CQ queue) noexcept = 0;
  virtual RIO_RQ CreateRequestQueue(SOCKET socket,
                                    ULONG max_outstanding_receive,
                                    ULONG max_receive_data_buffers,
                                    ULONG max_outstanding_send,
                                    ULONG max_send_data_buffers,
                                    RIO_CQ receive_queue, RIO_CQ send_queue,
                                    void* socket_context) noexcept = 0;
  virtual BOOL Receive(RIO_RQ queue, PRIO_BUF buffers, ULONG buffer_count,
                       DWORD flags, void* request_context) noexcept = 0;
  virtual BOOL Send(RIO_RQ queue, PRIO_BUF buffers, ULONG buffer_count,
                    DWORD flags, void* request_context) noexcept = 0;
  virtual RIO_BUFFERID RegisterBuffer(char* data, DWORD size) noexcept = 0;
  virtual void DeregisterBuffer(RIO_BUFFERID buffer_id) noexcept = 0;
};

class WindowsRioApi final : public RioApi {
 public:
  bool Initialize(SOCKET socket) noexcept override;
  RIO_CQ CreateCompletionQueue(
      std::uint32_t queue_size,
      PRIO_NOTIFICATION_COMPLETION notification) noexcept override;
  void CloseCompletionQueue(RIO_CQ queue) noexcept override;
  ULONG DequeueCompletion(RIO_CQ queue, PRIORESULT results,
                          ULONG result_count) noexcept override;
  int Notify(RIO_CQ queue) noexcept override;
  RIO_RQ CreateRequestQueue(SOCKET socket, ULONG max_outstanding_receive,
                            ULONG max_receive_data_buffers,
                            ULONG max_outstanding_send,
                            ULONG max_send_data_buffers, RIO_CQ receive_queue,
                            RIO_CQ send_queue,
                            void* socket_context) noexcept override;
  BOOL Receive(RIO_RQ queue, PRIO_BUF buffers, ULONG buffer_count, DWORD flags,
               void* request_context) noexcept override;
  BOOL Send(RIO_RQ queue, PRIO_BUF buffers, ULONG buffer_count, DWORD flags,
            void* request_context) noexcept override;
  RIO_BUFFERID RegisterBuffer(char* data, DWORD size) noexcept override;
  void DeregisterBuffer(RIO_BUFFERID buffer_id) noexcept override;

 private:
  RIO_EXTENSION_FUNCTION_TABLE functions_{};
};

}  // namespace rionetlite::detail
