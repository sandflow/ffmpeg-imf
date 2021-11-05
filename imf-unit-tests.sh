#!/bin/sh

make libavformat/tests/imf
valgrind --leak-check=full libavformat/tests/imf
