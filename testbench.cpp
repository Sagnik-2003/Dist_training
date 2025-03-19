#include "master.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cassert>
#include <cmath>
#include <random>

// Function to perform matrix multiplication using brute force approach (O(n^3))
Matrix bruteForceMultiplication(const Matrix& A, const Matrix& B) {
    assert(A.cols() == B.rows());
    Matrix C(A.rows(), B.cols());
    
    for (int i = 0; i < A.rows(); ++i) {
        for (int j = 0; j < B.cols(); ++j) {
            for (int k = 0; k < A.cols(); ++k) {
                C.at(i, j) += A.at(i, k) * B.at(k, j);
            }
        }
    }
    
    return C;
}

// Helper functions for Strassen's algorithm
Matrix addMat(const Matrix& A, const Matrix& B) {
    assert(A.rows() == B.rows() && A.cols() == B.cols());
    Matrix C(A.rows(), A.cols());
    for (int i = 0; i < A.rows(); ++i) {
        for (int j = 0; j < A.cols(); ++j) {
            C.at(i, j) = A.at(i, j) + B.at(i, j);
        }
    }
    return C;
}

Matrix subtract(const Matrix& A, const Matrix& B) {
    assert(A.rows() == B.rows() && A.cols() == B.cols());
    Matrix C(A.rows(), A.cols());
    for (int i = 0; i < A.rows(); ++i) {
        for (int j = 0; j < A.cols(); ++j) {
            C.at(i, j) = A.at(i, j) - B.at(i, j);
        }
    }
    return C;
}
// Function to perform matrix multiplication using Strassen's algorithm (O(n^2.7))
Matrix strassenMultiplication(const Matrix& A, const Matrix& B) {
    assert(A.cols() == B.rows());
    int n = A.rows();
    if (n <= 2) {
        return bruteForceMultiplication(A, B);
    }

    int newSize = n / 2;
    Matrix A11(newSize, newSize), A12(newSize, newSize), A21(newSize, newSize), A22(newSize, newSize);
    Matrix B11(newSize, newSize), B12(newSize, newSize), B21(newSize, newSize), B22(newSize, newSize);

    for (int i = 0; i < newSize; ++i) {
        for (int j = 0; j < newSize; ++j) {
            A11.at(i, j) = A.at(i, j);
            A12.at(i, j) = A.at(i, j + newSize);
            A21.at(i, j) = A.at(i + newSize, j);
            A22.at(i, j) = A.at(i + newSize, j + newSize);

            B11.at(i, j) = B.at(i, j);
            B12.at(i, j) = B.at(i, j + newSize);
            B21.at(i, j) = B.at(i + newSize, j);
            B22.at(i, j) = B.at(i + newSize, j + newSize);
        }
    }

    Matrix M1 = strassenMultiplication(addMat(A11, A22), addMat(B11, B22));
    Matrix M2 = strassenMultiplication(addMat(A21, A22), B11);
    Matrix M3 = strassenMultiplication(A11, subtract(B12, B22));
    Matrix M4 = strassenMultiplication(A22, subtract(B21, B11));
    Matrix M5 = strassenMultiplication(addMat(A11, A12), B22);
    Matrix M6 = strassenMultiplication(subtract(A21, A11), addMat(B11, B12));
    Matrix M7 = strassenMultiplication(subtract(A12, A22), addMat(B21, B22));

    Matrix C11 = addMat(subtract(addMat(M1, M4), M5), M7);
    Matrix C12 = addMat(M3, M5);
    Matrix C21 = addMat(M2, M4);
    Matrix C22 = addMat(subtract(addMat(M1, M3), M2), M6);

    Matrix C(n, n);
    for (int i = 0; i < newSize; ++i) {
        for (int j = 0; j < newSize; ++j) {
            C.at(i, j) = C11.at(i, j);
            C.at(i, j + newSize) = C12.at(i, j);
            C.at(i + newSize, j) = C21.at(i, j);
            C.at(i + newSize, j + newSize) = C22.at(i, j);
        }
    }

    return C;
}


Matrix generateRandomMatrix(int rows, int cols) {
    Matrix matrix(rows, cols);
    std::default_random_engine generator(time(nullptr));
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            matrix.at(i, j) = distribution(generator);
        }
    }
    
    return matrix;
}

// Function to compare two matrices
bool compareMatrices(const Matrix& A, const Matrix& B) {
    int n = A.rows();
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (std::abs(A.at(i,j) - B.at(i,j)) > 1e-6) {
                return false;
            }
        }
    }
    return true;
}

// Test bench
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port> [matrix_size=1000]\n";
        return 1;
    }
    int port = std::stoi(argv[1]);

    int matrixSize = (argc > 2) ? std::stoi(argv[2]) : 1000;
    std::cout << "Generating random matrices of size " << matrixSize << "x" << matrixSize << std::endl;
    Matrix A = generateRandomMatrix(matrixSize, matrixSize);
    Matrix B = generateRandomMatrix(matrixSize, matrixSize);

    std::cout << "Starting brute force approach" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    Matrix C_brute = bruteForceMultiplication(A, B);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Brute force multiplication time: " << elapsed.count() << " seconds\n";
    
    std::cout << "Starting Strassen's Algorithm approach" << std::endl;
    start = std::chrono::high_resolution_clock::now();
    Matrix C_strassen = strassenMultiplication(A, B);
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Strassen's algorithm multiplication time: " << elapsed.count() << " seconds\n";

    Master master(port);
    master.start();
    master.setMatrices(A, B);

    std::cout << "Press Enter when ready to start computation with the connected clients\n";
    std::cin.get();
    master.startComputation();

    start = std::chrono::high_resolution_clock::now();
    while (!master.isComplete()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    auto C_distributed =  master.getResult();
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Distributed computation multiplication time: " << elapsed.count() << " seconds\n";

    assert(compareMatrices(C_brute, C_strassen));
    assert(compareMatrices(C_brute, C_distributed));
    std::cout << "Matrix multiplication results are correct.\n";

    return 0;
}