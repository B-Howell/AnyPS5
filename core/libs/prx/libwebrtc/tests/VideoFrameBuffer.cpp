#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc VideoFrameBuffer check failed at line %u\n", location.line());
    std::abort();
}

template<typename TException, typename TAction> void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, location);
}

struct FakeBuffer {
    const void* const* vtable;
};

const WebRtcVideoFrameBuffer* expectedBuffer;
int width;
int height;
WebRtcVideoFrameBufferType type;
int addRefCalls;
int typeCalls;
int widthCalls;
int heightCalls;

void Reset() {
    addRefCalls = 0;
    typeCalls = 0;
    widthCalls = 0;
    heightCalls = 0;
}

void APS5_VABI AddRef(const WebRtcVideoFrameBuffer* buffer) {
    Require(buffer == expectedBuffer);
    ++addRefCalls;
}

WebRtcVideoFrameBufferType APS5_VABI Type(const WebRtcVideoFrameBuffer* buffer) {
    Require(buffer == expectedBuffer);
    ++typeCalls;
    return type;
}

int APS5_VABI Width(const WebRtcVideoFrameBuffer* buffer) {
    Require(buffer == expectedBuffer);
    ++widthCalls;
    return width;
}

int APS5_VABI Height(const WebRtcVideoFrameBuffer* buffer) {
    Require(buffer == expectedBuffer);
    ++heightCalls;
    return height;
}

const std::array<const void*, 7> vtable = {reinterpret_cast<const void*>(&AddRef), nullptr, nullptr, nullptr,
    reinterpret_cast<const void*>(&Type), reinterpret_cast<const void*>(&Width), reinterpret_cast<const void*>(&Height)};
const std::array<const void*, 7> emptyVtable = {};

FakeBuffer fake = {vtable.data()};
FakeBuffer empty = {emptyVtable.data()};

WebRtcVideoFrameBuffer* Buffer(FakeBuffer& buffer) {
    return reinterpret_cast<WebRtcVideoFrameBuffer*>(&buffer);
}

void TestChroma(int size, int chroma) {
    expectedBuffer = Buffer(fake);
    width = size;
    height = -7;
    Reset();
    Require(_ZNK6webrtc19I420BufferInterface11ChromaWidthEv_nid_postfix(Buffer(fake)) == chroma);
    Require(widthCalls == 1 && heightCalls == 0 && addRefCalls == 0 && typeCalls == 0);
    width = -7;
    height = size;
    Reset();
    Require(_ZNK6webrtc19I420BufferInterface12ChromaHeightEv_nid_postfix(Buffer(fake)) == chroma);
    Require(heightCalls == 1 && widthCalls == 0 && addRefCalls == 0 && typeCalls == 0);
}

void TestChromaSizes() {
    TestChroma(0, 0);
    TestChroma(1, 1);
    TestChroma(2, 1);
    TestChroma(3, 2);
    TestChroma(852, 426);
    TestChroma(853, 427);
    TestChroma(1080, 540);
    TestChroma(1081, 541);
    TestChroma(-1, 0);
    TestChroma(-2, 0);
    TestChroma(-3, -1);
    TestChroma(INT_MAX - 1, INT_MAX / 2);
    TestChroma(INT_MIN, INT_MIN / 2 + 1);
    expectedBuffer = Buffer(fake);
    width = INT_MAX;
    height = INT_MAX;
    RequireThrows<std::overflow_error>([] { _ZNK6webrtc19I420BufferInterface11ChromaWidthEv_nid_postfix(Buffer(fake)); });
    RequireThrows<std::overflow_error>([] { _ZNK6webrtc19I420BufferInterface12ChromaHeightEv_nid_postfix(Buffer(fake)); });
    RequireThrows<std::invalid_argument>([] { _ZNK6webrtc19I420BufferInterface11ChromaWidthEv_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZNK6webrtc19I420BufferInterface12ChromaHeightEv_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZNK6webrtc19I420BufferInterface11ChromaWidthEv_nid_postfix(Buffer(empty)); });
    RequireThrows<std::invalid_argument>([] { _ZNK6webrtc19I420BufferInterface12ChromaHeightEv_nid_postfix(Buffer(empty)); });
}

void TestToI420() {
    expectedBuffer = Buffer(fake);
    Reset();
    WebRtcVideoFrameBufferRef result = {reinterpret_cast<WebRtcVideoFrameBuffer*>(&empty)};
    Require(_ZN6webrtc19I420BufferInterface6ToI420Ev_nid_postfix(&result, Buffer(fake)) == &result);
    Require(result.buffer == Buffer(fake));
    Require(addRefCalls == 1 && typeCalls == 0 && widthCalls == 0 && heightCalls == 0);
    Require(fake.vtable == vtable.data());
    Reset();
    result.buffer = Buffer(fake);
    Require(_ZN6webrtc19I420BufferInterface6ToI420Ev_nid_postfix(&result, nullptr) == &result);
    Require(result.buffer == nullptr);
    Require(addRefCalls == 0);
    RequireThrows<std::invalid_argument>([] { _ZN6webrtc19I420BufferInterface6ToI420Ev_nid_postfix(nullptr, Buffer(fake)); });
    Require(addRefCalls == 0);
    result.buffer = nullptr;
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc19I420BufferInterface6ToI420Ev_nid_postfix(&result, Buffer(empty)); });
    Require(result.buffer == nullptr);
}

void TestGetI420() {
    Reset();
    Require(_ZNK6webrtc19I420BufferInterface7GetI420Ev_nid_postfix(Buffer(fake)) == Buffer(fake));
    Require(_ZNK6webrtc19I420BufferInterface7GetI420Ev_nid_postfix(Buffer(empty)) == Buffer(empty));
    Require(_ZNK6webrtc19I420BufferInterface7GetI420Ev_nid_postfix(nullptr) == nullptr);
    Require(addRefCalls == 0 && typeCalls == 0 && widthCalls == 0 && heightCalls == 0);
}

void TestGetMappedFrameBuffer() {
    expectedBuffer = Buffer(fake);
    std::array<WebRtcVideoFrameBufferType, 2> types = {WebRtcVideoFrameBufferType::I420, WebRtcVideoFrameBufferType::Native};
    type = WebRtcVideoFrameBufferType::Native;
    Reset();
    WebRtcVideoFrameBufferRef result = {Buffer(fake)};
    Require(_ZN6webrtc16VideoFrameBuffer20GetMappedFrameBufferEN3rtc9ArrayViewINS0_4TypeELln4711EEE_nid_postfix(&result, Buffer(fake), types.data(), types.size()) == &result);
    Require(result.buffer == nullptr);
    Require(typeCalls == 1 && addRefCalls == 0 && widthCalls == 0 && heightCalls == 0);
    Require(types[0] == WebRtcVideoFrameBufferType::I420 && types[1] == WebRtcVideoFrameBufferType::Native);
    Reset();
    result.buffer = Buffer(fake);
    Require(_ZN6webrtc16VideoFrameBuffer20GetMappedFrameBufferEN3rtc9ArrayViewINS0_4TypeELln4711EEE_nid_postfix(&result, Buffer(fake), nullptr, 0) == &result);
    Require(result.buffer == nullptr && typeCalls == 1);
    for (int value : {1, 2, 8, -1}) {
        type = static_cast<WebRtcVideoFrameBufferType>(value);
        Reset();
        result.buffer = Buffer(fake);
        RequireThrows<std::invalid_argument>([&] {
            _ZN6webrtc16VideoFrameBuffer20GetMappedFrameBufferEN3rtc9ArrayViewINS0_4TypeELln4711EEE_nid_postfix(&result, Buffer(fake), types.data(), types.size());
        });
        Require(typeCalls == 1 && result.buffer == Buffer(fake));
    }
    RequireThrows<std::invalid_argument>([&] {
        _ZN6webrtc16VideoFrameBuffer20GetMappedFrameBufferEN3rtc9ArrayViewINS0_4TypeELln4711EEE_nid_postfix(nullptr, Buffer(fake), types.data(), types.size());
    });
    RequireThrows<std::invalid_argument>([&] {
        _ZN6webrtc16VideoFrameBuffer20GetMappedFrameBufferEN3rtc9ArrayViewINS0_4TypeELln4711EEE_nid_postfix(&result, nullptr, types.data(), types.size());
    });
    RequireThrows<std::invalid_argument>([&] {
        _ZN6webrtc16VideoFrameBuffer20GetMappedFrameBufferEN3rtc9ArrayViewINS0_4TypeELln4711EEE_nid_postfix(&result, Buffer(empty), types.data(), types.size());
    });
}

}

int main() {
    TestChromaSizes();
    TestToI420();
    TestGetI420();
    TestGetMappedFrameBuffer();
    return 0;
}
