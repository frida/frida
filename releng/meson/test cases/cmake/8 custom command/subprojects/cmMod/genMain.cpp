#include <iostream>

using namespace std;

int main() {
  cout << R"asd(
#include <iostream>
#include <fstream>

using namespace std;

int main(int argc, const char *argv[]) {
  if(argc < 2) {
    cerr << argv[0] << " requires an output file!" << endl;
    return 1;
  }
  ofstream out1(string(argv[1]) + ".hpp");
  ofstream out2(string(argv[1]) + ".cpp");
  out1 << R"(
#pragma once

#include <string>

std::string getStr();
)";

  out2 << R"(
#include ")" << argv[1] << R"(.hpp"

std::string getStr() {
  return "Hello World";
}
)";

  return 0;
}
)asd";

  return 0;
}
