
FATE_IMF_PROBE += fate-imf-demux
fate-imf-demux: CMD = ffprobe_demux $(TARGET_SAMPLES)/imf/countdown/CPL_f5095caa-f204-4e1c-8a84-7af48c7ae16b.xml

FATE_SAMPLES_FFPROBE += $(FATE_IMF_PROBE)

fate-imf: $(FATE_IMF_PROBE)
