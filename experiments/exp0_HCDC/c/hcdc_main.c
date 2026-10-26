/* Build: cc -std=c11 -O2 -Wall -Wextra -Wpedantic hcdc_main.c -o hcdc_files
 * Requires hcdc_encoder.c and hcdc_decoder.c beside this file. Run from the directory containing data/.
 * File bytes use all 256 values and are represented by 8 bits each.
 * Uses the C two-table MSB-group format with its matching decoder.
 */
#include "hcdc_encoder.c"
#include <stdarg.h>
#include <errno.h>
#include <math.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

static FILE *log_file;
static void log_print(const char *fmt, ...) {
    va_list a, b;
    va_start(a, fmt); va_copy(b, a);
    vprintf(fmt, a); vfprintf(log_file, fmt, b);
    va_end(b); va_end(a); fflush(stdout); fflush(log_file);
}

#include "hcdc_decoder.c"

static int read_file(const char *path, Bits *bytes) {
    FILE *f = fopen(path, "rb");
    *bytes = (Bits){0};
    if (!f) return -1;
    size_t capacity = 0;
    uint8_t buffer[65536];
    size_t got;
    while ((got = fread(buffer, 1, sizeof buffer, f)) != 0) {
        if (got > SIZE_MAX - bytes->len) goto fail;
        size_t needed = bytes->len + got;
        if (needed > capacity) {
            size_t new_capacity = needed <= SIZE_MAX / 2 ? needed * 2 : needed;
            uint8_t *p = realloc(bytes->data, new_capacity);
            if (!p) goto fail;
            bytes->data = p; capacity = new_capacity;
        }
        memcpy(bytes->data + bytes->len, buffer, got); bytes->len += got;
    }
    if (ferror(f)) goto fail;
    return fclose(f) == 0 ? 0 : (bits_free(bytes), -1);
fail:
    fclose(f); bits_free(bytes); return -1;
}

static int process_file(const char *name) {
    char input_path[512], output_path[512];
    const char *suffix = strrchr(name, '.');
    if (!suffix || suffix == name) suffix = name + strlen(name);
    int a = snprintf(input_path, sizeof input_path, "data/%s", name);
    int b = snprintf(output_path, sizeof output_path, "dec_enc_data/%.*s_decoded%s",
                     (int)(suffix - name), name, suffix);
    if (a < 0 || b < 0 || (size_t)a >= sizeof input_path ||
        (size_t)b >= sizeof output_path) return -1;
    Bits bytes = {0}, input = {0}, encoded = {0}, decoded = {0}, output = {0};
    int status = -1;
    log_print("\n============================================================\nFile: %s\n"
              "============================================================\n", name);
    if (read_file(input_path, &bytes)) { log_print("ERROR: cannot read %s\n", input_path); goto done; }
    if (bytes.len > SIZE_MAX / 8 || bits_alloc(&input, bytes.len * 8)) goto done;
    size_t freq[256] = {0};
    double p_x[256] = {0};
    double h_x = 0;
    for (size_t i = 0; i < bytes.len; ++i) {
        int_to_bits(bytes.data[i], 8, input.data + i*8);
        // entropy calculation
        freq[bytes.data[i]]++;
    }
    for (size_t i = 0; i < 256; ++i) {
            p_x[i] = (double)freq[i] / (double)bytes.len;
            h_x -= p_x[i] * log2(p_x[i]);
    }
    log_print("Original length: %zu bits\n", input.len);
    log_print("Entropy: %.6f bits/symbol\n", h_x);
    int seven_bit = 1;
    for (size_t i = 0; i < bytes.len; ++i)
        if (bytes.data[i] >= 128) { seven_bit = 0; break; }
    log_print("Input mode: %s\n", seven_bit ? "7-bit (zero MSBs omitted)" : "8-bit");
    unsigned iterations;
    if (encode_n_times(input.data, input.len, &encoded, &iterations)) goto done;
    log_print("Encoded length: %zu\nNumber of iterations: %u\n", encoded.len, iterations);
    if (bytes.len)
        log_print("Bits per symbol: %.6f\n", (double)encoded.len / (double)bytes.len);
    else log_print("Bits per symbol: N/A (empty input)\n");
    if (decode_n_times(&encoded, &decoded)) { log_print("ERROR: decoding failed\n"); goto done; }
    if (input.len != decoded.len) {
        log_print("BIT STREAMS ARE DIFFERENT: original %zu bits, decoded %zu bits\n",
                  input.len, decoded.len);
        goto done;
    }
    for (size_t i = 0; i < input.len; ++i) {
        if (input.data[i] != decoded.data[i]) {
            log_print("BIT STREAMS ARE DIFFERENT: bit index %zu (zero-based), original %u, decoded %u\n",
                      i, (unsigned)input.data[i], (unsigned)decoded.data[i]);
            goto done;
        }
    }
    log_print("BIT STREAMS ARE IDENTICAL (%zu bits)\n", input.len);
    if (decoded.len % 8) { log_print("ERROR: decoded length is not divisible by 8\n"); goto done; }
    if (bits_alloc(&output, decoded.len / 8)) goto done;
    for (size_t i = 0; i < output.len; ++i) {
        unsigned value = 0;
        for (unsigned j = 0; j < 8; ++j)
            value = (value << 1) | decoded.data[i*8 + j];
        output.data[i] = (uint8_t)value;
    }
    FILE *f = fopen(output_path, "wb");
    if (!f) { log_print("ERROR: cannot write %s\n", output_path); goto done; }
    int write_failed = output.len && fwrite(output.data, 1, output.len, f) != output.len;
    if (fclose(f)) write_failed = 1;
    if (write_failed) { log_print("ERROR: writing failed\n"); goto done; }
    status = 0;
done:
    if (status) log_print("Processing failed for %s\n", name);
    bits_free(&bytes); bits_free(&input); bits_free(&encoded); bits_free(&decoded); bits_free(&output);
    return status;
}

#ifndef HCDC_FILES_NO_MAIN
int main(void) {
    const char *names[] = {
    "bible.txt", "bib", "book1", "book2", "E.coli", "geo", "news",
    "obj1", "obj2",
    "paper1", "paper2", "paper3", "paper4", "paper5", "paper6",
    "pic", "progc", "progl", "progp", "trans", "world192.txt"
};
    /* To include larger files, add "bible.txt", "E.coli", "world192.txt" above. */
    log_file = fopen("HCDC.log", "w");
    if (!log_file) { perror("HCDC.log"); return 1; }
#ifdef _WIN32
    int rc = _mkdir("dec_enc_data");
#else
    int rc = mkdir("dec_enc_data", 0777);
#endif
    if (rc && errno != EEXIST) { perror("dec_enc_data"); fclose(log_file); return 1; }
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i) {
        if (process_file(names[i])) { fclose(log_file); return 1; }
    }
    return fclose(log_file) == 0 ? 0 : 1;
}
#endif
