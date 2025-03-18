#pragma once
#include "common.h"
#include <thread>
#include <atomic>
#include <immintrin.h>  // For SIMD instructions

class Client {
public:
    Client(const std::string& masterIp, int masterPort);
    ~Client();
    
    bool connect();
    void disconnect();
    void start();
    void stop();

private:
    std::string masterIp_;
    int masterPort_;
    int socket_;
    std::atomic<bool> running_;
    std::thread workerThread_;
    
    Matrix matrixA_;
    Matrix matrixB_;
    
    void workerLoop();
    Result computeMatrixMultiplication(const Task& task);
    
    // SIMD optimized matrix multiplication
    void multiplyRowsSIMD(const Matrix& a, const Matrix& b, std::vector<double>& result, 
                          int startRow, int endRow);
};