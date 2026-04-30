#ifndef TEST_STUBS_GLOBAL_HPP
#define TEST_STUBS_GLOBAL_HPP

// Use #include_next so that this wrapper header properly pulls in the real
// components/global/src/global.hpp from the next directory in the search path.
// NOTE: intentionally using a traditional header guard instead of #pragma once
// to avoid a GCC bug where #pragma once can prevent #include_next from finding
// the next file when both files have #pragma once.
#include_next "global.hpp"

#undef MAIN_FOLDER
#define MAIN_FOLDER "/home/turgu1/Dev/EPub-InkPlate/test/fixtures/config_data"

#endif // TEST_STUBS_GLOBAL_HPP
