CC = gcc-4.9
CFLAGS_common ?= -Wall -std=gnu99
CFLAGS_ndk = -pie -fPIE
CFLAGS += -lpthread -ldl

#NDK_HOME = /home/intern/android-ndk-r12
NDK_HOME = /home/tester/Android/Sdk/ndk-bundle
CROSS = $(NDK_HOME)/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android
SYSROOT = --sysroot=/$(NDK_HOME)/platforms/android-23/arch-arm64/

EXEC = US000_02
all : $(EXEC)

US000_02: US000_02.c
	$(CROSS)-$(CC) $(CFLAGS_common) $(CFLAGS_ndk) $(SYSROOT) -o $@ $@.c


.PHONY: push run clean
push: $(EXEC)
	adb push $(EXEC) /data/local/tmp/

run: push
	adb shell /data/local/tmp/$(EXEC) && ./doCheck_logined

clean :
	rm -rf $(EXEC) datablock.cloud datablock.backup
