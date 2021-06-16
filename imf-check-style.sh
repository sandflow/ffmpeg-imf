#!/bin/sh

clang-format -n --style=file \
  libavformat/imf.h \
  libavformat/imf_cpl.c \
  libavformat/imf_internal.h \
  libavformat/imfdec.c \
  libavformat/tests/imf.c
