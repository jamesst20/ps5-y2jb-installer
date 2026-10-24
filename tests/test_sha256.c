/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>

#include "crypto/sha256.h"
#include "test.h"

static void hex_of(const unsigned char *digest, char out[65]) {
  for (int i = 0; i < 32; i++) {
    sprintf(out + i * 2, "%02x", digest[i]);
  }
  out[64] = '\0';
}

static void check_message(const char *msg, const char *expected_hex) {
  SHA256_CTX ctx;
  unsigned char digest[32];
  char hex[65];
  sha256_init(&ctx);
  sha256_update(&ctx, (const unsigned char *)msg, strlen(msg));
  sha256_final(&ctx, digest);
  hex_of(digest, hex);
  CHECK_STR(hex, expected_hex);
}

int main(void) {
  /* NIST FIPS 180-2 test vectors. */
  check_message("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  check_message("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  check_message("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

  /* One million 'a' characters, fed in pieces of 1000. */
  SHA256_CTX ctx;
  unsigned char digest[32];
  char hex[65];
  unsigned char chunk[1000];
  memset(chunk, 'a', sizeof chunk);
  sha256_init(&ctx);
  for (int i = 0; i < 1000; i++) {
    sha256_update(&ctx, chunk, sizeof chunk);
  }
  sha256_final(&ctx, digest);
  hex_of(digest, hex);
  CHECK_STR(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");

  /* The file helper hashes the file's bytes. */
  const char *path = "build/test/sha256-abc.bin";
  FILE *f = fopen(path, "wb");
  CHECK(f != NULL);
  if (f) {
    fputs("abc", f);
    fclose(f);
  }
  char file_hex[65];
  CHECK_INT(compute_sha256_file(path, file_hex), 0);
  CHECK_STR(file_hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK_INT(compute_sha256_file("build/test/does-not-exist.bin", file_hex), -1);

  TEST_MAIN_END();
}
