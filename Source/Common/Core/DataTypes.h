#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <inttypes.h>

typedef union Vec3f
{
    struct { float X, Y, Z; };
    struct { float R, G, B; };
} Vec3f;

typedef union Vec4f 
{ 
    struct { float X, Y, Z, W; };
    struct { float R, G, B, A; };
} Vec4f;

#ifdef __cplusplus
}
#endif
