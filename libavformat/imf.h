/*
 * Copyright (c) Sandflow Consulting LLC
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * @ingroup lavu_imf
 * Public header for processing of Interoperable Master Format packages
 */

#ifndef AVCODEC_IMF_H
#define AVCODEC_IMF_H

#include "avformat.h"
#include "libavformat/avio.h"
#include "libavutil/rational.h"
#include <libxml/tree.h>

typedef uint8_t UUID[16];

/**
 * IMF Composition Playlist Base Resource
 */
typedef struct IMFBaseResource {
    AVRational edit_rate;
    unsigned long entry_point;
    unsigned long duration;
    unsigned long repeat_count;
} IMFBaseResource;

/**
 * IMF Composition Playlist Track File Resource
 */
typedef struct IMFTrackFileResource {
    IMFBaseResource base;
    unsigned char track_file_uuid[16];
} IMFTrackFileResource;

/**
 * IMF Marker
 */
typedef struct IMFMarker {
    xmlChar *label_utf8;
    xmlChar *scope_utf8;
    unsigned long offset;
} IMFMarker;

/**
 * IMF Composition Playlist Marker Resource
 */
typedef struct IMFMarkerResource {
    IMFBaseResource base;
    unsigned long marker_count;
    IMFMarker *markers;
} IMFMarkerResource;

/**
 * IMF Composition Playlist Virtual Track
 */
typedef struct IMFBaseVirtualTrack {
    unsigned char id_uuid[16];
} IMFBaseVirtualTrack;

/**
 * IMF Composition Playlist Virtual Track that consists of Track File Resources
 */
typedef struct IMFTrackFileVirtualTrack {
    IMFBaseVirtualTrack base;
    unsigned long resource_count;
    IMFTrackFileResource *resources;
} IMFTrackFileVirtualTrack;

/**
 * IMF Composition Playlist Virtual Track that consists of Marker Resources
 */
typedef struct IMFMarkerVirtualTrack {
    IMFBaseVirtualTrack base;
    unsigned long resource_count;
    IMFMarkerResource *resources;
} IMFMarkerVirtualTrack;

/**
 * IMF Composition Playlist
 */
typedef struct IMFCPL {
    uint8_t id_uuid[16];
    xmlChar *content_title_utf8;
    AVRational edit_rate;
    IMFMarkerVirtualTrack *main_markers_track;
    IMFTrackFileVirtualTrack *main_image_2d_track;
    unsigned long main_audio_track_count;
    IMFTrackFileVirtualTrack *main_audio_tracks;
} IMFCPL;

/**
 * IMF Asset locator
 */
typedef struct IMFAssetLocator {
    UUID uuid;
    const char *path;
} IMFAssetLocator;

/**
 * IMF Asset Map
 */
typedef struct IMFAssetMap {
    uint8_t asset_count;
    IMFAssetLocator *assets[];
} IMFAssetMap;

int parse_imf_cpl_from_xml_dom(xmlDocPtr doc, IMFCPL **cpl);

int parse_imf_cpl(AVIOContext *in, IMFCPL **cpl);

IMFCPL *imf_cpl_alloc(void);

void imf_cpl_free(IMFCPL *cpl);

int parse_imf_asset_map_from_xml_dom(AVFormatContext *s, xmlDocPtr doc, IMFAssetMap **asset_map, const char *base_url);

IMFAssetMap *imf_asset_map_alloc(void);

void imf_asset_map_free(IMFAssetMap *asset_map);

#endif
