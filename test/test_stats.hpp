// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

struct TestStats {
  int passed{0};
  int failed{0};

  [[nodiscard]] bool ok() const noexcept { return failed == 0; }
  [[nodiscard]] int total() const noexcept { return passed + failed; }
};
