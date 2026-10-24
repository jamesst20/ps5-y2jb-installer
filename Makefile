# SPDX-License-Identifier: GPL-3.0-or-later

.DEFAULT_GOAL := help

PS5_PAYLOAD_SDK ?= /opt/ps5-payload-sdk
HOST_CC ?= cc

VERSION := $(shell cat VERSION)
GEN_VERSION := build/gen/version.h
PAYLOAD := build/y2jb-installer.elf

PURE_SRCS := \
	src/fw/firmware.c \
	src/crypto/sha256.c \
	third_party/cjson/cJSON.c

DEVICE_SRCS := \
	src/main.c \
	src/api/api_router.c \
	src/api/api_status.c \
	src/api/api_accounts.c \
	src/api/api_download0.c \
	src/api/api_power.c \
	src/api/api_upload.c \
	src/api/api_youtube.c \
	src/api/api_util.c \
	src/http/server.c \
	src/device/kernel_rw.c \
	src/device/netcheck.c \
	src/device/sysinfo.c \
	src/device/accounts.c \
	src/device/power.c \
	src/device/appdb.c \
	src/device/instance.c \
	src/device/notify.c \
	src/device/upload.c \
	src/device/download0.c \
	src/account/fakesignin.c \
	src/install/install.c \
	src/install/loopback.c \
	src/install/installed.c \
	third_party/offact/offact.c

HEADERS := $(shell find src -name '*.h' -type f) \
	third_party/cjson/cJSON.h \
	third_party/offact/offact.h

COMMON_CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic \
	-Isrc -Ithird_party/cjson

HOST_CFLAGS := $(COMMON_CFLAGS) -Itests -D_POSIX_C_SOURCE=200809L
ifeq ($(shell uname -s),Darwin)
HOST_CFLAGS += -D_DARWIN_C_SOURCE
endif

# The SDK's math.h trips -Wunreachable-code-generic-assoc inside vendored cJSON.c.
PAYLOAD_CFLAGS := $(COMMON_CFLAGS) -O2 -g \
	-Ibuild/gen -Ibuild/prefix/include -Ithird_party/offact \
	-Wno-unreachable-code-generic-assoc
PAYLOAD_LIBS := -Lbuild/prefix/lib \
	-lmicrohttpd -lsqlite3 -lkernel_sys -lSceSystemService -lSceAppInstUtil \
	-lSceRegMgr -lSceUserService -lSceNet -lSceNetCtl -lpthread

TEST_SRCS := $(wildcard tests/test_*.c)
TEST_BINS := $(patsubst tests/%.c,build/test/%,$(TEST_SRCS))
TEST_EXTRA_test_download0 := src/device/download0.c
TEST_LIBS_test_download0 := -lpthread

UI_INPUTS := $(shell find ui/src -type f) \
	ui/index.html ui/vite.config.js ui/svelte.config.js \
	ui/package.json ui/package-lock.json

ifneq ($(filter payload,$(MAKECMDGOALS)),)
  SDK_MAKEFILE := $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
  ifeq ($(wildcard $(SDK_MAKEFILE)),)
    $(error PS5 payload SDK not found at $(PS5_PAYLOAD_SDK); set PS5_PAYLOAD_SDK)
  endif
  include $(SDK_MAKEFILE)
endif

.PHONY: help test payload ui app clean version-check version-write

help:
	@printf '%s\n' \
		'Usage: make <target>' \
		'' \
		'  test           Host unit tests (no PS5 SDK needed)' \
		'  payload        Build the PS5 payload ELF' \
		'  ui             Build the Svelte frontend' \
		'  app            Build the desktop app (UI, payload, bundle)' \
		'  version-check  Verify every manifest matches VERSION' \
		'  version-write  Update every manifest from VERSION' \
		'  clean          Remove build outputs'

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "== $$t"; ./$$t || exit 1; done

build/test:
	@mkdir -p $@

build/test/%: tests/%.c $(PURE_SRCS) $(HEADERS) | build/test
	$(HOST_CC) $(HOST_CFLAGS) $< $(TEST_EXTRA_$*) $(PURE_SRCS) -o $@ $(TEST_LIBS_$*)

ui: ui/dist/index.html

ui/node_modules: ui/package-lock.json
	cd ui && npm ci

ui/dist/index.html: ui/node_modules $(UI_INPUTS)
	cd ui && npm run build

payload: $(PAYLOAD)

$(PAYLOAD): $(DEVICE_SRCS) $(PURE_SRCS) $(HEADERS) $(GEN_VERSION) | build
	@test -f build/prefix/lib/libmicrohttpd.a -a -f build/prefix/lib/libsqlite3.a || { \
		echo "error: payload dependencies are missing; run scripts/build_deps.sh" >&2; exit 1; }
	$(CC) $(PAYLOAD_CFLAGS) $(DEVICE_SRCS) $(PURE_SRCS) -o $@ $(PAYLOAD_LIBS)

$(GEN_VERSION): VERSION | build
	@mkdir -p $(dir $@)
	@printf '#ifndef Y2JB_VERSION_H\n#define Y2JB_VERSION_H\n\n#define Y2JB_VERSION "%s"\n\n#endif\n' '$(VERSION)' > $@

build:
	@mkdir -p $@

version-check:
	node scripts/version.mjs --check

version-write:
	node scripts/version.mjs --write

app:
	# CI=true keeps Tauri from running its Finder-prettifying AppleScript on DMG builds.
	cd app && CI=true npm run build

clean:
	rm -rf build/y2jb-installer.elf build/test build/gen ui/dist
