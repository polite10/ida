/*
 * ozet.c — SHA-256 (FIPS 180-4 §6.2). Sabitler işaretçisiz salt okunur tablodadır.
 */
#include "param/ozet.h"

static const uint32_t K256[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
    0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
    0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
    0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
    0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
    0xc67178f2u,
};

static uint32_t sag_dondur(uint32_t x, uint32_t n)
{
    return (x >> n) | (x << (32u - n));
}

static void blok_isle(ida_sha256_t *s)
{
    uint32_t w[64];
    for (uint32_t t = 0u; t < 16u; t++) {
        const uint32_t i = 4u * t;
        w[t] = ((uint32_t)s->blok[i] << 24) | ((uint32_t)s->blok[i + 1u] << 16) |
               ((uint32_t)s->blok[i + 2u] << 8) | (uint32_t)s->blok[i + 3u];
    }
    for (uint32_t t = 16u; t < 64u; t++) {
        const uint32_t s0 = sag_dondur(w[t - 15u], 7u) ^ sag_dondur(w[t - 15u], 18u) ^ (w[t - 15u] >> 3);
        const uint32_t s1 = sag_dondur(w[t - 2u], 17u) ^ sag_dondur(w[t - 2u], 19u) ^ (w[t - 2u] >> 10);
        w[t] = w[t - 16u] + s0 + w[t - 7u] + s1;
    }
    uint32_t a = s->h[0];
    uint32_t b = s->h[1];
    uint32_t c = s->h[2];
    uint32_t d = s->h[3];
    uint32_t e = s->h[4];
    uint32_t f = s->h[5];
    uint32_t g = s->h[6];
    uint32_t h = s->h[7];
    for (uint32_t t = 0u; t < 64u; t++) {
        const uint32_t S1 = sag_dondur(e, 6u) ^ sag_dondur(e, 11u) ^ sag_dondur(e, 25u);
        const uint32_t ch = (e & f) ^ ((~e) & g);
        const uint32_t t1 = h + S1 + ch + K256[t] + w[t];
        const uint32_t S0 = sag_dondur(a, 2u) ^ sag_dondur(a, 13u) ^ sag_dondur(a, 22u);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t t2 = S0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    s->h[0] += a;
    s->h[1] += b;
    s->h[2] += c;
    s->h[3] += d;
    s->h[4] += e;
    s->h[5] += f;
    s->h[6] += g;
    s->h[7] += h;
}

void ida_sha256_basla(ida_sha256_t *s)
{
    s->h[0] = 0x6a09e667u;
    s->h[1] = 0xbb67ae85u;
    s->h[2] = 0x3c6ef372u;
    s->h[3] = 0xa54ff53au;
    s->h[4] = 0x510e527fu;
    s->h[5] = 0x9b05688cu;
    s->h[6] = 0x1f83d9abu;
    s->h[7] = 0x5be0cd19u;
    s->blok_n = 0u;
    s->toplam_bit = 0u;
}

void ida_sha256_ekle(ida_sha256_t *s, const uint8_t *veri, uint32_t uzunluk)
{
    for (uint32_t i = 0u; i < uzunluk; i++) {
        s->blok[s->blok_n] = veri[i];
        s->blok_n++;
        s->toplam_bit += 8u;
        if (s->blok_n == 64u) {
            blok_isle(s);
            s->blok_n = 0u;
        }
    }
}

void ida_sha256_bitir(ida_sha256_t *s, uint8_t ozet[32])
{
    const uint64_t bit = s->toplam_bit;
    s->blok[s->blok_n] = 0x80u;
    s->blok_n++;
    if (s->blok_n > 56u) {
        while (s->blok_n < 64u) {
            s->blok[s->blok_n] = 0u;
            s->blok_n++;
        }
        blok_isle(s);
        s->blok_n = 0u;
    }
    while (s->blok_n < 56u) {
        s->blok[s->blok_n] = 0u;
        s->blok_n++;
    }
    for (uint32_t i = 0u; i < 8u; i++) {
        s->blok[56u + i] = (uint8_t)(bit >> (56u - (8u * i)));
    }
    blok_isle(s);
    for (uint32_t i = 0u; i < 8u; i++) {
        const uint32_t j = 4u * i;
        ozet[j] = (uint8_t)(s->h[i] >> 24);
        ozet[j + 1u] = (uint8_t)(s->h[i] >> 16);
        ozet[j + 2u] = (uint8_t)(s->h[i] >> 8);
        ozet[j + 3u] = (uint8_t)s->h[i];
    }
}
