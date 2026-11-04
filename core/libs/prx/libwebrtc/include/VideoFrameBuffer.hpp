#pragma once

#include <cstddef>

struct WebRtcVideoFrameBuffer;

enum class WebRtcVideoFrameBufferType : int {
    Native = 0,
    I420 = 1,
};

enum class WebRtcVideoFrameBufferSlot : std::size_t {
    AddRef = 0,
    Type = 4,
    Width = 5,
    Height = 6,
};

struct WebRtcVideoFrameBufferRef {
    WebRtcVideoFrameBuffer* buffer;
};

static_assert(sizeof(WebRtcVideoFrameBufferType) == 4);
static_assert(sizeof(WebRtcVideoFrameBufferRef) == 8);
