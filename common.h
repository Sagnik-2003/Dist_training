#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <cstring>

// Message types for communication protocol
enum MessageType {
    CLIENT_CONNECT = 1,
    CLIENT_DISCONNECT = 2,
    TASK_REQUEST = 3,
    TASK_RESPONSE = 4,
    MATRIX_DATA = 5,
    COMPUTATION_RESULT = 6,
    NO_WORK = 7,
    SHUTDOWN = 8,
    CPU_INFO = 9
};

// Task structure for matrix multiplication
struct Task {
    int taskId;
    int startRow;
    int endRow;
    int startCol;  // Start column for tiled multiplication
    int endCol;    // End column for tiled multiplication
    int matrixSize;
};

// Result structure
struct Result {
    int taskId;
    int startRow;
    int endRow;
    int startCol;
    int endCol;
    std::vector<double> resultTile;
    double executionTimeMs;  // Task execution time in milliseconds
};

// Matrix representation
// Ensure Matrix class uses heap memory

class Matrix {
    public:
        Matrix(int rows, int cols) : rows_(rows), cols_(cols) {
            data_ = new double[rows * cols];
            // Initialize all elements to zero
            std::memset(data_, 0, rows * cols * sizeof(double));
        }
        
        ~Matrix() {
            delete[] data_;
        }
        
        // Copy constructor
        Matrix(const Matrix& other) : rows_(other.rows_), cols_(other.cols_) {
            data_ = new double[rows_ * cols_];
            std::memcpy(data_, other.data_, rows_ * cols_ * sizeof(double));
        }
        
        // Move constructor
        Matrix(Matrix&& other) noexcept : rows_(other.rows_), cols_(other.cols_), data_(other.data_) {
            other.data_ = nullptr;
            other.rows_ = 0;
            other.cols_ = 0;
        }
        
        // Copy assignment
        Matrix& operator=(const Matrix& other) {
            if (this != &other) {
                delete[] data_;
                rows_ = other.rows_;
                cols_ = other.cols_;
                data_ = new double[rows_ * cols_];
                std::memcpy(data_, other.data_, rows_ * cols_ * sizeof(double));
            }
            return *this;
        }
        
        // Move assignment
        Matrix& operator=(Matrix&& other) noexcept {
            if (this != &other) {
                delete[] data_;
                rows_ = other.rows_;
                cols_ = other.cols_;
                data_ = other.data_;
                other.data_ = nullptr;
                other.rows_ = 0;
                other.cols_ = 0;
            }
            return *this;
        }
        
        inline double& at(int row, int col) {
            return data_[row * cols_ + col];
        }
        
        inline const double& at(int row, int col) const {
            return data_[row * cols_ + col];
        }
        
        int rows() const { return rows_; }
        int cols() const { return cols_; }
        double* data() { return data_; }
        const double* data() const { return data_; }
    
    private:
        int rows_;
        int cols_;
        double* data_; // Heap-allocated array
};

// Network message serialization/deserialization helpers
class NetworkMessage {
public:
    static std::vector<char> serializeMatrix(const Matrix& matrix);
    static Matrix deserializeMatrix(const std::vector<char>& data);
    
    static std::vector<char> serializeTask(const Task& task);
    static Task deserializeTask(const std::vector<char>& data);
    
    static std::vector<char> serializeResult(const Result& result);
    static Result deserializeResult(const std::vector<char>& data);
    
    static std::vector<char> createMessage(MessageType type, const std::vector<char>& payload);
    static std::pair<MessageType, std::vector<char>> parseMessage(const std::vector<char>& message);
    
    // Helper to send/receive messages over sockets
    static bool sendMessage(int sockfd, MessageType type, const std::vector<char>& payload);
    static std::pair<MessageType, std::vector<char>> receiveMessage(int sockfd);
};