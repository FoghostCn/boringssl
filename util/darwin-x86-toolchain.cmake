set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR "x86_64")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -target x86_64-apple-darwin" CACHE STRING "c++ flags")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -target x86_64-apple-darwin" CACHE STRING "c flags")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -target x86_64-apple-darwin" CACHE STRING "asm flags")
