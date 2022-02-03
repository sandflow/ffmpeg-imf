# FFmpeg IMF

## Introduction

This repository is used to refine support for [IMF
Compositions](https://ieeexplore.ieee.org/document/9097478) in
[FFMPEG](https://ffmpeg.org).

It is organized around the following branches:

* `main` tracks the `master` branch of the [upstream FFmpeg
  repository](https://github.com/FFmpeg/FFmpeg.git)
* `develop` integrates the latest IMF features, which are not yet part of
  FFmpeg

The original FFmpeg README is moved to [`README-GENERAL.md`](./README-GENERAL.md)

## Building FFmpeg with IMF support

### Release 

`./configure --enable-libxml2`

_NOTE_: The IMF demuxer will not be included unless `libxml2` is included, which
it is not by default.

### Debug

`./configure --enable-libxml2 --enable-debug --disable-optimizations --disable-stripping`

## Usage

`./ffmpeg -f imf -assetmaps <path of ASSETMAP1>,<path of ASSETMAP2>,... -i <path of CPL> ...`

If `-assetmaps` is not specified, FFMPEG looks for a file called `ASSETMAP.xml`
in the same directory as the CPL.

_NOTE_: `-f imf` is required since the IMF demuxer is currently marked as _experimental_.

For HTJ2K decoding, the `libopenjpeg` decoder must be used:

`./ffmpeg -c:v libopenjpeg -f imf -i <path of CPL> ...`

## Unit tests

```
make fate-rsync SAMPLES=fate-suite
make fate-imf SAMPLES=fate-suite
```
