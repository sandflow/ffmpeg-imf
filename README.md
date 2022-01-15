# FFmpeg IMF

## Introduction

This repository is used to refine support for [IMF
Compositions](https://ieeexplore.ieee.org/document/9097478) in
[FFMPEG](https://ffmpeg.org).

It is organized around the following branches:

* `main` tracks the `master` branch of the [upstream FFmpeg
  repository](https://github.com/FFmpeg/FFmpeg.git)
* `develop` integrates the latest IMF features, which are not yet part of
  FFmpeg, and includes artifacts useful for development, e.g. continuous
  integration scripts, under the `.imf` directory

The original FFmpeg README is moved to [`README-GENERAL.md`](./README-GENERAL.md)

## Building FFmpeg with IMF support

### Release 

`./configure --enable-libxml2`

_NOTE_: The IMF demuxer will not be included unless `libxml2` is included, which
it is not by default.

### Debug

`./configure --enable-libxml2 --enable-debug --disable-optimizations --disable-stripping`

## IMF demuxer usage in FFmpeg

`./ffmpeg -f imf -assetmaps <path of ASSETMAP1>,<path of ASSETMAP2>,... -i <path of CPL>`

If `-assetmaps` is not specified, FFMPEG looks for a file called `ASSETMAP.xml`
in the same directory as the CPL.

_NOTE_: `-f imf` is required since the IMF demuxer is currently marked as _experimental_.

## Development artifacts

### Unit tests

`.imf/imf-unit-tests.sh`

### Code style

`.clang-format`

_NOTE_: This does not enforce all FFmpeg code styles

### Dependencies

#### clang-format-12

```sh
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo add-apt-repository -y "deb https://apt.llvm.org/bionic/ llvm-toolchain-bionic-13 main"
sudo apt update -q
sudo apt install -y clang-format-13
sudo update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-13 101
```
