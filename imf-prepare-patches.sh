#!/bin/sh -x

set -e

BASE_BRANCH="upstream/master"
PATCH_BRANCH="rd/patches"
WORKING_BRANCH="origin/develop"

PATCHES_DIR="build/patches"

PATCHES_IMF_HEADERS="libavformat/imf_internal.h libavformat/imf.h"
PATCHES_IMF_DEC="libavformat/imfdec.c"
PATCHES_IMF_CPL="libavformat/imf_cpl.c"
PATCHES_IMF_TESTS="libavformat/tests/imf.c"
PATCHES_MXF="libavformat/mxf.h libavformat/mxfdec.c"
PATCHES_MISC="MAINTAINERS configure doc/demuxers.texi libavformat/Makefile libavformat/allformats.c"

PATCHES_ALL="$PATCHES_IMF_HEADERS $PATCHES_IMF_DEC $PATCHES_IMF_CPL $PATCHES_IMF_TESTS $PATCHES_MXF $PATCHES_MISC"

git fetch --all

git branch -f $PATCH_BRANCH $WORKING_BRANCH

git checkout $PATCH_BRANCH

git rebase $BASE_BRANCH

git reset $BASE_BRANCH

git add -- $PATCHES_ALL

git commit -m "[IMF demuxer] build system" -- $PATCHES_MISC
git notes add -m "Modify the FFMPEG build system to add support for an IMF demuxer. \
The Interoperable Master Format (IMF) is a file-based media format for the \
delivery and storage of professional audio-visual masters. \
An IMF Composition consists of an XML playlist (the Composition Playlist) \
and a collection of MXF files (the Track Files). The Composition Playlist (CPL) \
assembles the Track Files onto a timeline, which consists of multiple tracks. \
The location of the Track Files referenced by the Composition Playlist is stored \
in one or more XML documents called Asset Maps. More details at https://www.imfug.com/explainer. \
The IMF standard was first introduced in 2013 and is managed by the SMPTE."

# git commit -m "[IMF demuxer] MCA improvements to MXF decoder" -- $PATCHES_MXF
# git notes add -m "Add support for SMPTE ST 377-4 (Multichannel Audio Labeling -- MCA) \
# to the MXF decoder. MCA allows arbitrary audio channel configurations \
# in MXF files."

git commit -m "[IMF demuxer] Headers" -- $PATCHES_IMF_HEADERS
git notes add -m "Public and private header files for the IMF demuxer. The functions and constants \
defines in the public header file imf.h can be used by other modules."

git commit -m "[IMF demuxer] CPL processor" -- $PATCHES_IMF_CPL
git notes add -m "Implements IMF Composition Playlist (CPL) parsing. \
The IMF CPL is specified in SMTPE ST 2067-3 and defines a timeline onto \
which MXF files are placed."

git commit -m "[IMF demuxer] Demuxer implementation" -- $PATCHES_IMF_DEC
git notes add -m "Implements the IMF demuxer, which accepts as input an IMF CPL. \
The assets referenced by the CPL can be contained in multiple deliveries, each \
defined by an ASSETMAP file:\
\
./ffmpeg -assetmaps \<path of ASSETMAP1\>,\<path of ASSETMAP\>,... -i \<path of CPL\> \
\
If -assetmaps is not specified, FFMPEG looks for a file called ASSETMAP.xml in the same directory as the CPL."

git commit -m "[IMF demuxer] Tests" -- $PATCHES_IMF_TESTS

git notes add -m "Tests for the public API of the IMF demuxer."

mkdir -p $PATCHES_DIR

git format-patch -o $PATCHES_DIR --notes -s $BASE_BRANCH