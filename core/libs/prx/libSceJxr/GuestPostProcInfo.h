#pragma once
#include <stdint.h>

typedef struct JxrPostProcInfo {
    int32_t mbDc;
    uint8_t mbTexture;
    int32_t blockDc[4][4];
    uint8_t blockTexture[4][4];
} JxrPostProcInfo;
