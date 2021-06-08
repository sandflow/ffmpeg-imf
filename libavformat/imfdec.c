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
 *
 * References
 $ OV 2067-0:2018 - SMPTE Overview Document - Interoperable Master Format
 * ST 2067-2:2020 - SMPTE Standard - Interoperable Master Format — Core Constraints
 * ST 2067-3:2020 - SMPTE Standard - Interoperable Master Format — Composition Playlist
 * ST 2067-5:2020 - SMPTE Standard - Interoperable Master Format — Essence Component
 * ST 2067-20:2016 - SMPTE Standard - Interoperable Master Format — Application #2
 * ST 2067-21:2020 - SMPTE Standard - Interoperable Master Format — Application #2 Extended
 * ST 2067-102:2017 - SMPTE Standard - Interoperable Master Format — Common Image Pixel Color Schemes
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
    char *base_url;
    AVIOInterruptCB *interrupt_callback;
    AVDictionary *avio_opts;
    IMFAssetMapLocator *asset_map_locator;
} IMFContext;

int parse_imf_asset_map_from_xml_dom(AVFormatContext *s, xmlDocPtr doc, IMFAssetMapLocator **asset_map_locator) {
    xmlNodePtr asset_map_element = NULL;
    xmlNodePtr node = NULL;
    char *path;

    IMFAssetLocator *asset_locator = NULL;

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

        asset_locator = av_malloc(sizeof(IMFAssetLocator));

        if (xml_read_UUID(xml_get_child_element_by_name(node, "Id"), asset_locator->uuid)) {
            av_log(s, AV_LOG_ERROR, "Could not parse UUID from asset in ASSETMAP.\n");
            av_freep(asset_locator);
            return 1;
        }

        av_log(s, AV_LOG_DEBUG, "Found asset id: urn:uuid:%2hhx%2hhx%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx\n", UID_ARG(asset_locator->uuid));

        if (!(node = xml_get_child_element_by_name(node, "ChunkList"))) {
            av_log(s, AV_LOG_ERROR, "Unable to parse asset map XML - missing ChunkList node\n");
            return AVERROR_INVALIDDATA;
        }

        if (!(node = xml_get_child_element_by_name(node, "Chunk"))) {
            av_log(s, AV_LOG_ERROR, "Unable to parse asset map XML - missing Chunk node\n");
            return AVERROR_INVALIDDATA;
        }

        path = xmlNodeGetContent(xml_get_child_element_by_name(node, "Path"));

        if (av_strstart(path, "CPL_", NULL)) {
            asset_locator->asset_type = AV_IMF_ASSET_TYPE_CPL;
        } else if (av_strstart(path, "PKL_", NULL)) {
            asset_locator->asset_type = AV_IMF_ASSET_TYPE_PKL;
        } else {
            asset_locator->asset_type = AV_IMF_ASSET_TYPE_MEDIA;
        }

        path = av_append_path_component((*asset_map_locator)->root_url, path);
        asset_locator->path = strdup(path);
        av_free(path);

        av_log(s, AV_LOG_DEBUG, "Found asset path: %s (type=%d)\n", asset_locator->path, asset_locator->asset_type);

        node = xmlNextElementSibling(node->parent->parent);

        (*asset_map_locator)->assets[(*asset_map_locator)->assets_count] = asset_locator;
        (*asset_map_locator)->assets_count++;
    }

    return 0;
}

IMFAssetMapLocator *imf_asset_map_locator_alloc(void) {
    IMFAssetMapLocator *asset_map_locator;

    asset_map_locator = av_malloc(sizeof(IMFAssetMapLocator));
    if (!asset_map_locator)
        return NULL;

    asset_map_locator->root_url = "";
    asset_map_locator->assets_count = 0;
    return asset_map_locator;
}

void imf_asset_map_locator_free(IMFAssetMapLocator *asset_map_locator) {
    if (asset_map_locator == NULL) {
        return;
    }

    asset_map_locator->root_url = NULL;
    av_freep(asset_map_locator);
}

static int parse_assetmap(AVFormatContext *s, const char *url, AVIOContext *in) {
    IMFContext *c = s->priv_data;
    AVBPrint buf;
    AVDictionary *opts = NULL;
    xmlDoc *doc = NULL;

    int close_in = 0;
    int ret = 0;
    int64_t filesize = 0;

    c->asset_map_locator = imf_asset_map_locator_alloc();
    if (!c->asset_map_locator) {
        av_log(s, AV_LOG_ERROR, "Unable to allocate asset map locator\n");
        return AVERROR_BUG;
    }

    c->asset_map_locator->root_url = av_dirname((char *)url);
    av_log(s, AV_LOG_DEBUG, "Asset Map root URL: %s\n", c->asset_map_locator->root_url);

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
        av_log(s, AV_LOG_ERROR, "Unable to read to assetmap '%s'\n", url);
        if (ret == 0)
            ret = AVERROR_INVALIDDATA;
    } else {
        LIBXML_TEST_VERSION

        doc = xmlReadMemory(buf.str, filesize, c->base_url, NULL, 0);

        ret = parse_imf_asset_map_from_xml_dom(s, doc, &c->asset_map_locator);
        if (ret != 0) {
            goto cleanup;
        }

        av_log(s, AV_LOG_DEBUG, "Found %d assets from %s\n", c->asset_map_locator->assets_count, url);

    cleanup:
        /*free the document */
        xmlFreeDoc(doc);
        // FIXME:"double free or corruption" error
        // xmlCleanupParser();
    }
    if (close_in) {
        avio_close(in);
    }
    return ret;
}

static int open_asset_streams(AVFormatContext *s) {

    IMFContext *c = s->priv_data;
    AVStream *imf_stream;
    AVFormatContext *asset_format_context;
    AVStream *asset_stream;

    IMFAssetLocator *asset_locator;
    int ret = 0;

    for (int i = 0; i < c->asset_map_locator->assets_count; ++i) {
        asset_locator = c->asset_map_locator->assets[i];
        av_log(s, AV_LOG_DEBUG, "Open asset %s: %s (type=%d)\n", asset_locator->uuid, asset_locator->path, asset_locator->asset_type);

        if (asset_locator->asset_type == AV_IMF_ASSET_TYPE_CPL) {
            av_log(s, AV_LOG_DEBUG, "Skip CPL %s\n", asset_locator->path);
            continue;
        }

        if (asset_locator->asset_type == AV_IMF_ASSET_TYPE_PKL) {
            av_log(s, AV_LOG_DEBUG, "Skip PKL %s\n", asset_locator->path);
            continue;
        }

        asset_format_context = avformat_alloc_context();
        ret = avformat_open_input(&asset_format_context, asset_locator->path, NULL, NULL);
        if (ret < 0) {
            goto cleanup;
        }

        for (int j = 0; j < asset_format_context->nb_streams; j++) {
            av_log(s, AV_LOG_DEBUG, "Create new stream from file %s, stream=%d\n", asset_locator->path, j);
            imf_stream = avformat_new_stream(s, NULL);
            asset_stream = asset_format_context->streams[j];
            if (!imf_stream) {
                ret = AVERROR(ENOMEM);
                goto cleanup;
            }
            imf_stream->id = j;
            avformat_find_stream_info(asset_format_context, NULL);
            avcodec_parameters_copy(imf_stream->codecpar, asset_stream->codecpar);
            avpriv_set_pts_info(imf_stream, asset_stream->pts_wrap_bits, asset_stream->time_base.num, asset_stream->time_base.den);
        }

    cleanup:
        avformat_free_context(asset_format_context);
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

    if ((ret = open_asset_streams(s)) < 0)
        goto fail;

    av_log(s, AV_LOG_DEBUG, "parsed IMF package\n");
    return 0;

fail:
    imf_close(s);
    return ret;
}

static int ff_imf_read_packet(AVFormatContext *s, AVPacket *pkt) {
    return AVERROR_EOF;
}

static int imf_close(AVFormatContext *s) {
    av_log(s, AV_LOG_DEBUG, "Close IMF package\n");
    IMFContext *c = s->priv_data;
    av_dict_free(&c->avio_opts);
    av_freep(&c->base_url);
    imf_asset_map_locator_free(c->asset_map_locator);
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
