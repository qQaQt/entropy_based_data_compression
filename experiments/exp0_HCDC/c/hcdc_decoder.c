static int read_uint(const Bits *in, size_t *pos, unsigned width, unsigned *v) {
    if (*pos > in->len || width > in->len - *pos) return -1;
    *v = 0;
    for (unsigned i = 0; i < width; ++i) {
        uint8_t bit = in->data[(*pos)++];
        if (bit > 1) return -1;
        *v = (*v << 1) | bit;
    }
    return 0;
}

typedef struct {
    uint8_t mapping[2][128];
    size_t msb_count;
    unsigned byte_padding;
} Header;

static int decode_first_stage(const Bits *input, Bits *result) {
    Header headers[255];
    size_t pos = 0;
    unsigned count;
    Bits current = {0};
    *result = (Bits){0};
    if (read_uint(input, &pos, 8, &count) || count != 1) return -1;
    for (unsigned i = 0; i < count; ++i) {
        Header *h = &headers[i];
        unsigned width, bit;
        if (read_uint(input, &pos, 3, &h->byte_padding) ||
            read_uint(input, &pos, 8, &width) || !width || width > 64) return -1;
        uint64_t n = 0;
        for (unsigned j = 0; j < width; ++j) {
            if (read_uint(input, &pos, 1, &bit)) return -1;
            n = (n << 1) | bit;
        }
        if (n > SIZE_MAX / 8 || (!n && h->byte_padding)) return -1;
        h->msb_count = (size_t)n;
        for (unsigned g = 0; g < 2; ++g) {
            unsigned pairs;
            uint8_t used[128] = {0};
            if (read_uint(input, &pos, 5, &pairs) || pairs > 16) return -1;
            for (unsigned j = 0; j < 128; ++j) h->mapping[g][j] = (uint8_t)j;
            for (unsigned j = 0; j < pairs; ++j) {
                unsigned a, b;
                if (read_uint(input, &pos, 7, &a) || read_uint(input, &pos, 7, &b)) return -1;
                if (a == b || used[a] || used[b]) return -1;
                used[a] = used[b] = 1;
                h->mapping[g][a] = (uint8_t)b; h->mapping[g][b] = (uint8_t)a;
            }
        }
    }
    if (bits_alloc(&current, input->len - pos)) return -1;
    if (current.len) memcpy(current.data, input->data + pos, current.len);
    for (unsigned i = count; i > 0; --i) {
        Header *h = &headers[i-1];
        Bits next = {0}, msbs = {0};
        size_t n = h->msb_count, padding = (7 - n % 7) % 7;
        size_t words = n + padding;
        if (words > current.len / 5) goto fail;
        if (bits_alloc(&msbs, words)) goto fail;
        if (bits_alloc(&next, n * 8)) { bits_free(&msbs); goto fail; }
        size_t src = 0;
        for (size_t j = 0; j < words; j += 7) {
            unsigned flag, value;
            uint8_t *x = msbs.data + j;
            if (read_uint(&current, &src, 1, &flag)) goto bad_block;
            if (flag) {
                if (read_uint(&current, &src, 4, &value)) goto bad_block;
                int_to_bits(value, 4, x);
                x[4] = x[1] ^ x[2] ^ x[3];
                x[5] = x[0] ^ x[2] ^ x[3];
                x[6] = x[0] ^ x[1] ^ x[3];
            } else {
                if (read_uint(&current, &src, 7, &value)) goto bad_block;
                int_to_bits(value, 7, x);
            }
        }
        for (size_t j = 0; j < words; ++j) {
            unsigned flag, value, group = msbs.data[j];
            uint8_t x[7];
            if (group > 1 || (j >= n && group)) goto bad_block;
            if (read_uint(&current, &src, 1, &flag)) goto bad_block;
            if (flag) {
                if (read_uint(&current, &src, 4, &value)) goto bad_block;
                int_to_bits(value, 4, x);
                x[4] = x[1] ^ x[2] ^ x[3];
                x[5] = x[0] ^ x[2] ^ x[3];
                x[6] = x[0] ^ x[1] ^ x[3];
            } else {
                if (read_uint(&current, &src, 7, &value)) goto bad_block;
                int_to_bits(value, 7, x);
            }
            unsigned original = h->mapping[group][block_value(x)];
            if (j < n) {
                next.data[j*8] = (uint8_t)group;
                int_to_bits(original, 7, next.data + j*8 + 1);
            } else if (original != 0) goto bad_block;
        }
        if (src != current.len) goto bad_block;
        for (unsigned j = 0; j < h->byte_padding; ++j)
            if (next.data[next.len - 1 - j]) goto bad_block;
        next.len -= h->byte_padding;
        bits_free(&msbs); bits_free(&current); current = next;
        continue;
bad_block:
        bits_free(&msbs); bits_free(&next); goto fail;
    }
    *result = current; return 0;
fail:
    bits_free(&current); return -1;
}

static int decode_single_stage(const Bits *input, Bits *result) {
    *result = (Bits){0};
    if (input->len < 8) return -1;
    size_t pos = input->len - 8;
    unsigned header_len;
    if (read_uint(input, &pos, 8, &header_len) || header_len > input->len - 8) return -1;
    size_t payload_len = input->len - 8 - header_len;
    Bits h = {input->data + payload_len, header_len};
    size_t hp = 0;
    unsigned tail_len, pairs;
    uint8_t tail[6], mapping[128], used[128] = {0};
    if (read_uint(&h, &hp, 3, &tail_len) || tail_len > 6) return -1;
    for (unsigned i = 0; i < tail_len; ++i) {
        unsigned bit;
        if (read_uint(&h, &hp, 1, &bit)) return -1;
        tail[i] = (uint8_t)bit;
    }
    if (read_uint(&h, &hp, 5, &pairs) || pairs > 16) return -1;
    for (unsigned i = 0; i < 128; ++i) mapping[i] = (uint8_t)i;
    for (unsigned i = 0; i < pairs; ++i) {
        unsigned a, b;
        if (read_uint(&h, &hp, 7, &a) || read_uint(&h, &hp, 7, &b) ||
            a == b || used[a] || used[b]) return -1;
        used[a] = used[b] = 1; mapping[a] = (uint8_t)b; mapping[b] = (uint8_t)a;
    }
    if (hp != h.len) return -1;
    size_t blocks = payload_len / 5;
    if (blocks > (SIZE_MAX - tail_len) / 7 || bits_alloc(result, blocks*7 + tail_len)) return -1;
    Bits payload = {input->data, payload_len};
    size_t src = 0, dst = 0;
    while (src < payload.len) {
        unsigned flag, value;
        uint8_t x[7];
        if (dst / 7 >= blocks || read_uint(&payload, &src, 1, &flag)) goto fail;
        if (flag) {
            if (read_uint(&payload, &src, 4, &value)) goto fail;
            int_to_bits(value, 4, x);
            x[4] = x[1] ^ x[2] ^ x[3];
            x[5] = x[0] ^ x[2] ^ x[3];
            x[6] = x[0] ^ x[1] ^ x[3];
        } else {
            if (read_uint(&payload, &src, 7, &value)) goto fail;
            int_to_bits(value, 7, x);
        }
        int_to_bits(mapping[block_value(x)], 7, result->data + dst); dst += 7;
    }
    if (tail_len) memcpy(result->data + dst, tail, tail_len);
    result->len = dst + tail_len;
    return 0;
fail:
    bits_free(result); return -1;
}

int decode_n_times(const Bits *input, Bits *result) {
    *result = (Bits){0};
    size_t pos = 0;
    unsigned seven_bit, count;
    if (read_uint(input, &pos, 1, &seven_bit) || read_uint(input, &pos, 8, &count)) return -1;
    Bits current = {0}, next = {0};
    if (bits_alloc(&current, input->len - pos)) return -1;
    if (current.len) memcpy(current.data, input->data + pos, current.len);
    unsigned single_count = seven_bit ? count : (count ? count - 1 : 0);
    for (unsigned i = 0; i < single_count; ++i) {
        if (decode_single_stage(&current, &next)) goto fail;
        bits_free(&current); current = next; next = (Bits){0};
    }
    if (seven_bit) {
        if (current.len % 7 || current.len / 7 > SIZE_MAX / 8 ||
            bits_alloc(&next, current.len / 7 * 8)) goto fail;
        for (size_t i = 0; i < current.len / 7; ++i) {
            next.data[i*8] = 0;
            memcpy(next.data + i*8 + 1, current.data + i*7, 7);
        }
        bits_free(&current); current = next; next = (Bits){0};
    } else if (count) {
        if (current.len > SIZE_MAX - 8 || bits_alloc(&next, current.len + 8)) goto fail;
        int_to_bits(1, 8, next.data);
        if (current.len) memcpy(next.data + 8, current.data, current.len);
        bits_free(&current);
        int rc = decode_first_stage(&next, &current);
        bits_free(&next);
        if (rc) goto fail;
    }
    *result = current;
    return 0;
fail:
    bits_free(&current); bits_free(&next); return -1;
}
