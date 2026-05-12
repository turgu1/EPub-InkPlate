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
  -I components/config/src \
  -I components/fonts/src \
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
  components/config/src/config.cpp \
  components/config/src/fonts_db.cpp \
  components/fonts/src/fonts.cpp \
  components/fonts/src/ttf2.cpp \
  components/fonts/src/font.cpp \
  components/pugixml/src/pugixml.cpp \
  components/sys_functions/number_to_str.cpp \
  components/sys_functions/strlcpy.cpp \
  components/pictures/src/mypngle.cpp \
  components/pictures/src/jpeg_decoder.cpp \
  components/pictures/src/tjpgdec.cpp \
  components/pictures/src/picture.cpp \
  components/pictures/src/gif_decoder.cpp \
  components/pictures/src/gif_picture.cpp \
  components/pictures/src/svg_decoder.cpp \
  components/pictures/src/svg_picture.cpp \
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
SUPP_FILE := gtk3.supp

.PHONY: all clean valgrind_linux

all: $(BUILD)/$(TARGET)

# Run the GTK Linux binary under Valgrind, suppressing known GTK/GLib/Pango/
# Cairo/fontconfig one-time allocations.  Focuses the report on application
# leaks only.
#
# Usage:
#   make DEBUG=1          # build with debug symbols first
#   make valgrind_linux   # run under Valgrind
LINUX_EPUB ?= $(VALGRIND_EPUB)
valgrind_linux: $(BUILD)/$(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all \
	         --track-origins=yes --num-callers=30 \
	         --suppressions=$(SUPP_FILE) \
	         $(BUILD)/$(TARGET) "$(LINUX_EPUB)"

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
  -DDATE_TIME_RTC=1 \
  -DMAIN_FOLDER=\"$(CURDIR)/test/fixtures/config_data\"

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
  -I components/config/src \
  -I components/fonts/src \
  -I components/sys_functions/include \
  -I components/himem/src \
  -I components/simple_db/src \
  -I components/memory_pool/src \
  -I components/pictures/src \
  -I components/zip/src \
  -I components/pugixml/src \
  -I components/display_list/src \
  -I components/simple_list/src \
  $(FREETYPE_CFLAGS)

TEST_CXXFLAGS := -std=c++23 $(OPT_FLAGS) $(TEST_DEFINES) $(TEST_INCLUDES) \
                 -Wall -Wno-psabi -MMD -MP

TEST_SRC_C := \
  components/zip/src/miniz.c

TEST_SRC_CPP := \
  test/test_runner.cpp \
  test/test_gif_decoder.cpp \
  test/test_svg_decoder.cpp \
  test/test_himem.cpp \
  test/test_himem_pool.cpp \
  test/test_char_pool.cpp \
  test/test_fonts_cache.cpp \
  test/test_screen_stub.cpp \
  test/test_dom.cpp \
  test/test_simple_db.cpp \
  test/test_css.cpp \
  test/test_display_list.cpp \
  test/test_app_config.cpp \
  test/test_epub.cpp \
  test/test_unzip.cpp \
  test/test_simple_list.cpp \
  test/stubs.cpp \
  src/models/dom.cpp \
  src/models/css.cpp \
  src/models/epub.cpp \
  src/models/book_params.cpp \
  components/config/src/fonts_db.cpp \
  components/fonts/src/fonts.cpp \
  components/fonts/src/font.cpp \
  components/fonts/src/ttf2.cpp \
  components/pictures/src/mypngle.cpp \
  components/pictures/src/jpeg_decoder.cpp \
  components/pictures/src/gif_decoder.cpp \
  components/pictures/src/gif_picture.cpp \
  components/pictures/src/svg_decoder.cpp \
  components/pictures/src/svg_picture.cpp \
  components/zip/src/unzip.cpp \
  components/pugixml/src/pugixml.cpp \
  components/simple_db/src/simple_db.cpp \
  components/sys_functions/number_to_str.cpp \
  components/sys_functions/strlcpy.cpp \
  lib_linux/EPub_InkPlate/src/logging.cpp

TEST_OBJS_C   := $(patsubst %.c,$(TEST_BUILD)/%.o,$(TEST_SRC_C))
TEST_OBJS     := $(patsubst %.cpp,$(TEST_BUILD)/%.o,$(TEST_SRC_CPP)) $(TEST_OBJS_C)
TEST_DEPS     := $(TEST_OBJS:.o=.d)

.PHONY: test build_test clean_test all_tests \
  test_himem test_himem_pool_test test_char_pool test_fonts_cache test_fonts_cache_stress test_dom test_simple_db test_css \
  test_gif_decoder test_svg_decoder \
  test_display_list test_app_config test_epub test_unzip test_simple_list

build_test: $(TEST_BUILD)/$(TEST_TARGET)

test: $(TEST_BUILD)/$(TEST_TARGET)
	@echo "Running test suite..."
	@$(TEST_BUILD)/$(TEST_TARGET)

# Per-suite targets — build the test binary if needed then run a single suite.
test_himem:          $(TEST_BUILD)/$(TEST_TARGET) ; @$(TEST_BUILD)/$(TEST_TARGET) himem
test_himem_pool_test:$(TEST_BUILD)/$(TEST_TARGET) ; @$(TEST_BUILD)/$(TEST_TARGET) himem_pool_test
test_dom:            $(TEST_BUILD)/$(TEST_TARGET) ; @$(TEST_BUILD)/$(TEST_TARGET) dom
test_simple_db:      $(TEST_BUILD)/$(TEST_TARGET) ; @$(TEST_BUILD)/$(TEST_TARGET) simple_db
test_css:            $(TEST_BUILD)/$(TEST_TARGET) ; @$(TEST_BUILD)/$(TEST_TARGET) css
test_display_list:   $(TEST_BUILD)/$(TEST_TARGET) ; @$(TEST_BUILD)/$(TEST_TARGET) display_list
test_app_config:     $(TEST_BUILD)/$(TEST_TARGET) ; @$(TEST_BUILD)/$(TEST_TARGET) app_config
test_epub:           $(TEST_BUILD)/$(TEST_TARGET) ; @$(TEST_BUILD)/$(TEST_TARGET) epub
test_unzip:          $(TEST_BUILD)/$(TEST_TARGET) ; @$(TEST_BUILD)/$(TEST_TARGET) unzip
test_char_pool:      $(TEST_BUILD)/$(TEST_TARGET) ; @$(TEST_BUILD)/$(TEST_TARGET) char_pool
test_fonts_cache:    $(TEST_BUILD)/$(TEST_TARGET) ; @$(TEST_BUILD)/$(TEST_TARGET) fonts_cache
test_fonts_cache_stress: $(TEST_BUILD)/$(TEST_TARGET) ; @$(TEST_BUILD)/$(TEST_TARGET) fonts_cache_stress
test_gif_decoder:    $(TEST_BUILD)/$(TEST_TARGET) ; @$(TEST_BUILD)/$(TEST_TARGET) gif_decoder
test_svg_decoder:    $(TEST_BUILD)/$(TEST_TARGET) ; @$(TEST_BUILD)/$(TEST_TARGET) svg_decoder
test_simple_list:    $(TEST_BUILD)/$(TEST_TARGET) ; @$(TEST_BUILD)/$(TEST_TARGET) simple_list

# Convenience target: run both test suites in sequence.
all_tests: test config_test

$(TEST_BUILD)/$(TEST_TARGET): $(TEST_OBJS)
	@echo "Linking $@"
	@$(CXX) $(TEST_OBJS) -lpthread -lssl -lcrypto $(FREETYPE_LIBS) -o $@
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

# ---------------------------------------------------------------------------
# Standalone Config-class test
#
# Transitional helper while other suites are being adapted to the new fonts
# management flow integrated with Config.
#
# Usage:
#   make config_test        # build and run standalone Config test
#   make build_config_test  # build standalone Config test only
#   make clean_config_test  # remove standalone Config test artefacts
# ---------------------------------------------------------------------------

CONFIG_TEST_BUILD   := build_test_config
CONFIG_TEST_TARGET  := epub_config_test

CONFIG_TEST_DEFINES := \
  -DEPUB_LINUX_BUILD=1 \
  -DAPP_VERSION=\"$(APP_VERSION)\" \
  -DDATE_TIME_RTC=1

CONFIG_TEST_INCLUDES := \
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

CONFIG_TEST_CXXFLAGS := -std=c++23 $(OPT_FLAGS) $(CONFIG_TEST_DEFINES) $(CONFIG_TEST_INCLUDES) \
                        -Wall -Wno-psabi -MMD -MP

CONFIG_TEST_SRC_CPP := \
  test/test_config_standalone.cpp \
  components/config/fonts_db.cpp \
  components/pugixml/src/pugixml.cpp \
  components/sys_functions/number_to_str.cpp \
  components/sys_functions/strlcpy.cpp \
  lib_linux/EPub_InkPlate/src/logging.cpp

CONFIG_TEST_OBJS := $(patsubst %.cpp,$(CONFIG_TEST_BUILD)/%.o,$(CONFIG_TEST_SRC_CPP))
CONFIG_TEST_DEPS := $(CONFIG_TEST_OBJS:.o=.d)

.PHONY: config_test build_config_test clean_config_test

build_config_test: $(CONFIG_TEST_BUILD)/$(CONFIG_TEST_TARGET)

config_test: $(CONFIG_TEST_BUILD)/$(CONFIG_TEST_TARGET)
	@echo "Running standalone Config test..."
	@$(CONFIG_TEST_BUILD)/$(CONFIG_TEST_TARGET)

$(CONFIG_TEST_BUILD)/$(CONFIG_TEST_TARGET): $(CONFIG_TEST_OBJS)
	@echo "Linking $@"
	@$(CXX) $(CONFIG_TEST_OBJS) -lpthread -lssl -lcrypto -o $@
	@echo "Built: $@"

$(CONFIG_TEST_BUILD)/%.o: %.cpp
	@echo "Compiling (config-test) $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CONFIG_TEST_CXXFLAGS) -c $< -o $@

clean_config_test:
	rm -rf $(CONFIG_TEST_BUILD)

# ---------------------------------------------------------------------------
# Valgrind S5 build
#
# Headless Linux binary that runs the Scenario-5 (concurrent page navigation
# + mid-flight pageLocs restart) for a single epub file.  Use this target to
# detect memory leaks with Valgrind without needing a GTK display.
#
# Usage:
#   make DEBUG=1 valgrind_s5                   # build
#   make valgrind_run EPUB=/path/to/book.epub  # build + run under Valgrind
#   make clean_valgrind                        # remove build artefacts
#
# The binary can also be run directly:
#   build_valgrind/epub_valgrind_s5 [/path/to/book.epub]
# ---------------------------------------------------------------------------

VALGRIND_BUILD  := build_valgrind
VALGRIND_TARGET := epub_valgrind_s5

# test/stubs/ MUST come before lib_linux/ so that the headless screen.hpp and
# msg_viewer.hpp stubs shadow the real GTK-dependent headers.
VALGRIND_INCLUDES := \
  -I test/valgrind_stubs \
  -I test \
  -I src \
  -I src/controllers \
  -I src/helpers \
  -I src/models \
  -I src/viewers \
  -I lib_linux/EPub_InkPlate/src \
  -I components/global/src \
  -I components/config/src \
  -I components/fonts/src \
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

VALGRIND_DEFINES := \
  -DEPUB_LINUX_BUILD=1 \
  -DAPP_VERSION=\"$(APP_VERSION)\" \
  -DPNGLE_GRAYSCALE_OUTPUT=1 \
  -DPNGLE_NO_GAMMA_CORRECTION=1 \
  -DDATE_TIME_RTC=1

# Always build with debug symbols + no optimisation for useful Valgrind traces.
VALGRIND_CXXFLAGS := -std=c++23 -O0 -g3 -DDEBUGGING=0 \
                     $(VALGRIND_DEFINES) $(VALGRIND_INCLUDES) \
                     -Wall -Wno-psabi -MMD -MP

VALGRIND_SRC_C := \
  components/zip/src/miniz.c

VALGRIND_SRC_CPP := \
  test/linux_s5_valgrind.cpp \
  test/valgrind_stubs.cpp \
  src/helpers/picture_load_icon.cpp \
  src/models/book_params.cpp \
  src/models/css.cpp \
  src/models/dom.cpp \
  src/models/epub.cpp \
  src/models/page_locs.cpp \
  src/models/page_locs_control.cpp \
  src/models/page_locs_interpreter.cpp \
  src/models/page_locs_retriever.cpp \
  src/models/toc.cpp \
  src/viewers/html_interpreter.cpp \
  src/viewers/page.cpp \
  components/config/src/config.cpp \
  components/config/src/fonts_db.cpp \
  components/fonts/src/fonts.cpp \
  components/fonts/src/ttf2.cpp \
  components/fonts/src/font.cpp \
  components/pugixml/src/pugixml.cpp \
  components/zip/src/unzip.cpp \
  components/pictures/src/mypngle.cpp \
  components/pictures/src/jpeg_decoder.cpp \
  components/pictures/src/tjpgdec.cpp \
  components/pictures/src/picture.cpp \
  components/pictures/src/gif_decoder.cpp \
  components/pictures/src/gif_picture.cpp \
  components/pictures/src/svg_decoder.cpp \
  components/pictures/src/svg_picture.cpp \
  components/pictures/src/jpeg_picture.cpp \
  components/pictures/src/png_picture.cpp \
  components/simple_db/src/simple_db.cpp \
  components/display_list/src/display_list.cpp \
  components/sys_functions/number_to_str.cpp \
  components/sys_functions/strlcpy.cpp \
  lib_linux/EPub_InkPlate/src/logging.cpp

VALGRIND_OBJS_C   := $(patsubst %.c,$(VALGRIND_BUILD)/%.o,$(VALGRIND_SRC_C))
VALGRIND_OBJS     := $(patsubst %.cpp,$(VALGRIND_BUILD)/%.o,$(VALGRIND_SRC_CPP)) \
                     $(VALGRIND_OBJS_C)
VALGRIND_DEPS     := $(VALGRIND_OBJS:.o=.d)

.PHONY: valgrind_s5 valgrind_run clean_valgrind

valgrind_s5: $(VALGRIND_BUILD)/$(VALGRIND_TARGET)

$(VALGRIND_BUILD)/$(VALGRIND_TARGET): $(VALGRIND_OBJS)
	@echo "Linking $@"
	@$(CXX) $(VALGRIND_OBJS) -lpthread -lssl -lcrypto $(FREETYPE_LIBS) -o $@
	@echo "Built: $@"

$(VALGRIND_BUILD)/%.o: %.cpp
	@echo "Compiling (valgrind) $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(VALGRIND_CXXFLAGS) -c $< -o $@

$(VALGRIND_BUILD)/%.o: %.c
	@echo "Compiling (valgrind-C) $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(VALGRIND_DEFINES) $(VALGRIND_INCLUDES) -O0 -g3 -MMD -MP -c $< -o $@

# Convenience target: build then run under Valgrind.
# Override the epub path: make valgrind_run EPUB=/path/to/book.epub
VALGRIND_EPUB ?= /home/turgu1/Dev/EPub-InkPlate/SDCard/books/Austen, Jane - Pride and Prejudice.epub
valgrind_run: $(VALGRIND_BUILD)/$(VALGRIND_TARGET)
	valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all \
           --num-callers=30 --error-exitcode=1 \
           --suppressions=$(SUPP_FILE) \
	         $(VALGRIND_BUILD)/$(VALGRIND_TARGET) "$(VALGRIND_EPUB)"

clean_valgrind:
	rm -rf $(VALGRIND_BUILD)

-include $(VALGRIND_DEPS)

# Auto-generated header dependencies
-include $(CONFIG_TEST_DEPS)
-include $(TEST_DEPS)
-include $(DEPS)
