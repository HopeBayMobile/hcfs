CC = gcc
CFLAGS = -g -pthread -Wall -Wextra
LDFLAGS = -lsqlite3 -ljansson -lcrypto

# [Autodependencies with GNU make]
# http://scottmcpeak.com/autodepend/autodepend.html

OBJS_DIR = obj/

hcfsapid_OBJS = \
	socket_serv.o \
	pin_ops.o \
	hcfs_stat.o \
	hcfs_sys.o \
	enc.o \
	socket_util.o \
	logger.o \
	smart_cache.o
hcfsapid_OBJS := $(addprefix $(OBJS_DIR),$(hcfsapid_OBJS))

test_OBJS = \
	test.o \
	HCFS_api.o
test_OBJS := $(addprefix $(OBJS_DIR),$(test_OBJS))

hcfsconf_OBJS = \
	hcfsconf.o \
	enc.o \
	logger.o
hcfsconf_OBJS := $(addprefix $(OBJS_DIR),$(hcfsconf_OBJS))

deps =
OBJS := $(hcfsapid_OBJS) $(test_OBJS) $(hcfsconf_OBJS)
deps += $(OBJS:%.o=%.o.d)

hcfsapid: $(hcfsapid_OBJS)
	$(CC) $(CFLAGS) -g $(hcfsapid_OBJS) -o $@ $(LDFLAGS)

test: $(test_OBJS)
	$(CC) $(CFLAGS) -g $(test_OBJS) -o $@ $(LDFLAGS)

hcfsconf: $(hcfsconf_OBJS)
	$(CC) $(CFLAGS) -g $(hcfsconf_OBJS) -o $@ $(LDFLAGS)

publish: doc
	sudo cp -r doc /mnt/nas/CloudDataSolution/Gateway_2.0/

doc: HCFS_api.c HCFS_api.h
	cldoc generate -c -- --static --basedir . --type html --merge docs --output doc HCFS_api.h

clean:
	rm -rf $(OBJS_DIR) hcfsapid test hcfsconf

# compile and generate dependency info
$(OBJS_DIR)%.o: %.c | $(OBJS_DIR)
	$(CC) -c $(CFLAGS) $(realpath $<) -o $@ -MMD -MF $@.d

$(OBJS_DIR):
	mkdir -p $@

-include $(deps)
