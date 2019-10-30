CC       := armv7a-linux-androideabi16-clang
BIN      := android_main
SRCDIR   := ..
CFLAGS   := -Wall -I../../include -DMDG_DYNAMIC_LIBRARY -fPIE
LDFLAGS  := -L. -lmdglib-java -lm
SO_FILES := libmdglib-java.so

include ../Makefile.common

libmdglib-java.so : ../../binaries/mdg-lib-android.aar
	unzip -j $< jni/armeabi/$@
	touch $@
