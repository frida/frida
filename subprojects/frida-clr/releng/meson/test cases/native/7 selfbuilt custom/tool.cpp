#include<iostream>
#include<fstream>
#include<string>

using namespace std;

const char prefix[] = "int ";
const char suffix[] = " () {\n    return 52;}\n";

int main(int argc, char **argv) {
    if(argc != 3) {
        cout << "You is fail.\n";
        return 1;
    }
    ifstream is(argv[1], ifstream::binary);
    if(!is) {
        cout << "Opening input file failed.\n";
        return 1;
    }
    string funcname;
    is >> funcname;
    ofstream os(argv[2], ofstream::binary);
    if(!os) {
        cout << "Opening output file failed.\n";
        return 1;
    }
    os << prefix << funcname << suffix;
    os.close();
    if(!os.good()) {
        cout << "Writing data out failed.\n";
        return 1;
    }
    return 0;
}
