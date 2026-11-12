#ifndef PH_ZIP_H
#define PH_ZIP_H

#include <stddef.h>
#include <stdint.h>

#include "ph_hash.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PH_ZIP_STORED = 0,
    PH_ZIP_DEFLATED = 8,
    PH_ZIP_DAMAGED = -1,
};

struct ph_zip_entry {
    const char *name;
    int name_size;
    int method;
    uint32_t crc;
    uint32_t packed_size;
    uint32_t size;
    const uint8_t *data;
};

struct ph_zip_part {
    const char *name;
    uint32_t crc;
    uint32_t size;
    uint32_t offset;
    uint32_t step;
};

int ph_zip_is_zip(const uint8_t *file, size_t size);
int ph_zip_entries(const uint8_t *zip, size_t size, struct ph_zip_entry *entries, int limit);
int ph_zip_named(const struct ph_zip_entry *entry, const char *name);
int ph_zip_extract(const struct ph_zip_entry *entry, uint8_t *out);
int ph_zip_place(const struct ph_zip_entry *entries, int count, const struct ph_zip_part *part, uint8_t *out, size_t out_size);
int ph_inflate(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif

#if defined(PH_IMPLEMENTATION) && !defined(PH_ZIP_IMPLEMENTED)
#define PH_ZIP_IMPLEMENTED

#include <stdlib.h>
#include <string.h>

enum {
    PH_ZIP_END_SIZE = 22,
    PH_ZIP_END_SEARCH = PH_ZIP_END_SIZE + 0xFFFF,
    PH_ZIP_ENTRY_SIZE = 46,
    PH_ZIP_LOCAL_SIZE = 30,
    PH_ZIP_END_SIGNATURE = 0x06054B50,
    PH_ZIP_ENTRY_SIGNATURE = 0x02014B50,
    PH_ZIP_LOCAL_SIGNATURE = 0x04034B50,
    PH_INFLATE_BITS = 15,
    PH_INFLATE_LENGTH_CODES = 29,
    PH_INFLATE_DISTANCE_CODES = 30,
    PH_INFLATE_LITERAL_CODES = 288,
    PH_INFLATE_CODE_LENGTH_CODES = 19,
    PH_INFLATE_END_OF_BLOCK = 256,
    PH_INFLATE_FIRST_LENGTH = 257,
    PH_INFLATE_STORED = 0,
    PH_INFLATE_FIXED = 1,
    PH_INFLATE_DYNAMIC = 2,
};

struct ph_inflate_stream {
    const uint8_t *in;
    size_t in_size;
    size_t in_position;
    uint32_t buffer;
    int buffered;
    uint8_t *out;
    size_t out_size;
    size_t out_position;
    int failed;
};

struct ph_huffman {
    short count[PH_INFLATE_BITS + 1];
    short symbol[PH_INFLATE_LITERAL_CODES];
};

static const short ph_length_base[PH_INFLATE_LENGTH_CODES] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                                               35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const short ph_length_extra[PH_INFLATE_LENGTH_CODES] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                                                3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const unsigned short ph_distance_base[PH_INFLATE_DISTANCE_CODES] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                                                                            193, 257, 385, 513, 769, 1025, 1537, 2049, 3073,
                                                                            4097, 6145, 8193, 12289, 16385, 24577 };
static const short ph_distance_extra[PH_INFLATE_DISTANCE_CODES] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                                                                    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
static const short ph_code_length_order[PH_INFLATE_CODE_LENGTH_CODES] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

static uint32_t ph_zip_word(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8;
}

static uint32_t ph_zip_long(const uint8_t *bytes)
{
    return ph_zip_word(bytes) | ph_zip_word(bytes + 2) << 16;
}

int ph_zip_is_zip(const uint8_t *file, size_t size)
{
    return file && size >= 4 && ph_zip_long(file) == PH_ZIP_LOCAL_SIGNATURE;
}

static const uint8_t *ph_zip_end(const uint8_t *zip, size_t size)
{
    if (size < PH_ZIP_END_SIZE) {
        return NULL;
    }
    size_t lowest = size > PH_ZIP_END_SEARCH ? size - PH_ZIP_END_SEARCH : 0;
    for (size_t position = size - PH_ZIP_END_SIZE + 1; position-- > lowest;) {
        if (ph_zip_long(zip + position) == PH_ZIP_END_SIGNATURE) {
            return zip + position;
        }
    }
    return NULL;
}

static int ph_zip_read_entry(const uint8_t *zip, size_t size, const uint8_t *header, struct ph_zip_entry *entry)
{
    entry->method = (int)ph_zip_word(header + 10);
    entry->crc = ph_zip_long(header + 16);
    entry->packed_size = ph_zip_long(header + 20);
    entry->size = ph_zip_long(header + 24);
    entry->name_size = (int)ph_zip_word(header + 28);
    entry->name = (const char *)header + PH_ZIP_ENTRY_SIZE;
    size_t local = ph_zip_long(header + 42);
    if (size < PH_ZIP_LOCAL_SIZE || local > size - PH_ZIP_LOCAL_SIZE || ph_zip_long(zip + local) != PH_ZIP_LOCAL_SIGNATURE) {
        return 0;
    }
    size_t start = local + PH_ZIP_LOCAL_SIZE + ph_zip_word(zip + local + 26) + ph_zip_word(zip + local + 28);
    if (start > size || entry->packed_size > size - start) {
        return 0;
    }
    entry->data = zip + start;
    return 1;
}

int ph_zip_entries(const uint8_t *zip, size_t size, struct ph_zip_entry *entries, int limit)
{
    const uint8_t *end = ph_zip_end(zip, size);
    if (!end) {
        return PH_ZIP_DAMAGED;
    }
    size_t directory = ph_zip_long(end + 16);
    int total = (int)ph_zip_word(end + 10);
    size_t at = directory;
    int count = 0;
    for (int index = 0; index < total && count < limit; index++) {
        if (at > size || size - at < PH_ZIP_ENTRY_SIZE || ph_zip_long(zip + at) != PH_ZIP_ENTRY_SIGNATURE) {
            return PH_ZIP_DAMAGED;
        }
        const uint8_t *header = zip + at;
        size_t length = PH_ZIP_ENTRY_SIZE + ph_zip_word(header + 28) + ph_zip_word(header + 30) + ph_zip_word(header + 32);
        if (length > size - at || !ph_zip_read_entry(zip, size, header, &entries[count])) {
            return PH_ZIP_DAMAGED;
        }
        count++;
        at += length;
    }
    return count;
}

int ph_zip_named(const struct ph_zip_entry *entry, const char *name)
{
    int start = entry->name_size;
    while (start > 0 && entry->name[start - 1] != '/' && entry->name[start - 1] != '\\') {
        start--;
    }
    int length = entry->name_size - start;
    if ((int)strlen(name) != length) {
        return 0;
    }
    for (int index = 0; index < length; index++) {
        char left = entry->name[start + index];
        char right = name[index];
        if ((left >= 'A' && left <= 'Z' ? left + 32 : left) != (right >= 'A' && right <= 'Z' ? right + 32 : right)) {
            return 0;
        }
    }
    return 1;
}

static int ph_take_bits(struct ph_inflate_stream *stream, int count)
{
    uint32_t value = stream->buffer;
    while (stream->buffered < count) {
        if (stream->in_position == stream->in_size) {
            stream->failed = 1;
            return 0;
        }
        value |= (uint32_t)stream->in[stream->in_position++] << stream->buffered;
        stream->buffered += 8;
    }
    stream->buffer = value >> count;
    stream->buffered -= count;
    return (int)(value & ((1u << count) - 1));
}

static int ph_build_huffman(struct ph_huffman *huffman, const short *lengths, int symbols)
{
    short offsets[PH_INFLATE_BITS + 2];
    memset(huffman->count, 0, sizeof huffman->count);
    for (int symbol = 0; symbol < symbols; symbol++) {
        huffman->count[lengths[symbol]]++;
    }
    int left = 1;
    for (int length = 1; length <= PH_INFLATE_BITS; length++) {
        left = left * 2 - huffman->count[length];
        if (left < 0) {
            return 0;
        }
    }
    offsets[1] = 0;
    for (int length = 1; length <= PH_INFLATE_BITS; length++) {
        offsets[length + 1] = (short)(offsets[length] + huffman->count[length]);
    }
    for (int symbol = 0; symbol < symbols; symbol++) {
        if (lengths[symbol]) {
            huffman->symbol[offsets[lengths[symbol]]++] = (short)symbol;
        }
    }
    return 1;
}

static int ph_decode(struct ph_inflate_stream *stream, const struct ph_huffman *huffman)
{
    int code = 0;
    int first = 0;
    int index = 0;
    for (int length = 1; length <= PH_INFLATE_BITS && !stream->failed; length++) {
        code |= ph_take_bits(stream, 1);
        int count = huffman->count[length];
        if (code - first < count) {
            return huffman->symbol[index + code - first];
        }
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    stream->failed = 1;
    return -1;
}

static int ph_copy_stored(struct ph_inflate_stream *stream)
{
    stream->buffer = 0;
    stream->buffered = 0;
    if (stream->in_size - stream->in_position < 4) {
        return 0;
    }
    const uint8_t *header = stream->in + stream->in_position;
    size_t length = (size_t)(header[0] | header[1] << 8);
    if ((size_t)(header[2] | header[3] << 8) != (~length & 0xFFFF)) {
        return 0;
    }
    stream->in_position += 4;
    if (stream->in_size - stream->in_position < length || stream->out_size - stream->out_position < length) {
        return 0;
    }
    memcpy(stream->out + stream->out_position, stream->in + stream->in_position, length);
    stream->in_position += length;
    stream->out_position += length;
    return 1;
}

static int ph_inflate_codes(struct ph_inflate_stream *stream, const struct ph_huffman *literals, const struct ph_huffman *distances)
{
    for (;;) {
        int symbol = ph_decode(stream, literals);
        if (stream->failed) {
            return 0;
        }
        if (symbol < PH_INFLATE_END_OF_BLOCK) {
            if (stream->out_position == stream->out_size) {
                return 0;
            }
            stream->out[stream->out_position++] = (uint8_t)symbol;
            continue;
        }
        if (symbol == PH_INFLATE_END_OF_BLOCK) {
            return 1;
        }
        symbol -= PH_INFLATE_FIRST_LENGTH;
        if (symbol >= PH_INFLATE_LENGTH_CODES) {
            return 0;
        }
        size_t length = (size_t)(ph_length_base[symbol] + ph_take_bits(stream, ph_length_extra[symbol]));
        int distance_symbol = ph_decode(stream, distances);
        if (stream->failed || distance_symbol >= PH_INFLATE_DISTANCE_CODES) {
            return 0;
        }
        size_t distance = (size_t)(ph_distance_base[distance_symbol] + ph_take_bits(stream, ph_distance_extra[distance_symbol]));
        if (stream->failed || distance > stream->out_position || stream->out_size - stream->out_position < length) {
            return 0;
        }
        for (size_t index = 0; index < length; index++) {
            stream->out[stream->out_position] = stream->out[stream->out_position - distance];
            stream->out_position++;
        }
    }
}

static int ph_inflate_fixed(struct ph_inflate_stream *stream)
{
    short lengths[PH_INFLATE_LITERAL_CODES];
    struct ph_huffman literals;
    struct ph_huffman distances;
    for (int symbol = 0; symbol < PH_INFLATE_LITERAL_CODES; symbol++) {
        lengths[symbol] = symbol < 144 ? 8 : symbol < 256 ? 9 : symbol < 280 ? 7 : 8;
    }
    ph_build_huffman(&literals, lengths, PH_INFLATE_LITERAL_CODES);
    for (int symbol = 0; symbol < PH_INFLATE_DISTANCE_CODES; symbol++) {
        lengths[symbol] = 5;
    }
    ph_build_huffman(&distances, lengths, PH_INFLATE_DISTANCE_CODES);
    return ph_inflate_codes(stream, &literals, &distances);
}

static int ph_inflate_dynamic(struct ph_inflate_stream *stream)
{
    short lengths[PH_INFLATE_LITERAL_CODES + PH_INFLATE_DISTANCE_CODES];
    struct ph_huffman literals;
    struct ph_huffman distances;
    int literal_count = ph_take_bits(stream, 5) + PH_INFLATE_FIRST_LENGTH;
    int distance_count = ph_take_bits(stream, 5) + 1;
    int code_length_count = ph_take_bits(stream, 4) + 4;
    if (stream->failed || literal_count > PH_INFLATE_LITERAL_CODES - 2 || distance_count > PH_INFLATE_DISTANCE_CODES) {
        return 0;
    }
    memset(lengths, 0, sizeof lengths);
    for (int index = 0; index < code_length_count; index++) {
        lengths[ph_code_length_order[index]] = (short)ph_take_bits(stream, 3);
    }
    if (stream->failed || !ph_build_huffman(&literals, lengths, PH_INFLATE_CODE_LENGTH_CODES)) {
        return 0;
    }
    int total = literal_count + distance_count;
    int index = 0;
    while (index < total) {
        int symbol = ph_decode(stream, &literals);
        if (stream->failed) {
            return 0;
        }
        if (symbol < 16) {
            lengths[index++] = (short)symbol;
            continue;
        }
        short repeated = 0;
        int repeat = 0;
        if (symbol == 16) {
            if (!index) {
                return 0;
            }
            repeated = lengths[index - 1];
            repeat = 3 + ph_take_bits(stream, 2);
        } else if (symbol == 17) {
            repeat = 3 + ph_take_bits(stream, 3);
        } else {
            repeat = 11 + ph_take_bits(stream, 7);
        }
        if (stream->failed || index + repeat > total) {
            return 0;
        }
        while (repeat--) {
            lengths[index++] = repeated;
        }
    }
    if (!lengths[PH_INFLATE_END_OF_BLOCK] || !ph_build_huffman(&literals, lengths, literal_count) ||
        !ph_build_huffman(&distances, lengths + literal_count, distance_count)) {
        return 0;
    }
    return ph_inflate_codes(stream, &literals, &distances);
}

int ph_inflate(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_size)
{
    struct ph_inflate_stream stream = { in, in_size, 0, 0, 0, out, out_size, 0, 0 };
    int last = 0;
    while (!last) {
        last = ph_take_bits(&stream, 1);
        int type = ph_take_bits(&stream, 2);
        int finished = 0;
        if (stream.failed) {
            return 0;
        }
        if (type == PH_INFLATE_STORED) {
            finished = ph_copy_stored(&stream);
        } else if (type == PH_INFLATE_FIXED) {
            finished = ph_inflate_fixed(&stream);
        } else if (type == PH_INFLATE_DYNAMIC) {
            finished = ph_inflate_dynamic(&stream);
        }
        if (!finished) {
            return 0;
        }
    }
    return stream.out_position == out_size;
}

int ph_zip_extract(const struct ph_zip_entry *entry, uint8_t *out)
{
    if (entry->method == PH_ZIP_STORED) {
        if (entry->packed_size != entry->size) {
            return 0;
        }
        memcpy(out, entry->data, entry->size);
    } else if (entry->method != PH_ZIP_DEFLATED || !ph_inflate(entry->data, entry->packed_size, out, entry->size)) {
        return 0;
    }
    return ph_crc32(0, out, entry->size) == entry->crc;
}

static int ph_zip_part_matches(const struct ph_zip_entry *entry, const struct ph_zip_part *part)
{
    if (entry->size != part->size) {
        return 0;
    }
    return part->crc ? entry->crc == part->crc : part->name && ph_zip_named(entry, part->name);
}

int ph_zip_place(const struct ph_zip_entry *entries, int count, const struct ph_zip_part *part, uint8_t *out, size_t out_size)
{
    uint32_t step = part->step ? part->step : 1;
    if (!part->size || (size_t)part->offset + (size_t)(part->size - 1) * step >= out_size) {
        return 0;
    }
    for (int index = 0; index < count; index++) {
        if (!ph_zip_part_matches(&entries[index], part)) {
            continue;
        }
        if (step == 1) {
            return ph_zip_extract(&entries[index], out + part->offset);
        }
        uint8_t *data = (uint8_t *)malloc(part->size);
        int placed = data && ph_zip_extract(&entries[index], data);
        for (uint32_t at = 0; placed && at < part->size; at++) {
            out[part->offset + at * step] = data[at];
        }
        free(data);
        return placed;
    }
    return 0;
}

#endif
