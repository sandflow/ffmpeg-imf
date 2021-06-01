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

#include "imf.h"
#include "imf_internal.h"
#include "libavutil/bprint.h"
#include "libavutil/error.h"
#include <libxml/parser.h>

static const char *UUID_SCANF_FMT = "urn:uuid:%2hhx%2hhx%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx";

IMFCPL *imf_cpl_alloc(void) {
    IMFCPL *cpl;

    cpl = malloc(sizeof(IMFCPL));
    if (cpl == NULL)
        return NULL;
    memset(cpl->id_uuid, 0, sizeof(cpl->id_uuid));
    cpl->content_title_utf8 = NULL;
    cpl->edit_rate = av_make_q(0, 0);
    cpl->main_markers_track = NULL;
    cpl->main_image_2d_track = NULL;
    cpl->main_audio_track_count = 0;
    cpl->main_audio_tracks = NULL;
    return cpl;
}

xmlNodePtr getChildElementByName(xmlNodePtr parent, const char *name_utf8) {
    xmlNodePtr cur_element;

    cur_element = xmlFirstElementChild(parent);
    while (cur_element != NULL) {
        if (xmlStrcmp(cur_element->name, name_utf8) == 0)
            return cur_element;
        cur_element = xmlNextElementSibling(cur_element);
    }
    return NULL;
}

int xmlReadUUID(xmlNodePtr element, uint8_t uuid[16]) {
    xmlChar *element_text = NULL;
    int      scanf_ret;
    int      ret = 0;

    element_text = xmlNodeListGetString(element->doc, element->xmlChildrenNode, 1);
    scanf_ret = sscanf(element_text,
        UUID_SCANF_FMT,
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
    if (scanf_ret != 16)
        ret = 1;
    xmlFree(element_text);

    return ret;
}

int xmlReadRational(xmlNodePtr element, AVRational *rational) {
    xmlChar *element_text = NULL;
    int      scanf_ret;
    int      ret = 0;

    element_text = xmlNodeListGetString(element->doc, element->xmlChildrenNode, 1);
    scanf_ret = sscanf(element_text, "%i %i", &rational->num, &rational->den);
    if (scanf_ret != 2)
        ret = 1;
    xmlFree(element_text);

    return ret;
}

int xmlReadULong(xmlNodePtr element, unsigned long *number) {
    xmlChar *element_text = NULL;
    int      scanf_ret;
    int      ret = 0;

    element_text = xmlNodeListGetString(element->doc, element->xmlChildrenNode, 1);
    scanf_ret = sscanf(element_text, "%lu", number);
    if (scanf_ret != 1)
        ret = 1;
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

static void imf_base_resource_init(IMFBaseResource *r) {
    r->duration = 0;
    r->edit_rate = av_make_q(0, 0);
    r->entry_point = 0;
    r->repeat_count = 1;
}

static void imf_marker_resource_init(IMFMarkerResource *r) {
    imf_base_resource_init((IMFBaseResource *)r);
    r->marker_count = 0;
    r->markers = NULL;
}

static void imf_marker_init(IMFMarker *m) {
    m->label_utf8 = NULL;
    m->offset = 0;
    m->scope_utf8 = NULL;
}

static void imf_trackfile_resource_init(IMFTrackFileResource *r) {
    imf_base_resource_init((IMFBaseResource *)r);
    memset(r->track_file_uuid, 0, sizeof(r->track_file_uuid));
}

static int fill_content_title(xmlNodePtr cpl_element, IMFCPL *cpl) {
    xmlNodePtr element = NULL;

    element = getChildElementByName(cpl_element, "ContentTitle");
    if (element == NULL)
        return 1;
    cpl->content_title_utf8 = xmlNodeListGetString(cpl_element->doc, element->xmlChildrenNode, 1);

    return 0;
}

static int fill_edit_rate(xmlNodePtr cpl_element, IMFCPL *cpl) {
    int        ret = 0;
    xmlNodePtr element = NULL;

    element = getChildElementByName(cpl_element, "EditRate");
    if (element == NULL) {
        ret = 1;
    }
    ret = xmlReadRational(element, &cpl->edit_rate);
    return ret;
}

static int fill_id(xmlNodePtr cpl_element, IMFCPL *cpl) {
    xmlNodePtr element = NULL;

    element = getChildElementByName(cpl_element, "Id");
    if (element == NULL)
        return 1;
    return xmlReadUUID(element, cpl->id_uuid);
}

static int fill_marker(xmlNodePtr marker_elem, IMFMarker *marker) {
    xmlNodePtr element = NULL;
    int        ret = 0;

    /* read Offset */
    element = getChildElementByName(marker_elem, "Offset");
    if (element == NULL)
        return 1;
    ret = xmlReadULong(element, &marker->offset);
    if (ret)
        return ret;

    /* read Label and Scope */
    element = getChildElementByName(marker_elem, "Label");
    if (element == NULL)
        return 1;
    marker->label_utf8 = xmlNodeListGetString(element->doc, element->xmlChildrenNode, 1);
    if (marker->label_utf8 == NULL)
        return 1;
    marker->scope_utf8 = xmlGetNoNsProp(element, "scope");
    if (marker->scope_utf8 == NULL) {
        marker->scope_utf8 = xmlCharStrdup("http://www.smpte-ra.org/schemas/2067-3/2013#standard-markers");
    }

    return ret;
}

static int fill_base_resource(xmlNodePtr resource_elem,
    IMFBaseResource *                    resource,
    IMFCPL *                             cpl) {
    xmlNodePtr element = NULL;
    int        ret = 0;

    /* read EditRate */
    element = getChildElementByName(resource_elem, "EditRate");
    if (element == NULL) {
        resource->edit_rate = cpl->edit_rate;
    } else {
        ret = xmlReadRational(element, &resource->edit_rate);
        if (ret)
            return ret;
    }

    /* read EntryPoint */
    element = getChildElementByName(resource_elem, "EntryPoint");
    if (element != NULL) {
        ret = xmlReadULong(element, &resource->entry_point);
        if (ret)
            return ret;
    } else
        resource->entry_point = 0;

    /* read IntrinsicDuration */
    element = getChildElementByName(resource_elem, "IntrinsicDuration");
    if (element == NULL)
        return 1;
    ret = xmlReadULong(element, &resource->duration);
    if (ret)
        return ret;
    resource->duration -= resource->entry_point;

    /* read SourceDuration */
    element = getChildElementByName(resource_elem, "SourceDuration");
    if (element != NULL) {
        ret = xmlReadULong(element, &resource->duration);
        if (ret)
            return ret;
    }

    /* read RepeatCount */
    element = getChildElementByName(resource_elem, "RepeatCount");
    if (element != NULL) {
        ret = xmlReadULong(element, &resource->repeat_count);
        if (ret)
            return ret;
    }

    return 0;
}

static int fill_trackfile_resource(xmlNodePtr tf_resource_elem,
    IMFTrackFileResource *                    tf_resource,
    IMFCPL *                                  cpl) {
    xmlNodePtr element = NULL;
    int        ret = 0;

    ret = fill_base_resource(tf_resource_elem, (IMFBaseResource *)tf_resource, cpl);
    if (ret)
        return ret;

    /* read TrackFileId */
    element = getChildElementByName(tf_resource_elem, "TrackFileId");
    if (element) {
        ret = xmlReadUUID(element, tf_resource->track_file_uuid);
        if (ret)
            return ret;
    }

    return 0;
}

static int fill_marker_resource(xmlNodePtr marker_resource_elem, IMFMarkerResource *marker_resource, IMFCPL *cpl) {
    xmlNodePtr element = NULL;
    int        ret = 0;

    ret = fill_base_resource(marker_resource_elem, (IMFBaseResource *)marker_resource, cpl);
    if (ret)
        return ret;

    /* read markers */
    element = xmlFirstElementChild(marker_resource_elem);
    while (element != NULL) {
        if (xmlStrcmp(element->name, "Marker") == 0) {
            marker_resource->markers = realloc(marker_resource->markers, (++marker_resource->marker_count) * sizeof(IMFMarker));
            if (marker_resource->markers == NULL)
                return 1;
            imf_marker_init(&marker_resource->markers[marker_resource->marker_count - 1]);
            fill_marker(element, &marker_resource->markers[marker_resource->marker_count - 1]);
        }
        element = xmlNextElementSibling(element);
    }

    return 0;
}

static int push_marker_sequence(xmlNodePtr marker_sequence_elem, IMFCPL *cpl) {
    int        ret;
    uint8_t    uuid[16];
    xmlNodePtr resource_list_elem = NULL;
    xmlNodePtr resource_elem = NULL;
    xmlNodePtr track_id_elem = NULL;

    /* read TrackID element */
    track_id_elem = getChildElementByName(marker_sequence_elem, "TrackId");
    if (track_id_elem == NULL)
        return 1;
    ret = xmlReadUUID(track_id_elem, uuid);
    if (ret)
        return ret;

    /* create main marker virtual track if it does not exist */
    if (cpl->main_markers_track == NULL) {
        cpl->main_markers_track = malloc(sizeof(IMFMarkerVirtualTrack));
        imf_marker_virtual_track_init(cpl->main_markers_track);
        memcpy(cpl->main_markers_track->base.id_uuid, uuid, sizeof(uuid));
    } else if (memcmp(cpl->main_markers_track->base.id_uuid, uuid, sizeof(uuid)) != 0)
        /* multiple marker virtual tracks were found */
        return 1;

    /* process resources */
    resource_list_elem = getChildElementByName(marker_sequence_elem, "ResourceList");
    if (resource_list_elem == NULL)
        return 0;
    resource_elem = xmlFirstElementChild(resource_list_elem);
    while (resource_elem != NULL) {
        /* TODO: do we need to check realloc result for NULL? */
        cpl->main_markers_track->resources = realloc(cpl->main_markers_track->resources, (++cpl->main_markers_track->resource_count) * sizeof(IMFMarkerResource));
        imf_marker_resource_init(&cpl->main_markers_track->resources[cpl->main_markers_track->resource_count - 1]);
        fill_marker_resource(resource_elem, &cpl->main_markers_track->resources[cpl->main_markers_track->resource_count - 1], cpl);
        resource_elem = xmlNextElementSibling(resource_elem);
    }

    return 0;
}

static int has_stereo_resources(xmlNodePtr element) {
    if (xmlStrcmp(element->name, "Left") == 0 || xmlStrcmp(element->name, "Right") == 0)
        return 1;
    element = xmlFirstElementChild(element);
    while (element != NULL) {
        if (has_stereo_resources(element))
            return 1;
        element = xmlNextElementSibling(element);
    }
    return 0;
}

static int push_main_audio_sequence(xmlNodePtr audio_sequence_elem, IMFCPL *cpl) {
    int                       ret;
    uint8_t                   uuid[16];
    xmlNodePtr                resource_list_elem = NULL;
    xmlNodePtr                resource_elem = NULL;
    xmlNodePtr                track_id_elem = NULL;
    IMFTrackFileVirtualTrack *vt = NULL;

    /* read TrackID element */
    track_id_elem = getChildElementByName(audio_sequence_elem, "TrackId");
    if (track_id_elem == NULL)
        return 1;
    ret = xmlReadUUID(track_id_elem, uuid);
    if (ret)
        return ret;

    /* get the main audio virtual track corresponding to the sequence */
    for (int i; i < cpl->main_audio_track_count; i++)
        if (memcmp(cpl->main_audio_tracks[i].base.id_uuid, uuid, sizeof(uuid)) == 0) {
            vt = &cpl->main_audio_tracks[i];
            break;
        }

    /* create a main audio virtual track if none exists for the sequence */
    if (vt == NULL) {
        cpl->main_audio_tracks = realloc(cpl->main_audio_tracks, sizeof(IMFTrackFileVirtualTrack) * (++cpl->main_audio_track_count));
        vt = &cpl->main_audio_tracks[cpl->main_audio_track_count - 1];
        imf_trackfile_virtual_track_init(vt);
        memcpy(vt->base.id_uuid, uuid, sizeof(uuid));
    }

    /* process resources */
    resource_list_elem = getChildElementByName(audio_sequence_elem, "ResourceList");
    if (resource_list_elem == NULL)
        return 0;
    resource_elem = xmlFirstElementChild(resource_list_elem);
    while (resource_elem != NULL) {
        vt->resources = realloc(vt->resources, (++vt->resource_count) * sizeof(IMFTrackFileResource));
        imf_trackfile_resource_init(&vt->resources[vt->resource_count - 1]);
        fill_trackfile_resource(resource_elem, &vt->resources[vt->resource_count - 1], cpl);
        resource_elem = xmlNextElementSibling(resource_elem);
    }

    return 0;
}

static int push_main_image_2d_sequence(xmlNodePtr image_sequence_elem, IMFCPL *cpl) {
    int        ret;
    uint8_t    uuid[16];
    xmlNodePtr resource_list_elem = NULL;
    xmlNodePtr resource_elem = NULL;
    xmlNodePtr track_id_elem = NULL;

    /* skip stereoscopic resources */
    if (has_stereo_resources(image_sequence_elem))
        return 1;

    /* read TrackId element*/
    track_id_elem = getChildElementByName(image_sequence_elem, "TrackId");
    if (track_id_elem == NULL)
        return 1;
    ret = xmlReadUUID(track_id_elem, uuid);
    if (ret)
        return ret;

    /* create main image virtual track if one does not exist */
    if (cpl->main_image_2d_track == NULL) {
        cpl->main_image_2d_track = malloc(sizeof(IMFTrackFileVirtualTrack));
        imf_trackfile_virtual_track_init(cpl->main_image_2d_track);
        memcpy(cpl->main_image_2d_track->base.id_uuid, uuid, sizeof(uuid));
    } else if (memcmp(cpl->main_image_2d_track->base.id_uuid, uuid, sizeof(uuid)) != 0)
        /* multiple main image virtual track */
        return 1;

    /* process resources */
    resource_list_elem = getChildElementByName(image_sequence_elem, "ResourceList");
    if (resource_list_elem == NULL)
        return 0;
    resource_elem = xmlFirstElementChild(resource_list_elem);
    while (resource_elem != NULL) {
        cpl->main_image_2d_track->resources = realloc(cpl->main_image_2d_track->resources, (++cpl->main_image_2d_track->resource_count) * sizeof(IMFTrackFileResource));
        imf_trackfile_resource_init(&cpl->main_image_2d_track->resources[cpl->main_image_2d_track->resource_count - 1]);
        fill_trackfile_resource(resource_elem, &cpl->main_image_2d_track->resources[cpl->main_image_2d_track->resource_count - 1], cpl);
        resource_elem = xmlNextElementSibling(resource_elem);
    }

    return 0;
}

static int fill_virtual_tracks(xmlNodePtr cpl_element, IMFCPL *cpl) {
    int        ret = 0;
    xmlNodePtr segment_list_elem = NULL;
    xmlNodePtr segment_elem = NULL;
    xmlNodePtr sequence_list_elem = NULL;
    xmlNodePtr sequence_elem = NULL;

    segment_list_elem = getChildElementByName(cpl_element, "SegmentList");
    if (segment_list_elem == NULL)
        return 1;

    /* process sequences */
    segment_elem = xmlFirstElementChild(segment_list_elem);
    while (segment_elem != NULL) {
        sequence_list_elem = getChildElementByName(segment_elem, "SequenceList");
        if (segment_list_elem == NULL)
            continue;
        sequence_elem = xmlFirstElementChild(sequence_list_elem);
        while (sequence_elem != NULL) {
            /* TODO: compare namespaces */
            if (xmlStrcmp(sequence_elem->name, "MarkerSequence") == 0)
                push_marker_sequence(sequence_elem, cpl);
            else if (xmlStrcmp(sequence_elem->name, "MainImageSequence") == 0)
                push_main_image_2d_sequence(sequence_elem, cpl);
            else if (xmlStrcmp(sequence_elem->name, "MainAudioSequence") == 0)
                push_main_audio_sequence(sequence_elem, cpl);
            sequence_elem = xmlNextElementSibling(sequence_elem);
        }
        segment_elem = xmlNextElementSibling(segment_elem);
    }

    return ret;
}

int parse_imf_cpl_from_xml_dom(xmlDocPtr doc, IMFCPL **cpl) {
    int        ret = 0;
    xmlNodePtr cpl_element = NULL;

    *cpl = imf_cpl_alloc();
    if (*cpl == NULL) {
        ret = AVERROR_BUG;
        goto cleanup;
    }
    cpl_element = xmlDocGetRootElement(doc);
    if (fill_content_title(cpl_element, *cpl)) {
        ret = AVERROR_BUG;
        goto cleanup;
    }
    if (fill_id(cpl_element, *cpl)) {
        ret = AVERROR_BUG;
        goto cleanup;
    }
    if (fill_edit_rate(cpl_element, *cpl)) {
        ret = AVERROR_BUG;
        goto cleanup;
    }
    if (fill_virtual_tracks(cpl_element, *cpl)) {
        ret = AVERROR_BUG;
        goto cleanup;
    }
cleanup:
    if (*cpl && ret)
        imf_cpl_free(*cpl);
    return ret;
}

static void imf_marker_free(IMFMarker *m) {
    if (m == NULL)
        return;
    xmlFree(m->label_utf8);
    xmlFree(m->scope_utf8);
}

static void imf_marker_resource_free(IMFMarkerResource *r) {
    if (r == NULL)
        return;
    for (unsigned long i = 0; i < r->marker_count; i++)
        imf_marker_free(&r->markers[i]);
    free(r->markers);
}

static void imf_marker_virtual_track_free(IMFMarkerVirtualTrack *vt) {
    if (vt == NULL)
        return;
    for (unsigned long i = 0; i < vt->resource_count; i++)
        imf_marker_resource_free(&vt->resources[i]);
    free(vt->resources);
}

static void imf_trackfile_virtual_track_free(IMFTrackFileVirtualTrack *vt) {
    if (vt == NULL)
        return;
    free(vt->resources);
}

void imf_cpl_free(IMFCPL *cpl) {
    if (cpl) {
        xmlFree(cpl->content_title_utf8);
        imf_marker_virtual_track_free(cpl->main_markers_track);
        imf_trackfile_virtual_track_free(cpl->main_image_2d_track);
        for (unsigned long i = 0; i < cpl->main_audio_track_count; i++)
            imf_trackfile_virtual_track_free(&cpl->main_audio_tracks[i]);
    }
    free(cpl);
    cpl = NULL;
}

int parse_imf_cpl(AVIOContext *in, IMFCPL **cpl) {

    AVBPrint buf;
    xmlDoc * doc = NULL;
    int      ret = 0;
    int64_t  filesize = 0;

    filesize = avio_size(in);
    filesize = filesize > 0 ? filesize : 8192;
    av_bprint_init(&buf, filesize + 1, AV_BPRINT_SIZE_UNLIMITED);
    if ((ret = avio_read_to_bprint(in, &buf, UINT_MAX - 1)) < 0 || !avio_feof(in) || (filesize = buf.len) == 0) {
        if (ret == 0)
            ret = AVERROR_INVALIDDATA;
    } else {
        LIBXML_TEST_VERSION
        doc = xmlReadMemory(buf.str, filesize, NULL, NULL, 0);
        if (doc == NULL)
            return AVERROR_INVALIDDATA;
        ret = parse_imf_cpl_from_xml_dom(doc, cpl);
        xmlFreeDoc(doc);
        xmlCleanupParser();
    }

    return ret;
}
