CC       := armv7a-linux-androideabi16-clang
BIN      := android_main
SRCDIR   := ..
CFLAGS   := -Wall -I../../include -DMDG_DYNAMIC_LIBRARY -fPIE
LDFLAGS  := -L. -lmdglib-java -lm -Wl,-rpath=.
SO_FILES := libmdglib-java.so
PREFIX   := /data/DEVICom

include ../Makefile.common

libmdglib-java.so : ../../binaries/mdg-lib-android.aar
	unzip -j $< jni/armeabi/$@
	touch $@

install: android_main
	MSYS2_ARG_CONV_EXCL=$(PREFIX) adb push ../run.sh android_main $(SO_FILES) $(PREFIX)/
	MSYS2_ARG_CONV_EXCL=$(PREFIX) adb shell chmod 0755 $(PREFIX)/*
