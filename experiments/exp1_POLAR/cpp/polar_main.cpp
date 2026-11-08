#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <random>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <stdexcept>


struct ByteBuffer {
    uint8_t* data = nullptr;
    size_t len = 0;
    size_t capacity = 0;
};

static FILE* log_file = nullptr;

void log_print(const char* fmt, ...) {
    va_list args, copy;
    va_start(args, fmt);
    va_copy(copy, args);

    vprintf(fmt, args);
    if (log_file)
        vfprintf(log_file, fmt, copy);

    va_end(copy);
    va_end(args);

    fflush(stdout);
    if (log_file)
        fflush(log_file);
}


void buffer_free(ByteBuffer* buffer) {
    if (!buffer) return;

    free(buffer->data);
    *buffer = {};
}

int read_file(const char* path, ByteBuffer* bytes) {
    if (!bytes) return -1;

    *bytes = {};

    FILE* file = fopen(path, "rb");
    if (!file) return -1;

    uint8_t buffer[65536];
    size_t got;

    while ((got = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        if (got > SIZE_MAX - bytes->len)
            goto fail;

        size_t needed = bytes->len + got;

        if (needed > bytes->capacity) {
            size_t new_capacity =
                needed <= SIZE_MAX / 2 ? needed * 2 : needed;

            uint8_t* p = static_cast<uint8_t*>(
                realloc(bytes->data, new_capacity)
            );

            if (!p) goto fail;

            bytes->data = p;
            bytes->capacity = new_capacity;
        }

        memcpy(bytes->data + bytes->len, buffer, got);
        bytes->len += got;
    }

    if (ferror(file)) goto fail;

    if (fclose(file) != 0) {
        buffer_free(bytes);
        return -1;
    }

    return 0;

fail:
    fclose(file);
    buffer_free(bytes);
    return -1;
}

using Matrix = std::vector<std::vector<uint8_t>>;

size_t reverse_bits(size_t x, size_t n) {
    size_t result = 0;

    for (size_t i = 0; i < n; ++i) {
        result = (result << 1) | (x & 1);
        x >>= 1;
    }

    return result;
}

Matrix build_generator_matrix(size_t N) {
    if (N == 0 || (N & (N - 1)) != 0)
        throw std::invalid_argument("N must be a power of 2");

    Matrix G = {{1}};
    size_t n = 0;

    while (G.size() < N) {
        size_t m = G.size();
        Matrix next(2 * m, std::vector<uint8_t>(2 * m, 0));

        for (size_t i = 0; i < m; ++i) {
            for (size_t j = 0; j < m; ++j) {
                next[i][j] = G[i][j];
                next[i + m][j] = G[i][j];
                next[i + m][j + m] = G[i][j];
            }
        }

        G = std::move(next);
        ++n;
    }

    Matrix result(N, std::vector<uint8_t>(N, 0));

    for (size_t i = 0; i < N; ++i)
        result[i] = G[reverse_bits(i, n)];

    return result;
}

void print_matrix(const Matrix& G) {
    for (const auto& row : G) {
        for (uint8_t bit : row)
            std::cout << static_cast<int>(bit) << ' ';

        std::cout << '\n';
    }
}
std::vector<uint8_t> bytes_to_bits(const ByteBuffer& bytes) {
    std::vector<uint8_t> bits;

    bits.reserve(bytes.len * 8);

    for (size_t i = 0; i < bytes.len; ++i) {
        for (int j = 7; j >= 0; --j) {
            bits.push_back((bytes.data[i] >> j) & 1);
        }
    }

    return bits;
}

std::vector<uint8_t> bit_generator(double p, size_t size) {
    if (p < 0.0 || p > 1.0)
        throw std::invalid_argument("Probability must be between 0 and 1");

    static std::mt19937_64 rng(std::random_device{}());
    std::bernoulli_distribution distribution(p);

    std::vector<uint8_t> bits(size);

    for (size_t i = 0; i < size; ++i) {
        bits[i] = static_cast<uint8_t>(distribution(rng));
    }

    return bits;
}
int main() {
    const size_t N = 4;
    ByteBuffer bytes;
    if (read_file("../../data/paper6", &bytes) != 0) {
        printf("Error reading file %zu", read_file("../data/book1", &bytes));
        return 1;
    }
    size_t bit_count;
    //size_t total_bits = bytes.len * 8;
    //size_t count = std::min(bit_count, total_bits);
    //std::vector<uint8_t> dat = bytes_to_bits(bytes);
buffer_free(&bytes);

Matrix G = build_generator_matrix(N);

std::vector<uint8_t> dat = bit_generator(0.1, 12800);
size_t Segm_block = dat.size() / N + (dat.size() % N != 0);
    std::vector<std::vector<size_t>> counts(N);
    std::vector<std::vector<size_t>> ones(N);

    for (size_t i = 0; i < N; ++i) {
        counts[i].resize(1ULL << i, 0);
        ones[i].resize(1ULL << i, 0);
    }

for (size_t k = 0; k < Segm_block; ++k) {

    std::vector<uint8_t> X(N, 0);
    std::vector<uint8_t> U(N, 0);
    // Заполняем входной блок
    for (size_t j = 0; j < N; ++j) {
        size_t index = k * N + j;

        if (index < dat.size()) {
            X[j] = dat[index];
        }
    }

    // Полярное преобразование
    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j) {
            U[i] ^= X[j] & G[j][i];
        }
    }

    // Вывод результата
    // for (size_t i = 0; i < N; ++i) {
    //     std::cout << static_cast<int>(U[i]);

    //     if ((i + 1) % 8 == 0)
    //         std::cout << ' ';
    // }


    for (size_t i = 0; i < N; ++i) {
        size_t prefix = 0;
    
        for (size_t j = 0; j < i; ++j) {
            prefix = (prefix << 1) | U[j];
        }
    
        counts[i][prefix]++;
    
        if (U[i] == 1) {
            ones[i][prefix]++;
        }
    }
for (size_t i = 0; i < N; ++i) {
    for (size_t j = 0; j < (1ULL << i); ++j) {
        std::cout << ones[i][j] << ' ';
    }
    std::cout << "                                              ";
    for (size_t j = 0; j < (1ULL << i); ++j) {
        std::cout << counts[i][j] << ' ';
    }
    std::cout << '\n';
}
}
    //print_matrix(G);




    return 0;
};