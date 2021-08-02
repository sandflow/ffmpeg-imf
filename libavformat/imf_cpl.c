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
 * @file
 * @ingroup lavu_imf
 * Private header for processing of Interoperable Master Format packages
 */

#include "imf.h"
#include "imf_internal.h"
#include "libavutil/bprint.h"
#include "libavutil/error.h"
#include <libxml/parser.h>

xmlNodePtr xml_get_child_element_by_name(xmlNodePtr parent, const char *name_utf8) {
    xmlNodePtr cur_element;

    cur_element = xmlFirstElementChild(parent);
    while (cur_element) {
        if (xmlStrcmp(cur_element->name, name_utf8) == 0)
            return cur_element;
        cur_element = xmlNextElementSibling(cur_element);
    }
    return NULL;
}

int xml_read_UUID(xmlNodePtr element, uint8_t uuid[16]) {
    xmlChar *element_text = NULL;
    int scanf_ret;
    int ret = 0;

    element_text = xmlNodeListGetString(element->doc, element->xmlChildrenNode, 1);
    scanf_ret = sscanf(element_text,
        UUID_FORMAT,
        &uuid[0],
        &uuid[1],
        &uuid[2],
        &uuid[3],
        &uuid[4],
        &uuid[5],
        &uuid[6],
        &uuid[7],
        &uuid[8],
        &uuid[9],
        &uuid[10],
        &uuid[11],
        &uuid[12],
        &uuid[13],
        &uuid[14],
        &uuid[15]);
    if (scanf_ret != 16) {
        av_log(NULL, AV_LOG_ERROR, "Invalid UUID\n");
        ret = AVERROR_INVALIDDATA;
    }
    xmlFree(element_text);

    return ret;
}

int xml_read_rational(xmlNodePtr element, AVRational *rational) {
    xmlChar *element_text = NULL;
    int ret = 0;

    element_text = xmlNodeListGetString(element->doc, element->xmlChildrenNode, 1);
    if (sscanf(element_text, "%i %i", &rational->num, &rational->den) != 2) {
        av_log(NULL, AV_LOG_ERROR, "Invalid rational number\n");
        ret = AVERROR_INVALIDDATA;
    }
    xmlFree(element_text);

    return ret;
}

int xml_read_ulong(xmlNodePtr element, unsigned long *number) {
    xmlChar *element_text = NULL;
    int ret = 0;

    element_text = xmlNodeListGetString(element->doc, element->xmlChildrenNode, 1);
    if (sscanf(element_text, "%lu", number) != 1) {
        av_log(NULL, AV_LOG_ERROR, "Invalid unsigned long");
        ret = AVERROR_INVALIDDATA;
    }
    xmlFree(element_text);

    return ret;
}

static void imf_base_virtual_track_init(IMFBaseVirtualTrack *track) {
    memset(track->id_uuid, 0, sizeof(track->id_uuid));
}

static void imf_marker_virtual_track_init(IMFMarkerVirtualTrack *track) {
    imf_base_virtual_track_init((IMFBaseVirtualTrack *)track);
    track->resource_count = 0;
    track->resources = NULL;
}

static void imf_trackfile_virtual_track_init(IMFTrackFileVirtualTrack *track) {
    imf_base_virtual_track_init((IMFBaseVirtualTrack *)track);
    track->resource_count = 0;
    track->resources = NULL;
}

static void imf_base_resource_init(IMFBaseResource *rsrc) {
    rsrc->duration = 0;
    rsrc->edit_rate = av_make_q(0, 0);
    rsrc->entry_point = 0;
    rsrc->repeat_count = 1;
}

static void imf_marker_resource_init(IMFMarkerResource *rsrc) {
    imf_base_resource_init((IMFBaseResource *)rsrc);
    rsrc->marker_count = 0;
    rsrc->markers = NULL;
}

static void imf_marker_init(IMFMarker *marker) {
    marker->label_utf8 = NULL;
    marker->offset = 0;
    marker->scope_utf8 = NULL;
}

static void imf_trackfile_resource_init(IMFTrackFileResource *rsrc) {
    imf_base_resource_init((IMFBaseResource *)rsrc);
    memset(rsrc->track_file_uuid, 0, sizeof(rsrc->track_file_uuid));
}

static int fill_content_title(xmlNodePtr cpl_element, IMFCPL *cpl) {
    xmlNodePtr element = NULL;

    if (!(element = xml_get_child_element_by_name(cpl_element, "ContentTitle"))) {
        av_log(NULL, AV_LOG_ERROR, "ContentTitle element not found in the IMF CPL\n");
        return AVERROR_INVALIDDATA;
    }
    cpl->content_title_utf8 = xmlNodeListGetString(cpl_element->doc, element->xmlChildrenNode, 1);

    return 0;
}

static int fill_edit_rate(xmlNodePtr cpl_element, IMFCPL *cpl) {
    xmlNodePtr element = NULL;

    if (!(element = xml_get_child_element_by_name(cpl_element, "EditRate"))) {
        av_log(NULL, AV_LOG_ERROR, "EditRate element not found in the IMF CPL\n");
        return AVERROR_INVALIDDATA;
    }

    return xml_read_rational(element, &cpl->edit_rate);
}

static int fill_id(xmlNodePtr cpl_element, IMFCPL *cpl) {
    xmlNodePtr element = NULL;

    if (!(element = xml_get_child_element_by_name(cpl_element, "Id"))) {
        av_log(NULL, AV_LOG_ERROR, "Id element not found in the IMF CPL\n");
        return AVERROR_INVALIDDATA;
    }

    return xml_read_UUID(element, cpl->id_uuid);
}

static int fill_marker(xmlNodePtr marker_elem, IMFMarker *marker) {
    xmlNodePtr element = NULL;
    int ret = 0;

    /* read Offset */
    if (!(element = xml_get_child_element_by_name(marker_elem, "Offset"))) {
        av_log(NULL, AV_LOG_ERROR, "Offset element not found in a Marker\n");
        return AVERROR_INVALIDDATA;
    }
    if ((ret = xml_read_ulong(element, &marker->offset)))
        return ret;

    /* read Label and Scope */
    if (!(element = xml_get_child_element_by_name(marker_elem, "Label"))) {
        av_log(NULL, AV_LOG_ERROR, "Label element not found in a Marker\n");
        return AVERROR_INVALIDDATA;
    }
    if (!(marker->label_utf8 = xmlNodeListGetString(element->doc, element->xmlChildrenNode, 1))) {
        av_log(NULL, AV_LOG_ERROR, "Empty Label element found in a Marker\n");
        return AVERROR_INVALIDDATA;
    }
    if (!(marker->scope_utf8 = xmlGetNoNsProp(element, "scope"))) {
        marker->scope_utf8 = xmlCharStrdup("http://www.smpte-ra.org/schemas/2067-3/2013#standard-markers");
    }

    return ret;
}

static int fill_base_resource(xmlNodePtr resource_elem, IMFBaseResource *resource, IMFCPL *cpl) {
    xmlNodePtr element = NULL;
    int ret = 0;

    /* read EditRate */
    if (!(element = xml_get_child_element_by_name(resource_elem, "EditRate"))) {
        resource->edit_rate = cpl->edit_rate;
    } else if (ret = xml_read_rational(element, &resource->edit_rate)) {
        av_log(NULL, AV_LOG_ERROR, "Invalid EditRate element found in a Resource\n");
        return ret;
    }

    /* read EntryPoint */
    if (element = xml_get_child_element_by_name(resource_elem, "EntryPoint")) {
        if (ret = xml_read_ulong(element, &resource->entry_point)) {
            av_log(NULL, AV_LOG_ERROR, "Invalid EntryPoint element found in a Resource\n");
            return ret;
        }
    } else
        resource->entry_point = 0;

    /* read IntrinsicDuration */
    if (!(element = xml_get_child_element_by_name(resource_elem, "IntrinsicDuration"))) {
        av_log(NULL, AV_LOG_ERROR, "IntrinsicDuration element missing from Resource\n");
        return AVERROR_INVALIDDATA;
    }
    if (ret = xml_read_ulong(element, &resource->duration)) {
        av_log(NULL, AV_LOG_ERROR, "Invalid IntrinsicDuration element found in a Resource\n");
        return ret;
    }
    resource->duration -= resource->entry_point;

    /* read SourceDuration */
    if (element = xml_get_child_element_by_name(resource_elem, "SourceDuration")) {
        if (ret = xml_read_ulong(element, &resource->duration)) {
            av_log(NULL, AV_LOG_ERROR, "SourceDuration element missing from Resource\n");
            return ret;
        }
    }

    /* read RepeatCount */
    if (element = xml_get_child_element_by_name(resource_elem, "RepeatCount")) {
        ret = xml_read_ulong(element, &resource->repeat_count);
    }

    return ret;
}

static int fill_trackfile_resource(xmlNodePtr tf_resource_elem, IMFTrackFileResource *tf_resource, IMFCPL *cpl) {
    xmlNodePtr element = NULL;
    int ret = 0;

    if (ret = fill_base_resource(tf_resource_elem, (IMFBaseResource *)tf_resource, cpl))
        return ret;

    /* read TrackFileId */
    if (element = xml_get_child_element_by_name(tf_resource_elem, "TrackFileId")) {
        if (ret = xml_read_UUID(element, tf_resource->track_file_uuid)) {
            av_log(NULL, AV_LOG_ERROR, "Invalid TrackFileId element found in Resource\n");
            return ret;
        }
    } else {
        av_log(NULL, AV_LOG_ERROR, "TrackFileId element missing from Resource\n");
        return AVERROR_INVALIDDATA;
    }

    return ret;
}

static int fill_marker_resource(xmlNodePtr marker_resource_elem, IMFMarkerResource *marker_resource, IMFCPL *cpl) {
    xmlNodePtr element = NULL;
    int ret = 0;

    if (ret = fill_base_resource(marker_resource_elem, (IMFBaseResource *)marker_resource, cpl))
        return ret;

    /* read markers */
    element = xmlFirstElementChild(marker_resource_elem);
    while (element) {
        if (xmlStrcmp(element->name, "Marker") == 0) {
            marker_resource->markers = av_realloc(marker_resource->markers, (++marker_resource->marker_count) * sizeof(IMFMarker));
            if (!marker_resource->markers) {
                av_log(NULL, AV_LOG_PANIC, "Cannot allocate Marker\n");
                exit(1);
            }
            imf_marker_init(&marker_resource->markers[marker_resource->marker_count - 1]);
            fill_marker(element, &marker_resource->markers[marker_resource->marker_count - 1]);
        }
        element = xmlNextElementSibling(element);
    }

    return ret;
}

static int push_marker_sequence(xmlNodePtr marker_sequence_elem, IMFCPL *cpl) {
    int ret = 0;
    uint8_t uuid[16];
    xmlNodePtr resource_list_elem = NULL;
    xmlNodePtr resource_elem = NULL;
    xmlNodePtr track_id_elem = NULL;

    /* read TrackID element */
    if (!(track_id_elem = xml_get_child_element_by_name(marker_sequence_elem, "TrackId"))) {
        av_log(NULL, AV_LOG_ERROR, "TrackId element missing from Sequence\n");
        return AVERROR_INVALIDDATA;
    }
    if (ret = xml_read_UUID(track_id_elem, uuid)) {
        av_log(NULL, AV_LOG_ERROR, "Invalid TrackId element found in Sequence\n");
        return AVERROR_INVALIDDATA;
    }

    /* create main marker virtual track if it does not exist */
    if (!cpl->main_markers_track) {
        cpl->main_markers_track = av_malloc(sizeof(IMFMarkerVirtualTrack));
        if (!cpl->main_markers_track) {
            av_log(NULL, AV_LOG_PANIC, "Cannot allocate Marker Virtual Track\n");
            exit(1);
        }
        imf_marker_virtual_track_init(cpl->main_markers_track);
        memcpy(cpl->main_markers_track->base.id_uuid, uuid, sizeof(uuid));
    } else if (memcmp(cpl->main_markers_track->base.id_uuid, uuid, sizeof(uuid)) != 0) {
        av_log(NULL, AV_LOG_ERROR, "Multiple marker virtual tracks were found\n");
        return AVERROR_INVALIDDATA;
    }

    /* process resources */
    resource_list_elem = xml_get_child_element_by_name(marker_sequence_elem, "ResourceList");
    if (!resource_list_elem)
        return 0;
    resource_elem = xmlFirstElementChild(resource_list_elem);
    while (resource_elem) {
        cpl->main_markers_track->resources = av_realloc(cpl->main_markers_track->resources, (++cpl->main_markers_track->resource_count) * sizeof(IMFMarkerResource));
        if (!cpl->main_markers_track->resources) {
            av_log(NULL, AV_LOG_PANIC, "Cannot allocate Resource\n");
            exit(1);
        }
        imf_marker_resource_init(&cpl->main_markers_track->resources[cpl->main_markers_track->resource_count - 1]);
        fill_marker_resource(resource_elem, &cpl->main_markers_track->resources[cpl->main_markers_track->resource_count - 1], cpl);
        resource_elem = xmlNextElementSibling(resource_elem);
    }

    return ret;
}

static int has_stereo_resources(xmlNodePtr element) {
    if (xmlStrcmp(element->name, "Left") == 0 || xmlStrcmp(element->name, "Right") == 0)
        return 1;
    element = xmlFirstElementChild(element);
    while (element) {
        if (has_stereo_resources(element))
            return 1;
        element = xmlNextElementSibling(element);
    }
    return 0;
}

static int push_main_audio_sequence(xmlNodePtr audio_sequence_elem, IMFCPL *cpl) {
    int ret = 0;
    uint8_t uuid[16];
    xmlNodePtr resource_list_elem = NULL;
    xmlNodePtr resource_elem = NULL;
    xmlNodePtr track_id_elem = NULL;
    IMFTrackFileVirtualTrack *vt = NULL;

    /* read TrackID element */
    if (!(track_id_elem = xml_get_child_element_by_name(audio_sequence_elem, "TrackId"))) {
        av_log(NULL, AV_LOG_ERROR, "TrackId element missing from audio sequence\n");
        return AVERROR_INVALIDDATA;
    }
    if (ret = xml_read_UUID(track_id_elem, uuid)) {
        av_log(NULL, AV_LOG_ERROR, "Invalid TrackId element found in audio sequence\n");
        return ret;
    }

    /* get the main audio virtual track corresponding to the sequence */
    for (int i = 0; i < cpl->main_audio_track_count; i++)
        if (memcmp(cpl->main_audio_tracks[i].base.id_uuid, uuid, sizeof(uuid)) == 0) {
            vt = &cpl->main_audio_tracks[i];
            break;
        }

    /* create a main audio virtual track if none exists for the sequence */
    if (!vt) {
        cpl->main_audio_tracks = av_realloc(cpl->main_audio_tracks, sizeof(IMFTrackFileVirtualTrack) * (++cpl->main_audio_track_count));
        if (!cpl->main_audio_tracks) {
            av_log(NULL, AV_LOG_PANIC, "Cannot allocate MainAudio virtual track\n");
            exit(1);
        }
        vt = &cpl->main_audio_tracks[cpl->main_audio_track_count - 1];
        imf_trackfile_virtual_track_init(vt);
        memcpy(vt->base.id_uuid, uuid, sizeof(uuid));
    }

    /* process resources */
    resource_list_elem = xml_get_child_element_by_name(audio_sequence_elem, "ResourceList");
    if (!resource_list_elem)
        return 0;
    resource_elem = xmlFirstElementChild(resource_list_elem);
    while (resource_elem) {
        vt->resources = av_realloc(vt->resources, (++vt->resource_count) * sizeof(IMFTrackFileResource));
        if (!vt->resources) {
            av_log(NULL, AV_LOG_PANIC, "Cannot allocate Resource\n");
            exit(1);
        }
        imf_trackfile_resource_init(&vt->resources[vt->resource_count - 1]);
        fill_trackfile_resource(resource_elem, &vt->resources[vt->resource_count - 1], cpl);
        resource_elem = xmlNextElementSibling(resource_elem);
    }

    return ret;
}

static int push_main_image_2d_sequence(xmlNodePtr image_sequence_elem, IMFCPL *cpl) {
    int ret = 0;
    uint8_t uuid[16];
    xmlNodePtr resource_list_elem = NULL;
    xmlNodePtr resource_elem = NULL;
    xmlNodePtr track_id_elem = NULL;

    /* skip stereoscopic resources */
    if (has_stereo_resources(image_sequence_elem)) {
        av_log(NULL, AV_LOG_ERROR, "Stereoscopic 3D image virtual tracks not supported\n");
        return AVERROR_PATCHWELCOME;
    }

    /* read TrackId element*/
    if (!(track_id_elem = xml_get_child_element_by_name(image_sequence_elem, "TrackId"))) {
        av_log(NULL, AV_LOG_ERROR, "TrackId element missing from audio sequence\n");
        return AVERROR_INVALIDDATA;
    }
    if (ret = xml_read_UUID(track_id_elem, uuid)) {
        av_log(NULL, AV_LOG_ERROR, "Invalid TrackId element found in audio sequence\n");
        return ret;
    }

    /* create main image virtual track if one does not exist */
    if (!cpl->main_image_2d_track) {
        cpl->main_image_2d_track = av_malloc(sizeof(IMFTrackFileVirtualTrack));
        if (!cpl->main_image_2d_track) {
            av_log(NULL, AV_LOG_PANIC, "Cannot allocate MainImage virtual track\n");
            exit(1);
        }
        imf_trackfile_virtual_track_init(cpl->main_image_2d_track);
        memcpy(cpl->main_image_2d_track->base.id_uuid, uuid, sizeof(uuid));
    } else if (memcmp(cpl->main_image_2d_track->base.id_uuid, uuid, sizeof(uuid)) != 0) {
        av_log(NULL, AV_LOG_ERROR, "Multiple MainImage virtual tracks found\n");
        return AVERROR_INVALIDDATA;
    }

    /* process resources */
    if (!(resource_list_elem = xml_get_child_element_by_name(image_sequence_elem, "ResourceList")))
        return 0;
    resource_elem = xmlFirstElementChild(resource_list_elem);
    while (resource_elem) {
        cpl->main_image_2d_track->resources = av_realloc(cpl->main_image_2d_track->resources, (++cpl->main_image_2d_track->resource_count) * sizeof(IMFTrackFileResource));
        if (!cpl->main_image_2d_track->resources) {
            av_log(NULL, AV_LOG_PANIC, "Cannot allocate Resource\n");
            exit(1);
        }
        imf_trackfile_resource_init(&cpl->main_image_2d_track->resources[cpl->main_image_2d_track->resource_count - 1]);
        fill_trackfile_resource(resource_elem, &cpl->main_image_2d_track->resources[cpl->main_image_2d_track->resource_count - 1], cpl);
        resource_elem = xmlNextElementSibling(resource_elem);
    }

    return 0;
}

static int fill_virtual_tracks(xmlNodePtr cpl_element, IMFCPL *cpl) {
    int ret = 0;
    xmlNodePtr segment_list_elem = NULL;
    xmlNodePtr segment_elem = NULL;
    xmlNodePtr sequence_list_elem = NULL;
    xmlNodePtr sequence_elem = NULL;

    if (!(segment_list_elem = xml_get_child_element_by_name(cpl_element, "SegmentList"))) {
        av_log(NULL, AV_LOG_ERROR, "SegmentList element missing\n");
        return AVERROR_INVALIDDATA;
    }

    /* process sequences */
    segment_elem = xmlFirstElementChild(segment_list_elem);
    while (segment_elem) {
        sequence_list_elem = xml_get_child_element_by_name(segment_elem, "SequenceList");
        if (!segment_list_elem)
            continue;
        sequence_elem = xmlFirstElementChild(sequence_list_elem);
        while (sequence_elem) {
            /* TODO: compare namespaces */
            if (xmlStrcmp(sequence_elem->name, "MarkerSequence") == 0)
                push_marker_sequence(sequence_elem, cpl);
            else if (xmlStrcmp(sequence_elem->name, "MainImageSequence") == 0)
                push_main_image_2d_sequence(sequence_elem, cpl);
            else if (xmlStrcmp(sequence_elem->name, "MainAudioSequence") == 0)
                push_main_audio_sequence(sequence_elem, cpl);
            else {
                av_log(NULL, AV_LOG_INFO, "The following Sequence is not supported and is ignored: %s\n", sequence_elem->name);
            }
            sequence_elem = xmlNextElementSibling(sequence_elem);
        }
        segment_elem = xmlNextElementSibling(segment_elem);
    }

    return ret;
}

int parse_imf_cpl_from_xml_dom(xmlDocPtr doc, IMFCPL **cpl) {
    int ret = 0;
    xmlNodePtr cpl_element = NULL;

    *cpl = imf_cpl_alloc();
    if (!*cpl) {
        av_log(NULL, AV_LOG_FATAL, "Cannot allocate CPL\n");
        ret = AVERROR_BUG;
        goto cleanup;
    }
    cpl_element = xmlDocGetRootElement(doc);
    if (xmlStrcmp(cpl_element->name, "CompositionPlaylist")) {
        av_log(NULL, AV_LOG_ERROR, "The root element of the CPL is not CompositionPlaylist\n");
        ret = AVERROR_INVALIDDATA;
        goto cleanup;
    }
    if (ret = fill_content_title(cpl_element, *cpl))
        goto cleanup;
    if (ret = fill_id(cpl_element, *cpl))
        goto cleanup;
    if (ret = fill_edit_rate(cpl_element, *cpl))
        goto cleanup;
    if (ret = fill_virtual_tracks(cpl_element, *cpl))
        goto cleanup;

cleanup:
    if (*cpl && ret) {
        imf_cpl_free(*cpl);
        *cpl = NULL;
    }
    return ret;
}

static void imf_marker_free(IMFMarker *marker) {
    if (!marker)
        return;
    xmlFree(marker->label_utf8);
    xmlFree(marker->scope_utf8);
}

static void imf_marker_resource_free(IMFMarkerResource *rsrc) {
    if (!rsrc)
        return;
    for (unsigned long i = 0; i < rsrc->marker_count; i++)
        imf_marker_free(&rsrc->markers[i]);
    av_free(rsrc->markers);
}

static void imf_marker_virtual_track_free(IMFMarkerVirtualTrack *vt) {
    if (!vt)
        return;
    for (unsigned long i = 0; i < vt->resource_count; i++)
        imf_marker_resource_free(&vt->resources[i]);
    av_free(vt->resources);
}

static void imf_trackfile_virtual_track_free(IMFTrackFileVirtualTrack *vt) {
    if (!vt)
        return;
    av_free(vt->resources);
}

static void imf_cpl_init(IMFCPL *cpl) {
    memset(cpl->id_uuid, 0, sizeof(cpl->id_uuid));
    cpl->content_title_utf8 = NULL;
    cpl->edit_rate = av_make_q(0, 0);
    cpl->main_markers_track = NULL;
    cpl->main_image_2d_track = NULL;
    cpl->main_audio_track_count = 0;
    cpl->main_audio_tracks = NULL;
}

IMFCPL *imf_cpl_alloc(void) {
    IMFCPL *cpl;

    cpl = av_malloc(sizeof(IMFCPL));
    if (!cpl)
        return NULL;
    imf_cpl_init(cpl);
    return cpl;
}

void imf_cpl_free(IMFCPL *cpl) {
    if (cpl) {
        xmlFree(cpl->content_title_utf8);
        imf_marker_virtual_track_free(cpl->main_markers_track);
        imf_trackfile_virtual_track_free(cpl->main_image_2d_track);
        for (unsigned long i = 0; i < cpl->main_audio_track_count; i++)
            imf_trackfile_virtual_track_free(&cpl->main_audio_tracks[i]);
    }
    av_free(cpl);
    cpl = NULL;
}

int parse_imf_cpl(AVIOContext *in, IMFCPL **cpl) {
    AVBPrint buf;
    xmlDoc *doc = NULL;
    int ret = 0;
    int64_t filesize = 0;

    filesize = avio_size(in);
    filesize = filesize > 0 ? filesize : 8192;
    av_bprint_init(&buf, filesize + 1, AV_BPRINT_SIZE_UNLIMITED);
    if ((ret = avio_read_to_bprint(in, &buf, UINT_MAX - 1)) < 0 || !avio_feof(in) || (filesize = buf.len) == 0) {
        if (ret == 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot read IMF CPL\n");
            return AVERROR_INVALIDDATA;
        }
    } else {
        LIBXML_TEST_VERSION
        doc = xmlReadMemory(buf.str, filesize, NULL, NULL, 0);
        if (!doc) {
            av_log(NULL, AV_LOG_ERROR, "XML parsing failed when reading the IMF CPL\n");
            ret = AVERROR_INVALIDDATA;
        }
        if (ret = parse_imf_cpl_from_xml_dom(doc, cpl)) {
            av_log(NULL, AV_LOG_ERROR, "Cannot parse IMF CPL\n");
        }
        xmlFreeDoc(doc);
        xmlCleanupParser();
    }

    return ret;
}
