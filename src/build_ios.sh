#! /bin/sh

#create src/third-party/llvm-build for compile
ROOT_DIR=$(cd "$(dirname "$0")"; pwd)

cd $ROOT_DIR/third_party
rm -rf llvm-build
#ln -s $ROOT_DIR/src/third_party/llvm-build-mac llvm-build
ln -s $ROOT_DIR/third_party/llvm-build-ios llvm-build
cd $ROOT_DIR

# arches
ARCH_ARM64=false
ARCH_ARMV7=false
IS_DEBUG=false

#help
me=$(basename $0)
HELP_INFO="$me [arch/--allarch]\nExisting arches:  arm64 x64"

if [ "$*" ]; then
    for input in $@
    do
        if [[ $input = "-h"  || $input = "--help" ]]; then echo $HELP_INFO; exit 0; fi
        if [ $input = "arm64" ]; then ARCH_ARM64=true; fi
        if [ $input = "armv7" ]; then ARCH_ARMV7=true; fi
        if [ $input = "--allarch" ]; then
            ARCH_ARM64=true
            ARCH_ARMV7=true
        fi
        if [ $input = "--debug" ]; then IS_DEBUG=true; fi
    done
fi


#ffmpeg_branding="Chrome"     #rtc_use_h264 is disabled, so no need for ffmpeg_branding
gnargs='target_os="ios" ios_enable_code_signing=false  enable_ios_bitcode=false ios_deployment_target="9.0" use_xcode_clang=true '
gnargs+='rtc_include_internal_audio_device=false rtc_build_examples=false rtc_include_tests=false '
gnargs+='rtc_enable_protobuf=false is_component_build=false rtc_build_tools=false '
gnargs+='rtc_enable_sctp=false rtc_enable_avx2=false rtc_include_ilbc=false '
gnargs+='use_rtti=true  '


echo $gnargs

if [ $IS_DEBUG == true ]; then
    gnargs=$gnargs" is_debug=true"
else
    gnargs=$gnargs" is_debug=false"
fi
   
set -e
#cd $ROOT_DIR/src

if [ $ARCH_ARM64 = true ]; then
	IOS64_OUT_DIR="out/ios_arm64"
	arm64gnargs='target_cpu="arm64" '$gnargs
	arm64gncmd="gn gen $IOS64_OUT_DIR --args="\'$arm64gnargs\'" --ide=xcode"
    echo "build ios arm64 library"
    echo ""
    echo $arm64gncmd
    echo ""
    eval $arm64gncmd
	ninja -C $IOS64_OUT_DIR webrtc
fi

if [ $ARCH_ARMV7 = true ]; then
	IOS32_OUT_DIR="out/ios_armv7"
	armv7gnargs='target_cpu="arm" '$gnargs
	armv7gncmd="gn gen $IOS32_OUT_DIR --args="\'$armv7gnargs\'" --ide=xcode"
    echo "build ios armv7 library"
    echo ""
    echo $armv7gncmd
    echo ""
    eval $armv7gncmd
	ninja -C $IOS32_OUT_DIR webrtc
fi


LIBS=""
if [ -f $ROOT_DIR/out/ios_arm64/obj/libwebrtc.a ]; then
	LIBS=$LIBS" "$ROOT_DIR/out/ios_arm64/obj/libwebrtc.a
fi

if [ -f $ROOT_DIR/out/ios_armv7/obj/libwebrtc.a ]; then
	LIBS=$LIBS" "$ROOT_DIR/out/ios_armv7/obj/libwebrtc.a
fi

if [[ $LIBS != "" ]]; then
	libtool -static $LIBS -o $ROOT_DIR/out/libwebrtc.a
fi

if [ ! -d  $ROOT_DIR/rtd/project/ios/out ];then
    rm -rf $ROOT_DIR/rtd/project/ios/out
    mkdir $ROOT_DIR/rtd/project/ios/out
fi

if [ ! -d  $ROOT_DIR/rtd/project/ios/out/lib ];then
    rm -rf $ROOT_DIR/rtd/project/ios/out/lib
    mkdir $ROOT_DIR/rtd/project/ios/out/lib
fi
cp $ROOT_DIR/out/libwebrtc.a  $ROOT_DIR/rtd/project/ios/out/lib
cp $ROOT_DIR/third_party/http/libcurl/libs/ios/libcurl.a   $ROOT_DIR/rtd/project/ios/out/lib
cd $ROOT_DIR/rtd/project/ios
./build.sh

cd $ROOT_DIR
set +e

echo "Done"
