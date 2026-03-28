// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

/// Run the full himem capability test suite.
///
/// Every check is logged individually as PASS or FAIL via ESP_LOGI on
/// EPUB_INKPLATE_BUILD targets, and via printf on Linux/host builds.
/// On INKPLATE builds, pointer-in-PSRAM assertions use esp_ptr_external_ram().
/// On Linux builds those assertions are skipped (always pass) so the suite
/// can still be executed as a smoke-test on the host.
///
/// @return true if every check passed, false if any check failed.
bool himem_run_tests();
