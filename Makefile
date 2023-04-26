CUR_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

CC = gcc
CFLAGS += -g -O2 -Wall -lpthread
LDFLAGS +=

DEPS_DIR  := $(CUR_DIR)/.deps$(LIB_SUFFIX)
DEPCFLAGS = -MD -MF $(DEPS_DIR)/$*.d -MP

SRC_FILES = $(wildcard *.c)
OBJ_FILES := mainmaster.c master.o outils.o
OBJ_FILESs := mainslave.c slave.o outils.o



EXE_FILES = $(SRC_FILES:.c=)

%.o:%.c $(DEPS_DIR)
	$(CC) $(CFLAGS) $(DEPCFLAGS) -c $(input) -o $(output)



mainmaster: $(OBJ_FILES)
	$(CC) -o $@ $(CFLAGS) $(OBJ_FILES) $(LDFLAGS)

mainslave: $(OBJ_FILESs)
	$(CC) -o $@ $(CFLAGS) $(OBJ_FILESs) $(LDFLAGS)








all: $(EXE_FILES)
	echo $(EXE_FILES)

clean:
	rm -f dsm_userspace $(OBJ_FILES)

.PHONY: all clean
