#!/bin/sh

clang-format -n --style=file \
  libavformat/imf.h \
  libavformat/imf_cpl.c \
  libavformat/imfdec.c \
  libavformat/tests/imf.c
