# Auto-generated file, do not edit

$(call inherit-product, vendor/lge/bullhead/bullhead-vendor-blobs.mk)

# Prebuilt APKs/JARs from 'vendor/app'
PRODUCT_PACKAGES += \
    datastatusnotification \
    ims

# Prebuilt APKs libs symlinks from 'vendor/app'
PRODUCT_PACKAGES += \
    libimsmedia_jni_64.so \
    libimscamera_jni_64.so

# Prebuilt APKs/JARs from 'proprietary/app'
PRODUCT_PACKAGES += \
    RcsImsBootstraputil \
    RCSBootstraputil \
    HiddenMenu \
    TimeService \
    atfwd \
    qcrilmsgtunnel

# Prebuilt APKs/JARs from 'proprietary/framework'
PRODUCT_PACKAGES += \
    rcsservice \
    com.google.widevine.software.drm \
    rcsimssettings \
    cneapiclient \
    qcrilhook

# Prebuilt APKs/JARs from 'proprietary/priv-app'
PRODUCT_PACKAGES += \
    DiagMon \
    LifeTimerService \
    ConnMO \
    DMService \
    HotwordEnrollment \
    CNEService \
    SprintDM \
    DMConfigUpdate \
    DCMO

# Prebuilt APKs libs symlinks from 'proprietary/priv-app'
PRODUCT_PACKAGES += \
    libdmjavaplugin_32.so \
    libdmengine_32.so
