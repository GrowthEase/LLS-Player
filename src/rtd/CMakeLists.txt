cmake_minimum_required (VERSION 3.8)
project(rtd)

add_definitions(-DWEBRTC_POSIX -DWEBRTC_LINUX -DWEBRTC_ANDROID)
add_compile_options(-fvisibility=hidden -fno-rtti -Oz)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_link_options(-Wl,--gc-sections)

include_directories("${PROJECT_SOURCE_DIR}/../third_party/webrtc/src")
include_directories("${PROJECT_SOURCE_DIR}/../third_party/webrtc/src/third_party/abseil-cpp")
include_directories("${PROJECT_SOURCE_DIR}/../third_party/webrtc/src/third_party/opus/src/include")
include_directories("${PROJECT_SOURCE_DIR}/../third_party/webrtc/src/third_party/http/src")
include_directories("${PROJECT_SOURCE_DIR}/../third_party/webrtc/src/third_party/http/libcurl/include")

SET(RTD_SRC rtd_api.cpp
			rtd_api_impl.cpp
			rtd_demuxer.cpp
			rtd_engine_interface.cpp
			rtd_engine_impl.cpp
			rtd_signaling.cpp
			../third_party/webrtc/src/third_party/http/src/rtd_http.cpp
			rtd_frame_queue.cpp
			rtd_log.cpp
			rtd_video_decoder_factory.cpp
			rtd_audio_decoder_factory.cpp)

add_library (${PROJECT_NAME} SHARED ${RTD_SRC})

if (${ANDROID_ABI} STREQUAL "arm64-v8a")
	set(LD_LIBS
		${CMAKE_SOURCE_DIR}/../third_party/webrtc/src/third_party/http/libcurl/libs/android/arm64/libcurl.a
		${CMAKE_SOURCE_DIR}/../third_party/webrtc/src/out/andorid_arm64/obj/libwebrtc.a)
elseif (${ANDROID_ABI} STREQUAL "armeabi-v7a")
	set(LD_LIBS
		${CMAKE_SOURCE_DIR}/../third_party/webrtc/src/third_party/http/libcurl/libs/android/arm32/libcurl.a
		${CMAKE_SOURCE_DIR}/../third_party/webrtc/src/out/andorid_arm32/obj/libwebrtc.a)
elseif (${ANDROID_ABI} STREQUAL "x86_64")
	set(LD_LIBS
		${CMAKE_SOURCE_DIR}/../third_party/webrtc/src/third_party/http/libcurl/libs/android/x64/libcurl.a
		${CMAKE_SOURCE_DIR}/../third_party/webrtc/src/out/andorid_x64/obj/libwebrtc.a)
elseif (${ANDROID_ABI} STREQUAL "x86")
	set(LD_LIBS
		${CMAKE_SOURCE_DIR}/../third_party/webrtc/src/third_party/http/libcurl/libs/android/x86/libcurl.a
		${CMAKE_SOURCE_DIR}/../third_party/webrtc/src/out/andorid_x86/obj/libwebrtc.a)
endif ()

target_link_libraries(${PROJECT_NAME}
	PRIVATE 
	${LD_LIBS}
	log
	z)
