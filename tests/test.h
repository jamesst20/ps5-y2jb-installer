/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_TEST_H
#define Y2JB_TEST_H

#include <stdio.h>
#include <string.h>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                                 \
  do {                                                                              \
    g_checks++;                                                                     \
    if (!(cond)) {                                                                  \
      g_failures++;                                                                 \
      fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);     \
    }                                                                               \
  } while (0)

#define CHECK_INT(actual, expected)                                                 \
  do {                                                                              \
    g_checks++;                                                                     \
    long long _a = (long long)(actual), _e = (long long)(expected);                 \
    if (_a != _e) {                                                                 \
      g_failures++;                                                                 \
      fprintf(stderr, "%s:%d: expected %lld, got %lld (%s)\n", __FILE__, __LINE__,  \
              _e, _a, #actual);                                                     \
    }                                                                               \
  } while (0)

#define CHECK_STR(actual, expected)                                                 \
  do {                                                                              \
    g_checks++;                                                                     \
    const char *_a = (actual), *_e = (expected);                                    \
    if (!_a || !_e || strcmp(_a, _e) != 0) {                                        \
      g_failures++;                                                                 \
      fprintf(stderr, "%s:%d: expected \"%s\", got \"%s\" (%s)\n", __FILE__,        \
              __LINE__, _e ? _e : "(null)", _a ? _a : "(null)", #actual);           \
    }                                                                               \
  } while (0)

#define TEST_MAIN_END()                                                             \
  do {                                                                              \
    printf("%d checks, %d failures\n", g_checks, g_failures);                       \
    return g_failures ? 1 : 0;                                                      \
  } while (0)

#endif
