#include "k2_crypto.h"

#include <stdbool.h>
#include <string.h>

static const uint8_t s_fwd[256] = {
    99, 124, 119, 123, 242, 107, 111, 197, 48, 1, 103, 43, 254, 215, 171, 118, 202, 130, 201, 125,
    250, 89, 71, 240, 173, 212, 162, 175, 156, 164, 114, 192, 183, 253, 147, 38, 54, 63, 247, 204,
    52, 165, 229, 241, 113, 216, 49, 21, 4, 199, 35, 195, 24, 150, 5, 154, 7, 18, 128, 226, 235,
    39, 178, 117, 9, 131, 44, 26, 27, 110, 90, 160, 82, 59, 214, 179, 41, 227, 47, 132, 83, 209,
    0, 237, 32, 252, 177, 91, 106, 203, 190, 57, 74, 76, 88, 207, 208, 239, 170, 251, 67, 77, 51,
    133, 69, 249, 2, 127, 80, 60, 159, 168, 81, 163, 64, 143, 146, 157, 56, 245, 188, 182, 218, 33,
    16, 255, 243, 210, 205, 12, 19, 236, 95, 151, 68, 23, 196, 167, 126, 61, 100, 93, 25, 115, 96,
    129, 79, 220, 34, 42, 144, 136, 70, 238, 184, 20, 222, 94, 11, 219, 224, 50, 58, 10, 73, 6, 36,
    92, 194, 211, 172, 98, 145, 149, 228, 121, 231, 200, 55, 109, 141, 213, 78, 169, 108, 86, 244,
    234, 101, 122, 174, 8, 186, 120, 37, 46, 28, 166, 180, 198, 232, 221, 116, 31, 75, 189, 139, 138,
    112, 62, 181, 102, 72, 3, 246, 14, 97, 53, 87, 185, 134, 193, 29, 158, 225, 248, 152, 17, 105,
    217, 142, 148, 155, 30, 135, 233, 206, 85, 40, 223, 140, 161, 137, 13, 191, 230, 66, 104, 65,
    153, 45, 15, 176, 84, 187, 22,
};

static const uint8_t u_key[16] = {
    113, 51, 98, 117, 94, 116, 49, 110, 113, 102, 90, 40, 112, 102, 36, 49,
};

static const uint8_t d_key[16] = {
    72, 64, 67, 70, 107, 82, 110, 122, 64, 75, 65, 116, 66, 74, 112, 50,
};

#define F2(x) ((uint8_t)(((x) & 0x80) ? ((x) << 1) ^ 0x011B : (x) << 1))

static uint8_t s_box(uint8_t x) {
    return s_fwd[x];
}

static void copy_n_bytes(uint8_t* d, const uint8_t* s, uint8_t nn) {
    while(nn >= 4) {
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        nn -= 4;
    }
    while(nn--) *d++ = *s++;
}

static void copy_and_key(uint8_t* d, const uint8_t* s, const uint8_t* k) {
    for(uint8_t i = 0; i < 16; i += 4) {
        *d++ = *s++ ^ *k++;
        *d++ = *s++ ^ *k++;
        *d++ = *s++ ^ *k++;
        *d++ = *s++ ^ *k++;
    }
}

static void shift_sub_rows(uint8_t st[16]) {
    st[0] = s_box(st[0]);
    st[4] = s_box(st[4]);
    st[8] = s_box(st[8]);
    st[12] = s_box(st[12]);

    uint8_t tt = st[1];
    st[1] = s_box(st[5]);
    st[5] = s_box(st[9]);
    st[9] = s_box(st[13]);
    st[13] = s_box(tt);

    tt = st[2];
    st[2] = s_box(st[10]);
    st[10] = s_box(tt);
    tt = st[6];
    st[6] = s_box(st[14]);
    st[14] = s_box(tt);

    tt = st[15];
    st[15] = s_box(st[11]);
    st[11] = s_box(st[7]);
    st[7] = s_box(st[3]);
    st[3] = s_box(tt);
}

static void mix_sub_columns(uint8_t dt[16], const uint8_t st[16]) {
    uint8_t j = 5;
    uint8_t k = 10;
    uint8_t l = 15;
    for(uint8_t i = 0; i < 16; i += 4) {
        uint8_t a = st[i];
        uint8_t b = st[j];
        j = (j + 4) & 15;
        uint8_t c = st[k];
        k = (k + 4) & 15;
        uint8_t d = st[l];
        l = (l + 4) & 15;
        uint8_t a1 = s_box(a);
        uint8_t b1 = s_box(b);
        uint8_t c1 = s_box(c);
        uint8_t d1 = s_box(d);
        uint8_t a2 = F2(a1);
        uint8_t b2 = F2(b1);
        uint8_t c2 = F2(c1);
        uint8_t d2 = F2(d1);
        dt[i] = a2 ^ b2 ^ b1 ^ c1 ^ d1;
        dt[i + 1] = a1 ^ b2 ^ c2 ^ c1 ^ d1;
        dt[i + 2] = a1 ^ b1 ^ c2 ^ d2 ^ d1;
        dt[i + 3] = a2 ^ a1 ^ b1 ^ c1 ^ d2;
    }
}

static void aes_set_key(int keytype, uint8_t key_sched[176]) {
    const uint8_t hi = 11 << 4;
    if(keytype == 1) {
        copy_n_bytes(key_sched, d_key, 16);
    } else {
        copy_n_bytes(key_sched, u_key, 16);
    }

    uint8_t t[4];
    uint8_t next = 16;
    for(uint8_t cc = 16, rc = 1; cc < hi; cc += 4) {
        for(uint8_t i = 0; i < 4; i++) t[i] = key_sched[cc - 4 + i];
        if(cc == next) {
            next += 16;
            uint8_t ttt = t[0];
            t[0] = s_box(t[1]) ^ rc;
            t[1] = s_box(t[2]);
            t[2] = s_box(t[3]);
            t[3] = s_box(ttt);
            rc = F2(rc);
        }
        const uint8_t tt = cc - 16;
        for(uint8_t i = 0; i < 4; i++) key_sched[cc + i] = key_sched[tt + i] ^ t[i];
    }
}

static const uint8_t s_inv[256] = {
    82, 9, 106, 213, 48, 54, 165, 56, 191, 64, 163, 158, 129, 243, 215, 251, 124, 227, 57, 130,
    155, 47, 255, 135, 52, 142, 67, 68, 196, 222, 233, 203, 84, 123, 148, 50, 166, 194, 35, 61,
    238, 76, 149, 11, 66, 250, 195, 78, 8, 46, 161, 102, 40, 217, 36, 178, 118, 91, 162, 73, 109,
    139, 209, 37, 114, 248, 246, 100, 134, 104, 152, 22, 212, 164, 92, 204, 93, 101, 182, 146,
    108, 112, 72, 80, 253, 237, 185, 218, 94, 21, 70, 87, 167, 141, 157, 132, 144, 216, 171, 0,
    140, 188, 211, 10, 247, 228, 88, 5, 184, 179, 69, 6, 208, 44, 30, 143, 202, 63, 15, 2, 193,
    175, 189, 3, 1, 19, 138, 107, 58, 145, 17, 65, 79, 103, 220, 234, 151, 242, 207, 206, 240,
    180, 230, 115, 150, 172, 116, 34, 231, 173, 53, 133, 226, 249, 55, 232, 28, 117, 223, 110, 71,
    241, 26, 113, 29, 41, 197, 137, 111, 183, 98, 14, 170, 24, 190, 27, 252, 86, 62, 75, 198, 210,
    121, 32, 154, 219, 192, 254, 120, 205, 90, 244, 31, 221, 168, 51, 136, 7, 199, 49, 177, 18, 16,
    89, 39, 128, 236, 95, 96, 81, 127, 169, 25, 181, 74, 13, 45, 229, 122, 159, 147, 201, 156, 239,
    160, 224, 59, 77, 174, 42, 245, 176, 200, 235, 187, 60, 131, 83, 153, 97, 23, 43, 4, 126, 186,
    119, 214, 38, 225, 105, 20, 99, 85, 33, 12, 125,
};

static uint8_t inv_sbox(uint8_t x) {
    return s_inv[x];
}

static void inv_shift_sub_rows(uint8_t st[16]) {
    st[0] = inv_sbox(st[0]);
    st[4] = inv_sbox(st[4]);
    st[8] = inv_sbox(st[8]);
    st[12] = inv_sbox(st[12]);

    uint8_t tt = st[13];
    st[13] = inv_sbox(st[9]);
    st[9] = inv_sbox(st[5]);
    st[5] = inv_sbox(st[1]);
    st[1] = inv_sbox(tt);

    tt = st[2];
    st[2] = inv_sbox(st[10]);
    st[10] = inv_sbox(tt);
    tt = st[6];
    st[6] = inv_sbox(st[14]);
    st[14] = inv_sbox(tt);

    tt = st[3];
    st[3] = inv_sbox(st[7]);
    st[7] = inv_sbox(st[11]);
    st[11] = inv_sbox(st[15]);
    st[15] = inv_sbox(tt);
}

static uint8_t gmul(uint8_t a, uint8_t b) {
    uint8_t p = 0;
    for(uint8_t i = 0; i < 8; i++) {
        if(b & 1) p ^= a;
        const bool hi = a & 0x80;
        a <<= 1;
        if(hi) a ^= 0x1B;
        b >>= 1;
    }
    return p;
}

static void inv_mix_columns(uint8_t dt[16]) {
    for(uint8_t i = 0; i < 16; i += 4) {
        const uint8_t a = dt[i];
        const uint8_t b = dt[i + 1];
        const uint8_t c = dt[i + 2];
        const uint8_t d = dt[i + 3];
        dt[i] = gmul(a, 0x0E) ^ gmul(b, 0x0B) ^ gmul(c, 0x0D) ^ gmul(d, 0x09);
        dt[i + 1] = gmul(a, 0x09) ^ gmul(b, 0x0E) ^ gmul(c, 0x0B) ^ gmul(d, 0x0D);
        dt[i + 2] = gmul(a, 0x0D) ^ gmul(b, 0x09) ^ gmul(c, 0x0E) ^ gmul(d, 0x0B);
        dt[i + 3] = gmul(a, 0x0B) ^ gmul(b, 0x0D) ^ gmul(c, 0x09) ^ gmul(d, 0x0E);
    }
}

static void aes_decrypt_block(int keytype, const uint8_t cipher[16], uint8_t plain[16]) {
    uint8_t key_sched[176];
    aes_set_key(keytype, key_sched);

    uint8_t state[16];
    copy_and_key(state, cipher, key_sched + 160);

    for(uint8_t r = 9; r > 0; r--) {
        inv_shift_sub_rows(state);
        copy_and_key(state, state, key_sched + r * 16);
        inv_mix_columns(state);
    }

    inv_shift_sub_rows(state);
    copy_and_key(plain, state, key_sched);
}

static void aes_encrypt_block(int keytype, const uint8_t plain[16], uint8_t cipher[16]) {
    uint8_t key_sched[176];
    aes_set_key(keytype, key_sched);

    uint8_t s1[16];
    copy_and_key(s1, plain, key_sched);

    uint8_t r = 1;
    for(; r < 10; r++) {
        uint8_t s2[16];
        mix_sub_columns(s2, s1);
        copy_and_key(s1, s2, key_sched + r * 16);
    }

    shift_sub_rows(s1);
    copy_and_key(cipher, s1, key_sched + r * 16);
}

void k2_crypto_create_key(const uint8_t uid[4], uint8_t key_out[6]) {
    uint8_t block[16];
    uint8_t cipher[16];
    uint8_t x = 0;
    for(size_t i = 0; i < 16; i++) {
        if(x >= 4) x = 0;
        block[i] = uid[x++];
    }
    aes_encrypt_block(0, block, cipher);
    memcpy(key_out, cipher, 6);
}

void k2_crypto_cipher_block(int encrypt, const uint8_t plain[16], uint8_t out[16]) {
    if(encrypt) {
        aes_encrypt_block(1, plain, out);
    } else {
        aes_decrypt_block(1, plain, out);
    }
}
