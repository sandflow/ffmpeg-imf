
FATE_IMF += fate-imf-cpl-with-repeat
fate-imf-cpl-with-repeat: CMD = framecrc -i $(TARGET_SAMPLES)/imf/countdown/CPL_f5095caa-f204-4e1c-8a84-7af48c7ae16b.xml -c:v copy

FATE_IMF-$(CONFIG_IMF_DEMUXER) += $(FATE_IMF)

FATE_SAMPLES_AVCONV += $(FATE_IMF-yes)

fate-imf: $(FATE_IMF-yes)
