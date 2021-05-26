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
    int scanf_ret;

    element =
        getChildElementByName(cpl_element, "EditRate");

    if (! element) {
        ret = 1;
        goto cleanup;
    }

    edit_rate_text =
        xmlNodeListGetString(element->doc, element->xmlChildrenNode, 1);

    scanf_ret = sscanf(edit_rate_text, "%i %i", &cpl->edit_rate.num, &cpl->edit_rate.den);

    if (scanf_ret != 2) {
        ret = 1;
        goto cleanup;
    }

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

int push_marker_sequence(xmlNodePtr marker_sequence_elem, IMFCPL * cpl);

int push_marker_sequence(xmlNodePtr marker_sequence_elem, IMFCPL * cpl) {
    int ret;
    uint8_t uuid[16];

    ret = readUUID(marker_sequence_elem, uuid);

    if (ret) return ret;

    if (cpl->main_markers_track == NULL) {
        cpl->main_markers_track = malloc(sizeof(IMFMarkerVirtualTrack));
        assert(cpl->main_markers_track);

        memcpy(cpl->main_markers_track->base.id_uuid, uuid, sizeof(uuid));
    } else {
        
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
