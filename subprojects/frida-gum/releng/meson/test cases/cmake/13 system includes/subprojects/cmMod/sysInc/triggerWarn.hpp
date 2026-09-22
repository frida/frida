#pragma once

enum Foo {
    Hello,
    World
};

inline int bar( Foo foo ) {
  switch(foo) {
    case Hello: return 0;
    // Warn because of missung case for World
  }
  return 1;
}
