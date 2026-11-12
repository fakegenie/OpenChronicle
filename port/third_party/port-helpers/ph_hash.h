#ifndef PH_HASH_H
#define PH_HASH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PH_SHA1_SIZE = 20,
    PH_SHA1_TEXT_SIZE = 41,
};

#define PH_FNV32_START 0x811C9DC5u
#define PH_FNV64_START 0xCBF29CE484222325ull

uint32_t ph_crc32(uint32_t crc, const void *data, size_t size);
uint32_t ph_fnv32(uint32_t hash, const void *data, size_t size);
uint64_t ph_fnv64(uint64_t hash, const void *data, size_t size);
void ph_sha1(const void *data, size_t size, uint8_t digest[PH_SHA1_SIZE]);
void ph_sha1_text(const void *data, size_t size, char text[PH_SHA1_TEXT_SIZE]);

#ifdef __cplusplus
}
#endif

#endif

#if defined(PH_IMPLEMENTATION) && !defined(PH_HASH_IMPLEMENTED)
#define PH_HASH_IMPLEMENTED

static uint32_t ph_crc_table[256];

static void ph_fill_crc_table(void)
{
    for (uint32_t value = 0; value < 256; value++) {
        uint32_t entry = value;
        for (int bit = 0; bit < 8; bit++) {
            entry = entry & 1 ? entry >> 1 ^ 0xEDB88320u : entry >> 1;
        }
        ph_crc_table[value] = entry;
    }
}

uint32_t ph_crc32(uint32_t crc, const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    if (!ph_crc_table[1]) {
        ph_fill_crc_table();
    }
    crc = ~crc;
    for (size_t index = 0; index < size; index++) {
        crc = ph_crc_table[(crc ^ bytes[index]) & 0xFF] ^ crc >> 8;
    }
    return ~crc;
}

uint32_t ph_fnv32(uint32_t hash, const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t index = 0; index < size; index++) {
        hash = (hash ^ bytes[index]) * 0x01000193u;
    }
    return hash;
}

uint64_t ph_fnv64(uint64_t hash, const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t index = 0; index < size; index++) {
        hash = (hash ^ bytes[index]) * 0x100000001B3ull;
    }
    return hash;
}

static uint32_t ph_rotate(uint32_t value, int bits)
{
    return value << bits | value >> (32 - bits);
}

static void ph_sha1_block(uint32_t hash[5], const uint8_t *block)
{
    uint32_t words[80];
    for (int index = 0; index < 16; index++) {
        const uint8_t *at = block + index * 4;
        words[index] = (uint32_t)at[0] << 24 | (uint32_t)at[1] << 16 | (uint32_t)at[2] << 8 | at[3];
    }
    for (int index = 16; index < 80; index++) {
        words[index] = ph_rotate(words[index - 3] ^ words[index - 8] ^ words[index - 14] ^ words[index - 16], 1);
    }
    uint32_t a = hash[0], b = hash[1], c = hash[2], d = hash[3], e = hash[4];
    for (int index = 0; index < 80; index++) {
        uint32_t mix = b ^ c ^ d;
        uint32_t constant = index < 40 ? 0x6ED9EBA1u : 0xCA62C1D6u;
        if (index < 20) {
            mix = (b & c) | (~b & d);
            constant = 0x5A827999u;
        } else if (index >= 40 && index < 60) {
            mix = (b & c) | (b & d) | (c & d);
            constant = 0x8F1BBCDCu;
        }
        uint32_t next = ph_rotate(a, 5) + mix + e + constant + words[index];
        e = d;
        d = c;
        c = ph_rotate(b, 30);
        b = a;
        a = next;
    }
    hash[0] += a;
    hash[1] += b;
    hash[2] += c;
    hash[3] += d;
    hash[4] += e;
}

void ph_sha1(const void *data, size_t size, uint8_t digest[PH_SHA1_SIZE])
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t hash[5] = { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u };
    size_t whole = size / 64 * 64;
    for (size_t at = 0; at < whole; at += 64) {
        ph_sha1_block(hash, bytes + at);
    }
    uint8_t tail[128] = { 0 };
    size_t rest = size - whole;
    for (size_t index = 0; index < rest; index++) {
        tail[index] = bytes[whole + index];
    }
    tail[rest] = 0x80;
    size_t tail_size = rest < 56 ? 64 : 128;
    uint64_t bits = (uint64_t)size * 8;
    for (int index = 0; index < 8; index++) {
        tail[tail_size - 1 - index] = (uint8_t)(bits >> index * 8);
    }
    for (size_t at = 0; at < tail_size; at += 64) {
        ph_sha1_block(hash, tail + at);
    }
    for (int index = 0; index < PH_SHA1_SIZE; index++) {
        digest[index] = (uint8_t)(hash[index / 4] >> (24 - index % 4 * 8));
    }
}

void ph_sha1_text(const void *data, size_t size, char text[PH_SHA1_TEXT_SIZE])
{
    static const char digits[] = "0123456789abcdef";
    uint8_t digest[PH_SHA1_SIZE];
    ph_sha1(data, size, digest);
    for (int index = 0; index < PH_SHA1_SIZE; index++) {
        text[index * 2] = digits[digest[index] >> 4];
        text[index * 2 + 1] = digits[digest[index] & 15];
    }
    text[PH_SHA1_SIZE * 2] = 0;
}

#endif
