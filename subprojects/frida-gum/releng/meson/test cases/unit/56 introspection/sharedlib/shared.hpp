#pragma once

class SharedClass {
  private:
    int number = 42;
  public:
    SharedClass() = default;
    void doStuff();
    int getNumber() const;
};