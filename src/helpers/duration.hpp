#pragma once
#include <chrono>

class Duration {

  public:
    Duration() : start_time(std::chrono::steady_clock::now()) {}

    auto reset() -> void { start_time = std::chrono::steady_clock::now(); }

    auto elapsedSeconds() const -> double {
      auto                          end_time                         = std::chrono::steady_clock::now();
      std::chrono::duration<double> elapsed = end_time - start_time;
      return elapsed.count();
    }

  private:
    static constexpr const char *TAG = "Duration";

    std::chrono::time_point<std::chrono::steady_clock> start_time;
};