# Makefile — Linux build for EPub-InkPlate
#
# This Makefile compiles the EPub-InkPlate application for Linux using g++. 
# It's mainly used to debug all the code that is not tight to the specifics 
# of the ESP32 platform, and to test the application on a desktop environment.
#
# Usage:
#   make          # build (release)
#   make DEBUG=1  # build with debug symbols
#   make clean    # remove build artefacts
#   make test     # build and run test suite (see below)

CXX := g++
CC  := gcc

TARGET      := epub_linux
BUILD       := build_linux
APP_VERSION := 3.0.0

# ---------------------------------------------------------------------------
# Debug / Release flags
# ---------------------------------------------------------------------------
ifdef DEBUG
  OPT_FLAGS := -O0 -g3 -DDEBUGGING=0
else
  OPT_FLAGS := -O2 -DDEBUGGING=0
endif

# ---------------------------------------------------------------------------
# External library flags (GTK+3 and FreeType)
# ---------------------------------------------------------------------------
GTK_CFLAGS      := $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS        := $(shell pkg-config --libs   gtk+-3.0)

FREETYPE_CFLAGS := $(shell pkg-config --cflags freetype2)
FREETYPE_LIBS   := $(shell pkg-config --libs   freetype2)

# ---------------------------------------------------------------------------
# Preprocessor definitions
# ---------------------------------------------------------------------------
DEFINES := \
  -DEPUB_LINUX_BUILD=1 \
  -DAPP_VERSION=\"$(APP_VERSION)\" \
  -DPNGLE_GRAYSCALE_OUTPUT=1 \
  -DPNGLE_NO_GAMMA_CORRECTION=1 \
  -DDATE_TIME_RTC=1

# ---------------------------------------------------------------------------
# Include paths
# lib_linux comes before components so that Linux-specific headers
# (screen.hpp, logging.hpp, alloc.hpp, non_copyable.hpp) shadow any
# ESP32-only versions.
# ---------------------------------------------------------------------------
INCLUDES := \
  -I src \
  -I src/controllers \
  -I src/helpers \
  -I src/models \
  -I src/viewers \
  -I lib_linux/EPub_InkPlate/src \
  -I components/global/src \
  -I components/sys_functions/include \
  -I components/pugixml/src \
  -I components/zip/src \
  -I components/pictures/src \
  -I components/memory_pool/src \
  -I components/himem/src \
  -I components/simple_db/src \
  -I components/display_list/src \
  -I components/simple_list/src \
  $(FREETYPE_CFLAGS)

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
CXXFLAGS := -std=c++23 $(OPT_FLAGS) $(DEFINES) $(INCLUDES) $(GTK_CFLAGS) $(FREETYPE_CFLAGS) \
            -Wall -Wno-psabi -MMD -MP

CFLAGS   := -std=c11   $(OPT_FLAGS) $(DEFINES) $(INCLUDES) $(GTK_CFLAGS) $(FREETYPE_CFLAGS) \
            -Wall -MMD -MP

# ---------------------------------------------------------------------------
# Linker flags
# ---------------------------------------------------------------------------
LDFLAGS := \
  $(GTK_LIBS) \
  $(FREETYPE_LIBS) \
  -lpthread \
  -lcrypto

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------

# Application source tree (all .cpp under src/, excluding the embed/ subtree)
SRC_CPP := $(shell find src -name '*.cpp' ! -path 'src/embed/*')

# Component sources compiled for Linux
SRC_CPP += \
  components/pugixml/src/pugixml.cpp \
  components/sys_functions/int_to_str.cpp \
  components/sys_functions/strlcpy.cpp \
  components/pictures/src/mypngle.cpp \
  components/pictures/src/tjpgdec.cpp \
  components/pictures/src/picture.cpp \
  components/pictures/src/jpeg_picture.cpp \
  components/pictures/src/png_picture.cpp \
  components/simple_db/src/simple_db.cpp \
  components/display_list/src/display_list.cpp \
  components/zip/src/unzip.cpp \
  lib_linux/EPub_InkPlate/src/logging.cpp \
  lib_linux/EPub_InkPlate/src/screen.cpp \

# Plain-C sources
SRC_C := \
  components/zip/src/miniz.c

# ---------------------------------------------------------------------------
# Object files (mirror source tree under BUILD/)
# ---------------------------------------------------------------------------
OBJS := $(patsubst %.cpp,$(BUILD)/%.o,$(SRC_CPP)) \
        $(patsubst %.c,  $(BUILD)/%.o,$(SRC_C))

DEPS := $(OBJS:.o=.d)

# ---------------------------------------------------------------------------
# Rules
# ---------------------------------------------------------------------------
.PHONY: all clean

all: $(BUILD)/$(TARGET)

$(BUILD)/$(TARGET): $(OBJS)
	@echo Linking $@
	@$(CXX) $(OBJS) $(LDFLAGS) -o $@
	@echo "Built: $@"

$(BUILD)/%.o: %.cpp
	@echo Compiling $<
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.c
	@echo Compiling $<
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

# ---------------------------------------------------------------------------
# Test suite
#
# Builds a standalone binary (build_test/epub_test) that exercises the
# components that can run without hardware or GTK:
#   • himem allocator / smart-pointer helpers
#   • DOM tree construction
#   • SimpleDB CRUD operations
#   • ConfigBase template (read / write / defaults)
#
# Usage:
#   make test          # build test binary and run it
#   make build_test    # build test binary only
#   make clean_test    # remove test build artefacts
# ---------------------------------------------------------------------------

TEST_BUILD   := build_test
TEST_TARGET  := epub_test

TEST_DEFINES := \
  -DEPUB_LINUX_BUILD=1 \
  -DAPP_VERSION=\"$(APP_VERSION)\" \
  -DDATE_TIME_RTC=1

TEST_INCLUDES := \
  -I test/stubs \
  -I test \
  -I src \
  -I src/controllers \
  -I src/helpers \
  -I src/models \
  -I src/viewers \
  -I lib_linux/EPub_InkPlate/src \
  -I components/global/src \
  -I components/sys_functions/include \
  -I components/himem/src \
  -I components/simple_db/src \
  -I components/memory_pool/src \
  -I components/pictures/src \
  -I components/zip/src \
  -I components/pugixml/src \
  -I components/display_list/src \
  -I components/simple_list/src

TEST_CXXFLAGS := -std=c++23 $(OPT_FLAGS) $(TEST_DEFINES) $(TEST_INCLUDES) \
                 -Wall -Wno-psabi -MMD -MP

TEST_SRC_C := \
  components/zip/src/miniz.c

TEST_SRC_CPP := \
  test/test_runner.cpp \
  test/test_himem.cpp \
  test/test_dom.cpp \
  test/test_simple_db.cpp \
  test/test_config.cpp \
  test/test_css.cpp \
  test/test_display_list.cpp \
  test/test_app_config.cpp \
  test/test_epub.cpp \
  test/test_simple_list.cpp \
  test/stubs.cpp \
  src/models/dom.cpp \
  src/models/css.cpp \
  src/models/epub.cpp \
  src/models/book_params.cpp \
  components/zip/src/unzip.cpp \
  components/pugixml/src/pugixml.cpp \
  components/simple_db/src/simple_db.cpp \
  components/sys_functions/int_to_str.cpp \
  components/sys_functions/strlcpy.cpp \
  lib_linux/EPub_InkPlate/src/logging.cpp

TEST_OBJS_C   := $(patsubst %.c,$(TEST_BUILD)/%.o,$(TEST_SRC_C))
TEST_OBJS     := $(patsubst %.cpp,$(TEST_BUILD)/%.o,$(TEST_SRC_CPP)) $(TEST_OBJS_C)
TEST_DEPS     := $(TEST_OBJS:.o=.d)

.PHONY: test build_test clean_test

build_test: $(TEST_BUILD)/$(TEST_TARGET)

test: $(TEST_BUILD)/$(TEST_TARGET)
	@echo "Running test suite..."
	@$(TEST_BUILD)/$(TEST_TARGET)

$(TEST_BUILD)/$(TEST_TARGET): $(TEST_OBJS)
	@echo "Linking $@"
	@$(CXX) $(TEST_OBJS) -lpthread -lssl -lcrypto -o $@
	@echo "Built: $@"

$(TEST_BUILD)/%.o: %.cpp
	@echo "Compiling (test) $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(TEST_CXXFLAGS) -c $< -o $@

$(TEST_BUILD)/%.o: %.c
	@echo "Compiling (test-C) $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(TEST_DEFINES) $(TEST_INCLUDES) -MMD -MP -c $< -o $@

clean_test:
	rm -rf $(TEST_BUILD)

# Auto-generated header dependencies
-include $(TEST_DEPS)
-include $(DEPS)
