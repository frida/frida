#!/usr/bin/env python3

from gluon import gluonator

print('Running mainprog from root dir.')

if gluonator.gluoninate() != 42:
    raise ValueError("!= 42")
