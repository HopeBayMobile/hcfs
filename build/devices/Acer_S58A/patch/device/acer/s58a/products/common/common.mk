# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
MY_LOCAL_PATH := device/acer/s58a

DEVICE_PACKAGE_OVERLAYS := $(MY_LOCAL_PATH)/overlay

# Liquid sound file
PRODUCT_COPY_FILES += \
    $(MY_LOCAL_PATH)/misc/effects/Celebration.mp3:system/media/audio/ringtones/Celebration.mp3 \
    $(MY_LOCAL_PATH)/misc/effects/notification.mp3:system/media/audio/notifications/Crystal.mp3 \
    $(MY_LOCAL_PATH)/misc/effects/touch.ogg:system/media/audio/ui/Effect_Tick.ogg \
    $(MY_LOCAL_PATH)/misc/effects/sus.ogg:system/media/audio/ui/Lock.ogg \
    $(MY_LOCAL_PATH)/misc/effects/unlock.ogg:system/media/audio/ui/Unlock.ogg

ADDITIONAL_DEFAULT_PROPERTIES += \
    ro.config.notification_sound=Crystal.mp3 \
    ro.config.ringtone=Celebration.mp3 \
    ro.config.alarm_alert=Alarm_Classic.ogg

ifneq ($(TARGET_USES_AOSP),true)
# TARGET_USES_QCA_NFC := true
TARGET_USES_QCOM_BSP := true
endif
TARGET_ENABLE_QC_AV_ENHANCEMENTS := true

# copy customized media_profiles and media_codecs xmls for 8992
ifeq ($(TARGET_ENABLE_QC_AV_ENHANCEMENTS), true)
PRODUCT_COPY_FILES += $(MY_LOCAL_PATH)/media_profiles.xml:system/etc/media_profiles.xml \
                      $(MY_LOCAL_PATH)/media_codecs.xml:system/etc/media_codecs.xml \
                      $(MY_LOCAL_PATH)/media_codecs_performance.xml:system/etc/media_codecs_performance.xml
endif  #TARGET_ENABLE_QC_AV_ENHANCEMENTS

PRODUCT_COPY_FILES += $(MY_LOCAL_PATH)/whitelistedapps.xml:system/etc/whitelistedapps.xml

PRODUCT_COPY_FILES += $(MY_LOCAL_PATH)/thermal-engine.conf:system/etc/thermal-engine.conf
# Override heap growth limit due to high display density on device
PRODUCT_PROPERTY_OVERRIDES += \
    dalvik.vm.heapgrowthlimit=256m
$(call inherit-product, frameworks/native/build/phone-xhdpi-2048-dalvik-heap.mk)
$(call inherit-product, device/qcom/common/common64.mk)

PRODUCT_DEVICE := s58a
PRODUCT_BRAND := acer
PRODUCT_MODEL := S58A
PRODUCT_MANUFACTURER := Acer
PRODUCT_ID := S58A
PRODUCT_TARGET_DEVICE := s58a

PRODUCT_BOOT_JARS += vcard
PRODUCT_BOOT_JARS += tcmiface
PRODUCT_BOOT_JARS += qcmediaplayer
#PRODUCT_BOOT_JARS += org.codeaurora.Performance

ifneq ($(strip $(QCPATH)),)
PRODUCT_BOOT_JARS += com.qti.dpmframework
PRODUCT_BOOT_JARS += dpmapi
PRODUCT_BOOT_JARS += com.qti.location.sdk
PRODUCT_BOOT_JARS += oem-services
PRODUCT_BOOT_JARS += WfdCommon
#PRODUCT_BOOT_JARS += extendedmediaextractor
#PRODUCT_BOOT_JARS += security-bridge
#PRODUCT_BOOT_JARS += qsb-port
endif

#Android EGL implementation
PRODUCT_PACKAGES += libGLES_android

# Audio configuration file
ifeq ($(TARGET_USES_AOSP), true)
PRODUCT_COPY_FILES += \
    device/qcom/common/media/audio_policy.conf:system/etc/audio_policy.conf
else
PRODUCT_COPY_FILES += \
    $(MY_LOCAL_PATH)/audio_policy.conf:system/etc/audio_policy.conf
endif

PRODUCT_COPY_FILES += \
    $(MY_LOCAL_PATH)/audio_output_policy.conf:system/vendor/etc/audio_output_policy.conf \
    $(MY_LOCAL_PATH)/audio_effects.conf:system/vendor/etc/audio_effects.conf \
    $(MY_LOCAL_PATH)/mixer_paths.xml:system/etc/mixer_paths.xml \
    $(MY_LOCAL_PATH)/mixer_paths_i2s.xml:system/etc/mixer_paths_i2s.xml \
    $(MY_LOCAL_PATH)/aanc_tuning_mixer.txt:system/etc/aanc_tuning_mixer.txt \
    $(MY_LOCAL_PATH)/audio_platform_info_i2s.xml:system/etc/audio_platform_info_i2s.xml \
    $(MY_LOCAL_PATH)/sound_trigger_mixer_paths.xml:system/etc/sound_trigger_mixer_paths.xml \
    $(MY_LOCAL_PATH)/sound_trigger_platform_info.xml:system/etc/sound_trigger_platform_info.xml \
    $(MY_LOCAL_PATH)/audio_platform_info.xml:system/etc/audio_platform_info.xml

# Listen configuration file
PRODUCT_COPY_FILES += \
    $(MY_LOCAL_PATH)/listen_platform_info.xml:system/etc/listen_platform_info.xml

# WLAN driver configuration files
PRODUCT_COPY_FILES += \
    $(MY_LOCAL_PATH)/WCNSS_cfg.dat:system/etc/firmware/wlan/qca_cld/WCNSS_cfg.dat \
    $(MY_LOCAL_PATH)/WCNSS_qcom_cfg.ini:system/etc/wifi/WCNSS_qcom_cfg.ini \
    $(MY_LOCAL_PATH)/WCNSS_qcom_wlan_nv.bin:system/etc/wifi/WCNSS_qcom_wlan_nv.bin

#FEATURE_OPENGLES_EXTENSION_PACK support string config file
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:system/etc/permissions/android.hardware.opengles.aep.xml

PRODUCT_PACKAGES += \
    wpa_supplicant \
    wpa_supplicant_overlay.conf \
    p2p_supplicant_overlay.conf

PRODUCT_PACKAGES += \
    libqcomvisualizer \
    libqcomvoiceprocessing \
    libqcompostprocbundle

#RIL
PRODUCT_PACKAGES += \
    libqcci_acer \
    libtoolbox_jni \
    AcerUtilityService \
    AcerUtilityService.xml

#NETDUMP DEBUG
PRODUCT_PACKAGES += \
    tcpdump

# MSM IRQ Balancer configuration file
PRODUCT_COPY_FILES += \
    $(MY_LOCAL_PATH)/msm_irqbalance.conf:system/vendor/etc/msm_irqbalance.conf \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:system/etc/permissions/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepcounter.xml:system/etc/permissions/android.hardware.sensor.stepcounter.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepdetector.xml:system/etc/permissions/android.hardware.sensor.stepdetector.xml \

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.software.verified_boot.xml:system/etc/permissions/android.software.verified_boot.xml \

PRODUCT_COPY_FILES += \
    $(MY_LOCAL_PATH)/sensors/hals.conf:system/etc/sensors/hals.conf

# MIDI feature
#PRODUCT_COPY_FILES += \
#    frameworks/native/data/etc/android.software.midi.xml:system/etc/permissions/android.software.midi.xml

#ANT+ stack
PRODUCT_PACKAGES += \
    AntHalService \
    libantradio \
    antradio_app

PRODUCT_SUPPORTS_VERITY := true
PRODUCT_SYSTEM_VERITY_PARTITION := /dev/block/bootdevice/by-name/system
PRODUCT_AAPT_CONFIG += xlarge large

# Reduce client buffer size for fast audio output tracks
PRODUCT_PROPERTY_OVERRIDES += \
    af.fast_track_multiplier=1

# Low latency audio buffer size in frames
PRODUCT_PROPERTY_OVERRIDES += \
    audio_hal.period_size=192

# Acer customization
PRODUCT_ACER_CUSTOMIZATION += $(MY_LOCAL_PATH)/customization.lst

# Optionally include pandora customized make file
-include pandora/pandora.mk

# Copy acer boot animation/audio to system/media/
PRODUCT_COPY_FILES += $(MY_LOCAL_PATH)/misc/bootanimation.zip:system/media/bootanimation.zip

# Acer preload App and gms config
ifneq ($(TARGET_BUILD_VARIANT),eng)
    -include $(MY_LOCAL_PATH)/products/common/acer-custom.mk
    -include $(MY_LOCAL_PATH)/products/common/gms.mk
endif

# acer ril database
-include external/rildb/acer_ril_e.mk

# acer APN database
-include external/rildb/phone/apns-conf.mk

# Set Dual-SIM property
ADDITIONAL_BUILD_PROPERTIES := \
    persist.radio.multisim.config=dsds

# acer flip
PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.lidhall.enable=1

#AcerLog
PRODUCT_PACKAGES += \
    alog

# Widevine components
-include $(MY_LOCAL_PATH)/products/common/drm/drm.mk
-include $(MY_LOCAL_PATH)/products/common/drm/widevine/WV-L3.mk

#Demo mode
PRODUCT_PACKAGES += \
    set_demo_mode.sh \
    set_dts_settings.sh

# Add System service "GamingManager"
BOARD_SEPOLICY_DIRS += \
    device/acer/s58a/sepolicy
BOARD_SEPOLICY_UNION += \
    service_contexts \
    service.te \
    file_contexts \
    file.te \
    untrusted_app.te \
    mediaserver.te \
    platform_app.te \
    system_app.te \
    dtshpxservice.te \
    init_shell.te \
    shell.te

# Enable live function of Acer GameZone
PRODUCT_PROPERTY_OVERRIDES += \
    ro.acer.live.enable = 1

# DTS license key and HPX service
PRODUCT_COPY_FILES += \
    $(MY_LOCAL_PATH)/dts_data/dts-eagle.lic:system/etc/dts/dts-eagle.lic \
    $(MY_LOCAL_PATH)/dts_data/dts-m6m8-lic.key:system/etc/dts/dts-m6m8-lic.key \
    $(MY_LOCAL_PATH)/dts_data/libdts_hpx_service_c.so:system/lib/libdts_hpx_service_c.so \
    $(MY_LOCAL_PATH)/dts_data/dts_hpx_service:system/bin/dts_hpx_service \
    $(MY_LOCAL_PATH)/dts_data/tuning_configs:system/etc/dts/tuning_configs \
    $(MY_LOCAL_PATH)/dts_data/dts_hpx_settings:system/etc/dts/dts_hpx_settings \
    $(call find-copy-subdir-files,*,$(MY_LOCAL_PATH)/dts_data/res,system/etc/dts) \
    $(call find-copy-subdir-files,*,$(MY_LOCAL_PATH)/dts_data/path,system/etc/dts)

include $(MY_LOCAL_PATH)/products/common/hb-common.mk
