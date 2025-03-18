#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <sys/socket.h>

// Message types for communication protocol
enum MessageType {
    CLIENT_CONNECT = 1,
    CLIENT_DISCONNECT = 2,
    TASK_REQUEST = 3,
    TASK_RESPONSE = 4,
    MATRIX_DATA = 5,
    COMPUTATION_RESULT = 6,
    NO_WORK = 7,
    SHUTDOWN = 8
};

// Task structure for matrix multiplication
struct Task {
    int taskId;
    int startRow;
    int endRow;
    int matrixSize;
    // More fields can be added as needed
};

// Result structure
struct Result {
    int taskId;
    int startRow;
    int endRow;
    std::vector<double> resultRows;
};

// Matrix representation
class Matrix {
public:
    Matrix(int rows, int cols) : rows_(rows), cols_(cols), data_(rows * cols, 0.0) {}
    
    double& at(int row, int col) {
        return data_[row * cols_ + col];
    }
    
    double at(int row, int col) const {
        return data_[row * cols_ + col];
    }
    
    int rows() const { return rows_; }
    int cols() const { return cols_; }
    
    const std::vector<double>& data() const { return data_; }
    std::vector<double>& data() { return data_; }

private:
    int rows_;
    int cols_;
    std::vector<double> data_;
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