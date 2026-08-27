#include "detail/rio_api.h"

namespace rionetlite::detail {

bool WindowsRioApi::Initialize(const SOCKET socket) noexcept {
  GUID function_table_id = WSAID_MULTIPLE_RIO;
  DWORD bytes = 0;
  functions_ = {};
  return WSAIoctl(socket, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
                  &function_table_id, sizeof(function_table_id), &functions_,
                  sizeof(functions_), &bytes, nullptr, nullptr) == 0;
}

RIO_CQ WindowsRioApi::CreateCompletionQueue(
    const std::uint32_t queue_size,
    PRIO_NOTIFICATION_COMPLETION notification) noexcept {
  return functions_.RIOCreateCompletionQueue(queue_size, notification);
}

void WindowsRioApi::CloseCompletionQueue(const RIO_CQ queue) noexcept {
  functions_.RIOCloseCompletionQueue(queue);
}

ULONG WindowsRioApi::DequeueCompletion(const RIO_CQ queue, PRIORESULT results,
                                       const ULONG result_count) noexcept {
  return functions_.RIODequeueCompletion(queue, results, result_count);
}

int WindowsRioApi::Notify(const RIO_CQ queue) noexcept {
  return functions_.RIONotify(queue);
}

RIO_RQ WindowsRioApi::CreateRequestQueue(
    const SOCKET socket, const ULONG max_outstanding_receive,
    const ULONG max_receive_data_buffers, const ULONG max_outstanding_send,
    const ULONG max_send_data_buffers, const RIO_CQ receive_queue,
    const RIO_CQ send_queue, void* socket_context) noexcept {
  return functions_.RIOCreateRequestQueue(
      socket, max_outstanding_receive, max_receive_data_buffers,
      max_outstanding_send, max_send_data_buffers, receive_queue, send_queue,
      socket_context);
}

BOOL WindowsRioApi::Receive(const RIO_RQ queue, PRIO_BUF buffers,
                            const ULONG buffer_count, const DWORD flags,
                            void* request_context) noexcept {
  return functions_.RIOReceive(queue, buffers, buffer_count, flags,
                               request_context);
}

BOOL WindowsRioApi::Send(const RIO_RQ queue, PRIO_BUF buffers,
                         const ULONG buffer_count, const DWORD flags,
                         void* request_context) noexcept {
  return functions_.RIOSend(queue, buffers, buffer_count, flags,
                            request_context);
}

RIO_BUFFERID WindowsRioApi::RegisterBuffer(char* data,
                                           const DWORD size) noexcept {
  return functions_.RIORegisterBuffer(data, size);
}

void WindowsRioApi::DeregisterBuffer(const RIO_BUFFERID buffer_id) noexcept {
  functions_.RIODeregisterBuffer(buffer_id);
}

}  // namespace rionetlite::detail
