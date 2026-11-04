#include "strcodec.h"
#include <stddef.h>
#include <stdint.h>

typedef struct tagPostProcInfo* (*ReferenceRows)[2];

void JxrReferenceSlideOneMBRow_nid_no_patch(void* rows, size_t channels, size_t mbWidth, int top, int bottom) {
    slideOneMBRow((ReferenceRows)rows, channels, mbWidth, top, bottom);
}

void JxrReferenceUpdatePostProcInfo_nid_no_patch(void* rows, int32_t* mb, size_t mbX, size_t channel) {
    updatePostProcInfo((ReferenceRows)rows, mb, mbX, channel);
}

void JxrReferencePostProcMB_nid_no_patch(void* rows, int32_t* p0, int32_t* p1, size_t mbX, size_t channel,
                                         int threshold) {
    postProcMB((ReferenceRows)rows, p0, p1, mbX, channel, threshold);
}

void JxrReferencePostProcBlock_nid_no_patch(void* rows, int32_t* p0, int32_t* p1, size_t mbX, size_t channel,
                                            int threshold) {
    postProcBlock((ReferenceRows)rows, p0, p1, mbX, channel, threshold);
}
