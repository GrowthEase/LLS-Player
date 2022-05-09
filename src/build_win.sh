#! /bin/sh

ROOT_DIR=$(cd "$(dirname "$0")"; pwd)

if [ ! -f $ROOT_DIR/third_party/llvm-build/Release+Asserts/bin/clang-cl.exe ]; then
    cd $ROOT_DIR/third_party
    rm -rf llvm-build
    ln -s $ROOT_DIR/third_party/llvm-build-win llvm-build
    cd $ROOT_DIR
fi

ARCH_X86=false
ARCH_X64=false
WITH_IDE=false
IS_DEBUG=false
ENABLE_SHARED=false

#help
me=$(basename $0)
HELP_INFO="$me [x64/x86/--allarch] [--ide] [--debug] "

if [ "$*" ]; then
    for input in $@
    do
        if [[ $input = "-h" || $input = "--help" ]]; then echo $HELP_INFO; exit 0;fi
        if [ $input = "x86" ]; then ARCH_X86=true; fi
        if [ $input = "x64" ]; then ARCH_X64=true; fi
        if [ $input = "--allarch" ]; then
            ARCH_X86=true
            ARCH_X64=true
        fi
        if [ $input = "--debug" ]; then IS_DEBUG=true; fi
        if [ $input = "--ide" ]; then WITH_IDE=true; fi
        if [ $input = "--enable-shared" ]; then ENABLE_SHARED=true; fi
    done
fi


set -e

gnargs=''

if [ $IS_DEBUG == true ]; then
	gnargs=$gnargs" is_debug=true"
else
    gnargs=$gnargs" is_debug=false"
fi

withIde=""
if [ $WITH_IDE == true ];then
    withIde="--ide=vs2019"
fi

if [ $ARCH_X64 = true ]; then
# out directories
	if [ $IS_DEBUG == true ]; then
		WIN64_OUT_DIR="out/win_64/Debug"
	else
		WIN64_OUT_DIR="out/win_64/Release"
	fi

    x64gnargs=$gnargs' target_cpu="x64" rtc_use_h264=true rtc_include_internal_audio_device=false'
    x64gncmd="gn gen $WIN64_OUT_DIR --args="\'$x64gnargs\'" $withIde"
    echo "build windows x64 library"
    echo ""
    echo $x64gncmd
    echo ""
    eval $x64gncmd
    if [ $ENABLE_SHARED == true ]; then
      ninja -C $WIN64_OUT_DIR rtd
    else
      ninja -C $WIN64_OUT_DIR
    fi
fi

if [ $ARCH_X86 = true ]; then
	if [ $IS_DEBUG == true ]; then
		WIN32_OUT_DIR="out/win_32/Debug"
	else
		WIN32_OUT_DIR="out/win_32/Release"
	fi

    x86gnargs=$gnargs' target_cpu="x86" rtc_use_h264=true rtc_include_internal_audio_device=false'
    x86gncmd="gn gen $WIN32_OUT_DIR --args="\'$x86gnargs\'" $withIde"
    echo "build windows x86 library"
    echo ""
    echo $x86gncmd
    echo ""
    eval $x86gncmd
    if [ $ENABLE_SHARED == true ]; then
      ninja -C $WIN32_OUT_DIR rtd
    else
      ninja -C $WIN32_OUT_DIR
    fi
fi

cd ..
set +e
echo "Done"
