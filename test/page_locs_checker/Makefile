CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
LDFLAGS :=

TARGET := page_locs_checker
TOP_BUILD_DIR := ../../build_page_locs_checker
OUT := $(TOP_BUILD_DIR)/$(TARGET)
SRC := src/main.cpp

.PHONY: all clean run

all: $(OUT)

$(OUT): $(SRC)
	@mkdir -p $(TOP_BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

run: $(OUT)
	./$(OUT)

clean:
	rm -f $(OUT)