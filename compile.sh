#!/bin/bash

g++-8 -O2 -std=c++17 -Wall `pkg-config --cflags libxml-2.0` steamroller.cc `pkg-config --libs libxml-2.0` -o steamroller -g -lstdc++fs
