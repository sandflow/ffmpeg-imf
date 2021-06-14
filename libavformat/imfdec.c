/*
 * Interoperable Master Format demuxer
 * Copyright (c) 2021 Sandflow Consulting LLC
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * Demuxes an IMF Composition
 * @file
 * @author Marc-Antoine Arnaud
 * @author Valentin Noel
 *
 * References
 $ OV 2067-0:2018 - SMPTE Overview Document - Interoperable Master Format
 * ST 2067-2:2020 - SMPTE Standard - Interoperable Master Format — Core Constraints
 * ST 2067-3:2020 - SMPTE Standard - Interoperable Master Format — Composition Playlist
 * ST 2067-5:2020 - SMPTE Standard - Interoperable Master Format — Essence Component
 * ST 2067-20:2016 - SMPTE Standard - Interoperable Master Format — Application #2
 * ST 2067-21:2020 - SMPTE Standard - Interoperable Master Format — Application #2 Extended
 * ST 2067-102:2017 - SMPTE Standard - Interoperable Master Format — Common Image Pixel Color Schemes
 * ST 429-9:2007 - SMPTE Standard - D-Cinema Packaging — Asset Mapping and File Segmentation
 */

#include "imf.h"
#include "imf_internal.h"
#include "internal.h"
#include "libavutil/opt.h"
#include "mxf.h"
#include <libxml/parser.h>

#define MAX_BPRINT_READ_SIZE (UINT_MAX - 1)
#define DEFAULT_ASSETMAP_SIZE 8 * 1024

typedef struct IMFContext {
    const AVClass *class;
    const char *base_url;
    AVIOInterruptCB *interrupt_callback;
    AVDictionary *avio_opts;
    IMFAssetMap *asset_map;
} IMFContext;

int parse_imf_asset_map_from_xml_dom(AVFormatContext *s, xmlDocPtr doc, IMFAssetMap **asset_map, const char *base_url) {
    xmlNodePtr asset_map_element = NULL;
    xmlNodePtr node = NULL;
    char *uri;

    IMFAssetLocator *asset = NULL;

    asset_map_element = xmlDocGetRootElement(doc);

    if (!asset_map_element) {
        av_log(s, AV_LOG_ERROR, "Unable to parse asset map XML - missing root node\n");
        return AVERROR_INVALIDDATA;
    }

    if (asset_map_element->type != XML_ELEMENT_NODE || av_strcasecmp(asset_map_element->name, "AssetMap")) {
        av_log(s, AV_LOG_ERROR, "Unable to parse asset map XML - wrong root node name[%s] type[%d]\n", asset_map_element->name, (int)asset_map_element->type);
        return AVERROR_INVALIDDATA;
    }

    // parse asset locators

    if (!(node = xml_get_child_element_by_name(asset_map_element, "AssetList"))) {
        av_log(s, AV_LOG_ERROR, "Unable to parse asset map XML - missing AssetList node\n");
        return AVERROR_INVALIDDATA;
    }

    node = xmlFirstElementChild(node);
    while (node) {
        if (av_strcasecmp(node->name, "Asset") != 0) {
            continue;
        }

        asset = av_malloc(sizeof(IMFAssetLocator));

        if (xml_read_UUID(xml_get_child_element_by_name(node, "Id"), asset->uuid)) {
            av_log(s, AV_LOG_ERROR, "Could not parse UUID from asset in asset map.\n");
            av_freep(&asset);
            return 1;
        }

        av_log(s, AV_LOG_DEBUG, "Found asset id: " UUID_FORMAT "\n", UID_ARG(asset->uuid));

        if (!(node = xml_get_child_element_by_name(node, "ChunkList"))) {
            av_log(s, AV_LOG_ERROR, "Unable to parse asset map XML - missing ChunkList node\n");
            return AVERROR_INVALIDDATA;
        }

        if (!(node = xml_get_child_element_by_name(node, "Chunk"))) {
            av_log(s, AV_LOG_ERROR, "Unable to parse asset map XML - missing Chunk node\n");
            return AVERROR_INVALIDDATA;
        }

        uri = xmlNodeGetContent(xml_get_child_element_by_name(node, "Path"));
        uri = av_append_path_component(base_url, uri);
        asset->absolute_uri = strdup(uri);
        av_free(uri);

        av_log(s, AV_LOG_DEBUG, "Found asset absolute URI: %s\n", asset->absolute_uri);

        node = xmlNextElementSibling(node->parent->parent);

        (*asset_map)->assets = av_realloc((*asset_map)->assets, (*asset_map)->asset_count + 1 * sizeof(IMFAssetLocator));
        (*asset_map)->assets[(*asset_map)->asset_count++] = asset;
    }

    return 0;
}

IMFAssetMap *imf_asset_map_alloc(void) {
    IMFAssetMap *asset_map;

    asset_map = av_malloc(sizeof(IMFAssetMap));
    if (!asset_map)
        return NULL;

    asset_map->assets = NULL;
    asset_map->asset_count = 0;
    return asset_map;
}

void imf_asset_map_free(IMFAssetMap *asset_map) {
    if (asset_map == NULL) {
        return;
    }

    for (int i = 0; i < asset_map->asset_count; ++i) {
        av_free(asset_map->assets[i]);
    }

    av_freep(&asset_map->assets);
    av_freep(&asset_map);
}

static int parse_assetmap(AVFormatContext *s, const char *url, AVIOContext *in) {
    IMFContext *c = s->priv_data;
    AVBPrint buf;
    AVDictionary *opts = NULL;
    xmlDoc *doc = NULL;

    int close_in = 0;
    int ret = 0;
    int64_t filesize = 0;

    c->base_url = av_dirname(strdup(url));
    c->asset_map = imf_asset_map_alloc();
    if (!c->asset_map) {
        av_log(s, AV_LOG_ERROR, "Unable to allocate asset map locator\n");
        return AVERROR_BUG;
    }

    av_log(s, AV_LOG_DEBUG, "Asset Map URL: %s\n", url);

    if (!in) {
        close_in = 1;

        av_dict_copy(&opts, c->avio_opts, 0);
        ret = avio_open2(&in, url, AVIO_FLAG_READ, c->interrupt_callback, &opts);
        av_dict_free(&opts);
        if (ret < 0)
            return ret;
    }

    filesize = avio_size(in);
    filesize = filesize > 0 ? filesize : DEFAULT_ASSETMAP_SIZE;

    av_bprint_init(&buf, filesize + 1, AV_BPRINT_SIZE_UNLIMITED);

    if ((ret = avio_read_to_bprint(in, &buf, MAX_BPRINT_READ_SIZE)) < 0 || !avio_feof(in) || (filesize = buf.len) == 0) {
        av_log(s, AV_LOG_ERROR, "Unable to read to asset map '%s'\n", url);
        if (ret == 0)
            ret = AVERROR_INVALIDDATA;
    } else {
        LIBXML_TEST_VERSION

        doc = xmlReadMemory(buf.str, filesize, url, NULL, 0);

        ret = parse_imf_asset_map_from_xml_dom(s, doc, &c->asset_map, c->base_url);
        if (ret != 0) {
            goto cleanup;
        }

        av_log(s, AV_LOG_DEBUG, "Found %d assets from %s\n", c->asset_map->asset_count, url);

    cleanup:
        xmlFreeDoc(doc);
    }
    if (close_in) {
        avio_close(in);
    }
    return ret;
}

static int imf_close(AVFormatContext *s);

static int imf_read_header(AVFormatContext *s) {
    int ret = 0;

    av_log(s, AV_LOG_DEBUG, "start to parse IMF package\n");

    if ((ret = parse_assetmap(s, s->url, s->pb)) < 0)
        goto fail;

    av_log(s, AV_LOG_DEBUG, "parsed IMF Asset Map \n");

    // av_log(s, AV_LOG_DEBUG, "parsed IMF package\n");
    return 0;

fail:
    imf_close(s);
    return ret;
}

static int ff_imf_read_packet(AVFormatContext *s, AVPacket *pkt) {
    return AVERROR_EOF;
}

static int imf_close(AVFormatContext *s) {
    IMFContext *c = s->priv_data;

    av_log(s, AV_LOG_DEBUG, "Close IMF package\n");
    av_dict_free(&c->avio_opts);
    av_freep(&c->base_url);
    imf_asset_map_free(c->asset_map);
    return 0;
}

static const AVOption imf_options[] = {
    {NULL}};

static const AVClass imf_class = {
    .class_name = "imf",
    .item_name = av_default_item_name,
    .option = imf_options,
    .version = LIBAVUTIL_VERSION_INT,
};

const AVInputFormat ff_imf_demuxer = {
    .name = "imf",
    .long_name = NULL_IF_CONFIG_SMALL("IMF (Interoperable Master Format)"),
    .priv_class = &imf_class,
    .priv_data_size = sizeof(IMFContext),
    .read_header = imf_read_header,
    .read_packet = ff_imf_read_packet,
    .read_close = imf_close,
    .extensions = "xml",
};
