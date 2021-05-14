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

#include <libxml/parser.h>
#include "libavutil/opt.h"
#include "internal.h"
#include "avformat.h"

#define MAX_BPRINT_READ_SIZE (UINT_MAX - 1)
#define DEFAULT_ASSETMAP_SIZE 8 * 1024

typedef struct IMFContext {
    const AVClass *class;
    char *base_url;
    AVIOInterruptCB *interrupt_callback;
    AVDictionary *avio_opts;
} IMFContext;

static int parse_assetmap(AVFormatContext *s, const char *url, AVIOContext *in)
{
    IMFContext *c = s->priv_data;
    AVBPrint buf;
    AVDictionary *opts = NULL;
    xmlDoc *doc = NULL;
    xmlNodePtr root_element = NULL;
    xmlNodePtr node = NULL;
    int close_in = 0;
    int ret = 0;
    int64_t filesize = 0;

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

    if ((ret = avio_read_to_bprint(in, &buf, MAX_BPRINT_READ_SIZE)) < 0 ||
        !avio_feof(in) ||
        (filesize = buf.len) == 0) {
        av_log(s, AV_LOG_ERROR, "Unable to read to assetmap '%s'\n", url);
        if (ret == 0)
            ret = AVERROR_INVALIDDATA;
    } else {
        LIBXML_TEST_VERSION

        doc = xmlReadMemory(buf.str, filesize, c->base_url, NULL, 0);
        root_element = xmlDocGetRootElement(doc);
        node = root_element;

        if (!node) {
            ret = AVERROR_INVALIDDATA;
            av_log(s, AV_LOG_ERROR, "Unable to parse '%s' - missing root node\n", url);
            goto cleanup;
        }

        if (node->type != XML_ELEMENT_NODE ||
            av_strcasecmp(node->name, "AssetMap")) {
            ret = AVERROR_INVALIDDATA;
            av_log(s, AV_LOG_ERROR, "Unable to parse '%s' - wrong root node name[%s] type[%d]\n", url, node->name, (int)node->type);
            goto cleanup;
        }
cleanup:
        /*free the document */
        xmlFreeDoc(doc);
        xmlCleanupParser();
        // xmlFreeNode(mpd_baseurl_node);
    }
    if (close_in) {
        avio_close(in);
    }
    return ret;
}

static int imf_close(AVFormatContext *s);

static int imf_read_header(AVFormatContext *s)
{
    AVStream *stream;
    int ret = 0;

    av_log(s, AV_LOG_DEBUG, "start to parse IMF package\n");

    if ((ret = parse_assetmap(s, s->url, s->pb)) < 0)
        goto fail;

    // @TODO create streams for each IMF virtual track
    stream = avformat_new_stream(s, NULL);
    if (!stream) {
      ret = AVERROR(ENOMEM);
      goto fail;
    }

    // @TODO fill information with track information.
    stream->time_base.num = 1;
    stream->time_base.den = 24;
    stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->codecpar->codec_id = AV_CODEC_ID_JPEG2000;
    stream->codecpar->format = AV_PIX_FMT_YUV420P10BE;
    stream->codecpar->width = 1920;
    stream->codecpar->height = 1080;

    av_log(s, AV_LOG_DEBUG, "parsed IMF package\n");
    return 0;

fail:
    imf_close(s);
    return ret;
}

static int ff_imf_read_packet(AVFormatContext *s, AVPacket *pkt)
{
  return AVERROR_EOF;
}

static int imf_close(AVFormatContext *s)
{
    av_log(s, AV_LOG_DEBUG, "Close IMF package\n");
    IMFContext *c = s->priv_data;
    av_dict_free(&c->avio_opts);
    av_freep(&c->base_url);
    return 0;
}

static const AVOption imf_options[] = {
    {NULL}
};

static const AVClass imf_class = {
    .class_name = "imf",
    .item_name  = av_default_item_name,
    .option     = imf_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

const AVInputFormat ff_imf_demuxer = {
    .name           = "imf",
    .long_name      = NULL_IF_CONFIG_SMALL("IMF (Interoperable Master Format)"),
    .priv_class     = &imf_class,
    .priv_data_size = sizeof(IMFContext),
    .read_header    = imf_read_header,
    .read_packet    = ff_imf_read_packet,
    .read_close     = imf_close,
    .extensions     = "xml",
};
