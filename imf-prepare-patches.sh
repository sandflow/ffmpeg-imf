#!/bin/sh -x

set -e

PATCH_VERSION="1"

PATCH_MSG="avformat/imf: fix error when the CPL root element is absent"

BASE_BRANCH="issues/handle-empty-input-url"
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
git notes add -m "Found through manual fuzzing."

git format-patch -o $PATCHES_DIR -v $PATCH_VERSION --notes -s $BASE_BRANCH