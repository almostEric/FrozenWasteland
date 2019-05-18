# FLAGS will be passed to both the C and C++ compiler
FLAGS += \
	-DTEST \
	-I./eurorack \
	-I./src/ui \
	-I./src/dsp-delay \
	-I./src/dsp-filter/utils -I./src/dsp-filter/filters -I./src/dsp-filter/third-party/falco \
	-Wno-unused-local-typedefs

CFLAGS +=
CXXFLAGS +=

# Careful about linking to shared libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine.
LDFLAGS +=

# Add .cpp and .c files to the build
SOURCES += $(wildcard src/*.cpp src/filters/*.cpp src/dsp-noise/*.cpp src/dsp-filter/*.cpp rc/dsp-delay/*.hpp src/stmlib/*.cc)

# Add files to the ZIP package when running `make dist`
# The compiled plugin is automatically added.
DISTRIBUTABLES += $(wildcard LICENSE*) 

# Static libs
libsamplerate := dep/lib/libsamplerate.a
OBJECTS += $(libsamplerate)

# Dependencies
DEP_LOCAL := dep
DEPS += $(libsamplerate)

$(libsamplerate):
	cd dep && $(WGET) http://www.mega-nerd.com/SRC/libsamplerate-0.1.9.tar.gz
	cd dep && $(UNTAR) libsamplerate-0.1.9.tar.gz
	cd dep/libsamplerate-0.1.9 && $(CONFIGURE)
	cd dep/libsamplerate-0.1.9/src && $(MAKE)
	cd dep/libsamplerate-0.1.9/src && $(MAKE) install

# Include the VCV plugin Makefile framework
RACK_DIR ?= ../..
include $(RACK_DIR)/plugin.mk
