# FFMPEG IMF

# IMF for FFMPEG

## Introduction

This project adds support for [IMF Compositions](https://ieeexplore.ieee.org/document/9097478) as an input format to [FFMPEG](https://ffmpeg.org).

## Usage

`./ffmpeg -assetmaps <path of ASSETMAP1>,<path of ASSETMAP2>,... -i <path of CPL>`

If `-assetmaps` is not specified, FFMPEG looks for a file called `ASSETMAP.xml` in the same directory as the CPL.

## Build

### Release 

`./configure --enable-libxml2`

### Debug

`./configure --enable-libxml2 --enable-debug --disable-optimizations --disable-stripping`

## Unit tests

`./imf-unit-tests.sh`

## Code style

`./imf-check-style.sh`

## Dependencies

### clang-format-12

```sh
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo add-apt-repository -y "deb https://apt.llvm.org/bionic/ llvm-toolchain-bionic-12 main"
sudo apt update -q
sudo apt install -y clang-format-12
sudo update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-12 100
```
