/**
 * mingw_compat.h - MinGW compatibility prefix header for HLSDK cross-compilation.
 *
 * Strategy: include windows.h first to get MinGW's HSPRITE definition,
 * then define HLSDK_HSPRITE_DEFINED so cdll_int.h skips its conflicting typedef.
 * This way all downstream code uses MinGW's HSPRITE type consistently.
 */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
/* Tell cdll_int.h not to redefine HSPRITE since windows.h already did */
#define HLSDK_HSPRITE_DEFINED
