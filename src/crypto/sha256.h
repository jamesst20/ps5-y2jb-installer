/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef Y2JB_SHA256_H
#define Y2JB_SHA256_H

#include <stddef.h>

typedef struct {
  unsigned char data[64];
  unsigned int datalen;
  unsigned long long bitlen;
  unsigned int state[8];
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const unsigned char data[], size_t len);
void sha256_final(SHA256_CTX *ctx, unsigned char hash[]);

/* Hashes a file into a lowercase 64-character hex string. Returns 0 on success. */
int compute_sha256_file(const char *path, char out_hex[65]);

#endif
