// Host stub for the rescue tests.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
extern uint8_t test_rp_flash[];
#define XIP_BASE ((uintptr_t)test_rp_flash)
uint64_t time_us_64(void);
