#!/bin/bash

set -e

testFN() {
  set +e
  false
}

testFN
false
exit 0
