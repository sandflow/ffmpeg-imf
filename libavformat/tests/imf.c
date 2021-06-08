/*
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

#include "libavformat/imf.h"
#include "libavformat/imf_internal.h"
#include "libavformat/mxf.h"

#define UUID_PRINTF_FMT "urn:uuid:%02hhx%02hhx%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"

const char *cpl_doc =
    "<CompositionPlaylist xmlns=\"http://example.com\">"
    "<Id>urn:uuid:8713c020-2489-45f5-a9f7-87be539e20b5</Id>"
    "<EditRate>24000 1001</EditRate>"
    "<SegmentList>"
    "<Segment>"
    "<SequenceList>"
    "<MarkerSequence>"
    "<TrackId>urn:uuid:461f5424-8f6e-48a9-a385-5eda46fda381</TrackId>"
    "<ResourceList>"
    "<Resource>"
    "<IntrinsicDuration>24</IntrinsicDuration>"
    "<Marker>"
    "<Label>LFOA</Label>"
    "<Offset>5</Offset>"
    "</Marker>"
    "</Resource>"
    "</ResourceList>"
    "</MarkerSequence>"
    "<MainImageSequence>"
    "<TrackId>urn:uuid:e8ef9653-565c-479c-8039-82d4547973c5</TrackId>"
    "<ResourceList>"
    "<Resource>"
    "<IntrinsicDuration>24</IntrinsicDuration>"
    "<TrackFileId>urn:uuid:6f768ca4-c89e-4dac-9056-a29425d40ba1</TrackFileId>"
    "</Resource>"
    "</ResourceList>"
    "</MainImageSequence>"
    "<MainAudioSequence>"
    "<TrackId>urn:uuid:68e3fae5-d94b-44d2-92a6-b94877fbcdb5</TrackId>"
    "<ResourceList>"
    "<Resource>"
    "<IntrinsicDuration>24</IntrinsicDuration>"
    "<TrackFileId>urn:uuid:381dadd2-061e-46cc-a63a-e3d58ce7f488</TrackFileId>"
    "</Resource>"
    "</ResourceList>"
    "</MainAudioSequence>"
    "<MainAudioSequence>"
    "<TrackId>urn:uuid:6978c106-95bc-424b-a17c-628206a5892d</TrackId>"
    "<ResourceList>"
    "<Resource>"
    "<IntrinsicDuration>24</IntrinsicDuration>"
    "<TrackFileId>urn:uuid:381dadd2-061e-46cc-a63a-e3d58ce7f488</TrackFileId>"
    "</Resource>"
    "</ResourceList>"
    "</MainAudioSequence>"
    "</SequenceList>"
    "</Segment>"
    "<Segment>"
    "<SequenceList>"
    "<MarkerSequence>"
    "<TrackId>urn:uuid:461f5424-8f6e-48a9-a385-5eda46fda381</TrackId>"
    "<ResourceList>"
    "<Resource>"
    "<IntrinsicDuration>36</IntrinsicDuration>"
    "<Marker>"
    "<Label>FFOA</Label>"
    "<Offset>20</Offset>"
    "</Marker>"
    "<Marker>"
    "<Label>LFOC</Label>"
    "<Offset>24</Offset>"
    "</Marker>"
    "</Resource>"
    "</ResourceList>"
    "</MarkerSequence>"
    "<MainImageSequence>"
    "<TrackId>urn:uuid:e8ef9653-565c-479c-8039-82d4547973c5</TrackId>"
    "<ResourceList>"
    "<Resource>"
    "<IntrinsicDuration>36</IntrinsicDuration>"
    "<TrackFileId>urn:uuid:f3b263b3-096b-4360-a952-b1a9623cd0ca</TrackFileId>"
    "</Resource>"
    "</ResourceList>"
    "</MainImageSequence>"
    "<MainAudioSequence>"
    "<TrackId>urn:uuid:68e3fae5-d94b-44d2-92a6-b94877fbcdb5</TrackId>"
    "<ResourceList>"
    "<Resource>"
    "<IntrinsicDuration>36</IntrinsicDuration>"
    "<TrackFileId>urn:uuid:2484d613-bb7d-4bcc-8b0f-2e65938f0535</TrackFileId>"
    "</Resource>"
    "</ResourceList>"
    "</MainAudioSequence>"
    "<MainAudioSequence>"
    "<TrackId>urn:uuid:6978c106-95bc-424b-a17c-628206a5892d</TrackId>"
    "<ResourceList>"
    "<Resource>"
    "<IntrinsicDuration>36</IntrinsicDuration>"
    "<TrackFileId>urn:uuid:2484d613-bb7d-4bcc-8b0f-2e65938f0535</TrackFileId>"
    "</Resource>"
    "</ResourceList>"
    "</MainAudioSequence>"
    "</SequenceList>"
    "</Segment>"
    "</SegmentList>"
    "<ContentTitle>Hello</ContentTitle>"
    "</CompositionPlaylist>";

const char *asset_map_doc =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
    "<am:AssetMap xmlns:am=\"http://www.smpte-ra.org/schemas/429-9/2007/AM\">"
    "<am:Id>urn:uuid:68d9f591-8191-46b5-38b4-affb87a14132</am:Id>"
    "<am:AnnotationText>IMF_TEST_ASSET_MAP</am:AnnotationText>"
    "<am:Creator>Some tool</am:Creator>"
    "<am:VolumeCount>1</am:VolumeCount>"
    "<am:IssueDate>2021-06-07T12:00:00+00:00</am:IssueDate>"
    "<am:Issuer>FFmpeg</am:Issuer>"
    "<am:AssetList>"
    "<am:Asset>"
    "<am:Id>urn:uuid:b5d674b8-c6ce-4bce-3bdf-be045dfdb2d0</am:Id>"
    "<am:ChunkList>"
    "<am:Chunk>"
    "<am:Path>IMF_TEST_ASSET_MAP_video.mxf</am:Path>"
    "<am:VolumeIndex>1</am:VolumeIndex>"
    "<am:Offset>0</am:Offset>"
    "<am:Length>1234567</am:Length>"
    "</am:Chunk>"
    "</am:ChunkList>"
    "</am:Asset>"
    "<am:Asset>"
    "<am:Id>urn:uuid:ec3467ec-ab2a-4f49-c8cb-89caa3761f4a</am:Id>"
    "<am:ChunkList>"
    "<am:Chunk>"
    "<am:Path>IMF_TEST_ASSET_MAP_video_1.mxf</am:Path>"
    "<am:VolumeIndex>1</am:VolumeIndex>"
    "<am:Offset>0</am:Offset>"
    "<am:Length>234567</am:Length>"
    "</am:Chunk>"
    "</am:ChunkList>"
    "</am:Asset>"
    "<am:Asset>"
    "<am:Id>urn:uuid:5cf5b5a7-8bb3-4f08-eaa6-3533d4b77fa6</am:Id>"
    "<am:ChunkList>"
    "<am:Chunk>"
    "<am:Path>IMF_TEST_ASSET_MAP_audio.mxf</am:Path>"
    "<am:VolumeIndex>1</am:VolumeIndex>"
    "<am:Offset>0</am:Offset>"
    "<am:Length>34567</am:Length>"
    "</am:Chunk>"
    "</am:ChunkList>"
    "</am:Asset>"
    "<am:Asset>"
    "<am:Id>urn:uuid:559777d6-ec29-4375-f90d-300b0bf73686</am:Id>"
    "<am:ChunkList>"
    "<am:Chunk>"
    "<am:Path>CPL_IMF_TEST_ASSET_MAP.xml</am:Path>"
    "<am:VolumeIndex>1</am:VolumeIndex>"
    "<am:Offset>0</am:Offset>"
    "<am:Length>12345</am:Length>"
    "</am:Chunk>"
    "</am:ChunkList>"
    "</am:Asset>"
    "<am:Asset>"
    "<am:Id>urn:uuid:dd04528d-9b80-452a-7a13-805b08278b3d</am:Id>"
    "<am:PackingList>true</am:PackingList>"
    "<am:ChunkList>"
    "<am:Chunk>"
    "<am:Path>PKL_IMF_TEST_ASSET_MAP.xml</am:Path>"
    "<am:VolumeIndex>1</am:VolumeIndex>"
    "<am:Offset>0</am:Offset>"
    "<am:Length>2345</am:Length>"
    "</am:Chunk>"
    "</am:ChunkList>"
    "</am:Asset>"
    "</am:AssetList>"
    "</am:AssetMap>";

static int test_cpl_parsing() {
    xmlDocPtr doc;
    IMFCPL *cpl;
    int ret;

    doc = xmlReadMemory(cpl_doc, strlen(cpl_doc), NULL, NULL, 0);
    if (doc == NULL) {
        printf("XML parsing failed.");
        return 1;
    }

    ret = parse_imf_cpl_from_xml_dom(doc, &cpl);
    if (ret) {
        printf("CPL parsing failed.");
        return 1;
    }

    printf("%s\n", cpl->content_title_utf8);
    printf(UUID_PRINTF_FMT "\n", UID_ARG(cpl->id_uuid));
    printf("%i %i\n", cpl->edit_rate.num, cpl->edit_rate.den);

    printf("Marker resource count: %lu\n", cpl->main_markers_track->resource_count);
    for (unsigned long i = 0; i < cpl->main_markers_track->resource_count; i++) {
        printf("Marker resource %lu\n", i);
        for (unsigned long j = 0; j < cpl->main_markers_track->resources[i].marker_count; j++) {
            printf("  Marker %lu\n", j);
            printf("    Label %s\n", cpl->main_markers_track->resources[i].markers[j].label_utf8);
            printf("    Offset %lu\n", cpl->main_markers_track->resources[i].markers[j].offset);
        }
    }

    printf("Main image resource count: %lu\n", cpl->main_image_2d_track->resource_count);
    for (unsigned long i = 0; i < cpl->main_image_2d_track->resource_count; i++) {
        printf("Track file resource %lu\n", i);
        printf("  " UUID_PRINTF_FMT "\n", UID_ARG(cpl->main_image_2d_track->resources[i].track_file_uuid));
    }

    printf("Main audio track count: %lu\n", cpl->main_audio_track_count);
    for (unsigned long i = 0; i < cpl->main_audio_track_count; i++) {
        printf("  Main audio virtual track %lu\n", i);
        printf("  Main audio resource count: %lu\n", cpl->main_audio_tracks[i].resource_count);
        for (unsigned long j = 0; j < cpl->main_audio_tracks[i].resource_count; j++) {
            printf("  Track file resource %lu\n", j);
            printf("    " UUID_PRINTF_FMT "\n", UID_ARG(cpl->main_audio_tracks[i].resources[j].track_file_uuid));
        }
    }

    imf_cpl_free(cpl);

    return 0;
}

static int check_asset_locator_attributes(IMFAssetLocator *asset_locator, IMFAssetLocator expected_asset_locator) {

    printf("\tCompare " UUID_PRINTF_FMT " to " UUID_PRINTF_FMT ".\n", UID_ARG(asset_locator->uuid), UID_ARG(expected_asset_locator.uuid));
    for (int i = 0; i < 16; ++i) {
        if (asset_locator->uuid[i] != expected_asset_locator.uuid[i]) {
            printf("Invalid asset locator UUID: found " UUID_PRINTF_FMT " instead of " UUID_PRINTF_FMT " expected.\n", UID_ARG(asset_locator->uuid), UID_ARG(expected_asset_locator.uuid));
            return 1;
        }
    }

    printf("\tCompare %s to %s.\n", asset_locator->path, expected_asset_locator.path);
    if (strcmp(asset_locator->path, expected_asset_locator.path) != 0) {
        printf("Invalid asset locator path: found %s instead of %s expected.\n", asset_locator->path, expected_asset_locator.path);
        return 1;
    }

    return 0;
}

static const IMFAssetLocator ASSET_MAP_EXPECTED_LOCATORS[5] = {
    [0] = {.uuid = {0xb5, 0xd6, 0x74, 0xb8, 0xc6, 0xce, 0x4b, 0xce, 0x3b, 0xdf, 0xbe, 0x04, 0x5d, 0xfd, 0xb2, 0xd0}, .path = "IMF_TEST_ASSET_MAP_video.mxf" },
    [1] = {.uuid = {0xec, 0x34, 0x67, 0xec, 0xab, 0x2a, 0x4f, 0x49, 0xc8, 0xcb, 0x89, 0xca, 0xa3, 0x76, 0x1f, 0x4a}, .path = "IMF_TEST_ASSET_MAP_video_1.mxf" },
    [2] = {.uuid = {0x5c, 0xf5, 0xb5, 0xa7, 0x8b, 0xb3, 0x4f, 0x08, 0xea, 0xa6, 0x35, 0x33, 0xd4, 0xb7, 0x7f, 0xa6}, .path = "IMF_TEST_ASSET_MAP_audio.mxf" },
    [3] = {.uuid = {0x55, 0x97, 0x77, 0xd6, 0xec, 0x29, 0x43, 0x75, 0xf9, 0x0d, 0x30, 0x0b, 0x0b, 0xf7, 0x36, 0x86}, .path = "CPL_IMF_TEST_ASSET_MAP.xml" },
    [4] = {.uuid = {0xdd, 0x04, 0x52, 0x8d, 0x9b, 0x80, 0x45, 0x2a, 0x7a, 0x13, 0x80, 0x5b, 0x08, 0x27, 0x8b, 0x3d}, .path = "PKL_IMF_TEST_ASSET_MAP.xml" },
};

static int test_asset_map_parsing() {
    IMFAssetMapLocator *asset_map_locator;
    xmlDoc *doc;
    int ret;

    doc = xmlReadMemory(asset_map_doc, strlen(asset_map_doc), NULL, NULL, 0);
    if (doc == NULL) {
        printf("Asset map XML parsing failed.\n");
        return 1;
    }

    printf("Allocate ASSETMAP locator\n");
    asset_map_locator = imf_asset_map_locator_alloc();

    printf("Parse ASSETMAP XML document\n");
    ret = parse_imf_asset_map_from_xml_dom(NULL, doc, &asset_map_locator);
    if (ret) {
        printf("Asset map parsing failed.\n");
        goto cleanup;
    }

    printf("Compare assets count: %d to 5\n", asset_map_locator->assets_count);
    if (asset_map_locator->assets_count != 5) {
        printf("Asset map parsing failed: found %d assets instead of 5 expected.\n", asset_map_locator->assets_count);
        ret = 1;
        goto cleanup;
    }

    for (int i = 0; i < asset_map_locator->assets_count; ++i) {
        printf("For asset: %d:\n", i);
        ret = check_asset_locator_attributes(asset_map_locator->assets[i], ASSET_MAP_EXPECTED_LOCATORS[i]);
        if (ret > 0) {
            goto cleanup;
        }
    }

cleanup:
    imf_asset_map_locator_free(asset_map_locator);
    xmlFreeDoc(doc);
    return ret;
}

int main(int argc, char *argv[]) {
    int ret = 0;

    if (test_cpl_parsing() != 0) {
        ret = 1;
    }

    if (test_asset_map_parsing() != 0) {
        ret = 1;
    }

    return ret;
}
