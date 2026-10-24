/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "fw/firmware.h"
#include "test.h"

static void check_parse(const char *kv, const char *expected) {
  char out[64];
  fw_from_kernel_version(kv, out, sizeof out);
  CHECK_STR(out, expected);
}

int main(void) {
  check_parse("FreeBSD 11.0-RELEASE-p0 #1 r218215/releases/09.60: Jul 18 2023", "9.60");
  check_parse("FreeBSD 11.0-RELEASE #0 releases/13.00", "13.00");
  check_parse("FreeBSD 11.0-RELEASE #0 releases/12.40", "12.40");
  check_parse("some build 12.60: date", "12.60");
  check_parse("", "unknown");

  /* The FreeBSD base version 11.0 has one digit after the point and never matches. */
  check_parse("FreeBSD 11.0-RELEASE #0", "FreeBSD 11.0-RELEASE #0");

  /* A string with no version comes back as is, with control characters replaced. */
  check_parse("odd\tbuild\n", "odd?build");

  char out[8];
  fw_from_kernel_version("FreeBSD 11.0-RELEASE #0 releases/13.00", out, sizeof out);
  CHECK_STR(out, "13.00");

  CHECK_INT(fw_code("4.03"), 403);
  CHECK_INT(fw_code("13.00"), 1300);
  CHECK_INT(fw_code("13.60"), 1360);
  CHECK_INT(fw_code("unknown"), -1);
  CHECK_INT(fw_code("13.0"), -1);
  CHECK_INT(fw_code("13.000"), -1);
  CHECK_INT(fw_code(""), -1);
  CHECK_INT(fw_code(NULL), -1);

  TEST_MAIN_END();
}
