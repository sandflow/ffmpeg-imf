#!/bin/sh -x

set -e

PATCH_VERSION="1"
PATCH_MSG="avformat/imf: Fix error handling in set_context_streams_from_tracks()"
BASE_BRANCH="upstream/master"
PATCH_BRANCH="rd/patches"
PATCHES_DIR="build/patches"
PATCHES_ALL="libavformat/tests/.gitignore"

mkdir -p $PATCHES_DIR

git fetch --all

git branch -f $PATCH_BRANCH

git checkout $PATCH_BRANCH

git merge $BASE_BRANCH

git reset $BASE_BRANCH

git add -- $PATCHES_ALL

git commit -m "$PATCH_MSG" -- $PATCHES_ALL

git format-patch -o $PATCHES_DIR -v $PATCH_VERSION --notes -s $BASE_BRANCH