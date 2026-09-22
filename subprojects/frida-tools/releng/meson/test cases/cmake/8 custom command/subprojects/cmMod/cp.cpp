#include <iostream>
#include <fstream>

using namespace std;

int main(int argc, char *argv[]) {
  if(argc < 3) {
    cerr << argv[0] << " requires an input and an output file!" << endl;
    return 1;
  }

  ifstream src(argv[1]);
  ofstream dst(argv[2]);

  if(!src.is_open()) {
    cerr << "Failed to open " << argv[1] << endl;
    return 2;
  }

  dst << src.rdbuf();
  return 0;
}
