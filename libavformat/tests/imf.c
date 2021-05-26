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
    "</SequenceList>"
    "</Segment>"
    "</SegmentList>"
    "<ContentTitle>Hello</ContentTitle>" "</CompositionPlaylist>";

int main(int argc, char *argv[])
{

    xmlDocPtr doc;

    IMFCPL *cpl;

    int ret;

    doc = xmlReadMemory(cpl_doc, strlen(cpl_doc), NULL, NULL, 0);

    if (!doc) {
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

    printf(UUID_FMT_STR,
           cpl->id_uuid[0],
           cpl->id_uuid[1],
           cpl->id_uuid[2],
           cpl->id_uuid[3],
           cpl->id_uuid[4],
           cpl->id_uuid[5],
           cpl->id_uuid[6],
           cpl->id_uuid[7],
           cpl->id_uuid[8],
           cpl->id_uuid[9],
           cpl->id_uuid[10],
           cpl->id_uuid[11],
           cpl->id_uuid[12],
           cpl->id_uuid[13], cpl->id_uuid[14], cpl->id_uuid[15]
        );

    printf("\n");

    printf("%i %i", cpl->edit_rate.num, cpl->edit_rate.den);

    printf("\n");

    assert(cpl->main_markers_track);

    printf("Marker resource count: %lu\n",
           cpl->main_markers_track->resource_count);

    for (unsigned long i = 0; i < cpl->main_markers_track->resource_count;
         i++) {
        printf("Marker resource %lu\n", i);

        for (unsigned long j = 0;
             j < cpl->main_markers_track->resources[i].marker_count; j++) {
            printf("  Marker %lu\n", j);
            printf("    Label %s\n",
                   cpl->main_markers_track->resources[i].markers[j].
                   label_utf8);
            printf("    Offset %lu\n",
                   cpl->main_markers_track->resources[i].markers[j].
                   offset);
        }
    }


    return 0;
}
