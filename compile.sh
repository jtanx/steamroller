#!/bin/bash

g++-8 -DNDEBUG -O2 -std=c++17 -Wall -Wextra -pedantic `pkg-config --cflags libxml-2.0` steamroller.cc `pkg-config --libs libxml-2.0` -o steamroller -g -lstdc++fs
#g++-8 -O2 -std=c++17 -Wall `pkg-config --cflags libxml-2.0` steamroller.cc `pkg-config --libs --static libxml-2.0` -lpthread -ldl -static -o steamroller -g -lstdc++fs
#g++-8 -O2 -std=c++17 -Wall `pkg-config --cflags libxml-2.0` steamroller.cc  -l:libxml2.a -l:libicui18n.a -l:libicuuc.a -l:libicudata.a -l:libz.a -l:liblzma.a -ldl -lstdc++fs -g -o steamroller
