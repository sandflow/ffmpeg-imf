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

static const char *UUID_PRINTF_FMT = "urn:uuid:%02hhx%02hhx%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx";

static void print_uuid(unsigned char uuid[16]) {
    printf(UUID_PRINTF_FMT,
        uuid[0],
        uuid[1],
        uuid[2],
        uuid[3],
        uuid[4],
        uuid[5],
        uuid[6],
        uuid[7],
        uuid[8],
        uuid[9],
        uuid[10],
        uuid[11],
        uuid[12],
        uuid[13],
        uuid[14],
        uuid[15]);
}

int main(int argc, char *argv[]) {
    xmlDocPtr doc;
    IMFCPL *  cpl;
    int       ret;

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

    printf("%s", cpl->content_title_utf8);
    printf("\n");
    print_uuid(cpl->id_uuid);
    printf("\n");
    printf("%i %i", cpl->edit_rate.num, cpl->edit_rate.den);
    printf("\n");

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
        printf("  ");
        print_uuid(cpl->main_image_2d_track->resources[i].track_file_uuid);
        printf("\n");
    }

    printf("Main audio track count: %lu\n", cpl->main_audio_track_count);
    for (unsigned long i = 0; i < cpl->main_audio_track_count; i++) {
        printf("  Main audio virtual track %lu\n", i);
        printf("  Main audio resource count: %lu\n", cpl->main_audio_tracks[i].resource_count);
        for (unsigned long j = 0; j < cpl->main_audio_tracks[i].resource_count; j++) {
            printf("  Track file resource %lu\n", j);
            printf("    ");
            print_uuid(cpl->main_audio_tracks[i].resources[j].track_file_uuid);
            printf("\n");
        }
    }

    imf_cpl_free(cpl);

    return 0;
}
