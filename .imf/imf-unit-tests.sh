#!/bin/sh

make libavformat/tests/imf

valgrind --leak-check=full ./ffmpeg -y -i http://ffmpeg-imf-samples-public.s3.us-west-1.amazonaws.com/callout_51_l_r_c_lfe_ls_rs.mxf -f wav /dev/null

valgrind --leak-check=full ./ffmpeg -y \
  -assetmaps http://ffmpeg-imf-samples-public.s3.us-west-1.amazonaws.com/countdown-qc/ASSETMAP.xml,http://ffmpeg-imf-samples-public.s3.us-west-1.amazonaws.com/countdown/ASSETMAP.xml \
  -i http://ffmpeg-imf-samples-public.s3-website-us-west-1.amazonaws.com/countdown/CPL_f5095caa-f204-4e1c-8a84-7af48c7ae16b.xml \
  -f mp4 /dev/null

valgrind --leak-check=full libavformat/tests/imf