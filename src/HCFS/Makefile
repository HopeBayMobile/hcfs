CC = gcc
CPPFLAGS = -DDEDUP_ENABLE=0 -DENCRYPT_ENABLE=0 -DCOMPRESS_ENABLE=0 -D_FILE_OFFSET_BITS=64 -DARM_32bit_ -D_ANDROID_ENV_ -DDEBUG_ON
CFLAGS = -g -pthread -Wall -Wextra
LFLAGS = -lcurl -lfuse -lssl -lcrypto -lm -lsqlite3
GCC_VERSION_GE_49 := $(shell expr `gcc -dumpversion | cut -f1-2 -d.` \>= 4.9)
ifeq "$(GCC_VERSION_GE_49)" "1"
	CPPFLAGS += -fdiagnostics-color
endif

# [Autodependencies with GNU make]
# http://scottmcpeak.com/autodepend/autodepend.html

OBJS_DIR = obj/
OBJS_F := objmeta.o \
	mount_manager.o \
	FS_manager.o \
	api_interface.o \
	logger.o \
	lookup_count.o \
	file_present.o \
	hfuse_system.o \
	utils.o \
	super_block.o \
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
	dedup_table.o \
	path_reconstruct.o \
	pin_scheduling.o \
	monitor.o \
	dir_statistics.o \
	parent_lookup.o \
	atomic_tocloud.o \
	tocloud_tools.o \
	do_fallocate.o
OBJS = $(addprefix $(OBJS_DIR), $(OBJS_F))

hcfs: $(OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -g $(OBJS) -o hcfs $(LFLAGS)

# pull in dependency info for *existing* .o files
-include $(OBJS:.o=.d)

# compile and generate dependency info
$(OBJS_DIR)%.o: %.c | $(OBJS_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $*.c -o $(OBJS_DIR)$*.o
	@$(CC) -MM $(CPPFLAGS) $(CFLAGS) $*.c > $(OBJS_DIR)$*.d
	@sed -i"" -r -e ':a;$$!N;$$!ba;s# \\\n##g' -e 'p;s/\\ //g' -e 's/.*: //' -e '$$s/ |$$/:\n/g' $(OBJS_DIR)$*.d

$(OBJS_DIR):
	mkdir $@

clean:
	rm -rf hcfs $(OBJS_DIR)*.o $(OBJS_DIR)*.d
