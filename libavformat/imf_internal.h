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
 * IMF unittest
 */

#ifndef AVFORMAT_IMF_INTERNAL_H
#define AVFORMAT_IMF_INTERNAL_H

#include "libavformat/avio.h"
#include "libavutil/rational.h"
#include <libxml/tree.h>

#define UUID_FORMAT "urn:uuid:%02hhx%02hhx%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"

/**
 * IMF Asset locator
 */
typedef struct IMFAssetLocator {
    UUID uuid;
    const char *absolute_uri;
    AVFormatContext *ctx;
} IMFAssetLocator;

/**
 * IMF Asset Map
 */
typedef struct IMFAssetMap {
    uint8_t asset_count;
    IMFAssetLocator **assets;
} IMFAssetMap;

int xml_read_ulong(xmlNodePtr element, unsigned long *number);

int xml_read_rational(xmlNodePtr element, AVRational *rational);

int xml_read_UUID(xmlNodePtr element, uint8_t uuid[16]);

xmlNodePtr xml_get_child_element_by_name(xmlNodePtr parent, const char *name_utf8);

/**
 * Parse a ASSETMAP XML file to extract the UUID-URI mapping of assets.
 * @param s the current format context, if any (can be NULL).
 * @param doc the XML document to be parsed.
 * @param asset_map pointer on the IMFAssetMap pointer to fill.
 * @param base_url the url of the asset map XML file, if any (can be NULL).
 * @return a negative value in case of error, 0 otherwise.
 */
int parse_imf_asset_map_from_xml_dom(AVFormatContext *s, xmlDocPtr doc, IMFAssetMap **asset_map, const char *base_url);

/**
 * Allocate a IMFAssetMap pointer and return it.
 * @return the allocated IMFAssetMap pointer.
 */
IMFAssetMap *imf_asset_map_alloc(void);

/**
 * Free a IMFAssetMap pointer.
 */
void imf_asset_map_free(IMFAssetMap *asset_map);

#endif
