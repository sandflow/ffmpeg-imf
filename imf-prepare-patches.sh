#!/bin/sh -x

set -e

PATCH_VERSION="1"

PATCH_MSG="avformat/imf: additional FATE test"

BASE_BRANCH="upstream/master"
PATCH_BRANCH="rd/patches"

PATCHES_DIR="build/patches"

mkdir -p $PATCHES_DIR

PATCHES_ALL="tests/Makefile tests/ref/fate/imf-cpl-with-repeat tests/fate/imf.mak"

git fetch --all

git branch -f $PATCH_BRANCH

git checkout $PATCH_BRANCH

git merge $BASE_BRANCH

git reset $BASE_BRANCH

git add -- $PATCHES_ALL
git commit -m "$PATCH_MSG" -- $PATCHES_ALL
git notes add -m "The following files need to be added to FATE samples:
    http://ffmpeg-imf-samples-public.s3-website-us-west-1.amazonaws.com/countdown/ASSETMAP.xml
    http://ffmpeg-imf-samples-public.s3-website-us-west-1.amazonaws.com/countdown/CPL_f5095caa-f204-4e1c-8a84-7af48c7ae16b.xml
    http://ffmpeg-imf-samples-public.s3-website-us-west-1.amazonaws.com/countdown/PKL_4671220f-c87a-4660-bf2a-6ef848791a2c.xml
    http://ffmpeg-imf-samples-public.s3-website-us-west-1.amazonaws.com/countdown/VOLINDEX.xml
"

git format-patch -o $PATCHES_DIR -v $PATCH_VERSION --notes -s $BASE_BRANCH