BIN      := unix_main
SRCDIR   := ..
CFLAGS   := -Wall -I../../include -DMDG_DYNAMIC_LIBRARY
LDFLAGS  := -L. -lmdglib -lm -lpthread -lrt -Wl,-rpath=.
SO_FILES := libmdglib.so

include ../Makefile.common

libmdglib.so:
	cp -p ../../binaries/sandbox/ubuntu/libmdglib.so .
