// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

/// Run the DOM instantiation test suite.
///
/// Verifies that DOM::Make() correctly allocates and constructs a DOM object
/// and that basic node-tree operations work as expected.
///
/// Results are logged individually as PASS or FAIL via printf on Linux/host
/// builds and via ESP_LOGI on EPUB_INKPLATE_BUILD targets.
///
/// @return true if every check passed, false if any check failed.
auto domRunTests() -> bool;
