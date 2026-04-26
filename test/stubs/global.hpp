#pragma once

// Use #include_next so that this wrapper header properly pulls in the real
// components/global/src/global.hpp from the next directory in the search path,
// avoiding the GCC multiple-include optimisation that would otherwise silently
// suppress the second lookup of the same file name.
#include_next "global.hpp"

#undef MAIN_FOLDER
#define MAIN_FOLDER "/home/turgu1/Dev/EPub-InkPlate/test/fixtures/config_data"
