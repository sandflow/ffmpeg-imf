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
    AVFormatContext *ctx;
} IMFVirtualTrackResourcePlaybackCtx;

typedef struct IMFVirtualTrackPlaybackCtx {
    // Track index in playlist
    int32_t index;
    // Time counters
    AVRational current_timestamp;
    AVRational duration;
    // Resources
    unsigned int resource_count;
    IMFVirtualTrackResourcePlaybackCtx **resources;
    // Decoding cursors
    uint32_t current_resource_index;
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

int is_url(const char *string) {
    char *substr = strstr(string, "://");
    return substr != NULL;
}

int is_unix_absolute_path(const char *string) {
    char *substr = strstr(string, "/");
    int index = (int)(substr - string);
    return index == 0;
}

int is_dos_absolute_path(const char *string) {
    // Absolute path case: `C:\path\to\somwhere`
    char *substr = strstr(string, ":\\");
    int index = (int)(substr - string);
    if (index == 1) {
        return 1;
    }

    // Absolute path case: `C:/path/to/somwhere`
    substr = strstr(string, ":/");
    index = (int)(substr - string);
    if (index == 1) {
        return 1;
    }

    // Network path case: `\\path\to\somwhere`
    substr = strstr(string, "\\\\");
    index = (int)(substr - string);
    return index == 0;
}

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

        if (!is_url(uri) && !is_unix_absolute_path(uri) && !is_dos_absolute_path(uri)) {
            uri = av_append_path_component(base_url, uri);
        }

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

    int close_in;
    int ret;
    int64_t filesize;

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

static int open_track_resource_context(AVFormatContext *s, IMFVirtualTrackResourcePlaybackCtx *track_resource) {
    int ret = 0;
    int64_t entry_point;

    if (!track_resource->ctx) {
        track_resource->ctx = avformat_alloc_context();
    }

    if (track_resource->ctx->iformat) {
        av_log(s, AV_LOG_DEBUG, "Input context already opened for %s.\n", track_resource->locator->absolute_uri);
        return ret;
    }

    ret = avformat_open_input(&track_resource->ctx, track_resource->locator->absolute_uri, NULL, NULL);
    if (ret < 0) {
        av_log(s, AV_LOG_ERROR, "Could not open %s input context: %s\n", track_resource->locator->absolute_uri, av_err2str(ret));
        goto cleanup;
    }

    ret = avformat_find_stream_info(track_resource->ctx, NULL);
    if (ret < 0) {
        av_log(s, AV_LOG_ERROR, "Could not find %s stream information: %s\n", track_resource->locator->absolute_uri, av_err2str(ret));
        goto cleanup;
    }

    // Compare the source timebase to the resource edit rate, considering the first stream of the source file
    if (av_cmp_q(track_resource->ctx->streams[0]->time_base, av_inv_q(track_resource->resource->base.edit_rate))) {
        av_log(s, AV_LOG_WARNING, "Incoherent source stream timebase %d/%d regarding resource edit rate: %d/%d", track_resource->ctx->streams[0]->time_base.num, track_resource->ctx->streams[0]->time_base.den, track_resource->resource->base.edit_rate.den, track_resource->resource->base.edit_rate.num);
    }

    entry_point = (int64_t)track_resource->resource->base.entry_point * track_resource->resource->base.edit_rate.den * AV_TIME_BASE / track_resource->resource->base.edit_rate.num;

    if (entry_point) {
        av_log(s, AV_LOG_DEBUG, "Seek at resource %s entry point: %ld\n", track_resource->locator->absolute_uri, track_resource->resource->base.entry_point);
        ret = avformat_seek_file(track_resource->ctx, -1, entry_point, entry_point, entry_point, 0);
        if (ret < 0) {
            av_log(s, AV_LOG_ERROR, "Could not seek at %ld on %s: %s\n", entry_point, track_resource->locator->absolute_uri, av_err2str(ret));
            goto cleanup;
        }
    }

    return ret;
cleanup:
    avformat_free_context(track_resource->ctx);
    return ret;
}

static int open_track_file_resource(AVFormatContext *s, IMFTrackFileResource *track_file_resource, IMFVirtualTrackPlaybackCtx *track) {
    IMFContext *c = s->priv_data;
    IMFVirtualTrackResourcePlaybackCtx *track_resource;
    IMFAssetLocator *asset_locator;

    int ret;

    if (!(asset_locator = find_asset_map_locator(c->asset_locator_map, track_file_resource->track_file_uuid))) {
        av_log(s, AV_LOG_ERROR, "Could not find asset locator for UUID: " UUID_FORMAT "\n", UID_ARG(track_file_resource->track_file_uuid));
        return AVERROR_INVALIDDATA;
    }

    av_log(s, AV_LOG_DEBUG, "Found locator for " UUID_FORMAT ": %s\n", UID_ARG(asset_locator->uuid), asset_locator->absolute_uri);

    track_resource = av_mallocz(sizeof(IMFVirtualTrackResourcePlaybackCtx));
    track_resource->locator = asset_locator;
    track_resource->resource = track_file_resource;

    if ((ret = open_track_resource_context(s, track_resource)) != 0) {
        return ret;
    }

    for (int repetition = 0; repetition < track_file_resource->base.repeat_count; ++repetition) {
        track->resources = av_realloc(track->resources, (track->resource_count + 1) * sizeof(IMFVirtualTrackResourcePlaybackCtx));
        track->resources[track->resource_count++] = track_resource;
        track->duration = av_add_q(track->duration, av_make_q((int)track_resource->resource->base.duration * track_resource->resource->base.edit_rate.den, track_resource->resource->base.edit_rate.num));
    }

    return ret;
}

static int open_virtual_track(AVFormatContext *s, IMFTrackFileVirtualTrack *virtual_track, int32_t track_index) {
    IMFContext *c = s->priv_data;
    IMFVirtualTrackPlaybackCtx *track;
    int ret = 0;

    track = av_mallocz(sizeof(IMFVirtualTrackPlaybackCtx));
    track->index = track_index;
    track->duration = av_make_q(0, INT32_MAX);

    for (int i = 0; i < virtual_track->resource_count; i++) {
        av_log(s, AV_LOG_DEBUG, "Open stream from file " UUID_FORMAT ", stream %d\n", UID_ARG(virtual_track->resources[i].track_file_uuid), i);
        if ((ret = open_track_file_resource(s, &virtual_track->resources[i], track)) != 0) {
            av_log(s, AV_LOG_ERROR, "Could not open image track resource " UUID_FORMAT "\n", UID_ARG(virtual_track->resources[i].track_file_uuid));
            return ret;
        }
    }

    track->current_timestamp = av_make_q(0, track->duration.den);

    c->tracks = av_realloc(c->tracks, (c->track_count + 1) * sizeof(IMFVirtualTrackPlaybackCtx));
    c->tracks[c->track_count++] = track;

    return ret;
}

static void imf_virtual_track_playback_context_free(IMFVirtualTrackPlaybackCtx *track) {
    if (!track) {
        return;
    }

    for (int i = 0; i < track->resource_count; ++i) {
        if (!track->resources[i]) {
            continue;
        }

        if (track->resources[i]->ctx && track->resources[i]->ctx->iformat) {
            avformat_free_context(track->resources[i]->ctx);
            track->resources[i]->ctx = NULL;
        }
    }
}

static int set_context_streams_from_tracks(AVFormatContext *s) {
    IMFContext *c = s->priv_data;

    AVStream *asset_stream;
    AVStream *first_resource_stream;

    int ret = 0;

    for (int i = 0; i < c->track_count; ++i) {
        // Open the first resource of the track to get stream information
        first_resource_stream = c->tracks[i]->resources[0]->ctx->streams[0];

        av_log(s, AV_LOG_DEBUG, "Open the first resource of track %d\n", c->tracks[i]->index);

        // Copy stream information
        asset_stream = avformat_new_stream(s, NULL);
        asset_stream->id = i;
        if ((ret = avcodec_parameters_copy(asset_stream->codecpar, first_resource_stream->codecpar)) < 0) {
            av_log(s, AV_LOG_ERROR, "Could not copy stream parameters\n");
            break;
        }
        avpriv_set_pts_info(asset_stream, first_resource_stream->pts_wrap_bits, first_resource_stream->time_base.num, first_resource_stream->time_base.den);
        asset_stream->duration = (int64_t)av_q2d(av_mul_q(c->tracks[i]->duration, av_inv_q(asset_stream->time_base)));
    }

    return ret;
}

static int open_cpl_tracks(AVFormatContext *s) {
    IMFContext *c = s->priv_data;
    int32_t track_index = 0;
    int ret;

    if (c->cpl->main_image_2d_track) {
        if ((ret = open_virtual_track(s, c->cpl->main_image_2d_track, track_index++)) != 0) {
            av_log(s, AV_LOG_ERROR, "Could not open image track " UUID_FORMAT "\n", UID_ARG(c->cpl->main_image_2d_track->base.id_uuid));
            return ret;
        }
    }

    for (int audio_track_index = 0; audio_track_index < c->cpl->main_audio_track_count; ++audio_track_index) {
        if ((ret = open_virtual_track(s, &c->cpl->main_audio_tracks[audio_track_index], track_index++)) != 0) {
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

static IMFVirtualTrackPlaybackCtx *get_next_track_with_minimum_timestamp(AVFormatContext *s) {
    IMFContext *c = s->priv_data;
    IMFVirtualTrackPlaybackCtx *track;

    AVRational minimum_timestamp = av_make_q(INT32_MAX, 1);
    for (int i = 0; i < c->track_count; ++i) {
        av_log(s, AV_LOG_DEBUG, "Compare track %d timestamp " AVRATIONAL_FORMAT " to minimum " AVRATIONAL_FORMAT " (over duration: " AVRATIONAL_FORMAT ")\n", i, AVRATIONAL_ARG(c->tracks[i]->current_timestamp), AVRATIONAL_ARG(minimum_timestamp), AVRATIONAL_ARG(c->tracks[i]->duration));
        if (av_cmp_q(c->tracks[i]->current_timestamp, minimum_timestamp) < 0) {
            track = c->tracks[i];
            minimum_timestamp = track->current_timestamp;
        }
    }

    av_log(s, AV_LOG_DEBUG, "Found next track to read: %d (timestamp: %lf / %lf)\n", track->index, av_q2d(track->current_timestamp), av_q2d(minimum_timestamp));
    return track;
}

static IMFVirtualTrackResourcePlaybackCtx *get_resource_context_for_timestamp(AVFormatContext *s, IMFVirtualTrackPlaybackCtx *track) {
    AVRational edit_unit_duration = av_inv_q(track->resources[0]->resource->base.edit_rate);
    AVRational cumulated_duration = av_make_q(0, edit_unit_duration.den);

    av_log(s, AV_LOG_DEBUG, "Looking for track %d resource for timestamp = %lf / %lf\n", track->index, av_q2d(track->current_timestamp), av_q2d(track->duration));
    for (int i = 0; i < track->resource_count; ++i) {
        cumulated_duration = av_add_q(cumulated_duration, av_make_q((int)track->resources[i]->resource->base.duration * edit_unit_duration.num, edit_unit_duration.den));

        if (av_cmp_q(av_add_q(track->current_timestamp, edit_unit_duration), cumulated_duration) <= 0) {
            av_log(s, AV_LOG_DEBUG, "Found resource %d in track %d to read for timestamp %lf (on cumulated=%lf): entry=%ld, duration=%lu, editrate=" AVRATIONAL_FORMAT " | edit_unit_duration=%lf\n", i, track->index, av_q2d(track->current_timestamp), av_q2d(cumulated_duration), track->resources[i]->resource->base.entry_point, track->resources[i]->resource->base.duration, AVRATIONAL_ARG(track->resources[i]->resource->base.edit_rate), av_q2d(edit_unit_duration));

            if (track->current_resource_index != i) {
                av_log(s, AV_LOG_DEBUG, "Switch resource on track %d: re-open context\n", track->index);
                if (track->resources[track->current_resource_index] != NULL) {
                    avformat_close_input(&track->resources[track->current_resource_index]->ctx);
                }
                if (open_track_resource_context(s, track->resources[i]) != 0) {
                    return NULL;
                }
                track->current_resource_index = i;
            }
            return track->resources[track->current_resource_index];
        }
    }
    return NULL;
}

static int ff_imf_read_packet(AVFormatContext *s, AVPacket *pkt) {
    IMFContext *c = s->priv_data;

    IMFVirtualTrackResourcePlaybackCtx *resource_to_read = NULL;
    AVRational edit_unit_duration;
    int ret = 0;

    IMFVirtualTrackPlaybackCtx *track_to_read = get_next_track_with_minimum_timestamp(s);

    if (av_cmp_q(track_to_read->current_timestamp, track_to_read->duration) == 0) {
        return AVERROR_EOF;
    }

    resource_to_read = get_resource_context_for_timestamp(s, track_to_read);

    if (!resource_to_read) {
        edit_unit_duration = av_inv_q(track_to_read->resources[track_to_read->current_resource_index]->resource->base.edit_rate);
        if (av_cmp_q(av_add_q(track_to_read->current_timestamp, edit_unit_duration), track_to_read->duration) > 0) {
            return AVERROR_EOF;
        }

        av_log(s, AV_LOG_ERROR, "Could not find IMF track resource to read\n");
        return AVERROR_STREAM_NOT_FOUND;
    }

    while (!ff_check_interrupt(c->interrupt_callback) && !ret) {
        ret = av_read_frame(resource_to_read->ctx, pkt);
        av_log(s, AV_LOG_DEBUG, "Got packet: pts=%ld, dts=%ld, duration=%ld, stream_index=%d, pos=%ld\n", pkt->pts, pkt->dts, pkt->duration, pkt->stream_index, pkt->pos);
        if (ret >= 0) {
            // Update packet info from track
            if (pkt->dts < s->streams[track_to_read->index]->cur_dts && track_to_read->last_pts > 0) {
                pkt->dts = s->streams[track_to_read->index]->cur_dts;
            }

            pkt->pts = track_to_read->last_pts;
            pkt->dts = pkt->dts - (int64_t)track_to_read->resources[track_to_read->current_resource_index]->resource->base.entry_point;
            pkt->stream_index = track_to_read->index;

            // Update track cursors
            track_to_read->current_timestamp = av_add_q(track_to_read->current_timestamp, av_make_q((int)pkt->duration * resource_to_read->ctx->streams[0]->time_base.num, resource_to_read->ctx->streams[0]->time_base.den));
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

    for (int i = 0; i < c->track_count; ++i) {
        imf_virtual_track_playback_context_free(c->tracks[i]);
    }

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
