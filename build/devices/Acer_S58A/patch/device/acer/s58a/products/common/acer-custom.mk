#PRODUCT_PACKAGES += \

# Acer Library
PRODUCT_PACKAGES += \
  com.acer.android.os

#FMW-Addon
PRODUCT_PACKAGES += \
    AcerRingtonePicker \
    AcerGallery \
    AcerBlueLightFilter

#AcerDinfoInit
PRODUCT_PACKAGES += \
  acer_dinfo_init

#AcerPortal
PRODUCT_PACKAGES += \
  AcerAidKit

#SystemDoctor
PRODUCT_PACKAGES += \
  SystemDoctor

#System service
PRODUCT_PACKAGES += \
    AcerCareCore

#Power Management
PRODUCT_PACKAGES += \
    PowerManagement

#AcerBcakup
PRODUCT_PACKAGES += \
    AccountBackup \
    AcerBackup

# AOSP Browser
PRODUCT_PACKAGES += \
    Browser

#Phorus begin_mod
PHORUS_DEVICE := msm8992
$(call inherit-product-if-exists, vendor/phorus/playfi/device-vendor.mk)
$(call inherit-product-if-exists, device/acer/s58a/libphorusaudio/Android.mk)
#Phorus end_mod

# For AcerOOBE
PRODUCT_PACKAGES += AcerOOBEM

#wifi
PRODUCT_PACKAGES += qca_cld_wlan.ko

# SNID eMMC Init
PRODUCT_PACKAGES += \
  snid_emmc_init

#AcerHome-Addon
PRODUCT_PACKAGES += \
    AcerWallpaper

#AcerAPNUpdater
PRODUCT_PACKAGES += \
    AcerAPNUpdaterBase

# Acer Nidus
PRODUCT_PACKAGES += AcerNidus

#AcerCamera Cyberon libs
PRODUCT_PACKAGES += \
    libCSpotter \
    libNINJA

#AcerCamera SDK libs
PRODUCT_PACKAGES += \
    com.acer.camerasdk \
    com.acer.camerasdk.xml

#AcerCamera Omron libs
PRODUCT_PACKAGES += \
    libCanvasUtil \
    libeOkao \
    libeOkaoBy \
    libeOkaoCo \
    libeOkaoDt \
    libeOkaoEd \
    libeOkaoPe \
    libeOkaoPt \
    libeOkaoSs \
    libeOkaoSSc \
    libeOkaoSSs \
    libeOkaoSSt \
    libImageUtil \
    libSilhouetteUtil \
    libUtility \
    libXYZ

# For Acer GameZone
PRODUCT_PACKAGES += \
    AcerGameZone

#HCFS management app
PRODUCT_PACKAGES +=\
    HopebayHCFSmgmt    
