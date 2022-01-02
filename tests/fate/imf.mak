FATE_IMF += fate-imf-cpl-with-repeat
fate-imf-cpl-with-repeat: $(TARGET_SAMPLES)/imf/countdown/CPL_f5095caa-f204-4e1c-8a84-7af48c7ae16b.xml
fate-imf-cpl-with-repeat: $(TARGET_SAMPLES)/imf/countdown/ASSETMAP.xml
fate-imf-cpl-with-repeat: $(TARGET_SAMPLES)/imf/countdown/frame-counter.mxf
fate-imf-cpl-with-repeat: $(TARGET_SAMPLES)/imf/countdown/PKL_4671220f-c87a-4660-bf2a-6ef848791a2c.xml
fate-imf-cpl-with-repeat: CMD = framecrc -i $(TARGET_SAMPLES)/imf/countdown/CPL_f5095caa-f204-4e1c-8a84-7af48c7ae16b.xml -c:v copy

FATE_IMF-$(CONFIG_IMF_DEMUXER) += $(FATE_IMF)

FATE_SAMPLES_AVCONV += $(FATE_IMF-yes)

fate-imf: $(FATE_IMF-yes)
