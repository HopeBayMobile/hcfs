-include ccache.mk

PROJS = HCFS HCFS_CLI API_SERV

all: $(PROJS)

$(PROJS) :
	make -C $@

clean:
	rm -rf libs obj

.PHONY: $(PROJS)
