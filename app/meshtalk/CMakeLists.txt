# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(meshtalk)


target_sources(                     app PRIVATE src/meshtalk.c)
target_sources(                     app PRIVATE src/udprx.c)
target_sources(                     app PRIVATE src/udptx.c)
target_sources(                     app PRIVATE src/leds.c)
target_sources(                     app PRIVATE src/send.c)

include(${ZEPHYR_BASE}/samples/net/common/common.cmake)

set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)
