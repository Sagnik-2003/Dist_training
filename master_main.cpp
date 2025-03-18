#include "master.h"
#include <iostream>
#include <random>
#include <ctime>
#include <thread>
#include <chrono>
#include <iomanip>

// Helper to generate random matrix
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

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port> [matrix_size=1000]\n";
        return 1;
    }
    
    int port = std::stoi(argv[1]);
    int matrixSize = (argc > 2) ? std::stoi(argv[2]) : 1000;
    
    // Create and start master
    Master master(port);
    master.start();
    
    // Generate random matrices
    std::cout << "Generating random matrices of size " << matrixSize << "x" << matrixSize << std::endl;
    Matrix matrixA = generateRandomMatrix(matrixSize, matrixSize);
    Matrix matrixB = generateRandomMatrix(matrixSize, matrixSize);
    
    // Set matrices for computation
    master.setMatrices(matrixA, matrixB);
    
    // Wait for clients to connect
    std::cout << "\nWaiting for clients to connect...\n";
    std::cout << "Press Enter when ready to start computation with the connected clients\n";
    std::cin.get();
    
    // Start computation with connected clients
    master.startComputation();
    
    // Wait for computation to complete
    std::cout << "Computation started. Waiting for completion...\n";
    while (!master.isComplete()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "Computation completed successfully!" << std::endl;
    
    // Display results (for small matrices only)
    if (matrixSize <= 10) {
        Matrix result = master.getResult();
        std::cout << "\nResult Matrix (" << result.rows() << "x" << result.cols() << "):\n";
        for (int i = 0; i < result.rows(); i++) {
            for (int j = 0; j < result.cols(); j++) {
                std::cout << std::fixed << std::setprecision(4) << result.at(i, j) << " ";
            }
            std::cout << std::endl;
        }
    }
    
    // Wait before shutdown to allow viewing output
    std::cout << "\nPress Enter to shutdown the server..." << std::endl;
    std::cin.get();
    
    return 0;
}