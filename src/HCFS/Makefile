include ../../build/ccache.mk

# For CI build version
ifndef VERSION_NUM
  VERSION_NUM:=Manual build $(shell date +%Y%m%d-%H%M%S)
  empty:=
  space:= $(empty) $(empty)
  VERSION_NUM := $(subst $(space),_,$(VERSION_NUM))
endif

CC = gcc
CPPFLAGS = -DDEDUP_ENABLE=0 -DENCRYPT_ENABLE=0 -DCOMPRESS_ENABLE=0 \
	   -D_FILE_OFFSET_BITS=64 -DARM_32bit_ -D_ANDROID_ENV_ \
	   -DVERSION_NUM=\"$(VERSION_NUM)\"

CFLAGS = -g -pthread -Wall -Wextra -Werror
LFLAGS = -lcurl -lfuse -lssl -lcrypto -lm -lsqlite3 -ljansson
GCC_VERSION_GE_49 := $(shell expr `gcc -dumpversion | cut -f1-2 -d.` \>= 4.9)
ifeq "$(GCC_VERSION_GE_49)" "1"
	CPPFLAGS += -fdiagnostics-color=auto
endif

# [Autodependencies with GNU make]
# http://scottmcpeak.com/autodepend/autodepend.html

OBJS_DIR = obj
OBJS_F := \
	objmeta.o \
	mount_manager.o \
	FS_manager.o \
	api_interface.o \
	logger.o \
	lookup_count.o \
	file_present.o \
	hfuse_system.o \
	utils.o \
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

# obj file used in android env
ifeq "$(findstring -D_ANDROID_ENV_, $(CPPFLAGS))" "-D_ANDROID_ENV_"
	OBJS_F += pkg_cache.o
endif

OBJS = $(addprefix $(OBJS_DIR)/, $(OBJS_F))

all: check_pkgs hcfs

check_pkgs:
	@$(shell cd ../../utils;pwd)/check_compile_pkgs.sh

hcfs: $(OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -g $(OBJS) -o hcfs $(LFLAGS)

# pull in dependency info for *existing* .o files
include $(wildcard $(OBJS_DIR)/*.d)

# compile and generate dependency info
$(OBJS_DIR)/%.o: %.c | $(OBJS_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $*.c -o $(OBJS_DIR)/$*.o
	@$(CC) -MM $(CPPFLAGS) $(CFLAGS) $*.c > $(OBJS_DIR)/$*.d
	@sed -i"" -r -e 's#.*\.o#obj/&#g' \
		-e ':a;$$!N;$$!ba;s# \\\n##g' \
		-e 'p;s/\\ //g' -e 's/.*: //' \
		-e '$$s/ |$$/:\n/g' $(OBJS_DIR)/$*.d

$(OBJS_DIR):
	mkdir -p $@

clean:
	rm -rf hcfs $(OBJS_DIR)/*.o $(OBJS_DIR)/*.d
