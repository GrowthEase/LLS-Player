#!/bin/bash

ROOT_DIR="$( cd "$( dirname "$0" )" && pwd )"
echo $ROOT_DIR

dir="$ROOT_DIR/out"
project_path="$ROOT_DIR/rtd.xcodeproj"
target_name="rtd"

echo "start build"

if [ ! -d "${dir}" ] ; then
    echo "${dir}目录不存在，创建目录"
    mkdir -p "$dir"
fi


echo "building iphoneos"

IOSSDK_VER=`xcrun -sdk iphoneos --show-sdk-version`

xcodebuild -configuration Release -target $target_name -sdk iphoneos${IOSSDK_VER} -project $project_path BITCODE_GENERATION_MODE=bitcode CONFIGURATION_BUILD_DIR="$ROOT_DIR/out/build-iphone"

echo "merging to framework..."

cp -rf $ROOT_DIR/out/build-iphone/$target_name.framework $ROOT_DIR/out/$target_name.framework

lipo -create `find $ROOT_DIR/out/build-*/*.framework/ -name $target_name` -output $ROOT_DIR/out/$target_name.framework/$target_name

rm -rf $ROOT_DIR/build

echo "generate product..."

# output product
cp -rf $ROOT_DIR/out/build-iphone/*.dSYM $dir

echo "end generate product..."

echo "done"
$ROOT_DIR/build_static.sh
