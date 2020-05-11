RACK_DIR ?= ../..

# FLAGS will be passed to both the C and C++ compiler
FLAGS += \
	-Idep/include \
	-I./src/ui \
	-I./src/model \
	-I./src/dsp-delay \
	-I./src/dsp-filter/utils -I./src/dsp-filter/filters -I./src/dsp-filter/third-party/falco	

CFLAGS += -g #-fno-omit-frame-pointer -fsanitize=address
CXXFLAGS += -g #-fno-omit-frame-pointer -fsanitize=address


# Add .cpp and .c files to the build
SOURCES += $(wildcard src/*.cpp src/filters/*.cpp src/dsp-noise/*.cpp src/dsp-filter/*.cpp  src/stmlib/*.cc)

# Add files to the ZIP package when running `make dist`
# The compiled plugin is automatically added.
DISTRIBUTABLES += $(wildcard LICENSE*) res

# Static libs
#libsamplerate := dep/lib/libsamplerate.a
#OBJECTS += $(libsamplerate)

# Dependencies
#DEP_LOCAL := dep
#DEPS += $(libsamplerate)


# Include the VCV plugin Makefile framework
include $(RACK_DIR)/plugin.mk
