# OpenAL Linux with Loki extension

Last old OpenAL that contains Loki extensions from Creative SVN at [5 May 2006](https://github.com/rpavlik/openal-svn-mirror/tree/5841ac2daed8f7c2e3e29a089e8eec4ac46fc487) before they restructure their repository that deleted some stuff. This repository contains OpenAL Linux that intended to programs that relies `*_LOKI()` extensions such as many Loki Games ported titles. For much older games, this library used by [OpenAL Compatibility Shim](https://github.com/thiekus/openal-compat) to fixes ABI breakage introduced around year 2000. This repo is slightly newer than [v0.0.8 tarball release](https://web.archive.org/web/20060502061805if_/http://www.openal.org:80/openal_webstf/downloads/openal-0.0.8.tar.gz) that Creatve provide on OpenAL website at some points.

While we have more updated Creative OpenAL forks such as [OpenAL-Soft](https://github.com/kcat/openal-soft) for newer applications, unfortunatelly it didn't implements Loki extensions since it was deleted around 2006 from Creative SVN. Games that relies on that extensions will be silent or just crashed on startup. I find `libopenal-0.0.so` old source code everywhere and still don't get it, even that it just have OSS backend. This old code however, has working ALSA and SDL v1 backends works, even it didn't have more modern PulseAudio or PipeWire backend yet. Alongside with OpenAL Compatibility Shim, this library intended to fix Loki Games titles that relies to ancient `libopenal-0.0.so` which only support OSS backend that removed and didn't works on modern Linux distros unless you install OSS proxy daemon.

Also, this repo have some minor fixes such as correct SDL library name dynamic load and options to override SDL and ALSA library to load.

You can read original readme [here](README.original).

## Building

Install some depedencies such as SMPEG, Vorbis (optional) and SDL 1.2. For newer Ubuntu/Debian that use SDL 1.2 compat instead, install this:
```
sudo apt install gcc-multilib libsmpeg0:i386 libvorbis0a:i386 libvorbisfile3:i386 libogg0:i386 libsdl1.2-compat-shim:i386 libsmpeg-dev libvorbis-dev libogg-dev libsdl1.2-dev libasound2-dev
```

For older Ubuntu/Debian that didn't ship with SDL 1.2 compat, install this:
```
sudo apt install gcc-multilib libsmpeg0:i386 libvorbis0a:i386 libvorbisfile3:i386 libogg0:i386 libsdl1.2debian:i386 libsmpeg-dev libvorbis-dev libogg-dev libsdl1.2-dev libasound2-dev
```

To build, clone this repo, then create build directory.
```
git clone https://github.com/thiekus/openal-loki
cd openal-loki
mkdir build
cd build
```

Then configure as x86 32-bit binary, prefix at `(current directory)/openal-pfx` to avoid overwrite your current system OpenAL library.
```
../configure --prefix=$PWD/openal-pfx \
    --build=i686-pc-linux-gnu \
    CFLAGS=-m32 \
    CXXFLAGS=-m32 \
    LDFLAGS=-m32 \
    --disable-static \
    --enable-shared \
    --enable-alsa \
    --enable-sdl \
    --enable-vorbis \
    --enable-mp3 \
    --enable-alsa-dlopen \
    --enable-sdl-dlopen \
    --enable-vorbis-dlopen \
    --enable-mp3-dlopen
```

After that, building as usual.
```
make
make install
```

Libraries will be out at `(current directory)/openal-pfx/lib`

Refer to [OpenAL Compatibility Shim](https://github.com/thiekus/openal-compat) page to use alongside with old Loki OpenAL games.

## Usage

By default, this library tries to use OSS backend which obviously didn't work in modern Linux. To enforce OpenAL to use SDL or ALSA, change `~/.openalrc` to this:
```
(define devices '(sdl alsa null))
```

SDL is recomended since it will handled by much preferable audio backend like PulseAudio or PipeWire which mixes with audio from another application. Use ALSA if you experience with delayed or crickled sound in some cases (with old soundcard).

You can also override library that used by backend.

* `OAL_SDL_LIBRARY` to override SDL 1.x library, defaults to `libSDL-1.2.so.0`.
* `OAL_ALSA_LIBRARY` to override ALSA library, defaults to `libasound.so.2`.
