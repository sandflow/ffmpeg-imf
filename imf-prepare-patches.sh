#!/bin/sh -x

set -e

PATCH_VERSION="5"

PATCH_NAME="avformat/imf"

BASE_BRANCH="upstream/master"
PATCH_BRANCH="rd/patches"

PATCHES_DIR="build/patches"

PATCHES_SRC="libavformat/imf.h libavformat/imf_cpl.c libavformat/imfdec.c"
PATCHES_MISC="MAINTAINERS configure doc/demuxers.texi libavformat/Makefile libavformat/allformats.c"
PATCHES_MAKEFILE="libavformat/Makefile"
PATCHES_TESTS="libavformat/tests/imf.c"

PATCHES_ALL="$PATCHES_SRC $PATCHES_MAKEFILE $PATCHES_MISC $PATCHES_TESTS"

git fetch --all

git branch -f $PATCH_BRANCH

git checkout $PATCH_BRANCH

git rebase $BASE_BRANCH

git reset $BASE_BRANCH

# update copyright header
GPLCC=" * This file is part of FFmpeg.\n\
 *\n\
 * FFmpeg is free software; you can redistribute it and\/or\n\
 * modify it under the terms of the GNU Lesser General Public\n\
 * License as published by the Free Software Foundation; either\n\
 * version 2.1 of the License, or (at your option) any later version.\n\
 *\n\
 * FFmpeg is distributed in the hope that it will be useful,\n\
 * but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n\
 * Lesser General Public License for more details.\n\
 *\n\
 * You should have received a copy of the GNU Lesser General Public\n\
 * License along with FFmpeg; if not, write to the Free Software\n\
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA\n\
 *\\/\n\
\n\
\\/*"

sed -i "s/^ \* This file is part of FFmpeg\./$GPLCC/" $PATCHES_SRC

# remove tests from Makefile
sed -i "/^TESTPROGS-$(CONFIG_IMF_DEMUXER)/,+1 d" $PATCHES_MAKEFILE

git add -- $PATCHES_SRC $PATCHES_MISC $PATCHES_MAKEFILE
git commit -m "${PATCH_NAME}: Demuxer" -- $PATCHES_SRC $PATCHES_MISC $PATCHES_MAKEFILE
git notes add -m "The IMF demuxer accepts as input an IMF CPL. The assets referenced by the CPL can be
contained in multiple deliveries, each defined by an ASSETMAP file:

ffmpeg -assetmaps <path of ASSETMAP1>,<path of ASSETMAP>,... -i <path of CPL>

If -assetmaps is not specified, FFMPEG looks for a file called ASSETMAP.xml in the same directory as the CPL.

EXAMPLE:
    ffmpeg -i http://ffmpeg-imf-samples-public.s3-website-us-west-1.amazonaws.com/countdown/CPL_f5095caa-f204-4e1c-8a84-7af48c7ae16b.xml out.mp4

The Interoperable Master Format (IMF) is a file-based media format for the
delivery and storage of professional audio-visual masters.
An IMF Composition consists of an XML playlist (the Composition Playlist)
and a collection of MXF files (the Track Files). The Composition Playlist (CPL)
assembles the Track Files onto a timeline, which consists of multiple tracks.
The location of the Track Files referenced by the Composition Playlist is stored
in one or more XML documents called Asset Maps. More details at https://www.imfug.com/explainer.
The IMF standard was first introduced in 2013 and is managed by the SMPTE.

CHANGE NOTES:

- reduced line width
- use ff_ and FF prefixes for non-local functions and structures
- modified copyright header
- fixed rational initialization
- removed extraneous call to xmlCleanupParser()
- fix if/for single line braces
"

# reset the makefile
git checkout $PATCHES_MAKEFILE

git add -- $PATCHES_IMF_TESTS $PATCHES_MAKEFILE
git commit -m "${PATCH_NAME}: Tests" -- $PATCHES_IMF_TESTS $PATCHES_MAKEFILE
git notes add -m "Tests for the IMF demuxer."

mkdir -p $PATCHES_DIR

git format-patch -o $PATCHES_DIR -v $PATCH_VERSION --notes -s $BASE_BRANCH