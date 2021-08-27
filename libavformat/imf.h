/*
 * This file is part of FFmpeg.
 *
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
 * Public header file for the processing of Interoperable Master Format (IMF) packages.
 * 
 * @author Pierre-Anthony Lemieux
 * @file
 * @ingroup lavu_imf
 */

#ifndef AVFORMAT_IMF_H
#define AVFORMAT_IMF_H

#include "avformat.h"
#include "libavformat/avio.h"
#include "libavutil/rational.h"
#include <libxml/tree.h>

/**
 * UUID as defined in IETF RFC 422
 */
typedef uint8_t UUID[16];

/**
 * IMF Composition Playlist Base Resource
 */
typedef struct IMFBaseResource {
    AVRational edit_rate; /**< BaseResourceType/EditRate */
    unsigned long entry_point; /**< BaseResourceType/EntryPoint */
    unsigned long duration; /**< BaseResourceType/Duration */
    unsigned long repeat_count; /**< BaseResourceType/RepeatCount */
} IMFBaseResource;

/**
 * IMF Composition Playlist Track File Resource
 */
typedef struct IMFTrackFileResource {
    IMFBaseResource base;
    UUID track_file_uuid; /**< TrackFileResourceType/TrackFileId */
} IMFTrackFileResource;

/**
 * IMF Marker
 */
typedef struct IMFMarker {
    xmlChar *label_utf8; /**< Marker/Label */
    xmlChar *scope_utf8; /**< Marker/Label/\@scope */
    unsigned long offset; /**< Marker/Offset */
} IMFMarker;

/**
 * IMF Composition Playlist Marker Resource
 */
typedef struct IMFMarkerResource {
    IMFBaseResource base;
    unsigned long marker_count; /**< Number of Marker elements */
    IMFMarker *markers; /**< Marker elements */
} IMFMarkerResource;

/**
 * IMF Composition Playlist Virtual Track
 */
typedef struct IMFBaseVirtualTrack {
    UUID id_uuid; /**< TrackId associated with the Virtual Track */
} IMFBaseVirtualTrack;

/**
 * IMF Composition Playlist Virtual Track that consists of Track File Resources
 */
typedef struct IMFTrackFileVirtualTrack {
    IMFBaseVirtualTrack base;
    unsigned long resource_count; /**< Number of Resource elements present in the Virtual Track */
    IMFTrackFileResource *resources; /**< Resource elements of the Virtual Track */
} IMFTrackFileVirtualTrack;

/**
 * IMF Composition Playlist Virtual Track that consists of Marker Resources
 */
typedef struct IMFMarkerVirtualTrack {
    IMFBaseVirtualTrack base;
    unsigned long resource_count; /**< Number of Resource elements present in the Virtual Track */
    IMFMarkerResource *resources; /**< Resource elements of the Virtual Track */
} IMFMarkerVirtualTrack;

/**
 * IMF Composition Playlist
 */
typedef struct IMFCPL {
    UUID id_uuid; /**< CompositionPlaylist/Id element */
    xmlChar *content_title_utf8; /**< CompositionPlaylist/ContentTitle element */
    AVRational edit_rate; /**< CompositionPlaylist/EditRate element */
    IMFMarkerVirtualTrack *main_markers_track; /**< Main Marker Virtual Track */
    IMFTrackFileVirtualTrack *main_image_2d_track; /**< Main Image Virtual Track */
    unsigned long main_audio_track_count; /**< Number of Main Audio Virtual Tracks */
    IMFTrackFileVirtualTrack *main_audio_tracks; /**< Main Audio Virtual Tracks */
} IMFCPL;

/**
 * Parse an IMF CompositionPlaylist element into the IMFCPL data structure.
 * @param[in] doc An XML document from which the CPL is read.
 * @param[out] cpl Pointer to a memory area (allocated by the client), where the function writes a pointer to the newly constructed
 * IMFCPL structure (or NULL if the CPL could not be parsed). The client is responsible for freeing the IMFCPL structure using
 * imf_cpl_free().
 * @return A non-zero value in case of an error.
 */
int parse_imf_cpl_from_xml_dom(xmlDocPtr doc, IMFCPL **cpl);

/**
 * Parse an IMF Composition Playlist document into the IMFCPL data structure.
 * @param[in] in The context from which the CPL is read.
 * @param[out] cpl Pointer to a memory area (allocated by the client), where the function writes a pointer to the newly constructed
 * IMFCPL structure (or NULL if the CPL could not be parsed). The client is responsible for freeing the IMFCPL structure using
 * imf_cpl_free().
 * @return A non-zero value in case of an error.
 */
int parse_imf_cpl(AVIOContext *in, IMFCPL **cpl);

/**
 * Allocates and initializes an IMFCPL data structure.
 * @return A pointer to the newly constructed IMFCPL structure (or NULL if the structure could not be constructed). The client is
 * responsible for freeing the IMFCPL structure using imf_cpl_free().
 */
IMFCPL *imf_cpl_alloc(void);

/**
 * Deletes an IMFCPL data structure previously instantiated with imf_cpl_alloc().
 * @param[in] cpl The IMFCPL structure to delete.
 */
void imf_cpl_free(IMFCPL *cpl);

#endif
