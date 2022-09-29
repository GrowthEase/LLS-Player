# env
cmake_minimum_required(VERSION 3.8)
project(rtd)

# common
message("*********************")
message(${CMAKE_CURRENT_SOURCE_DIR})
message("*********************")

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../)

# includes
#include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${ROOT_DIR}/third_party/webrtc/src")
include_directories("${ROOT_DIR}/third_party/webrtc/src/third_party/abseil-cpp")
include_directories("${ROOT_DIR}/third_party/webrtc/src/third_party/opus/src/include")
include_directories("${ROOT_DIR}/src")

# sources
#aux_source_directory(${ROOT_DIR}/src/rtd SOURCE_FILES)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_api.cpp)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_api.h)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_api_impl.cpp)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_api_impl.h)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_demuxer.cpp)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_demuxer.h)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_engine_interface.cpp)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_engine_interface.h)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_signaling.cpp)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_signaling.h)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_frame_queue.cpp)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_frame_queue.h)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_log.cpp)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_log.h)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_video_decoder_factory.cpp)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_video_decoder_factory.h)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_audio_decoder_factory.cpp)
list(APPEND SOURCE_FILES ${ROOT_DIR}/src/rtd_audio_decoder_factory.h)

# preprocessor macros
add_definitions(-DRTD_EXPORTS -DWEBRTC_POSIX -DWEBRTC_MAC -DWEBRTC_IOS)

# flags (c and c++)
add_compile_options(-Wall -Oz -fvisibility=hidden -miphoneos-version-min=8.0 -funwind-tables -fembed-bitcode)

# flags (just c)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

# flags (just c++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -std=c++14 -fno-exceptions -fno-rtti")


# add all source files to target shared lib(rtd)
add_library(rtd STATIC ${SOURCE_FILES})