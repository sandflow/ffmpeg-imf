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


const char* UUID_FMT_STR = "urn:uuid:%2hhx%2hhx%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx";
 

IMFCPL *imf_cpl_new(void)
{
    return calloc(1, sizeof(IMFCPL));
}

int fill_content_title(xmlXPathContextPtr ctx, IMFCPL * cpl);

int fill_content_title(xmlXPathContextPtr ctx, IMFCPL * cpl)
{
    int ret = 0;

    xmlXPathObjectPtr xpath_result = NULL;

    xpath_result =
        xmlXPathEvalExpression("/cpl:CompositionPlaylist/cpl:ContentTitle",
                               ctx);

    if (xmlXPathNodeSetGetLength(xpath_result->nodesetval) != 1) {
        ret = 1;
        goto cleanup;
    }

    cpl->content_title_utf8 =
        xmlNodeListGetString(ctx->doc,
                             xpath_result->nodesetval->
                             nodeTab[0]->xmlChildrenNode, 1);

  cleanup:
    if (xpath_result)
        xmlXPathFreeObject(xpath_result);

    return ret;
}

int fill_id(xmlXPathContextPtr ctx, IMFCPL * cpl);

int fill_id(xmlXPathContextPtr ctx, IMFCPL * cpl)
{
    int ret = 0;
    xmlXPathObjectPtr xpath_result = NULL;
    xmlChar* urn_text = NULL;
    int scanf_ret;

    xpath_result =
        xmlXPathEvalExpression("/cpl:CompositionPlaylist/cpl:Id",
                               ctx);

    if (xmlXPathNodeSetGetLength(xpath_result->nodesetval) != 1) {
        ret = 1;
        goto cleanup;
    }

    urn_text =
        xmlNodeListGetString(ctx->doc,
                             xpath_result->nodesetval->
                             nodeTab[0]->xmlChildrenNode, 1);

    scanf_ret =  sscanf(
        urn_text,
        UUID_FMT_STR,
        &cpl->id_uuid[0],
        &cpl->id_uuid[1],
        &cpl->id_uuid[2],
        &cpl->id_uuid[3],
        &cpl->id_uuid[4],
        &cpl->id_uuid[5],
        &cpl->id_uuid[6],
        &cpl->id_uuid[7],
        &cpl->id_uuid[8],
        &cpl->id_uuid[9],
        &cpl->id_uuid[10],
        &cpl->id_uuid[11],
        &cpl->id_uuid[12],
        &cpl->id_uuid[13],
        &cpl->id_uuid[14],
        &cpl->id_uuid[15]
    );
    
    if (scanf_ret != 16) {
        ret = 1;
        goto cleanup;
    }

  cleanup:
    if (xpath_result)
        xmlXPathFreeObject(xpath_result);

    if (urn_text)
        xmlFree(urn_text);

    return ret;
}

int parse_imf_cpl_from_xml_dom(xmlDocPtr doc, IMFCPL ** cpl)
{
    int ret = 0;

    xmlNodePtr root_element = NULL;

    xmlXPathContextPtr xpath_context = NULL;

    *cpl = NULL;

    *cpl = imf_cpl_new();

    if (*cpl == NULL) {
        ret = AVERROR_BUG;
        goto cleanup;
    }

    xpath_context = xmlXPathNewContext(doc);

    root_element = xmlDocGetRootElement(doc);

    if (root_element->ns == NULL) {
        ret = AVERROR_BUG;
        goto cleanup;
    }

    xmlXPathRegisterNs(xpath_context, "cpl", root_element->ns->href);

    if (fill_content_title(xpath_context, *cpl)) {
        ret = AVERROR_BUG;
        goto cleanup;
    }

    if (fill_id(xpath_context, *cpl)) {
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
