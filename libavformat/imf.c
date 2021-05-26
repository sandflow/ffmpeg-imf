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
#include "libavutil/error.h"
#include <libxml/xpath.h>


const char *UUID_FMT_STR =
    "urn:uuid:%2hhx%2hhx%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx";


IMFCPL *imf_cpl_new(void)
{
    return calloc(1, sizeof(IMFCPL));
}

xmlNodePtr getChildElementByName(xmlNodePtr parent, const char* name_utf8);

xmlNodePtr getChildElementByName(xmlNodePtr parent, const char* name_utf8)
{
    xmlNodePtr cur;

    cur = xmlFirstElementChild(parent);

    while (cur != NULL) {

        if (xmlStrcmp(cur->name, name_utf8) == 0) {
            
            return cur;
        }
                
        cur = xmlNextElementSibling(cur);
    }

    return NULL;
}

int readUUID(xmlNodePtr element, uint8_t uuid[16]);

int readUUID(xmlNodePtr element, uint8_t uuid[16])
{
    xmlChar *element_text = NULL;
    int scanf_ret;
    int ret = 0;

    element_text =
        xmlNodeListGetString(element->doc, element->xmlChildrenNode, 1);

    scanf_ret = sscanf(element_text,
                       UUID_FMT_STR,
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

    if (element_text)
        xmlFree(element_text);

    return ret;
}

int readRational(xmlNodePtr element, AVRational *rational);

int readRational(xmlNodePtr element, AVRational *rational)
{
    xmlChar *element_text = NULL;
    int scanf_ret;
    int ret = 0;

    element_text =
        xmlNodeListGetString(element->doc, element->xmlChildrenNode, 1);

    scanf_ret = sscanf(element_text, "%i %i", &rational->num, &rational->den);

    if (scanf_ret != 2) {
        ret = 1;
    }

    if (element_text)
        xmlFree(element_text);

    return ret;
}

int readULong(xmlNodePtr element, unsigned long *number);

int readULong(xmlNodePtr element, unsigned long *number)
{
    xmlChar *element_text = NULL;
    int scanf_ret;
    int ret = 0;

    element_text =
        xmlNodeListGetString(element->doc, element->xmlChildrenNode, 1);

    scanf_ret = sscanf(element_text, "%lu", number);

    if (scanf_ret != 1) {
        ret = 1;
    }

    if (element_text)
        xmlFree(element_text);

    return ret;
}

int fill_content_title(xmlNodePtr cpl_element, IMFCPL * cpl);

int fill_content_title(xmlNodePtr cpl_element, IMFCPL * cpl)
{
    xmlNodePtr element = NULL;

    element = getChildElementByName(cpl_element, "ContentTitle");

    if (! element)
        return 1;

    cpl->content_title_utf8 =
        xmlNodeListGetString(cpl_element->doc, element->xmlChildrenNode, 1);

    return 0;
}

int fill_edit_rate(xmlNodePtr cpl_element, IMFCPL * cpl);

int fill_edit_rate(xmlNodePtr cpl_element, IMFCPL * cpl)
{
    int ret = 0;
    xmlNodePtr element = NULL;
    xmlChar *edit_rate_text = NULL;

    element =
        getChildElementByName(cpl_element, "EditRate");

    if (! element) {
        ret = 1;
        goto cleanup;
    }

    ret = readRational(element, &cpl->edit_rate);

  cleanup:
    if (edit_rate_text)
        xmlFree(edit_rate_text);

    return ret;
}

int fill_id(xmlNodePtr cpl_element, IMFCPL * cpl);

int fill_id(xmlNodePtr cpl_element, IMFCPL * cpl)
{
    xmlNodePtr element = NULL;

    element = getChildElementByName(cpl_element, "Id");

    if (! element)
        return 1;

    return readUUID(element, cpl->id_uuid);
}

int fill_marker(xmlNodePtr marker_elem, IMFMarker * marker);

int fill_marker(xmlNodePtr marker_elem, IMFMarker * marker)
{
    xmlNodePtr element = NULL;
    int ret = 0;

    /* read Offset */

    element = getChildElementByName(marker_elem, "Offset");

    if (element) {

        ret = readULong(element, &marker->offset);

        if (ret) return ret;

    } else {

        return 1;
    }

    /* read Label and Scope */

    element = getChildElementByName(marker_elem, "Label");

    if (element) {

        marker->label_utf8 =
            xmlNodeListGetString(element->doc, element->xmlChildrenNode, 1);

        if (! marker->label_utf8) return 1;

        marker->scope_utf8 = xmlGetNoNsProp(element, "scope");

        if (! marker->scope_utf8) {
            marker->scope_utf8 = xmlCharStrdup("http://www.smpte-ra.org/schemas/2067-3/2013#standard-markers");
        }

    } else {

        return 1;
    }

    return ret;    
}

int fill_marker_resource(xmlNodePtr marker_resource_elem, IMFMarkerResource * marker_resource, IMFCPL * cpl);

int fill_marker_resource(xmlNodePtr marker_resource_elem, IMFMarkerResource * marker_resource, IMFCPL * cpl)
{
    xmlNodePtr element = NULL;
    int ret = 0;

    /* read EditRate */

    element = getChildElementByName(marker_resource_elem, "EditRate");

    if (! element) {

        marker_resource->base.edit_rate = cpl->edit_rate;

    } else {

        ret = readRational(element, &marker_resource->base.edit_rate);

        if (ret) return ret;
    }

    /* read EntryPoint */

    element = getChildElementByName(marker_resource_elem, "EntryPoint");

    if (element) {

        ret = readULong(element, &marker_resource->base.entry_point);

        if (ret) return ret;

    } else {

        marker_resource->base.entry_point = 0;

    }

    /* read IntrinsicDuration */

    element = getChildElementByName(marker_resource_elem, "IntrinsicDuration");

    if (element) {

        ret = readULong(element, &marker_resource->base.duration);
        
        if (ret) return ret;

        marker_resource->base.duration = marker_resource->base.duration - marker_resource->base.entry_point;

    } else {

        return 1;

    }

    /* read SourceDuration */

    element = getChildElementByName(marker_resource_elem, "SourceDuration");

    if (element) {

        ret = readULong(element, &marker_resource->base.duration);
        
        if (ret) return ret;

    }

    /* read RepeatCount */

    element = getChildElementByName(marker_resource_elem, "RepeatCount");

    if (element) {

        ret = readULong(element, &marker_resource->base.repeat_count);
        
        if (ret) return ret;

    }

    /* read markers */

    element = xmlFirstElementChild(marker_resource_elem);

    while (element != NULL)
    {

        if (xmlStrcmp(element->name, "Marker") != 0) continue;

        marker_resource->markers = realloc(
            marker_resource->markers,
            (++marker_resource->marker_count) * sizeof(IMFMarker)
        );

        assert(marker_resource->markers);

        fill_marker(element, &marker_resource->markers[marker_resource->marker_count - 1]);

        element = xmlNextElementSibling(element);
    }

    return 0;
}

int push_marker_sequence(xmlNodePtr marker_sequence_elem, IMFCPL * cpl);

int push_marker_sequence(xmlNodePtr marker_sequence_elem, IMFCPL * cpl) {
    int ret;
    uint8_t uuid[16];
    xmlNodePtr resource_list_elem = NULL;
    xmlNodePtr resource_elem = NULL;
    xmlNodePtr track_id_elem = NULL;

    track_id_elem = getChildElementByName(marker_sequence_elem, "TrackId");

    if (track_id_elem == NULL) {
        
        /* malformed sequence */

        return 1;
    }

    ret = readUUID(track_id_elem, uuid);

    if (ret) return ret;

    if (cpl->main_markers_track == NULL) {

        cpl->main_markers_track = calloc(1, sizeof(IMFMarkerVirtualTrack));
        assert(cpl->main_markers_track);
        memcpy(cpl->main_markers_track->base.id_uuid, uuid, sizeof(uuid));

    } else if (memcmp(cpl->main_markers_track->base.id_uuid, uuid, sizeof(uuid)) != 0) {

        /* multiple marker tracks */

        return 1;

    }

    /* process resources */

    resource_list_elem = getChildElementByName(marker_sequence_elem, "ResourceList");

    if (resource_list_elem == NULL) return 0;

    resource_elem = xmlFirstElementChild(resource_list_elem);

    while (resource_elem != NULL)
    {

        cpl->main_markers_track->resources = realloc(
            cpl->main_markers_track->resources,
            (cpl->main_markers_track->resource_count + 1) * sizeof(IMFMarkerResource)
        );

        assert(cpl->main_markers_track->resources);

        cpl->main_markers_track->resource_count++;

        fill_marker_resource(
            resource_elem,
            &cpl->main_markers_track->resources[cpl->main_markers_track->resource_count - 1],
            cpl
            );
                
        resource_elem = xmlNextElementSibling(resource_elem);
    }

    return 0;
}

int fill_virtual_tracks(xmlNodePtr cpl_element, IMFCPL * cpl);

int fill_virtual_tracks(xmlNodePtr cpl_element, IMFCPL * cpl)
{
    int ret = 0;
    xmlNodePtr segment_list_elem = NULL;
    xmlNodePtr segment_elem = NULL;
    xmlNodePtr sequence_list_elem = NULL;
    xmlNodePtr sequence_elem = NULL;

    segment_list_elem = getChildElementByName(cpl_element, "SegmentList");

    if (! segment_list_elem) return 1;

    segment_elem = xmlFirstElementChild(segment_list_elem);

    while (segment_elem != NULL)
    {

        sequence_list_elem = getChildElementByName(segment_elem, "SequenceList");

        if (! segment_list_elem) continue;

        sequence_elem = xmlFirstElementChild(sequence_list_elem);

        while (sequence_elem != NULL)
        {

            /* TODO: compare namespaces */

            if (xmlStrcmp(sequence_elem->name, "MarkerSequence") == 0) {
                
                push_marker_sequence(sequence_elem, cpl);

            }
                    
            sequence_elem = xmlNextElementSibling(sequence_elem);
        }

        segment_elem = xmlNextElementSibling(segment_elem);
    }

    return ret;
}


int parse_imf_cpl_from_xml_dom(xmlDocPtr doc, IMFCPL ** cpl)
{
    int ret = 0;
    xmlNodePtr cpl_element = NULL;

    *cpl = imf_cpl_new();

    if (*cpl == NULL) {
        ret = AVERROR_BUG;
        goto cleanup;
    }

    cpl_element = xmlDocGetRootElement(doc);

    if (cpl_element->ns == NULL) {
        ret = AVERROR_BUG;
        goto cleanup;
    }

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

  cleanup:
    if (*cpl && ret)
        imf_cpl_delete(*cpl);

    return ret;
}

void imf_cpl_delete(IMFCPL * cpl)
{
    if (cpl) {
        xmlFree(BAD_CAST cpl->content_title_utf8);
    }

    free(cpl);
}
