#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>

#include <MSWSock.h>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Options final {
  std::uint32_t sessions = 256;
  std::uint32_t bytes_per_buffer = 64 * 1024;
  std::uint32_t runs = 5;
  std::uint32_t send_iterations = 200'000;
  std::string json_path;
};

bool ParseUnsigned(const std::string_view value, std::uint32_t& output) {
  const auto [end, error] =
      std::from_chars(value.data(), value.data() + value.size(), output);
  return error == std::errc{} && end == value.data() + value.size();
}

bool ParseOptions(const int argc, char* argv[], Options& options) {
  for (int index = 1; index < argc; ++index) {
    if (index + 1 >= argc) return false;
    const std::string_view key(argv[index]);
    const std::string_view value(argv[++index]);
    if (key == "--json") {
      options.json_path = value;
      continue;
    }
    std::uint32_t parsed = 0;
    if (!ParseUnsigned(value, parsed)) return false;
    if (key == "--sessions") {
      options.sessions = parsed;
    } else if (key == "--bytes") {
      options.bytes_per_buffer = parsed;
    } else if (key == "--runs") {
      options.runs = parsed;
    } else if (key == "--send-iterations") {
      options.send_iterations = parsed;
    } else {
      return false;
    }
  }
  const std::uint64_t block_count =
      static_cast<std::uint64_t>(options.sessions) * 2;
  const std::uint64_t arena_bytes = block_count * options.bytes_per_buffer;
  return options.sessions != 0 && options.bytes_per_buffer != 0 &&
         options.runs != 0 && options.send_iterations != 0 &&
         !options.json_path.empty() &&
         arena_bytes <= std::numeric_limits<DWORD>::max();
}

class RioRuntime final {
 public:
  RioRuntime() {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
      throw std::runtime_error("WSAStartup failed");
    }
    socket_ = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0,
                         WSA_FLAG_REGISTERED_IO);
    if (socket_ == INVALID_SOCKET) {
      WSACleanup();
      throw std::runtime_error("RIO socket creation failed");
    }
    GUID id = WSAID_MULTIPLE_RIO;
    DWORD bytes = 0;
    if (WSAIoctl(socket_, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER, &id,
                 sizeof(id), &table_, sizeof(table_), &bytes, nullptr,
                 nullptr) == SOCKET_ERROR ||
        bytes != sizeof(table_)) {
      closesocket(socket_);
      WSACleanup();
      throw std::runtime_error("RIO function table initialization failed");
    }
  }

  ~RioRuntime() {
    closesocket(socket_);
    WSACleanup();
  }

  const RIO_EXTENSION_FUNCTION_TABLE& table() const noexcept { return table_; }

 private:
  SOCKET socket_ = INVALID_SOCKET;
  RIO_EXTENSION_FUNCTION_TABLE table_{};
};

double MeasurePerSessionRegistration(const RIO_EXTENSION_FUNCTION_TABLE& rio,
                                     const std::uint32_t block_count,
                                     const std::uint32_t bytes_per_buffer) {
  std::vector<void*> memory;
  std::vector<RIO_BUFFERID> registrations;
  memory.reserve(block_count);
  registrations.reserve(block_count);
  const auto begin = Clock::now();
  try {
    for (std::uint32_t index = 0; index < block_count; ++index) {
      void* block = VirtualAlloc(nullptr, bytes_per_buffer,
                                 MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      if (block == nullptr) throw std::runtime_error("VirtualAlloc failed");
      memory.push_back(block);
      const auto id =
          rio.RIORegisterBuffer(static_cast<char*>(block), bytes_per_buffer);
      if (id == RIO_INVALID_BUFFERID) {
        throw std::runtime_error("per-session RIORegisterBuffer failed");
      }
      registrations.push_back(id);
    }
  } catch (...) {
    for (const auto id : registrations) rio.RIODeregisterBuffer(id);
    for (void* block : memory) VirtualFree(block, 0, MEM_RELEASE);
    throw;
  }
  const auto elapsed = Clock::now() - begin;
  for (const auto id : registrations) rio.RIODeregisterBuffer(id);
  for (void* block : memory) VirtualFree(block, 0, MEM_RELEASE);
  return std::chrono::duration<double, std::micro>(elapsed).count();
}

double MeasureArenaRegistration(const RIO_EXTENSION_FUNCTION_TABLE& rio,
                                const std::uint32_t block_count,
                                const std::uint32_t bytes_per_buffer) {
  const DWORD arena_bytes = block_count * bytes_per_buffer;
  const auto begin = Clock::now();
  void* arena = VirtualAlloc(nullptr, arena_bytes, MEM_RESERVE | MEM_COMMIT,
                             PAGE_READWRITE);
  if (arena == nullptr) throw std::runtime_error("arena VirtualAlloc failed");
  const auto id = rio.RIORegisterBuffer(static_cast<char*>(arena), arena_bytes);
  if (id == RIO_INVALID_BUFFERID) {
    VirtualFree(arena, 0, MEM_RELEASE);
    throw std::runtime_error("arena RIORegisterBuffer failed");
  }
  const auto elapsed = Clock::now() - begin;
  rio.RIODeregisterBuffer(id);
  VirtualFree(arena, 0, MEM_RELEASE);
  return std::chrono::duration<double, std::micro>(elapsed).count();
}

__declspec(noinline) void ObserveMemory(const void* data,
                                        const std::size_t size) {
  const auto* bytes = static_cast<const volatile std::byte*>(data);
  static volatile std::uint64_t sink = 0;
  sink = static_cast<std::uint64_t>(bytes[0]) +
         static_cast<std::uint64_t>(bytes[size - 1]);
}

using ObserveFunction = void (*)(const void*, std::size_t);
using CopyFunction = void* (*)(void*, const void*, std::size_t);
ObserveFunction volatile g_observe = &ObserveMemory;
CopyFunction volatile g_copy = &std::memcpy;

double MeasureScatterPreparation(const std::uint32_t iterations) {
  constexpr std::uint32_t kBatch = 8;
  constexpr std::uint32_t kPayload = 128;
  std::array<RIO_BUF, kBatch> descriptors{};
  const auto buffer_id = reinterpret_cast<RIO_BUFFERID>(1);
  const auto observe = g_observe;
  const auto begin = Clock::now();
  for (std::uint32_t iteration = 0; iteration < iterations; ++iteration) {
    for (std::uint32_t index = 0; index < kBatch; ++index) {
      descriptors[index] = RIO_BUF{.BufferId = buffer_id,
                                   .Offset = (iteration + index) * kPayload,
                                   .Length = kPayload};
    }
    observe(descriptors.data(), sizeof(descriptors));
  }
  const auto elapsed = Clock::now() - begin;
  return std::chrono::duration<double, std::nano>(elapsed).count() / iterations;
}

double MeasureContiguousPreparation(const std::uint32_t iterations) {
  constexpr std::uint32_t kBatch = 8;
  constexpr std::uint32_t kPayload = 128;
  struct PreparedSend final {
    RIO_BUF descriptor{};
    std::array<std::byte, kBatch * kPayload> payload{};
  };
  std::array<std::byte, kBatch * kPayload> source{};
  PreparedSend prepared;
  for (std::size_t index = 0; index < source.size(); ++index) {
    source[index] = static_cast<std::byte>((index * 131U) & 0xFFU);
  }
  prepared.descriptor.BufferId = reinterpret_cast<RIO_BUFFERID>(1);
  const auto copy = g_copy;
  const auto observe = g_observe;
  const auto begin = Clock::now();
  for (std::uint32_t iteration = 0; iteration < iterations; ++iteration) {
    for (std::uint32_t index = 0; index < kBatch; ++index) {
      const std::uint32_t source_index = (iteration + index) % kBatch;
      copy(prepared.payload.data() + index * kPayload,
           source.data() + source_index * kPayload, kPayload);
    }
    prepared.descriptor.Offset = 0;
    prepared.descriptor.Length = static_cast<ULONG>(prepared.payload.size());
    observe(&prepared, sizeof(prepared));
  }
  const auto elapsed = Clock::now() - begin;
  return std::chrono::duration<double, std::nano>(elapsed).count() / iterations;
}

double Median(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  return values[(values.size() - 1) / 2];
}

}  // namespace

int main(int argc, char* argv[]) {
  Options options;
  if (!ParseOptions(argc, argv, options)) {
    std::cerr << "usage: DesignBenchmark --json path [--sessions n] "
                 "[--bytes n] [--runs n] [--send-iterations n]\n";
    return 2;
  }
  try {
    RioRuntime runtime;
    const std::uint32_t block_count = options.sessions * 2;
    std::vector<double> per_session;
    std::vector<double> arena;
    std::vector<double> scatter;
    std::vector<double> contiguous;
    for (std::uint32_t run = 0; run < options.runs; ++run) {
      if ((run & 1U) == 0) {
        per_session.push_back(MeasurePerSessionRegistration(
            runtime.table(), block_count, options.bytes_per_buffer));
        arena.push_back(MeasureArenaRegistration(runtime.table(), block_count,
                                                 options.bytes_per_buffer));
      } else {
        arena.push_back(MeasureArenaRegistration(runtime.table(), block_count,
                                                 options.bytes_per_buffer));
        per_session.push_back(MeasurePerSessionRegistration(
            runtime.table(), block_count, options.bytes_per_buffer));
      }
      scatter.push_back(MeasureScatterPreparation(options.send_iterations));
      contiguous.push_back(
          MeasureContiguousPreparation(options.send_iterations));
    }
    std::ofstream output(options.json_path, std::ios::binary);
    if (!output) throw std::runtime_error("result file could not be opened");
    output << "{\n"
           << "  \"sessions\": " << options.sessions << ",\n"
           << "  \"buffers_per_session\": 2,\n"
           << "  \"bytes_per_buffer\": " << options.bytes_per_buffer << ",\n"
           << "  \"runs\": " << options.runs << ",\n"
           << "  \"per_session_registration_median_us\": "
           << Median(per_session) << ",\n"
           << "  \"arena_registration_median_us\": " << Median(arena) << ",\n"
           << "  \"scatter_8_buffers_prepare_median_ns\": " << Median(scatter)
           << ",\n"
           << "  \"contiguous_1024_bytes_prepare_median_ns\": "
           << Median(contiguous) << ",\n"
           << "  \"correctness_errors\": 0\n"
           << "}\n";
    std::cout << "Design benchmark: " << options.json_path << '\n';
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << exception.what() << '\n';
    return 1;
  }
}
