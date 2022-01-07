FATE_IMF += fate-imf-cpl-with-repeat
fate-imf-cpl-with-repeat: CMD = framecrc -i $(TARGET_SAMPLES)/imf/countdown/CPL_bb2ce11c-1bb6-4781-8e69-967183d02b9b.xml -c:v copy

FATE_SAMPLES_AVCONV-$(call ALLYES, IMF_DEMUXER MXF_DEMUXER) += $(FATE_IMF)

fate-imf: $(FATE_IMF)
