/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "device/download0.h"
#include "titles.h"
#include "test.h"

static const char *g_test_app_root;

/* A stand-in for the payload's appinfo.db resolver: returns a host temporary directory. */
static int test_app_root(const char *title_id, char *out, size_t cap) {
  int written = snprintf(out, cap, "%s/%s", g_test_app_root, title_id);
  return written > 0 && (size_t)written < cap ? 0 : -1;
}

static FILE *open_file(const char *path) {
  FILE *file = fopen(path, "wb");
  CHECK(file != NULL);
  return file;
}

static void write_text(const char *path, const char *text) {
  FILE *file = open_file(path);
  if (file) {
    fputs(text, file);
    fclose(file);
  }
}

/* Fills a file with `count` copies of c, then appends the marker. Used to force the marker
 * across the scanner's 1 MiB read boundary. */
static void write_filler_then_marker(const char *path, size_t count, const char *marker) {
  FILE *file = open_file(path);
  if (!file) {
    return;
  }
  char chunk[4096];
  memset(chunk, 'x', sizeof chunk);
  size_t left = count;
  while (left > 0) {
    size_t n = left < sizeof chunk ? left : sizeof chunk;
    fwrite(chunk, 1, n, file);
    left -= n;
  }
  fputs(marker, file);
  fclose(file);
}

int main(void) {
  const char *MARKER = "const autoloader_version = \"v1.0.3-185cdf8\";\n";
  const char *OLDER = "const autoloader_version = \"v1.0.2-254c85f\";\n";

  char title_path[256];
  CHECK_INT(title_app_pkg("PPSA01651", title_path, sizeof title_path), 0);
  CHECK_STR(title_path, "/user/app/PPSA01651/app.pkg");
  CHECK_INT(title_download0_path("PPSA01652", title_path, sizeof title_path), 0);
  CHECK_STR(title_path, "/user/download/PPSA01652/download0.dat");
  CHECK_INT(title_app_json("PPSA01650", title_path, sizeof title_path), 0);
  CHECK_STR(title_path, "/user/app/PPSA01650/app.json");
  CHECK_INT(title_app_pkg("PPSA00000", title_path, sizeof title_path), -1);
  CHECK_INT(title_index("PPSA01650"), 0);
  CHECK_INT(title_index("PPSA00000"), -1);

  /* The appinfo.db resolver decides where a title lives. Extended storage uses the same
   * helper, so a host temporary directory stands in for /mnt/ext1. */
  char app_root[] = "/tmp/y2jb-app-XXXXXX";
  CHECK(mkdtemp(app_root) != NULL);
  char title_dir[256];
  snprintf(title_dir, sizeof title_dir, "%s/PPSA01651", app_root);
  CHECK(mkdir(title_dir, 0700) == 0);
  char app_pkg[sizeof title_dir + sizeof "/app.pkg"];
  snprintf(app_pkg, sizeof app_pkg, "%s/app.pkg", title_dir);
  write_text(app_pkg, "installed youtube package");

  g_test_app_root = app_root;
  title_set_app_root_resolver(test_app_root);
  CHECK_INT(title_app_pkg("PPSA01651", title_path, sizeof title_path), 0);
  CHECK_STR(title_path, app_pkg);
  CHECK_INT(title_app_dir("PPSA01651", title_path, sizeof title_path), 0);
  CHECK_STR(title_path, title_dir);
  CHECK_INT(title_has_app("PPSA01651"), 1);

  /* A resolver answer without an app.pkg is not an installed title; keep the conventional
   * internal-storage path so callers still get a usable location. */
  CHECK(unlink(app_pkg) == 0);
  CHECK_INT(title_app_pkg("PPSA01651", title_path, sizeof title_path), 0);
  CHECK_STR(title_path, "/user/app/PPSA01651/app.pkg");
  CHECK_INT(title_has_app("PPSA01651"), 0);

  title_set_app_root_resolver(NULL);
  CHECK_INT(title_app_pkg("PPSA01651", title_path, sizeof title_path), 0);
  CHECK_STR(title_path, "/user/app/PPSA01651/app.pkg");
  CHECK(rmdir(title_dir) == 0);
  CHECK(rmdir(app_root) == 0);

  char dir[] = "/tmp/y2jb-download0-XXXXXX";
  CHECK(mkdtemp(dir) != NULL);
  char path[256];
  snprintf(path, sizeof path, "%s/download0.dat", dir);
  char version[80];
  const char *title = TITLES[0].title_id;

  /* No file, no version. */
  CHECK_INT(download0_scan_version_at(title, path), -1);
  download0_version(title, version, sizeof version);
  CHECK_STR(version, "");
  CHECK_STR(download0_version_state_name(title), "unknown");

  /* No marker, no version. */
  write_text(path, "not an autoloader image\n");
  CHECK_INT(download0_scan_version_at(title, path), 1);
  download0_version(title, version, sizeof version);
  CHECK_STR(version, "");
  CHECK_STR(download0_version_state_name(title), "unknown");

  /* The release string is the identity, not the file hash. */
  write_text(path, MARKER);
  CHECK_INT(download0_scan_version_at(title, path), 0);
  download0_version(title, version, sizeof version);
  CHECK_STR(version, "v1.0.3-185cdf8");
  CHECK_STR(download0_version_state_name(title), "ready");

  /* Replacing the image changes the reported version. */
  write_text(path, OLDER);
  CHECK_INT(download0_scan_version_at(title, path), 0);
  download0_version(title, version, sizeof version);
  CHECK_STR(version, "v1.0.2-254c85f");

  /* An invalid marker is skipped and a later valid one is used. */
  write_text(path, "const autoloader_version = \"nope\";\nconst autoloader_version = \"v1.0.1-0866fb4\";\n");
  CHECK_INT(download0_scan_version_at(title, path), 0);
  download0_version(title, version, sizeof version);
  CHECK_STR(version, "v1.0.1-0866fb4");

  /* Gezine-based images (the upstream base) use const version_string. */
  write_text(path, "// Relapse port by maj0r\nconst version_string = \"Y2JB 1.7 by Gezine\";\n");
  CHECK_INT(download0_scan_version_at(title, path), 0);
  download0_version(title, version, sizeof version);
  CHECK_STR(version, "Y2JB 1.7 by Gezine");
  CHECK_STR(download0_version_state_name(title), "ready");

  /* The marker can straddle the scanner's read boundary. */
  write_filler_then_marker(path, (size_t)(1024 * 1024) - 10, MARKER);
  CHECK_INT(download0_scan_version_at(title, path), 0);
  download0_version(title, version, sizeof version);
  CHECK_STR(version, "v1.0.3-185cdf8");

  /* Removing the file clears the reported version. */
  remove(path);
  CHECK_INT(download0_scan_version_at(title, path), -1);
  download0_version(title, version, sizeof version);
  CHECK_STR(version, "");
  CHECK_STR(download0_version_state_name(title), "unknown");

  remove(path);
  rmdir(dir);
  TEST_MAIN_END();
}
