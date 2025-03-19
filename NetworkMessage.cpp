#include "common.h"
#include <cstring>

std::vector<char> NetworkMessage::serializeMatrix(const Matrix& matrix) {
    std::vector<char> result;
    int rows = matrix.rows();
    int cols = matrix.cols();
    size_t dataSize = rows * cols * sizeof(double);
    size_t headerSize = 2 * sizeof(int);
    result.resize(headerSize + dataSize);
    
    char* ptr = result.data();
    
    // Add dimensions
    std::memcpy(ptr, &rows, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(ptr, &cols, sizeof(int));
    ptr += sizeof(int);
    
    // Add data
    std::memcpy(ptr, matrix.data(), dataSize);
    
    return result;
}

Matrix NetworkMessage::deserializeMatrix(const std::vector<char>& data) {
    const char* ptr = data.data();
    int rows, cols;
    
    // Get dimensions
    std::memcpy(&rows, ptr, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(&cols, ptr, sizeof(int));
    ptr += sizeof(int);
    
    // Create matrix
    Matrix matrix(rows, cols);
    
    // Fill data (directly copy to the matrix's data array)
    std::memcpy(matrix.data(), ptr, rows * cols * sizeof(double));
    
    return matrix;
}

std::vector<char> NetworkMessage::serializeTask(const Task& task) {
    std::vector<char> result;
    size_t size = sizeof(int) * 6;
    
    result.resize(size);
    char* ptr = result.data();
    
    std::memcpy(ptr, &task.taskId, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(ptr, &task.startRow, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(ptr, &task.endRow, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(ptr, &task.startCol, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(ptr, &task.endCol, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(ptr, &task.matrixSize, sizeof(int));
    
    return result;
}

Task NetworkMessage::deserializeTask(const std::vector<char>& data) {
    Task task;
    const char* ptr = data.data();
    
    std::memcpy(&task.taskId, ptr, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(&task.startRow, ptr, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(&task.endRow, ptr, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(&task.startCol, ptr, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(&task.endCol, ptr, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(&task.matrixSize, ptr, sizeof(int));
    
    return task;
}

std::vector<char> NetworkMessage::serializeResult(const Result& result) {
    std::vector<char> data;
    size_t size = sizeof(int) * 5 + sizeof(double) * result.resultTile.size();
    
    data.resize(size);
    char* ptr = data.data();
    
    std::memcpy(ptr, &result.taskId, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(ptr, &result.startRow, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(ptr, &result.endRow, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(ptr, &result.startCol, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(ptr, &result.endCol, sizeof(int));
    ptr += sizeof(int);
    
    // Copy result tile
    std::memcpy(ptr, result.resultTile.data(), sizeof(double) * result.resultTile.size());
    
    return data;
}

Result NetworkMessage::deserializeResult(const std::vector<char>& data) {
    Result result;
    const char* ptr = data.data();
    
    std::memcpy(&result.taskId, ptr, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(&result.startRow, ptr, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(&result.endRow, ptr, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(&result.startCol, ptr, sizeof(int));
    ptr += sizeof(int);
    std::memcpy(&result.endCol, ptr, sizeof(int));
    ptr += sizeof(int);
    
    // Calculate size of result data
    int numRows = result.endRow - result.startRow;
    int numCols = result.endCol - result.startCol;
    
    // Extract result data
    result.resultTile.resize(numRows * numCols);
    std::memcpy(result.resultTile.data(), ptr, sizeof(double) * result.resultTile.size());
    
    return result;
}

std::vector<char> NetworkMessage::createMessage(MessageType type, const std::vector<char>& payload) {
    std::vector<char> message;
    size_t msgSize = sizeof(MessageType) + sizeof(size_t) + payload.size();
    message.resize(msgSize);
    
    char* ptr = message.data();
    
    // Add message type
    std::memcpy(ptr, &type, sizeof(MessageType));
    ptr += sizeof(MessageType);
    
    // Add payload size
    size_t payloadSize = payload.size();
    std::memcpy(ptr, &payloadSize, sizeof(size_t));
    ptr += sizeof(size_t);
    
    // Add payload
    if (!payload.empty()) {
        std::memcpy(ptr, payload.data(), payload.size());
    }
    
    return message;
}

std::pair<MessageType, std::vector<char>> NetworkMessage::parseMessage(const std::vector<char>& message) {
    const char* ptr = message.data();
    
    // Extract message type
    MessageType type;
    std::memcpy(&type, ptr, sizeof(MessageType));
    ptr += sizeof(MessageType);
    
    // Extract payload size
    size_t payloadSize;
    std::memcpy(&payloadSize, ptr, sizeof(size_t));
    ptr += sizeof(size_t);
    
    // Extract payload
    std::vector<char> payload(payloadSize);
    if (payloadSize > 0) {
        std::memcpy(payload.data(), ptr, payloadSize);
    }
    
    return {type, payload};
}

bool NetworkMessage::sendMessage(int sockfd, MessageType type, const std::vector<char>& payload) {
    std::vector<char> message = createMessage(type, payload);
    
    size_t totalSent = 0;
    while (totalSent < message.size()) {
        ssize_t sent = send(sockfd, message.data() + totalSent, message.size() - totalSent, 0);
        if (sent < 0) {
            return false;
        }
        totalSent += sent;
    }
    
    return true;
}

std::pair<MessageType, std::vector<char>> NetworkMessage::receiveMessage(int sockfd) {
    // First receive header to determine payload size
    char headerBuf[sizeof(MessageType) + sizeof(size_t)];
    size_t headerReceived = 0;
    
    while (headerReceived < sizeof(headerBuf)) {
        ssize_t received = recv(sockfd, headerBuf + headerReceived, 
                               sizeof(headerBuf) - headerReceived, 0);
        if (received <= 0) {
            return {CLIENT_DISCONNECT, {}};  // Connection closed or error
        }
        headerReceived += received;
    }
    
    // Extract message type and payload size
    MessageType type;
    std::memcpy(&type, headerBuf, sizeof(MessageType));
    
    size_t payloadSize;
    std::memcpy(&payloadSize, headerBuf + sizeof(MessageType), sizeof(size_t));
    
    // Receive payload
    std::vector<char> payload(payloadSize);
    size_t payloadReceived = 0;
    
    while (payloadReceived < payloadSize) {
        ssize_t received = recv(sockfd, payload.data() + payloadReceived, 
                               payloadSize - payloadReceived, 0);
        if (received <= 0) {
            return {CLIENT_DISCONNECT, {}};  // Connection closed or error
        }
        payloadReceived += received;
    }
    
    return {type, payload};
}