#!/bin/sh

echo $(uname -s | tr '[A-Z]' '[a-z]' | sed 's,^darwin$,macos,')
