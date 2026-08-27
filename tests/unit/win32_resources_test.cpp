#include "detail/win32_resources.h"
#include "fake_rio_api.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace rionetlite::detail {
namespace {

TEST(WinsockRuntimeTest, StartAndResetAreIdempotent) {
  WinsockRuntime winsock;
  EXPECT_EQ(winsock.Start(), 0);
  EXPECT_EQ(winsock.Start(), 0);
  EXPECT_TRUE(winsock.active());
  winsock.Reset();
  winsock.Reset();
  EXPECT_FALSE(winsock.active());
}

TEST(UniqueHandleTest, MoveTransfersOwnership) {
  UniqueHandle first(CreateEventW(nullptr, TRUE, FALSE, nullptr));
  ASSERT_TRUE(first);
  const HANDLE original = first.get();
  UniqueHandle second(std::move(first));
  EXPECT_FALSE(first);
  EXPECT_EQ(second.get(), original);
}

TEST(DoubleMappedBufferTest, RejectsUnalignedSize) {
  auto buffer = DoubleMappedBuffer::Create(4097);
  ASSERT_FALSE(buffer.has_value());
  EXPECT_EQ(buffer.error(), static_cast<DWORD>(ERROR_INVALID_PARAMETER));
}

TEST(DoubleMappedBufferTest, RejectsSizeThatWouldOverflowMappedRange) {
  auto buffer =
      DoubleMappedBuffer::Create(std::numeric_limits<std::uint32_t>::max());
  ASSERT_FALSE(buffer.has_value());
  EXPECT_EQ(buffer.error(), static_cast<DWORD>(ERROR_INVALID_PARAMETER));
}

TEST(DoubleMappedBufferTest, MapsTheSameStorageTwice) {
  auto buffer = DoubleMappedBuffer::Create(4096);
  ASSERT_TRUE(buffer.has_value());
  ASSERT_NE(buffer->data(), nullptr);
  buffer->data()[0] = 'R';
  EXPECT_EQ(buffer->data()[buffer->size()], 'R');
}

TEST(RegisteredRioBufferTest, DoesNotDeregisterOutstandingMemory) {
  test::FakeRioApi rio;
  char storage[16]{};

  RegisteredRioBuffer buffer;
  ASSERT_TRUE(buffer.Register(rio, storage, sizeof(storage)));
  buffer.AddOutstanding();
  EXPECT_FALSE(buffer.Reset());
  EXPECT_EQ(rio.deregister_buffer_calls, 0U);
  buffer.CompleteOutstanding();
  EXPECT_TRUE(buffer.Reset());
  EXPECT_EQ(rio.deregister_buffer_calls, 1U);
}

}  // namespace
}  // namespace rionetlite::detail
