#! /bin/bash

ROOT_DIR=$(
	cd "$(dirname "$0")"
	pwd
)

cd $ROOT_DIR/third_party
rm -rf llvm-build
ln -s $ROOT_DIR/third_party/llvm-build-android llvm-build
cd $ROOT_DIR

# arches
ARCH_ARM64=false
ARCH_ARMV7=false
ARCH_X86=false
ARCH_X64=false

ENABLE_SHARED=false

#help
me=$(basename $0)
HELP_INFO="$me [arch/--allarch]\nExisting arches: armv7 arm64 x86 x64"

if [ "$*" ]; then
	for input in $@; do
		if [ $input = "-h" ] || [ $input = "--help" ]; then
			echo $HELP_INFO
			exit 0
		fi
		if [ $input = "arm64" ]; then ARCH_ARM64=true; fi
		if [ $input = "armv7" ]; then ARCH_ARMV7=true; fi
		if [ $input = "x64" ]; then ARCH_X64=true; fi
		if [ $input = "x86" ]; then ARCH_X86=true; fi
		if [ $input = "--allarch" ]; then
			ARCH_ARM64=true
			ARCH_ARMV7=true
			ARCH_X86=true
			ARCH_X64=true
		fi
		if [ $input = "--enable-shared" ]; then ENABLE_SHARED=true; fi
	done
fi

export ANDROID_NDK_HOME=$ROOT_DIR/third_party/android_ndk
export ANDROID_NDK_STRIP=$ROOT_DIR/third_party/android_ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip

function build_arm64() {
	OUT_DIR=$ROOT_DIR/out/andorid_arm64/cmake
	echo $OUT_DIR
	mkdir -p $OUT_DIR
	cd $OUT_DIR
	cmake ${ROOT_DIR}/rtd \
	-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
	-DANDROID_ARM_MODE=arm \
	-DANDROID_ABI=arm64-v8a \
	-DANDROID_PLATFORM=android-21 \
	-DANDROID_STL=c++_shared \
	-DCMAKE_BUILD_TYPE=Release
	make VERBOSE=true -j
	cp $ROOT_DIR/out/andorid_arm64/cmake/librtd.so $ROOT_DIR/out/andorid_arm64/librtd.so
	$ANDROID_NDK_STRIP -s $ROOT_DIR/out/andorid_arm64/librtd.so
	cp $ROOT_DIR/third_party/android_ndk/sources/cxx-stl/llvm-libc++/libs/arm64-v8a/libc++_shared.so $ROOT_DIR/out/andorid_arm64
}

function build_arm32() {
	OUT_DIR=$ROOT_DIR/out/andorid_arm32/cmake
	echo $OUT_DIR
	mkdir -p $OUT_DIR
	cd $OUT_DIR
	cmake ${ROOT_DIR}/rtd \
	-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
	-DANDROID_ARM_MODE=arm \
	-DANDROID_ABI=armeabi-v7a \
	-DANDROID_PLATFORM=android-21 \
	-DANDROID_STL=c++_shared \
	-DCMAKE_BUILD_TYPE=Release
	make VERBOSE=true -j
	cp $ROOT_DIR/out/andorid_arm32/cmake/librtd.so $ROOT_DIR/out/andorid_arm32/librtd.so
	$ANDROID_NDK_STRIP -s $ROOT_DIR/out/andorid_arm32/librtd.so
	cp $ROOT_DIR/third_party/android_ndk/sources/cxx-stl/llvm-libc++/libs/armeabi-v7a/libc++_shared.so $ROOT_DIR/out/andorid_arm32
}

function build_x64() {
	OUT_DIR=$ROOT_DIR/out/andorid_x64/cmake
	echo $OUT_DIR
	mkdir -p $OUT_DIR
	cd $OUT_DIR
	cmake ${ROOT_DIR}/rtd \
	-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
	-DANDROID_ABI=x86_64 \
	-DANDROID_PLATFORM=android-21 \
	-DANDROID_STL=c++_shared \
	-DCMAKE_BUILD_TYPE=Release
	make VERBOSE=true -j
	cp $ROOT_DIR/out/andorid_x64/cmake/librtd.so $ROOT_DIR/out/andorid_x64/librtd.so
	$ANDROID_NDK_STRIP -s $ROOT_DIR/out/andorid_x64/librtd.so
	cp $ROOT_DIR/third_party/android_ndk/sources/cxx-stl/llvm-libc++/libs/x86_64/libc++_shared.so $ROOT_DIR/out/andorid_x64
}

function build_x86() {
	OUT_DIR=$ROOT_DIR/out/andorid_x86/cmake
	echo $OUT_DIR
	mkdir -p $OUT_DIR
	cd $OUT_DIR
	cmake ${ROOT_DIR}/rtd \
	-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
	-DANDROID_ABI=x86 \
	-DANDROID_PLATFORM=android-21 \
	-DANDROID_STL=c++_shared \
	-DCMAKE_BUILD_TYPE=Release
	make VERBOSE=true -j
	cp $ROOT_DIR/out/andorid_x86/cmake/librtd.so $ROOT_DIR/out/andorid_x86/librtd.so
	$ANDROID_NDK_STRIP -s $ROOT_DIR/out/andorid_x86/librtd.so
	cp $ROOT_DIR/third_party/android_ndk/sources/cxx-stl/llvm-libc++/libs/x86/libc++_shared.so $ROOT_DIR/out/andorid_x86
}

set -e
gnargs='target_os="android" rtc_use_h264=true rtc_include_internal_audio_device=false treat_warnings_as_errors=false use_custom_libcxx=false'

if [ $ARCH_ARM64 = true ]; then
	cd $ROOT_DIR
	ARM64_OUT_DIR="out/andorid_arm64"

	arm64gnargs=$gnargs' target_cpu="arm64"'
	arm64gncmd="gn gen $ARM64_OUT_DIR --args='$arm64gnargs'"
	echo "build android arm64 library"
	echo ""
	echo $arm64gncmd
	echo ""
	eval $arm64gncmd
	ninja -C $ARM64_OUT_DIR webrtc
	if [ $ENABLE_SHARED = true ]; then
		build_arm64
	fi
fi

if [ $ARCH_ARMV7 = true ]; then
	cd $ROOT_DIR
	ARM32_OUT_DIR="out/andorid_arm32"

	arm32gnargs=$gnargs' target_cpu="arm"'
	arm32gncmd="gn gen $ARM32_OUT_DIR --args='$arm32gnargs'"
	echo "build android amr32 library"
	echo ""
	echo $arm32gncmd
	echo ""
	eval $arm32gncmd
	ninja -C $ARM32_OUT_DIR webrtc
	if [ $ENABLE_SHARED = true ]; then
		build_arm32
	fi
fi

if [ $ARCH_X64 = true ]; then
	cd $ROOT_DIR
	X64_OUT_DIR="out/andorid_x64"

	x64gnargs=$gnargs' target_cpu="x64"'
	x64gncmd="gn gen $X64_OUT_DIR --args='$x64gnargs'"
	echo "build android x64 library"
	echo ""
	echo $x64gncmd
	echo ""
	eval $x64gncmd
	ninja -C $X64_OUT_DIR webrtc
	if [ $ENABLE_SHARED = true ]; then
		build_x64
	fi
fi

if [ $ARCH_X86 = true ]; then
	cd $ROOT_DIR
	X86_OUT_DIR="out/andorid_x86"

	x86gnargs=$gnargs' target_cpu="x86"'
	x86gncmd="gn gen $X86_OUT_DIR --args='$x86gnargs'"
	echo "build android x86 library"
	echo ""
	echo $x86gncmd
	echo ""
	eval $x86gncmd
	ninja -C $X86_OUT_DIR webrtc
	if [ $ENABLE_SHARED = true ]; then
		build_x86
	fi
fi

cd ..
set +e
echo "Done"

