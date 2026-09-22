#include "cmMod.hpp"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

void CmMod::asyncIncrement() {
  std::thread t1([this]() {
    std::this_thread::sleep_for(100ms);
    num += 1;
  });

  t1.join();
}
