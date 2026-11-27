# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# JIANT FM on the MPC Sample: sample-base's layers (fetched into build/sample-base by build.sh) built with JIANT's
# app (src/) and JIANT's core (ports/core, built here for the same compiler). Run from this folder: ./build.sh.
BASE_SRC := build/sample-base/src
TARGET := build/jiant
APP_SOURCES := $(wildcard src/*.cpp src/*/*.cpp)
include build/sample-base/base.mk

CPPFLAGS += -I../core
# (newer g++ than Debian bookworm's no longer brings <cstdint> in with the other headers; sample-base's
# drivers/usb_gadget.h relies on it)
CXXFLAGS += -include cstdint
OBJECTS += build/core/jiant_core.o
$(TARGET): build/core/jiant_core.o
CORE_SOURCES := $(wildcard ../../firmware/src/*.c ../../firmware/src/*.h ../../web/emu/felucca_web.c ../../tests/hostsim.c ../core/jiant_core.*)
build/core/jiant_core.o: $(CORE_SOURCES)
	cd ../.. && CC="$(CC)" AR="$(AR)" CORE_OUT=ports/mpc-sample/build/core sh ports/core/build.sh
