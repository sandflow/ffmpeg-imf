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


IMFCPL* imf_cpl_new(void) {
    return calloc(1, sizeof(IMFCPL));
}


int parse_imf_cpl_from_xml_dom(xmlDocPtr doc, IMFCPL** cpl) {
    int ret = 0;

    xmlNodePtr root_element = NULL;

    xmlXPathContextPtr xpath_context = NULL;

	xmlXPathObjectPtr xpath_result = NULL;

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

	xpath_result = xmlXPathEvalExpression("/cpl:CompositionPlaylist/cpl:ContentTitle", xpath_context);

	if(xmlXPathNodeSetGetLength(xpath_result->nodesetval) != 1) {
        ret = AVERROR_BUG;
		goto cleanup;
    }

    

cleanup:
    if (*cpl && ret) imf_cpl_delete(*cpl);

    if (xpath_result) xmlXPathFreeObject(xpath_result);

    return ret;
}

void imf_cpl_delete(IMFCPL* cpl) {
    free(cpl);
}