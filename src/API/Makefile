CC = gcc
CPPFLAGS =
CFLAGS = -g -pthread -Wall -Wextra
LFLAGS = -lsqlite3 -ljansson

# [Autodependencies with GNU make]
# http://scottmcpeak.com/autodepend/autodepend.html

OBJS_DIR = obj/

socket_serv_OBJS = $(addprefix $(OBJS_DIR), \
		   socket_serv.o pin_ops.o hcfs_stat.o hcfs_sys.o utils.o)

test_OBJS = $(addprefix $(OBJS_DIR), \
	    test.o HCFS_api.o)

OBJS := $(socket_serv_OBJS) $(test_OBJS)

socket_serv: $(socket_serv_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -g $(socket_serv_OBJS) -o $@ $(LFLAGS)

test: $(test_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -g $(test_OBJS) -o $@ $(LFLAGS)

publish: doc
	sudo cp -r doc /mnt/nas/CloudDataSolution/Gateway_2.0/

doc: HCFS_api.c HCFS_api.h
	cldoc generate -c -- --basedir . --type html --static --merge docs  --output doc HCFS_api.h

clean:
	rm -rf $(OBJS_DIR)*.o $(OBJS_DIR)*.d socket_serv test

# pull in dependency info for *existing* .o files
-include $(OBJS:.o=.d)

# compile and generate dependency info
$(OBJS_DIR)%.o: %.c | $(OBJS_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $*.c -o $(OBJS_DIR)$*.o
	@$(CC) -MM $(CPPFLAGS) $(CFLAGS) $*.c > $(OBJS_DIR)$*.d
	@sed -i"" -r -e ':a;$$!N;$$!ba;s# \\\n##g' -e 'p;s/\\ //g' -e 's/.*: //' -e '$$s/ |$$/:\n/g' $(OBJS_DIR)$*.d

$(OBJS_DIR):
	mkdir $@
