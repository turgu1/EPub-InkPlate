#pragma once
#include <chrono>

class Duration {

public:
  Duration() : start_time(std::chrono::steady_clock::now()) {}

  void reset() { start_time = std::chrono::steady_clock::now(); }

  double elapsed_seconds() const {
    auto end_time                         = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    return elapsed.count();
  }

private:
  const char *TAG = "Duration";

  std::chrono::time_point<std::chrono::steady_clock> start_time;
};