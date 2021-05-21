
/*
 * Copyright (c), Sandflow Consulting LLC
 * All rights reserved.
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
#ifndef AVCODEC_IMF_H
#define AVCODEC_IMF_H

#include "libavutil/rational.h"
#include "libavcodec/codec_par.h"

/**
 * IMF Composition Playlist
 */
typedef struct IMFCPL {
    uint8_t id_uuid[16];
    const char *content_title_utf8;
    uint32_t marker_count;
    IMFMarker *markers;
    uint32_t virtual_track_count;
    IMFVirtualTrack *virtual_tracks;
} IMFCPL;

int parse_imf_cpl_from_xml_dom(xmlDocPtr* doc, IMFCPL** cpl);

int parse_imf_cpl(AVIOContext* avio_context, IMFCPL** cpl);

void delete_imf_cpl(IMFCPL* cpl);

/**
 * IMF Composition Playlist Virtual Track
 */
typedef struct IMFVirtualTrack {
    uint8_t id_uuid[16];
    IMFVirtualTrackKind kind;
    IMFVirtualTrackDescriptor descriptor;
    IMFTrackFileResource *resources;
} IMFVirtualTrack;


/**
 * IMF Composition Playlist Resource
 */
typedef struct IMFTrackFileResource {
    AVRational edit_rate;
    uint64_t entry_point;
    uint64_t duration;
    uint32_t repeat_count;
    uint8_t track_file_uuid[16];
} IMFTrackFileResource;


/**
 * IMF Marker
 */
typedef struct IMFMarker {
    AVRational edit_rate;
    const char *label_utf8;
    const char *scope;
    uint64_t offset;
} IMFMarker;


/**
 * IMF Virtual Track Kind
 */
typedef enum IMFVirtualTrackKind {
    MainImage2D     = 1,
    MainAudio       = 2
} IMFVirtualTrackKind;


/**
 * IMF Virtual Track Descriptor
 */
typedef union IMFVirtualTrackDescriptor {
    IMFAudioDescriptor audio_descriptor;
    IMFImageDescriptor image_descriptor;
} IMFVirtualTrackDescriptor;


/**
 * IMF Image Virtual Track Descriptor
 */
typedef struct IMFImageDescriptor {
    uint8_t essence_coding_ul[16];
} IMFImageDescriptor;


/**
 * IMF Audio Virtual Track Descriptor
 */
typedef struct IMFAudioDescriptor {
    IMFAudioSampleRate sample_rate;
    IMFAudioChannel *channels;
} IMFAudioDescriptor;


/**
 * IMF Audio Channel
 */
typedef enum IMFAudioChannel {
    uint32_t index;
    const char *symbol;
    uint8_t ul[16];
    const char *spoken_language;
    uint8_t soundfield_ul[16];
    const char *soundfield_symbol;
} IMFAudioChannel;


/**
 * IMF Audio Sample Rate
 */
typedef enum IMFAudioSampleRate {
    FS48000     = 1,
    FS96000     = 2
} IMFAudioSampleRate;

#endif