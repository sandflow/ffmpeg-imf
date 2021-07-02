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

typedef struct IMFVirtualTrackResourcePlaybackCtx {
    IMFAssetLocator *locator;
    IMFTrackFileResource *resource;
} IMFVirtualTrackResourcePlaybackCtx;

typedef struct IMFVirtualTrackPlaybackCtx {
    // Track index in playlist
    int32_t index;
    // Time counters
    int64_t current_timestamp;
    int64_t duration;
    // Resources
    unsigned int resource_count;
    IMFVirtualTrackResourcePlaybackCtx **resources;
    // Decoding cursors
    IMFVirtualTrackResourcePlaybackCtx *current_resource;
    int64_t last_pts;
} IMFVirtualTrackPlaybackCtx;

typedef struct IMFContext {
    const AVClass *class;
    const char *base_url;
    char *asset_map_paths;
    AVIOInterruptCB *interrupt_callback;
    AVDictionary *avio_opts;
    IMFCPL *cpl;
    IMFAssetLocatorMap *asset_locator_map;
    unsigned int track_count;
    IMFVirtualTrackPlaybackCtx **tracks;
} IMFContext;

int parse_imf_asset_map_from_xml_dom(AVFormatContext *s, xmlDocPtr doc, IMFAssetLocatorMap **asset_map, const char *base_url) {
    xmlNodePtr asset_map_element = NULL;
    xmlNodePtr node = NULL;
    char *uri;
    int ret = 0;

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
        asset->ctx = NULL;

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

        (*asset_map)->assets = av_realloc((*asset_map)->assets, ((*asset_map)->asset_count + 1) * sizeof(IMFAssetLocator));
        (*asset_map)->assets[(*asset_map)->asset_count++] = asset;
    }

    return ret;
}

IMFAssetLocatorMap *imf_asset_locator_map_alloc(void) {
    IMFAssetLocatorMap *asset_map;

    asset_map = av_malloc(sizeof(IMFAssetLocatorMap));
    if (!asset_map)
        return NULL;

    asset_map->assets = NULL;
    asset_map->asset_count = 0;
    return asset_map;
}

void imf_asset_locator_map_free(IMFAssetLocatorMap *asset_map) {
    if (asset_map == NULL) {
        return;
    }

    for (int i = 0; i < asset_map->asset_count; ++i) {
        avformat_free_context(asset_map->assets[i]->ctx);
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

    const char *base_url = av_dirname(strdup(url));
    if (c->asset_locator_map == NULL) {
        c->asset_locator_map = imf_asset_locator_map_alloc();
        if (!c->asset_locator_map) {
            av_log(s, AV_LOG_ERROR, "Unable to allocate asset map locator\n");
            return AVERROR_BUG;
        }
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

        ret = parse_imf_asset_map_from_xml_dom(s, doc, &c->asset_locator_map, base_url);
        if (ret != 0) {
            goto cleanup;
        }

        av_log(s, AV_LOG_DEBUG, "Found %d assets from %s\n", c->asset_locator_map->asset_count, url);

    cleanup:
        xmlFreeDoc(doc);
    }
    if (close_in) {
        avio_close(in);
    }
    return ret;
}

static IMFAssetLocator *find_asset_map_locator(IMFAssetLocatorMap *asset_map, UUID uuid) {
    IMFAssetLocator *asset_locator;
    for (int i = 0; i < asset_map->asset_count; ++i) {
        asset_locator = asset_map->assets[i];
        if (memcmp(asset_map->assets[i]->uuid, uuid, 16) == 0) {
            return asset_locator;
        }
    }
    return NULL;
}

static int open_resource_locator_context(AVFormatContext *s, IMFAssetLocator *asset_locator) {
    int ret = 0;

    if (asset_locator->ctx) {
        avformat_free_context(asset_locator->ctx);
    }
    asset_locator->ctx = avformat_alloc_context();

    ret = avformat_open_input(&asset_locator->ctx, asset_locator->absolute_uri, NULL, NULL);
    if (ret < 0) {
        av_log(s, AV_LOG_ERROR, "Could not open %s input context.\n", asset_locator->absolute_uri);
        goto cleanup;
    }

    ret = avformat_find_stream_info(asset_locator->ctx, NULL);
    if (ret < 0) {
        av_log(s, AV_LOG_ERROR, "Could not find %s stream information.\n", asset_locator->absolute_uri);
        goto cleanup;
    }

    return ret;
cleanup:
    avformat_free_context(asset_locator->ctx);
    av_freep(&asset_locator);

    return ret;
}

static int open_track_file_resource(AVFormatContext *s, IMFTrackFileResource *track_file_resource, IMFVirtualTrackPlaybackCtx *track) {
    IMFContext *c = s->priv_data;
    IMFVirtualTrackResourcePlaybackCtx *track_resource;
    IMFAssetLocator *asset_locator;

    int ret = 0;

    if (!(asset_locator = find_asset_map_locator(c->asset_locator_map, track_file_resource->track_file_uuid))) {
        av_log(s, AV_LOG_ERROR, "Could not find asset locator for UUID: " UUID_FORMAT "\n", UID_ARG(track_file_resource->track_file_uuid));
        return AVERROR_INVALIDDATA;
    }

    av_log(s, AV_LOG_DEBUG, "Found locator for " UUID_FORMAT ": %s\n", UID_ARG(asset_locator->uuid), asset_locator->absolute_uri);

    if ((ret = open_resource_locator_context(s, asset_locator)) != 0) {
        return ret;
    }

    track_resource = av_mallocz(sizeof(IMFVirtualTrackResourcePlaybackCtx));
    track_resource->locator = asset_locator;
    track_resource->resource = track_file_resource;

    track->resources = av_realloc(track->resources, track->resource_count + 1 * sizeof(IMFVirtualTrackResourcePlaybackCtx));
    track->resources[track->resource_count++] = track_resource;
    track->duration += track_resource->locator->ctx->duration;

    return ret;
}

static int open_track_file(AVFormatContext *s, IMFTrackFileVirtualTrack *track_file, int32_t track_index) {
    IMFContext *c = s->priv_data;
    IMFVirtualTrackPlaybackCtx *track;
    int ret = 0;

    track = av_mallocz(sizeof(IMFVirtualTrackPlaybackCtx));
    track->index = track_index;

    for (int i = 0; i < track_file->resource_count; i++) {
        av_log(s, AV_LOG_DEBUG, "Open stream from file " UUID_FORMAT ", stream %d\n", UID_ARG(track_file->resources[i].track_file_uuid), i);
        if ((ret = open_track_file_resource(s, &track_file->resources[i], track)) != 0) {
            av_log(s, AV_LOG_ERROR, "Could not open image track resource " UUID_FORMAT "\n", UID_ARG(track_file->resources[i].track_file_uuid));
            return ret;
        }
    }

    c->tracks = av_realloc(c->tracks, c->track_count + 1 * sizeof(IMFVirtualTrackPlaybackCtx));
    c->tracks[c->track_count++] = track;

    return ret;
}

static int set_context_streams_from_tracks(AVFormatContext *s) {
    IMFContext *c = s->priv_data;

    AVStream *asset_stream;
    AVStream *first_resource_stream;

    int ret = 0;

    for (int i = 0; i < c->track_count; ++i) {
        // Open the first resource of the track to get stream information
        first_resource_stream = c->tracks[i]->resources[0]->locator->ctx->streams[0];

        av_log(s, AV_LOG_DEBUG, "Open the first resource of track %d\n", c->tracks[i]->index);

        // Copy stream information
        asset_stream = avformat_new_stream(s, NULL);
        asset_stream->id = i;
        if ((ret = avcodec_parameters_copy(asset_stream->codecpar, first_resource_stream->codecpar)) < 0) {
            av_log(s, AV_LOG_ERROR, "Could not copy stream parameters\n");
            break;
        }
        avpriv_set_pts_info(asset_stream, first_resource_stream->pts_wrap_bits, first_resource_stream->time_base.num, first_resource_stream->time_base.den);
    }

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
    int32_t track_index = 0;
    int ret;

    if (c->cpl->main_image_2d_track) {
        if ((ret = open_track_file(s, c->cpl->main_image_2d_track, track_index++)) != 0) {
            av_log(s, AV_LOG_ERROR, "Could not open image track " UUID_FORMAT "\n", UID_ARG(c->cpl->main_image_2d_track->base.id_uuid));
            return ret;
        }
    }

    for (int audio_track_index = 0; audio_track_index < c->cpl->main_audio_track_count; ++audio_track_index) {
        if ((ret = open_track_file(s, &c->cpl->main_audio_tracks[audio_track_index], track_index++)) != 0) {
            av_log(s, AV_LOG_ERROR, "Could not open audio track " UUID_FORMAT "\n", UID_ARG(c->cpl->main_audio_tracks[audio_track_index].base.id_uuid));
            return ret;
        }
    }

    return set_context_streams_from_tracks(s);
}

static int imf_close(AVFormatContext *s);

static int imf_read_header(AVFormatContext *s) {
    IMFContext *c = s->priv_data;
    char *asset_map_path;
    int ret;

    c->base_url = av_dirname(av_strdup(s->url));

    av_log(s, AV_LOG_DEBUG, "start parsing IMF CPL: %s\n", s->url);

    if ((ret = parse_imf_cpl(s->pb, &c->cpl)) < 0)
        goto fail;

    av_log(s, AV_LOG_DEBUG, "parsed IMF CPL: " UUID_FORMAT "\n", UID_ARG(c->cpl->id_uuid));

    if (!c->asset_map_paths) {
        c->asset_map_paths = av_append_path_component(c->base_url, "ASSETMAP.xml");
    }

    // Parse each asset map XML file
    asset_map_path = strtok(c->asset_map_paths, ",");
    while (asset_map_path != NULL) {
        av_log(s, AV_LOG_DEBUG, "start parsing IMF Asset Map: %s\n", asset_map_path);

        if ((ret = parse_assetmap(s, asset_map_path, NULL)) < 0)
            goto fail;

        asset_map_path = strtok(NULL, ",");
    }

    av_log(s, AV_LOG_DEBUG, "parsed IMF Asset Maps\n");

    if ((ret = open_cpl_tracks(s)) != 0) {
        goto fail;
    }

    av_log(s, AV_LOG_DEBUG, "parsed IMF package\n");
    return ret;

fail:
    imf_close(s);
    return ret;
}

static IMFVirtualTrackResourcePlaybackCtx *get_resource_context_for_timestamp(AVFormatContext *s, IMFVirtualTrackPlaybackCtx *track) {
    unsigned long cumulated_duration = 0;
    unsigned long edit_unit_duration;

    av_log(s, AV_LOG_DEBUG, "Looking for track %d resource for timestamp = %ld / %ld\n", track->index, track->current_timestamp, track->duration);
    for (int i = 0; i < track->resource_count; ++i) {
        edit_unit_duration = track->resources[i]->resource->base.edit_rate.den * AV_TIME_BASE / track->resources[i]->resource->base.edit_rate.num;
        cumulated_duration += track->resources[i]->resource->base.duration * track->resources[i]->resource->base.edit_rate.den * AV_TIME_BASE / track->resources[i]->resource->base.edit_rate.num;

        if (track->current_timestamp + edit_unit_duration <= cumulated_duration) {
            av_log(s, AV_LOG_DEBUG, "Found resource %d in track %d to read for timestamp %ld (on cumulated=%ld): entry=%ld, duration=%lu, editrate=%d/%d | edit_unit_duration=%ld\n", i, track->index, track->current_timestamp, cumulated_duration, track->resources[i]->resource->base.entry_point, track->resources[i]->resource->base.duration, track->resources[i]->resource->base.edit_rate.num, track->resources[i]->resource->base.edit_rate.den, edit_unit_duration);

            if (track->current_resource != track->resources[i]) {
                av_log(s, AV_LOG_DEBUG, "Switch resource on track %d: re-open context\n", track->index);
                if (open_resource_locator_context(s, track->resources[i]->locator) != 0) {
                    return NULL;
                }
                track->current_resource = track->resources[i];
            }
            return track->current_resource;
        }
    }
    return NULL;
}

static int ff_imf_read_packet(AVFormatContext *s, AVPacket *pkt) {
    IMFContext *c = s->priv_data;

    IMFVirtualTrackPlaybackCtx *track;
    IMFVirtualTrackPlaybackCtx *track_to_read = NULL;
    IMFVirtualTrackResourcePlaybackCtx *resource_to_read = NULL;
    AVFormatContext *read_context = NULL;

    int64_t minimum_timestamp = get_minimum_track_timestamp(c);
    int ret = 0, i;

    for (i = 0; i < c->track_count; i++) {
        track = c->tracks[i];
        av_log(s, AV_LOG_DEBUG, "Compare track %p timestamp %ld to minimum %ld (over duration: %ld)\n", track, track->current_timestamp, minimum_timestamp, track->duration);
        if (track->current_timestamp <= minimum_timestamp) {
            track_to_read = track;
            minimum_timestamp = track->current_timestamp;
            av_log(s, AV_LOG_DEBUG, "Found next track to read: %d (timestamp: %ld / %ld)\n", track_to_read->index, track->current_timestamp, minimum_timestamp);
            break;
        }
    }

    if (!track_to_read) {
        av_log(s, AV_LOG_ERROR, "Could not find IMF track to read\n");
        return AVERROR_STREAM_NOT_FOUND;
    }

    if (track_to_read->current_timestamp == track_to_read->duration) {
        return AVERROR_EOF;
    }

    resource_to_read = get_resource_context_for_timestamp(s, track_to_read);

    if (!resource_to_read) {
        av_log(s, AV_LOG_ERROR, "Could not find IMF track resource to read\n");
        return AVERROR_STREAM_NOT_FOUND;
    }

    while (!ff_check_interrupt(c->interrupt_callback) && !ret) {
        read_context = resource_to_read->locator->ctx;
        ret = av_read_frame(read_context, pkt);
        av_log(s, AV_LOG_DEBUG, "Got packet: pts=%ld, dts=%ld, duration=%ld, stream_index=%d, pos=%ld,\n", pkt->pts, pkt->dts, pkt->duration, pkt->stream_index, pkt->pos);
        if (ret >= 0) {
            // Update packet info from track
            pkt->pts = track_to_read->last_pts;
            pkt->stream_index = track_to_read->index;

            // Update track cursors
            track_to_read->current_timestamp += av_rescale(pkt->duration, (int64_t)read_context->streams[0]->time_base.num * AV_TIME_BASE, read_context->streams[0]->time_base.den);
            track_to_read->last_pts += pkt->duration;

            return 0;
        } else if (ret != AVERROR_EOF) {
            av_log(s, AV_LOG_ERROR, "Could not get packet from track %d: %s\n", track_to_read->index, av_err2str(ret));
            return ret;
        }
    }

    return AVERROR_EOF;
}

static int imf_close(AVFormatContext *s) {
    IMFContext *c = s->priv_data;

    av_log(s, AV_LOG_DEBUG, "Close IMF package\n");
    av_dict_free(&c->avio_opts);
    av_freep(&c->base_url);
    imf_asset_locator_map_free(c->asset_locator_map);
    imf_cpl_free(c->cpl);

    return 0;
}

static const AVOption imf_options[] = {
    {"assetmaps", "IMF CPL-related asset map comma-separated absolute paths. If not specified, the CPL sibling `ASSETMAP.xml` file is used.", offsetof(IMFContext, asset_map_paths), AV_OPT_TYPE_STRING, {.str = NULL}, 0, 0, AV_OPT_FLAG_DECODING_PARAM},
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
