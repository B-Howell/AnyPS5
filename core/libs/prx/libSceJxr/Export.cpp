#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "Stream.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
constexpr std::int64_t WmpErrSuccess = 0;
constexpr std::int64_t WmpErrOutOfSequence = -109;

struct EncoderMetadata {
    std::uint8_t* data;
    std::uint32_t size;
    std::uint32_t reserved;
};

struct EncoderMetadataPrefix {
    std::byte reservedThroughHeaderDone[0xb0];
    std::int32_t headerDone;
    std::uint32_t headerPadding;
    std::uint64_t pixelOffset;
    std::uint64_t pixelBytes;
    EncoderMetadata colorContext;
    EncoderMetadata exif;
    EncoderMetadata gps;
    EncoderMetadata iptc;
    EncoderMetadata xmp;
    EncoderMetadata photoshop;
};

static_assert(sizeof(EncoderMetadata) == 0x10);
static_assert(sizeof(void*) == 8);
static_assert(sizeof(std::size_t) == 8);
static_assert(offsetof(EncoderMetadataPrefix, headerDone) == 0xb0);
static_assert(offsetof(EncoderMetadataPrefix, exif) + offsetof(EncoderMetadata, data) == 0xd8);
static_assert(offsetof(EncoderMetadataPrefix, exif) + offsetof(EncoderMetadata, size) == 0xe0);
static_assert(offsetof(EncoderMetadataPrefix, gps) + offsetof(EncoderMetadata, data) == 0xe8);
static_assert(offsetof(EncoderMetadataPrefix, gps) + offsetof(EncoderMetadata, size) == 0xf0);
static_assert(offsetof(EncoderMetadataPrefix, iptc) + offsetof(EncoderMetadata, data) == 0xf8);
static_assert(offsetof(EncoderMetadataPrefix, iptc) + offsetof(EncoderMetadata, size) == 0x100);
static_assert(offsetof(EncoderMetadataPrefix, photoshop) + offsetof(EncoderMetadata, data) == 0x118);
static_assert(offsetof(EncoderMetadataPrefix, photoshop) + offsetof(EncoderMetadata, size) == 0x120);
static_assert(sizeof(EncoderMetadataPrefix) == 0x128);

bool Overlaps(const std::uint8_t* first, std::uint32_t firstSize, const std::uint8_t* second, std::uint32_t secondSize) {
    if (first == second && first != nullptr && firstSize != 0) return true;
    if (first == nullptr || second == nullptr || firstSize == 0 || secondSize == 0) return false;
    const auto firstAddress = reinterpret_cast<std::uintptr_t>(first);
    const auto secondAddress = reinterpret_cast<std::uintptr_t>(second);
    if (firstAddress <= secondAddress) return secondAddress - firstAddress < firstSize;
    return firstAddress - secondAddress < secondSize;
}

std::int64_t SetMetadata(void* encoder, const std::uint8_t* data, std::uint32_t size, std::size_t offset) {
    if (encoder == nullptr) APS5_INVALID_ARG_EX;
    auto* bytes = static_cast<std::byte*>(encoder);
    std::int32_t headerDone;
    std::memcpy(&headerDone, bytes + offsetof(EncoderMetadataPrefix, headerDone), sizeof(headerDone));
    if (headerDone != 0) return WmpErrOutOfSequence;
    if (size != 0 && data == nullptr) APS5_INVALID_ARG_EX;

    auto* metadata = bytes + offset;
    auto* dataOffset = metadata + offsetof(EncoderMetadata, data);
    auto* sizeOffset = metadata + offsetof(EncoderMetadata, size);
    std::uint8_t* oldData = nullptr;
    std::uint32_t oldSize = 0;
    std::memcpy(&oldData, dataOffset, sizeof(oldData));
    std::memcpy(&oldSize, sizeOffset, sizeof(oldSize));
    if (Overlaps(data, size, oldData, oldSize)) APS5_INVALID_ARG_EX;
    void* oldGuestData = oldData;
    PKFree_nid_postfix(&oldGuestData);
    std::uint8_t* clearedData = nullptr;
    std::memcpy(dataOffset, &clearedData, sizeof(clearedData));
    const std::uint32_t clearedSize = 0;
    std::memcpy(sizeOffset, &clearedSize, sizeof(clearedSize));

    void* newGuestData = nullptr;
    const auto result = PKAlloc_nid_postfix(&newGuestData, size);
    auto* newData = static_cast<std::uint8_t*>(newGuestData);
    std::memcpy(dataOffset, &newData, sizeof(newData));
    if (result != WmpErrSuccess) return result;
    if (size != 0) std::memcpy(newData, data, size);
    std::memcpy(sizeOffset, &size, sizeof(size));
    return WmpErrSuccess;
}
}

extern "C" {

APS5_DUMMY_FUN
std::int64_t APS5_VABI PKFree_nid_postfix(void** memory) {
    if (memory != nullptr) {
        free_nid_postfix(*memory);

        *memory = nullptr;
    }
    return WmpErrSuccess;
}

std::int64_t APS5_VABI readIS_nid_postfix(void*, Jxr::BitIo* io) {
    if (!io) APS5_INVALID_ARG_EX;
    constexpr std::uintptr_t packetSize = 4096;
    const auto start = reinterpret_cast<std::uintptr_t>(io->start);
    if (((start ^ reinterpret_cast<std::uintptr_t>(io->current)) & packetSize) == 0) return WmpErrSuccess;
    auto* stream = io->stream;
    if (!stream || !stream->setPosition) APS5_INVALID_ARG_EX;
    stream->setPosition(stream, io->referenceOffset);
    if (!stream->read) APS5_INVALID_ARG_EX;
    {
        GuestArena::HostWrite access(io->start, packetSize);
        if (!access.Open()) APS5_INVALID_ARG_EX;
        stream->read(stream, io->start, packetSize);
    }
    io->referenceOffset += packetSize;
    if (!io->start) APS5_INVALID_ARG_EX;
    std::memcpy(&io->shadow, io->start, sizeof(io->shadow));
    const auto mask = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(io->mask));
    io->start = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(io->start) + packetSize) & mask);
    return WmpErrSuccess;
}

std::int64_t APS5_VABI writeIS_nid_postfix(void*, Jxr::BitIo* io) {
    if (!io) APS5_INVALID_ARG_EX;
    constexpr std::uintptr_t packetSize = 4096;
    const auto start = reinterpret_cast<std::uintptr_t>(io->start);
    if (((start ^ reinterpret_cast<std::uintptr_t>(io->current)) & packetSize) == 0) return WmpErrSuccess;
    if (!io->stream || !io->stream->write) APS5_INVALID_ARG_EX;
    const auto result = io->stream->write(io->stream, io->start, packetSize);
    if (result < 0) return result;
    if (!io->start) APS5_INVALID_ARG_EX;
    const auto mask = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(io->mask));
    io->start = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(io->start) + packetSize) & mask);
    return result;
}

int APS5_VABI AllocateCodingContextDec_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI CloseWS_List_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI CreateWS_List_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI CreateWS_Memory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EOSWS_File_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EOSWS_Memory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FreeDescMetadata_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI GetImageDecodeIID_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI GetImageEncodeIID_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI GetPixelFormatFromHash_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI GetPosWS_File_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI GetPosWS_List_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI GetPosWS_Memory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI IDPEmpty_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI InitZigzagScan_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKAllocAligned_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCodecFactory_CreateCodec_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCodecFactory_CreateDecoderFromFile_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCodecFactory_CreateFormatConverter_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCreateCodecFactory_Release_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCreateCodecFactory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCreateFactory_CreateStream_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCreateFactory_Release_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCreateFactory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_Convert_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_Copy_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_EnumConversions_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_GetPixelFormat_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_GetResolution_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_GetSize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_GetSourcePixelFormat_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_InitializeConvert_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_Initialize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_Release_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFreeAligned_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_Copy_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_Create_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_Create_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetColorContext_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetDescriptiveMetadata_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetFrameCount_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetPixelFormat_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetResolution_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetSize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_Initialize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_Release_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_SelectFrame_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_CreateNewFrame_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Create_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Create_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Initialize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Release_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetColorContext_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetDescriptiveMetadata_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

std::int64_t APS5_VABI PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(void* encoder, const std::uint8_t* data, std::uint32_t size) {
    return SetMetadata(encoder, data, size, offsetof(EncoderMetadataPrefix, exif));
}

std::int64_t APS5_VABI PKImageEncode_SetGPSInfoMetadata_WMP_nid_postfix(void* encoder, const std::uint8_t* data, std::uint32_t size) {
    return SetMetadata(encoder, data, size, offsetof(EncoderMetadataPrefix, gps));
}

std::int64_t APS5_VABI PKImageEncode_SetIPTCNAAMetadata_WMP_nid_postfix(void* encoder, const std::uint8_t* data, std::uint32_t size) {
    return SetMetadata(encoder, data, size, offsetof(EncoderMetadataPrefix, iptc));
}

std::int64_t APS5_VABI PKImageEncode_SetPhotoshopMetadata_WMP_nid_postfix(void* encoder, const std::uint8_t* data, std::uint32_t size) {
    return SetMetadata(encoder, data, size, offsetof(EncoderMetadataPrefix, photoshop));
}

int APS5_VABI PKImageEncode_SetPixelFormat_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetResolution_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetSize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetXMPMetadata_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Terminate_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Transcode_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_WritePixels_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_WriteSource_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ReadWS_File_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ReadWS_List_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ReadWS_Memory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ResetCodingContext_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SetPosWS_File_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SetPosWS_List_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SetPosWS_Memory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI WMPAlloc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI WMPFree_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI WriteWS_File_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI WriteWS_List_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI WriteWS_Memory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI advanceMRPtr_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI advanceOneMBRow_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI allocateBitIOInfo_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI allocatePredInfo_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI allocateQuantizer_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI allocateTileInfo_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI attachISRead_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI attachISWrite_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI attach_SB_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI checkImageBuffer_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI detachISRead_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI detachISWrite_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI detach_SB_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI dquantBits_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI fillToByte_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI flushBit16_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI flushToByte_SB_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI flushToByte_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI formatQuantizer_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI freePredInfo_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI freeQuantizer_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI freeTileInfo_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getACPredMode_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getBit16_S_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getBit16_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getBit16s_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getBit32_SB_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getBit32_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getBool16_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getByteRead_SB_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getDCACPredMode_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getPosRead_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getSizeRead_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getSizeWrite_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getTilePos_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI initMRPtr_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI initPostProc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI peekBit16_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI postProcBlock_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI postProcMB_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI putBit16_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI putBit16z_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI putBit32_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI remapQP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI setBitIOPointers_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI setUniformQuantizer_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI slideOneMBRow_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI swapMRPtr_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI termPostProc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI updatePostProcInfo_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI updatePredInfo_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI useDCQuantizer_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI useLPQuantizer_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

}
