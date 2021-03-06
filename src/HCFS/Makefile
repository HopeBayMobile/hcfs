include ../../build/ccache.mk

# For CI build version
ifndef VERSION_NUM
  VERSION_NUM:=Manual build $(shell date +%Y%m%d-%H%M%S)
  empty:=
  space:= $(empty) $(empty)
  VERSION_NUM := $(subst $(space),_,$(VERSION_NUM))
endif

CC = gcc
CPPFLAGS = -DENABLE_DEDUP=0 \
	   -DENABLE_ENCRYPT=0 \
	   -DENABLE_COMPRESS=0 \
	   -D_FILE_OFFSET_BITS=64 \
	   -D_ANDROID_ENV_ \
	   -D_XOPEN_SOURCE=700 \
	   -D_DEFAULT_SOURCE \
	   -DVERSION_NUM=\"$(VERSION_NUM)\" \
	   -D_BSD_SOURCE \
	   -DFUSE_NOXATTR \

CFLAGS = -std=gnu11 -g -pthread -Wall -Wextra -Werror
LDFLAGS = -lcurl -lfuse -lssl -lcrypto -lm -lsqlite3 -ljansson
GCC_VERSION_GE_49 := $(shell expr `gcc -dumpversion | cut -f1-2 -d.` \>= 4.9)
ifeq "$(GCC_VERSION_GE_49)" "1"
	CPPFLAGS += -fdiagnostics-color=auto
endif

# [Autodependencies with GNU make]
# http://scottmcpeak.com/autodepend/autodepend.html

OBJS_DIR = obj
OBJS_F := \
	meta_iterator.o \
	objmeta.o \
	mount_manager.o \
	FS_manager.o \
	api_interface.o \
	logger.o \
	lookup_count.o \
	file_present.o \
	hfuse_system.o \
	utils.o \
	alias.o \
	fuseop.o \
	metaops.o \
	filetables.o \
	hcfscurl.o \
	hcfs_tocloud.o \
	hcfs_fromcloud.o \
	hcfs_cacheops.o \
	hcfs_clouddelete.o \
	hcfs_cachebuild.o \
	b64encode.o \
	meta_mem_cache.o \
	dir_entry_btree.o \
	xattr_ops.o \
	enc.o \
	compress.o \
	syncpoint_control.o \
	super_block.o \
	dedup_table.o \
	path_reconstruct.o \
	pin_scheduling.o \
	monitor.o \
	dir_statistics.o \
	parent_lookup.o \
	atomic_tocloud.o \
	tocloud_tools.o \
	do_fallocate.o \
	fuseproc_comm.o \
	event_notification.o \
	event_filter.o \
	meta.o \
	params.o \
	fuse_notify.o \
	rebuild_super_block.o \
	rebuild_parent_dirstat.o \
	restoration_utils.o \
	do_restoration.o \
	control_smartcache.o \
	recover_super_block.o \
	apk_mgmt.o \
	hash_list_struct.o \
	googledrive_curl.o \
	pthread_control.o \
	errcode.o \
	backend_generic.o \

# obj file used in android env
ifeq "$(findstring -D_ANDROID_ENV_, $(CPPFLAGS))" "-D_ANDROID_ENV_"
	OBJS_F += pkg_cache.o
endif

deps =
OBJS = $(addprefix $(OBJS_DIR)/, $(OBJS_F))
deps += $(OBJS:%.o=%.o.d)

all: check_pkgs hcfs

check_pkgs:
	@$(shell cd ../../utils;pwd)/check_compile_pkgs.sh

hcfs: $(OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -g $(OBJS) -o hcfs $(LDFLAGS)

# compile and generate dependency info
$(OBJS_DIR)/%.o: %.c | $(OBJS_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $(realpath $<) -o $@ -MMD -MF $@.d

$(OBJS_DIR):
	mkdir -p $@

clean:
	rm -rf hcfs $(OBJS_DIR)/*.o $(deps)

-include $(deps)
