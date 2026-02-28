#include <unistd.h>
#include <thread>
#include <chrono>
/*
 * AIE-OS AI Workload Demo: Matrix Compute
 * CPU-intensive tensor operations to trigger batch/AI classification.
 */

#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include <chrono>
#include <omp.h>

class Matrix {
public:
    std::vector<float> data;
    int rows, cols;
    
    Matrix(int r, int c) : rows(r), cols(c) {
        data.resize(r * c);
    }
    
    float& at(int i, int j) {
        return data[i * cols + j];
    }
    
    void randomize() {
        for (auto &v : data) {
            v = (float)rand() / RAND_MAX;
        }
    }
    
    void print_summary() const {
        float sum = 0, min_val = 1e9, max_val = -1e9;
        for (const auto &v : data) {
            sum += v;
            if (v < min_val) min_val = v;
            if (v > max_val) max_val = v;
        }
        std::cout << "[matrix_compute] Shape: (" << rows << "x" << cols << ") "
                  << "Mean: " << (sum / data.size()) << " "
                  << "Min: " << min_val << " Max: " << max_val << std::endl;
    }
};

Matrix matmul(const Matrix &A, const Matrix &B) {
    if (A.cols != B.rows) {
        throw std::runtime_error("Dimension mismatch");
    }
    
    Matrix C(A.rows, B.cols);
    
#pragma omp parallel for collapse(2)
    for (int i = 0; i < A.rows; i++) {
        for (int j = 0; j < B.cols; j++) {
            float sum = 0.0f;
            for (int k = 0; k < A.cols; k++) {
                sum += A.data[i * A.cols + k] * B.data[k * B.cols + j];
            }
            C.at(i, j) = sum;
        }
    }
    
    return C;
}

Matrix relu(const Matrix &A) {
    Matrix B = A;
#pragma omp parallel for
    for (auto &v : B.data) {
        if (v < 0) v = 0;
    }
    return B;
}

void run_demo(int matrix_size, int num_rounds) {
    std::cout << "[matrix_compute] Starting matrix compute demo" << std::endl;
    std::cout << "[matrix_compute] PID: " << getpid() << std::endl;
    std::cout << "[matrix_compute] Matrix size: " << matrix_size << "x" << matrix_size << std::endl;
    std::cout << "[matrix_compute] Rounds: " << num_rounds << std::endl;
    
    for (int round = 0; round < num_rounds; round++) {
        std::cout << "\n[matrix_compute] Round " << (round + 1) << "/" << num_rounds << std::endl;
        
        // Create random matrices
        Matrix A(matrix_size, matrix_size);
        Matrix B(matrix_size, matrix_size);
        A.randomize();
        B.randomize();
        
        // Perform computation
        auto start = std::chrono::high_resolution_clock::now();
        
        Matrix C = matmul(A, B);
        C = relu(C);  // Apply activation
        Matrix D = matmul(C, A);  // Another multiplication
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "[matrix_compute] Computation time: " << duration.count() << " ms | ";
        D.print_summary();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "[matrix_compute] Demo complete" << std::endl;
}

int main(int argc, char **argv) {
    int matrix_size = 256;
    int num_rounds = 5;
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--size" && i + 1 < argc) {
            matrix_size = std::atoi(argv[++i]);
        } else if (std::string(argv[i]) == "--rounds" && i + 1 < argc) {
            num_rounds = std::atoi(argv[++i]);
        }
    }
    
    try {
        run_demo(matrix_size, num_rounds);
    } catch (const std::exception &e) {
        std::cerr << "[matrix_compute] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
