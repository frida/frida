#include <memory>

class Dummy {
  int x;
};

int foo() {
  auto obj = std::make_unique<Dummy>();
  return 0;
}
