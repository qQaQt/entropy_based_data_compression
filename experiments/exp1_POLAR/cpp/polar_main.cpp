#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <random>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <algorithm>
#include <functional>


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

#include <cmath>

double binary_entropy(double p) {
    if (p <= 0.0 || p >= 1.0)
        return 0.0;

    return -p * std::log2(p)
           - (1.0 - p) * std::log2(1.0 - p);
}











void exact_entropy(size_t N, const Matrix& G, double p) {

    // Количество всех возможных входных блоков
    size_t total_blocks = 1ULL << N;

    // Вероятность префикса
    std::vector<std::vector<double>> prefix_prob(N);

    // Вероятность префикса и Ui = 1
    std::vector<std::vector<double>> prefix_one_prob(N);

    for (size_t i = 0; i < N; ++i) {
        prefix_prob[i].resize(1ULL << i, 0.0);
        prefix_one_prob[i].resize(1ULL << i, 0.0);
    }

    // Перебираем все возможные X
    for (size_t x = 0; x < total_blocks; ++x) {

        std::vector<uint8_t> X(N, 0);
        std::vector<uint8_t> U(N, 0);

        size_t ones_count = 0;

        // Заполняем X
        for (size_t j = 0; j < N; ++j) {

            X[j] = (x >> (N - 1 - j)) & 1;

            ones_count += X[j];
        }

        // Вероятность данного блока X
        double probability =
            std::pow(p, ones_count) *
            std::pow(1.0 - p, N - ones_count);

        // Полярное преобразование U = XG
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < N; ++j) {
                U[i] ^= X[j] & G[j][i];
            }
        }

        // Собираем вероятности префиксов
        for (size_t i = 0; i < N; ++i) {

            size_t prefix = 0;

            for (size_t j = 0; j < i; ++j) {
                prefix = (prefix << 1) | U[j];
            }

            prefix_prob[i][prefix] += probability;

            if (U[i] == 1) {
                prefix_one_prob[i][prefix] += probability;
            }
        }
    }

    // Вычисляем условные энтропии
    double H_total = 0.0;

    for (size_t i = 0; i < N; ++i) {

        double H_i = 0.0;

        for (size_t j = 0; j < (1ULL << i); ++j) {

            double prob = prefix_prob[i][j];

            if (prob == 0.0)
                continue;

            double q = prefix_one_prob[i][j] / prob;

            H_i += prob * binary_entropy(q);
        }

        std::cout << "H(U" << i << ") = "
                  << H_i << '\n';

        H_total += H_i;
    }

    std::cout << "Total entropy H(U) = "
              << H_total << '\n';
}

// Population Monte Carlo for full source-polarization conditional entropies.
// Each population represents posterior P(X=1 | a random preceding prefix).
std::vector<double> estimate_polar_entropies(double p, size_t N, size_t samples) {
    if (N == 0 || (N & (N - 1)) != 0 || samples == 0)
        throw std::invalid_argument("N must be a power of two and samples must be positive");

    std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> choose_sample(0, samples - 1);
    std::vector<double> initial(samples, p);

    std::function<std::vector<double>(const std::vector<double>&, size_t)> recurse;
    recurse = [&](const std::vector<double>& posterior, size_t length) {
        if (length == 1) {
            double entropy = 0.0;
            for (double q : posterior)
                entropy += binary_entropy(q);
            return std::vector<double>{entropy / posterior.size()};
        }

        std::vector<double> minus(samples), plus(samples);
        std::uniform_real_distribution<double> uniform01(0.0, 1.0);

        for (size_t k = 0; k < samples; ++k) {
            const double q1 = posterior[choose_sample(rng)];
            const double q2 = posterior[choose_sample(rng)];
            const double parity_one = q1 * (1.0 - q2) + (1.0 - q1) * q2;
            minus[k] = parity_one;

            const bool parity_is_one = uniform01(rng) < parity_one;
            if (parity_is_one) {
                const double denominator = parity_one;
                plus[k] = denominator > 0.0
                    ? ((1.0 - q1) * q2) / denominator
                    : 0.0;
            } else {
                const double parity_zero = 1.0 - parity_one;
                plus[k] = parity_zero > 0.0
                    ? (q1 * q2 + 0.0) / parity_zero
                    : 0.0;
            }
        }

        std::vector<double> result = recurse(minus, length / 2);
        std::vector<double> right = recurse(plus, length / 2);
        result.insert(result.end(), right.begin(), right.end());
        return result;
    };

    return recurse(initial, N);
}



int main() {
    const size_t N = 64;
    const double p = 0.1;

    const size_t samples = 100000;
    const double high_entropy_threshold = 0.9;
    const std::vector<double> entropies =
        estimate_polar_entropies(p, N, samples);

    double entropy_sum = 0.0;
    size_t high_entropy_count = 0;
    std::cout << "Estimated full conditional entropies (population MC):\n";
    for (size_t i = 0; i < entropies.size(); ++i) {
        std::cout << "H(U" << i;
        if (i > 0)
            std::cout << " | U[0.." << i - 1 << "]";
        std::cout << ") = " << entropies[i] << '\n';
        entropy_sum += entropies[i];
        if (entropies[i] >= high_entropy_threshold)
            ++high_entropy_count;
    }

    std::cout << "Sum of conditional entropies = " << entropy_sum << '\n'
              << "Theoretical H(U) = N*h2(p) = "
              << N * binary_entropy(p) << '\n'
              << "Positions with H >= " << high_entropy_threshold << ": "
              << high_entropy_count << " / " << N << '\n';

    return 0;
}
