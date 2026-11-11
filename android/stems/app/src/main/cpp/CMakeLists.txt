# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
cmake_minimum_required(VERSION 3.18)
project(jiantstems C)
add_library(jiantstems SHARED usbstream.c)
