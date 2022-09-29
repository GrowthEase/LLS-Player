ROOT_DIR=$(cd "$(dirname "$0")"; pwd)

XCODE_DIR=`xcode-select -print-path`

function build_ios_arm64() {
	TARGET_DIR=$ROOT_DIR/out/arm64
	mkdir -p $TARGET_DIR
	cd $TARGET_DIR

	IOS_PLATFORMDIR=$XCODE_DIR/Platforms/iPhoneOS.platform
	IOS_SYSROOT=($IOS_PLATFORMDIR/Developer/SDKs/iPhoneOS*.sdk)

	cmake -G"Unix Makefiles" \
		-DCMAKE_SYSTEM_NAME=Darwin \
		-DCMAKE_TARGET_SYSTEM=ios \
		-DCMAKE_OSX_ARCHITECTURES=arm64 \
		-DCMAKE_BUILD_TYPE=release \
		-DCMAKE_OSX_SYSROOT=${IOS_SYSROOT[0]} \
		-DDISABLE_VIDEO=OFF \
		../..
	make -j
}

function build_ios_arm32() {
	TARGET_DIR=$ROOT_DIR/out/arm32
	mkdir -p $TARGET_DIR
	cd $TARGET_DIR

	IOS_PLATFORMDIR=$XCODE_DIR/Platforms/iPhoneOS.platform
	IOS_SYSROOT=($IOS_PLATFORMDIR/Developer/SDKs/iPhoneOS*.sdk)

	cmake -G"Unix Makefiles" \
		-DCMAKE_SYSTEM_NAME=Darwin \
		-DCMAKE_TARGET_SYSTEM=ios \
		-DCMAKE_OSX_ARCHITECTURES=armv7 \
		-DCMAKE_BUILD_TYPE=release \
		-DCMAKE_OSX_SYSROOT=${IOS_SYSROOT[0]} \
		-DDISABLE_VIDEO=OFF \
		../..
	make -j
}


function build_ios_x64() {
	TARGET_DIR=$ROOT_DIR/out/x64
	mkdir -p $TARGET_DIR
	cd $TARGET_DIR

	IOS_PLATFORMDIR=$XCODE_DIR/Platforms/iPhoneSimulator.platform
	IOS_SYSROOT=($IOS_PLATFORMDIR/Developer/SDKs/iPhoneSimulator*.sdk)

	cmake -G"Unix Makefiles" \
		-DCMAKE_SYSTEM_NAME=Darwin \
		-DCMAKE_TARGET_SYSTEM=ios \
		-DCMAKE_OSX_ARCHITECTURES=x86_64 \
		-DCMAKE_BUILD_TYPE=release \
		-DCMAKE_OSX_SYSROOT=${IOS_SYSROOT[0]} \
		-DDISABLE_VIDEO=OFF \
		../..
	make -j
}


function archive_library() {
	libtool  -static $ROOT_DIR/out/arm64/librtd.a  \
		$ROOT_DIR/out/arm32/librtd.a \
		-o $ROOT_DIR/out/librtd.a

	libtool -static $ROOT_DIR/out/librtd.a \
		$ROOT_DIR/out/lib/libcurl.a \
		$ROOT_DIR/out/lib/libwebrtc.a \
		-o $ROOT_DIR/out/librtd.a

	strip -x $ROOT_DIR/out/librtd.a
}


build_ios_arm64
build_ios_arm32
archive_library
