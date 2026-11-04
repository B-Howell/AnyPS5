#include "strcodec.h"
#include "prx/libSceJxr/GuestPostProcInfo.h"
#include <stddef.h>

_Static_assert(sizeof(struct tagPostProcInfo) == 88 && sizeof(JxrPostProcInfo) == 88, "post-processing record size");
_Static_assert(offsetof(struct tagPostProcInfo, ucMBTexture) == offsetof(JxrPostProcInfo, mbTexture) &&
                   offsetof(struct tagPostProcInfo, iBlockDC) == offsetof(JxrPostProcInfo, blockDc) &&
                   offsetof(struct tagPostProcInfo, ucBlockTexture) == offsetof(JxrPostProcInfo, blockTexture),
               "post-processing record offsets");
_Static_assert(offsetof(JxrPostProcInfo, mbTexture) == 4 && offsetof(JxrPostProcInfo, blockDc) == 8 &&
                   offsetof(JxrPostProcInfo, blockTexture) == 72,
               "LP64 post-processing record offsets");
_Static_assert(MAX_CHANNELS == 16 && sizeof(PixelI) == 4 && sizeof(Bool) == 4, "post-processing types");

typedef struct tagPostProcInfo* (*NativeRows)[2];

void JxrSlideOneMBRow_nid_no_patch(void* rows, size_t channels, size_t mbWidth, int top, int bottom) {
    slideOneMBRow((NativeRows)rows, channels, mbWidth, top, bottom);
}

void JxrUpdatePostProcInfo_nid_no_patch(void* rows, int32_t* mb, size_t mbX, size_t channel) {
    updatePostProcInfo((NativeRows)rows, mb, mbX, channel);
}

void JxrPostProcMB_nid_no_patch(void* rows, int32_t* p0, int32_t* p1, size_t mbX, size_t channel, int threshold) {
    postProcMB((NativeRows)rows, p0, p1, mbX, channel, threshold);
}

void JxrPostProcBlock_nid_no_patch(void* rows, int32_t* p0, int32_t* p1, size_t mbX, size_t channel, int threshold) {
    postProcBlock((NativeRows)rows, p0, p1, mbX, channel, threshold);
}
