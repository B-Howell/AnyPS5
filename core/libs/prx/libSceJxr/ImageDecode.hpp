#ifndef CORE_LIBS_PRX_LIBSCEJXR_IMAGEDECODE_HPP
#define CORE_LIBS_PRX_LIBSCEJXR_IMAGEDECODE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJxr/Stream.hpp"
#include "prx/libSceJxr/include/GuidExports.hpp"

namespace Jxr {

struct ImageDecode {
    std::int64_t (APS5_VABI* initialize)(ImageDecode*, Stream*);
    std::int64_t (APS5_VABI* getPixelFormat)(ImageDecode*, JxrGuestGuid*);
    std::int64_t (APS5_VABI* getSize)(ImageDecode*, std::int32_t*, std::int32_t*);
    std::int64_t (APS5_VABI* getResolution)(ImageDecode*, float*, float*);
    std::int64_t (APS5_VABI* getColorContext)(ImageDecode*, std::uint8_t*, std::uint32_t*);
    std::int64_t (APS5_VABI* getDescriptiveMetadata)(ImageDecode*, void*);
    std::int64_t (APS5_VABI* getRawStream)(ImageDecode*, Stream**);
    std::int64_t (APS5_VABI* copy)(ImageDecode*, const void*, std::uint8_t*, std::uint32_t);
    std::int64_t (APS5_VABI* getFrameCount)(ImageDecode*, std::uint32_t*);
    std::int64_t (APS5_VABI* selectFrame)(ImageDecode*, std::uint32_t);
    std::int64_t (APS5_VABI* release)(ImageDecode**);

    Stream* stream;
    std::int32_t streamOwner;
    std::uint32_t reserved0;
    std::size_t startOffset;

    JxrGuestGuid pixelFormat;

    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t currentLine;

    float resolutionX;
    float resolutionY;

    std::uint32_t frameCount;

    std::array<std::uint8_t, 88> misc;
    std::array<std::uint8_t, 66320> reserved;
};

static_assert(sizeof(ImageDecode) == 66560);
static_assert(offsetof(ImageDecode, initialize) == 0);
static_assert(offsetof(ImageDecode, getPixelFormat) == 8);
static_assert(offsetof(ImageDecode, getSize) == 16);
static_assert(offsetof(ImageDecode, getResolution) == 24);
static_assert(offsetof(ImageDecode, getColorContext) == 32);
static_assert(offsetof(ImageDecode, getDescriptiveMetadata) == 40);
static_assert(offsetof(ImageDecode, getRawStream) == 48);
static_assert(offsetof(ImageDecode, copy) == 56);
static_assert(offsetof(ImageDecode, getFrameCount) == 64);
static_assert(offsetof(ImageDecode, selectFrame) == 72);
static_assert(offsetof(ImageDecode, release) == 80);
static_assert(offsetof(ImageDecode, stream) == 88);
static_assert(offsetof(ImageDecode, streamOwner) == 96);
static_assert(offsetof(ImageDecode, startOffset) == 104);
static_assert(offsetof(ImageDecode, pixelFormat) == 112);
static_assert(offsetof(ImageDecode, width) == 128);
static_assert(offsetof(ImageDecode, height) == 132);
static_assert(offsetof(ImageDecode, currentLine) == 136);
static_assert(offsetof(ImageDecode, resolutionX) == 140);
static_assert(offsetof(ImageDecode, resolutionY) == 144);
static_assert(offsetof(ImageDecode, frameCount) == 148);
static_assert(offsetof(ImageDecode, misc) == 152);

} // namespace Jxr

#endif
