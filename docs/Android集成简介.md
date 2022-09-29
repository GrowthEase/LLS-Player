## 搭建编译环境

### 安装 Ubuntu 20.04。

根据WebRTC官方推荐，操作系统需要安装Ubuntu 20.04。

登陆并安装依赖的软件：

```
# apt-get update
# apt-get install build-essential pkg-config git python cmake
```

### 安装JDK

 下载jdk 1.8.0：

        https://repo.huaweicloud.com/java/jdk/8u202-b08/

解压后，配置JAVA_HOME PATH变量

```
export JAVA_HOME=/path/to/jdk/jdk1.8.0_202
export CLASSPATH=.:$JAVA_HOME/lib/tools.jar:$JAVA_HOME/lib/dt.jar
export PATH=$PATH:$JAVA_HOME/bin
```

### 安装deptools

谷歌官方下载地址： https://storage.googleapis.com/chrome-infra/depot_tools.zip

**depot_tools** 解压后，进入目录，打开cmd，执行如下命令来安装一些基础库

```
gclient
```

安装完成后将路径添加到环境变量path中

## 下载源码

本项目基于WebRTC M94版本进行开发，需要先下载WebRTC原生代码，然后下载云信的低延时直播代码LLS-Player，最后将LLS-Player代码覆盖到WebRTC原生代码中。

### 下载WebRTC M94源码

WebRTC对应的代码分支和commitId如下，跟进下面的步骤操作即可下载对应的源码。

```shell
mkdir webrtc
cd webrtc
fetch --nohooks webrtc_android                     // 拉取WebRTC代码
cd src
git checkout -b m94 branch-heads/4606              // 此处基于4606创建m94分支。
git reset --hard  b83487f08ff836437715b488f73416215e5570dd      // 重置到我们使用的版本。
gclient sync 
```

### 下载LLS-Player源码

```shell
git clone https://github.com/GrowthEase/LLS-Player.git
```

**代码下载后，将LLS-Player/src目录下所有文件覆盖到上面下载的WebRTC M94版本中**

## 代码编译

Linux打开Shell，切换到src目录，执行如下命令：

```
./build_android.sh arm64 --enable-shared
```

编译完成后，在src/out/andorid_arm64目录下会生成动态库librtd.so和对应的libc++\_shared.so.

## SDK集成

将SDK中的头文件和库文件和rtd_dec.c文件集成到播放器工程中，以**ijkplayer**为例

### 导入动态库和头文件

将动态库拷贝到指定的库目录下，（具体目录根据实际情况指定），如下图所示：

（1）将librtd.so放入对应架构的路径中:

ijkplayer/android/contrib/build/ffmpeg-$arch/output

（2）头文件nertd_api.h和rtd_def.h放入对应架构的路径中 ：

ijkplayer/android/contrib/build/ffmpeg-$arch/output/include

### 编译引入nertd动态库

修改 android/ijkplayer/ijkplayer-$arch/src/main/jni/ffmpeg/[Android.mk](http://android.mk/)文件，如下图所示：

![](C:./ijkplayer-makefile-modify.png)

### 添加FFMPEG插件代码和动态库依赖

将rtd_dec.c导入ijkplayer工程(具体位置可以自己指定)，

将rtd_dec.c文件拷贝到工程的源码目录下参与编译，（具体位置可以自己指定），例如ijkplayer中放到ijkavformat目录下。修改ijkplayer/ijkmedia/ijkplayer/[Android.mk](

![](C:./ijkplayer-aos-makefile-modify.png)

### SDK接入修改

以ijkplayer为例，在ff_ffplay.c中添加低延时拉流的逻辑

**（1）包含头文件和必要的变量声明**

```c++
#include "rtd_api.h" // 设置好头文件路径即可

extern AVInputFormat ff_rtd_demuxer;
```

**（2）注册低延时拉流协议**

 在read_thread()函数里，从url中区分出低延时拉流协议头，例如（"nertc"），设置AVInputFormat为ff_rtd_demuxer。

**以上都需要在avformat_open_input()之前设置，avformat_open_input()最后一个参数需要设置options**

```c
if (strncmp(is->filename, "nertc://", 8) == 0) { // 协议头根据实际情况进行设置，这里以"nertc"为例
    // 设置AVInputFormat 为ff_rtd_demuxer
	is->iformat = &ff_rtd_demuxer;
}

// 上述代码在avformat_open_input前
err = avformat_open_input(&ic, is->filename, is->iformat, &ffp->format_opts);
```

### 重新编译播放器

上述设置完成后需要重新编译播放器工程，例如ijkplayer，在android目录下

```shell
./compile-ijk.sh all
```

### 拉流播放

输入对应的拉流URL进行播放，等待avformat_open_input()函数返回，返回成功则表示建连成功。

后续按照播放器的流程进行音视频帧的读取，解码，渲染即可正常播放。

