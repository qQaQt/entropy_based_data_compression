#include <iostream>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <stdexcept>
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
    if (N == 0 || (N & (N - 1)) != 0) {
        throw std::invalid_argument("N must be a power of 2");
    }

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

    for (size_t i = 0; i < N; ++i) {
        size_t j = reverse_bits(i, n);
        result[i] = G[j];
    }

    return G;
}

void print_matrix(const Matrix& G) {
    for (const auto& row : G) {
        for (uint8_t bit : row) {
            std::cout << static_cast<int>(bit) << " ";
        }
        std::cout << '\n';
    }
}

int main() {
    size_t N = 4;

    Matrix G = build_generator_matrix(N);

    print_matrix(G);

    return 0;
}