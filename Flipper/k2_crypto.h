#pragma once

#include <stdint.h>
#include <stddef.h>

/** UID-derived MIFARE key (u_key / keytype 0). */
void k2_crypto_create_key(const uint8_t uid[4], uint8_t key_out[6]);

/** Encrypt or decrypt 16-byte block (keytype 1 = data, uses d_key). */
void k2_crypto_cipher_block(int encrypt, const uint8_t plain[16], uint8_t out[16]);
