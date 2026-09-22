#!/bin/sh
#
# This tests that using a shell as an intermediary between Meson and the
# actual unit test which dies due to a signal is still recorded correctly.
#
# The quotes are because the path may contain spaces.
"$1"
