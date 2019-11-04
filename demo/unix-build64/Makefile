BIN      := unix_main
SRCDIR   := ..
CFLAGS   := -Wall -I../../include -DMDG_DYNAMIC_LIBRARY
LDFLAGS  := -L. -lmdglib -lm -lpthread -lrt
SO_FILES := libmdglib.so

include ../Makefile.common

launch-unix_main: unix_main
	echo Launching runix chat demo, dynamically linked:
	LD_LIBRARY_PATH=.:${LD_LIBRARY_PATH} ./unix_main

libmdglib.so:
	cp -p ../../binaries/sandbox/ubuntu/libmdglib.so .
