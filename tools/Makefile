CC      = /usr/x-tools/arm-unknown-linux-musleabihf/bin/arm-unknown-linux-musleabihf-gcc
CFLAGS  = -s -Os -mfpu=neon -Wall
INCDIR  = .
LIBDIR  = /tmp/rootfs_mnt/usr/lib/dlam
RPATH   = /usr/lib/dlam

SYSROOT = /tmp/rootfs_mnt

all: test_libmb test_stbi

test_libmb: test_libmb.c libMb.h
	$(CC) $(CFLAGS) -I$(INCDIR) -L$(LIBDIR) \
		-Wl,-rpath,$(RPATH) -Wl,-rpath-link,$(SYSROOT)/usr/lib \
		-o $@ test_libmb.c -lMb -lm

test_stbi: test_stbi.c libMb.h
	$(CC) $(CFLAGS) -I$(INCDIR) -L$(LIBDIR) \
		-Wl,-rpath,$(RPATH) -Wl,-rpath-link,$(SYSROOT)/usr/lib \
		-o $@ test_stbi.c -lMb -lm

clean:
	rm -f test_libmb test_stbi

.PHONY: clean
