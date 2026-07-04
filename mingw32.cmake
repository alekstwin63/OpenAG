set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(CMAKE_C_COMPILER   i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  i686-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Fix old HLSDK/Steam __intN types not recognized by MinGW (MSVC-only types)
# and fix CBASE_DLLEXPORT to use visibility attr instead of _declspec (HLSDK uses EXPORT as return type qualifier)
add_compile_options(
    "-D__int8=char"
    "-D__int16=short"
    "-D__int32=int"
    "-D__int64=long long"
    "-DCBASE_DLLEXPORT=__attribute__((visibility(\"default\")))"
    "-DWIN32_LEAN_AND_MEAN"
    "-Wno-attributes"
    # Force-include compat header that undef's conflicting Windows types before HLSDK headers see them
    "-include/home/angel/OpenAG/mingw_compat.h"
)
