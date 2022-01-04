#!/bin/sh -x

set -e

PATCH_VERSION="1"

PATCH_MSG="avformat/imf: fix CPL parsing error handling"

BASE_BRANCH="issues/empty-root-element-segfault"
PATCH_BRANCH="rd/patches"

PATCHES_DIR="build/patches"

mkdir -p $PATCHES_DIR

PATCHES_ALL="libavformat/imf_cpl.c"

git fetch --all

git branch -f $PATCH_BRANCH

git checkout $PATCH_BRANCH

git merge $BASE_BRANCH

git reset $BASE_BRANCH

git add -- $PATCHES_ALL
git commit -m "$PATCH_MSG" -- $PATCHES_ALL

git format-patch -o $PATCHES_DIR -v $PATCH_VERSION --notes -s $BASE_BRANCH