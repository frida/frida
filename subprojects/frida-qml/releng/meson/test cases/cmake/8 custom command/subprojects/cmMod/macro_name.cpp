#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

using namespace std;

#ifdef TEST_CMD_INCLUDE
#if CPY_INC_WAS_INCLUDED != 1
#error "cpyInc.hpp was not included"
#endif
#endif

int main() {
  this_thread::sleep_for(chrono::seconds(1));
  ofstream out1("macro_name.txt");
  out1 << "FOO";

  return 0;
}
