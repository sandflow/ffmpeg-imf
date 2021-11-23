#!/bin/sh -x

set -e

PATCH_VERSION="5"

PATCH_NAME="avformat/imf"

BASE_BRANCH="upstream/master"
PATCH_BRANCH="rd/patches"

PATCHES_DIR="build/patches"

PATCHES_IMF_HEADERS="libavformat/imf.h"
PATCHES_IMF_DEC="libavformat/imfdec.c"
PATCHES_IMF_CPL="libavformat/imf_cpl.c"
PATCHES_IMF_TESTS="libavformat/tests/imf.c"
PATCHES_MXF="libavformat/mxf.h libavformat/mxfdec.c"
PATCHES_MISC="MAINTAINERS configure doc/demuxers.texi libavformat/Makefile libavformat/allformats.c"

PATCHES_ALL="$PATCHES_IMF_HEADERS $PATCHES_IMF_DEC $PATCHES_IMF_CPL $PATCHES_IMF_TESTS $PATCHES_MXF $PATCHES_MISC"

git fetch --all

git branch -f $PATCH_BRANCH

git checkout $PATCH_BRANCH

git rebase $BASE_BRANCH

git reset $BASE_BRANCH

AUGMENTED=" * POSSIBILITY OF SUCH DAMAGE.\n\
 *\n\
 * This file is part of FFmpeg.\n\
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA"

sed -i "/^ \* This file is part of FFmpeg\./,+1 d" $PATCHES_IMF_HEADERS $PATCHES_IMF_DEC $PATCHES_IMF_CPL $PATCHES_IMF_TESTS 
sed -i "s/^ \* POSSIBILITY OF SUCH DAMAGE\./$AUGMENTED/" $PATCHES_IMF_HEADERS $PATCHES_IMF_DEC $PATCHES_IMF_CPL $PATCHES_IMF_TESTS

git add -- $PATCHES_ALL

git commit -m "${PATCH_NAME}: Headers" -- $PATCHES_IMF_HEADERS
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

Header and build files.

CHANGE NOTES:

- fixed patchwork warnings
- updated patch notes
- added LGPL license
- removed imf_internal.h
- Improve error handling, including removing exit()
- Fix code style
- Allow custom I/O for all files (following DASH and HLS template)
- replace realloc with av_realloc_f to fix leaks"

# git commit -m "[IMF demuxer] MCA improvements to MXF decoder" -- $PATCHES_MXF
# git notes add -m "Add support for SMPTE ST 377-4 (Multichannel Audio Labeling -- MCA) \
# to the MXF decoder. MCA allows arbitrary audio channel configurations \
# in MXF files."

git commit -m "${PATCH_NAME}: CPL processor" -- $PATCHES_IMF_CPL
git notes add -m "Implements IMF Composition Playlist (CPL) parsing."

git commit -m "${PATCH_NAME}: Demuxer implementation" -- $PATCHES_IMF_DEC
git notes add -m "Implements the IMF demuxer."

git commit -m "${PATCH_NAME}: Tests and build files" -- $PATCHES_IMF_TESTS $PATCHES_MISC 
git notes add -m "Tests and build files for the IMF demuxer."

mkdir -p $PATCHES_DIR

git format-patch -o $PATCHES_DIR -v $PATCH_VERSION --notes -s $BASE_BRANCH