# Build all 3rdparty libraries for cross-compilation to ARM musl Linux (V853)
# Order: zlib → alsa-lib → openssl → ffmpeg → libevdev
#
# Usage:
#   make all              Build enabled libraries in order
#   make clean            Clean all build artifacts
#   make <lib>            Build a specific library (e.g. make zlib)

TOOLCHAIN    := /usr/x-tools/arm-unknown-linux-musleabihf/bin/arm-unknown-linux-musleabihf
CC           := $(TOOLCHAIN)-gcc
CXX          := $(TOOLCHAIN)-g++
AR           := $(TOOLCHAIN)-ar
RANLIB       := $(TOOLCHAIN)-ranlib
STRIP        := $(TOOLCHAIN)-strip
CROSS_PREFIX := $(TOOLCHAIN)-

LIBS         := $(CURDIR)/../libs
JOBS         := $(shell nproc 2>/dev/null || echo 4)

# --------------------------------------------------------------------
# Default target
# --------------------------------------------------------------------
.PHONY: all
all: zlib alsa-lib openssl ffmpeg libevdev

# --------------------------------------------------------------------
# zlib (shared + static)
# --------------------------------------------------------------------
zlib:
	@echo "=== Building zlib ==="
	cd zlib && \
	CC="$(CC)" CFLAGS="-fPIC" ./configure --shared --prefix=$(LIBS)/zlib
	+$(MAKE) -C zlib -j$(JOBS)
	+$(MAKE) -C zlib install

# --------------------------------------------------------------------
# alsa-lib (autotools, static)
# --------------------------------------------------------------------
alsa-lib: zlib
	@echo "=== Building alsa-lib ==="
	cd alsa-lib && \
	autoreconf -i && \
	PKG_CONFIG_PATH="$(LIBS)/zlib/lib/pkgconfig" \
	./configure \
		--host=arm-unknown-linux-musleabihf \
		--prefix=$(LIBS)/alsa \
		--enable-shared \
		--disable-python --disable-alisp --disable-old-symbols \
		--disable-rawmidi --disable-seq --disable-hwdep --disable-ucm \
		CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" STRIP="$(STRIP)"
	+$(MAKE) -C alsa-lib -j$(JOBS)
	+$(MAKE) -C alsa-lib install

# --------------------------------------------------------------------
# openssl (static, no apps/docs)
# --------------------------------------------------------------------
openssl: zlib
	@echo "=== Building OpenSSL ==="
	cd openssl && \
	./Configure linux-generic32 \
		--prefix=$(LIBS)/openssl \
		--cross-compile-prefix="$(CROSS_PREFIX)" \
		 no-dso no-tests no-apps no-docs \
		-DOPENSSL_NO_SECURE_MEMORY
	+$(MAKE) -C openssl -j$(JOBS)
	+$(MAKE) -C openssl install_sw

# --------------------------------------------------------------------
# ffmpeg (static, with zlib + openssl support)
# --------------------------------------------------------------------
ffmpeg: zlib alsa-lib openssl
	@echo "=== Building FFmpeg ==="
	cd ffmpeg && \
	PKG_CONFIG_PATH="$(LIBS)/zlib/lib/pkgconfig:$(LIBS)/alsa/lib/pkgconfig:$(LIBS)/openssl/lib/pkgconfig" \
	./configure \
		--enable-cross-compile \
		--target-os=linux \
		--arch=arm \
		--cpu=cortex-a7 \
		--cross-prefix="$(CROSS_PREFIX)" \
		--prefix=$(LIBS)/ffmpeg \
		--enable-shared \
		--extra-cflags="-I$(LIBS)/zlib/include -I$(LIBS)/openssl/include -I$(LIBS)/alsa/include -mfpu=neon -mcpu=cortex-a7" \
		--extra-ldflags="-L$(LIBS)/zlib/lib -L$(LIBS)/alsa/lib -L$(LIBS)/openssl/lib" \
		--extra-libs="-lpthread -lm -ldl" \
		--enable-zlib --enable-openssl \
		--disable-debug --disable-doc \
		--disable-everything \
		--enable-decoder=mjpeg,png,bmp \
		--enable-demuxer=image2,mov,mp4,avi,matroska,ogg,wav \
		--enable-parser=mjpeg,png,bmp \
		--enable-protocol=file \
		--enable-filter=scale,format \
		--enable-swscale --enable-swresample
	+$(MAKE) -C ffmpeg -j$(JOBS)
	+$(MAKE) -C ffmpeg install

# --------------------------------------------------------------------
# libevdev (autotools, static)
# --------------------------------------------------------------------
libevdev: zlib
	@echo "=== Building libevdev ==="
	cd libevdev && \
	autoreconf -i && \
	./configure \
		--host=arm-unknown-linux-musleabihf \
		--prefix=$(LIBS)/evdev \
		--enable-shared \
		CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" STRIP="$(STRIP)"
	+$(MAKE) -C libevdev -j$(JOBS)
	+$(MAKE) -C libevdev install

# --------------------------------------------------------------------
# Meta targets
# --------------------------------------------------------------------
.PHONY: clean clean-cache clean-all help zlib alsa-lib openssl ffmpeg libevdev

clean:
	@echo "=== Cleaning all 3rdparty builds ==="
	-for d in zlib alsa-lib openssl ffmpeg libevdev; do \
		[ -f $$d/Makefile ] && $(MAKE) -C $$d clean 2>/dev/null || true; \
	done
	rm -rf $(LIBS)/zlib $(LIBS)/alsa $(LIBS)/openssl $(LIBS)/ffmpeg $(LIBS)/evdev

clean-cache:
	@echo "=== Cleaning build caches ==="
	find zlib alsa-lib openssl ffmpeg libevdev -type d -name '.deps' -exec rm -rf {} + 2>/dev/null || true
	find zlib alsa-lib openssl ffmpeg libevdev -type d -name 'autom4te.cache' -exec rm -rf {} + 2>/dev/null || true
	rm -rf ffmpeg/ffbuild

clean-all: clean clean-cache
	for d in zlib alsa-lib openssl ffmpeg libevdev; do \
		rm -f $$d/Makefile $$d/configure; \
	done

help:
	@echo "Targets:"
	@echo "  make all        Build all libraries (default)"
	@echo "  make zlib       Build zlib only"
	@echo "  make alsa-lib   Build alsa-lib (requires zlib)"
	@echo "  make openssl    Build OpenSSL (requires zlib)"
	@echo "  make ffmpeg     Build FFmpeg (requires zlib, alsa-lib, openssl)"
	@echo "  make libevdev   Build libevdev (requires zlib)"
	@echo "  make clean       Remove build artifacts"
	@echo "  make clean-cache Remove autotools/cmake caches"
	@echo "  make clean-all   Deep clean (caches + generated configure)"
	@echo ""
	@echo "Toolchain: $(TOOLCHAIN)"
	@echo "Lib prefix: $(LIBS)"
