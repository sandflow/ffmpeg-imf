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
#include "url.h"
#include <libxml/parser.h>

#define MAX_BPRINT_READ_SIZE (UINT_MAX - 1)
#define DEFAULT_ASSETMAP_SIZE 8 * 1024

typedef struct IMFTrack {
    // IMF playlist context
    AVFormatContext *parent;
    // Track context
    AVFormatContext *ctx;
    // Track index in playlist
    int32_t index;
    // Reading timestamp
    int64_t current_timestamp;
} IMFTrack;

typedef struct IMFContext {
    const AVClass *class;
    const char *base_url;
    char *asset_map_path;
    AVIOInterruptCB *interrupt_callback;
    AVDictionary *avio_opts;
    IMFCPL *cpl;
    IMFAssetMap *asset_map;
    unsigned int track_count;
    IMFTrack **tracks;
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

static IMFAssetLocator *find_asset_map_locator(IMFAssetMap *asset_map, UUID uuid) {
    IMFAssetLocator *asset_locator;
    for (int i = 0; i < asset_map->asset_count; ++i) {
        asset_locator = asset_map->assets[i];
        if (memcmp(asset_map->assets[i]->uuid, uuid, 16) == 0) {
            return asset_locator;
        }
    }
    return NULL;
}

static int open_track_file_resource(AVFormatContext *s, IMFTrackFileResource *track_file_resource, int32_t track_index) {
    IMFContext *c = s->priv_data;
    IMFTrack *track;
    int ret = 0;

    IMFAssetLocator *asset_locator;
    AVStream *asset_stream;

    if (!(asset_locator = find_asset_map_locator(c->asset_map, track_file_resource->track_file_uuid))) {
        av_log(s, AV_LOG_ERROR, "Could not find asset locator for UUID: " UUID_FORMAT "\n", UID_ARG(track_file_resource->track_file_uuid));
        return AVERROR_INVALIDDATA;
    }

    av_log(s, AV_LOG_DEBUG, "Found locator for " UUID_FORMAT ": %s\n", UID_ARG(asset_locator->uuid), asset_locator->absolute_uri);

    track = av_mallocz(sizeof(IMFTrack));
    track->parent = s;
    track->ctx = avformat_alloc_context();
    track->index = track_index;

    ret = avformat_open_input(&track->ctx, asset_locator->absolute_uri, NULL, NULL);
    if (ret < 0) {
        goto cleanup;
    }

    ret = avformat_find_stream_info(track->ctx, NULL);
    if (ret < 0) {
        av_log(s, AV_LOG_ERROR, "Could not find stream information\n");
        goto cleanup;
    }

    for (int stream_index = 0; stream_index < track->ctx->nb_streams; stream_index++) {
        av_log(s, AV_LOG_DEBUG, "Open stream from file %s, stream %d\n", track->ctx->url, stream_index);

        // Copy stream information
        asset_stream = avformat_new_stream(s, NULL);
        asset_stream->id = stream_index;
        avcodec_parameters_copy(asset_stream->codecpar, track->ctx->streams[stream_index]->codecpar);
        avpriv_set_pts_info(asset_stream, track->ctx->streams[stream_index]->pts_wrap_bits, track->ctx->streams[stream_index]->time_base.num, track->ctx->streams[stream_index]->time_base.den);

        c->tracks[c->track_count++] = track;
    }

    return ret;

cleanup:
    avformat_free_context(track->ctx);
    av_freep(&track);

    return ret;
}

static int64_t get_minimum_track_timestamp(IMFContext *c) {
    int64_t minimum_timestamp = INT64_MAX;
    for (int i = 0; i < c->track_count; ++i) {
        if (c->tracks[i]->current_timestamp < minimum_timestamp) {
            minimum_timestamp = c->tracks[i]->current_timestamp;
        }
    }
    return minimum_timestamp;
}

static int open_cpl_tracks(AVFormatContext *s) {
    IMFContext *c = s->priv_data;
    IMFTrackFileVirtualTrack virtual_track;
    int32_t track_index = 0;
    int ret;

    c->track_count = 0;
    c->tracks = av_malloc_array(1, sizeof(IMFTrack *));

    if (c->cpl->main_image_2d_track) {
        // Open the first resource of the track to get stream information
        ret = open_track_file_resource(s, &c->cpl->main_image_2d_track->resources[0], track_index++);
    }

    for (int audio_track_index = 0; audio_track_index < c->cpl->main_audio_track_count; ++audio_track_index) {
        virtual_track = c->cpl->main_audio_tracks[audio_track_index];
        // Open the first resource of the track to get stream information
        ret = open_track_file_resource(s, &virtual_track.resources[0], track_index++);
    }

    return ret;
}

static int imf_close(AVFormatContext *s);

static int imf_read_header(AVFormatContext *s) {
    IMFContext *c = s->priv_data;
    int ret;

    av_log(s, AV_LOG_DEBUG, "start parsing IMF CPL: %s\n", s->url);

    if ((ret = parse_imf_cpl(s->pb, &c->cpl)) < 0)
        goto fail;

    av_log(s, AV_LOG_INFO, "parsed IMF CPL: " UUID_FORMAT "\n", UID_ARG(c->cpl->id_uuid));

    if (!c->asset_map_path) {
        c->asset_map_path = av_append_path_component(av_dirname(s->url), "ASSETMAP.xml");
    }

    av_log(s, AV_LOG_DEBUG, "start parsing IMF Asset Map: %s\n", c->asset_map_path);

    if ((ret = parse_assetmap(s, c->asset_map_path, NULL)) < 0)
        goto fail;

    av_log(s, AV_LOG_DEBUG, "parsed IMF Asset Map \n");

    if ((ret = open_cpl_tracks(s))  != 0) {
        goto fail;
    }

    av_log(s, AV_LOG_DEBUG, "parsed IMF package\n");
    return ret;

fail:
    imf_close(s);
    return ret;
}

static int ff_imf_read_packet(AVFormatContext *s, AVPacket *pkt) {
    IMFContext *c = s->priv_data;

    IMFTrack *track;
    IMFTrack *track_to_read = NULL;

    int64_t minimum_timestamp = get_minimum_track_timestamp(c);
    int ret = 0, i;

    for (i = 0; i < c->track_count; i++) {
        track = c->tracks[i];
        av_log(s, AV_LOG_DEBUG, "Compare track %p timestamp %ld to %ld\n", track, track->current_timestamp, minimum_timestamp);
        if (!track->ctx)
            continue;
        if (track->current_timestamp <= minimum_timestamp) {
            track_to_read = track;
            minimum_timestamp = track->current_timestamp;
            av_log(s, AV_LOG_DEBUG, "Found next track to read: %s (timestamp: %ld / %ld)\n", track_to_read->ctx->url, track->current_timestamp, minimum_timestamp);
            break;
        }
    }

    if (!track_to_read) {
        av_log(s, AV_LOG_ERROR, "Could not find IMF track to read\n");
        return AVERROR_STREAM_NOT_FOUND;
    }

    while (!ff_check_interrupt(c->interrupt_callback) && !ret) {
        av_log(s, AV_LOG_DEBUG, "Try av_read_frame: %d\n", track_to_read->index);
        ret = av_read_frame(track_to_read->ctx, pkt);
        av_log(s, AV_LOG_DEBUG, "av_read_frame: ret=%d, pkt->pts=%ld\n", ret, pkt->pts);
        if (ret >= 0) {
            // We got a packet, return it
            track_to_read->current_timestamp = av_rescale(pkt->pts + 1, (int64_t)track_to_read->ctx->streams[0]->time_base.num * 90000, track_to_read->ctx->streams[0]->time_base.den);
            av_log(s, AV_LOG_DEBUG, "track_to_read: index=%d, current_timestamp=%ld\n", track_to_read->index, track_to_read->current_timestamp);
            pkt->stream_index = track_to_read->index;
            return 0;
        }
    }

    return AVERROR_EOF;
}

static int imf_close(AVFormatContext *s) {
    IMFContext *c = s->priv_data;

    av_log(s, AV_LOG_DEBUG, "Close IMF package\n");
    av_dict_free(&c->avio_opts);
    av_freep(&c->base_url);
    imf_asset_map_free(c->asset_map);
    imf_cpl_free(c->cpl);

    for (int i = 0; i < c->track_count; ++i) {
        avformat_free_context(c->tracks[i]->ctx);
    }

    return 0;
}

static const AVOption imf_options[] = {
    {"assetmap", "IMF CPL-related asset map absolute path. If not specified, the CPL sibling `ASSETMAP.xml` file is used.", offsetof(IMFContext, asset_map_path), AV_OPT_TYPE_STRING, {.str = NULL}, 0, 0, AV_OPT_FLAG_DECODING_PARAM},
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
