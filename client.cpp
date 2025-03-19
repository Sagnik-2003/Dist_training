#include "client.h"
#include <cstring>
#include <iostream>

Client::Client(const std::string& masterIp, int masterPort)
    : masterIp_(masterIp), masterPort_(masterPort), socket_(-1), running_(false),
      matrixA_(1, 1), matrixB_(1, 1), cpuClockSpeed_(detectCpuClockSpeed()) {}

Client::~Client() {
    disconnect();
}

bool Client::connect() {
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ < 0) {
        std::cerr << "Error creating socket\n";
        return false;
    }
    
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(masterPort_);
    
    if (inet_pton(AF_INET, masterIp_.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address\n";
        close(socket_);
        socket_ = -1;
        return false;
    }
    
    if (::connect(socket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed\n";
        close(socket_);
        socket_ = -1;
        return false;
    }
    
    std::cout << "Connected to master at " << masterIp_ << ":" << masterPort_ << std::endl;
    
    std::vector<char> cpuInfo(sizeof(double));
    std::memcpy(cpuInfo.data(), &cpuClockSpeed_, sizeof(double));
    if (!NetworkMessage::sendMessage(socket_, CPU_INFO, cpuInfo)) {
        std::cerr << "Failed to send CPU info\n";
        disconnect();
        return false;
    }
    
    std::cout << "Sent CPU clock speed: " << cpuClockSpeed_ << " GHz\n";

    // Receive matrices from master
    // auto [msgType1, payload1] = NetworkMessage::receiveMessage(socket_);
    // if (msgType1 != MATRIX_DATA) {
    //     std::cerr << "Expected matrix A data\n";
    //     disconnect();
    //     return false;
    // }
    // matrixA_ = NetworkMessage::deserializeMatrix(payload1);
    
    // auto [msgType2, payload2] = NetworkMessage::receiveMessage(socket_);
    // if (msgType2 != MATRIX_DATA) {
    //     std::cerr << "Expected matrix B data\n";
    //     disconnect();
    //     return false;
    // }
    // matrixB_ = NetworkMessage::deserializeMatrix(payload2);
    
    // std::cout << "Received matrices: A(" << matrixA_.rows() << "x" << matrixA_.cols() 
    //           << "), B(" << matrixB_.rows() << "x" << matrixB_.cols() << ")" << std::endl;
    
    return true;
}

void Client::disconnect() {
    stop();
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }
}

void Client::start() {
    if (socket_ < 0) {
        std::cerr << "Cannot start client: not connected to master\n";
        return;
    }
    
    running_ = true;
    workerThread_ = std::thread(&Client::workerLoop, this);
}

void Client::stop() {
    if (running_) {
        running_ = false;
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
    }
}

void Client::workerLoop() {
    while (running_) {
        // Request a task from the master
        if (!NetworkMessage::sendMessage(socket_, TASK_REQUEST, {})) {
            std::cerr << "Error requesting task\n";
            break;
        }
        
        // Receive task or other response
        auto [msgType, payload] = NetworkMessage::receiveMessage(socket_);
        
        if (msgType == TASK_RESPONSE) {
            // Process the task
            Task task = NetworkMessage::deserializeTask(payload);
            std::cout << "Received task " << task.taskId << " (rows " << task.startRow 
                      << " to " << task.endRow << ")\n";
            
            // Compute the result
            Result result = computeMatrixMultiplication(task);
            
            // Send the result back
            std::vector<char> resultData = NetworkMessage::serializeResult(result);
            if (!NetworkMessage::sendMessage(socket_, COMPUTATION_RESULT, resultData)) {
                std::cerr << "Error sending result\n";
                break;
            }
        }
        else if (msgType == NO_WORK) {
            // No work available right now, wait and try again
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        else if (msgType == SHUTDOWN || msgType == CLIENT_DISCONNECT) {
            // Master sent shutdown signal
            std::cout << "Received shutdown from master\n";
            break;
        }
        else {
            std::cerr << "Unexpected message type: " << msgType << std::endl;
            break;
        }
    }
    
    std::cout << "Worker thread stopped\n";
}

double Client::detectCpuClockSpeed() {
    // Try to read CPU frequency from /proc/cpuinfo
    FILE* fp = fopen("/proc/cpuinfo", "r");
    if (!fp) return 2.0; // Default value if can't read
    
    char buffer[1024];
    double speed = 0.0;
    
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (strstr(buffer, "cpu MHz") || strstr(buffer, "CPU MHz")) {
            char* value = strchr(buffer, ':');
            if (value) {
                speed = strtod(value + 1, NULL) / 1000.0; // Convert MHz to GHz
                break;
            }
        } else if (strstr(buffer, "GHz")) {
            // Try to parse GHz value directly
            char* value = strstr(buffer, ":") + 1;
            if (value) {
                // Skip leading spaces
                while (*value == ' ') value++;
                // Extract the number (e.g. "3.2 GHz")
                speed = strtod(value, NULL);
                break;
            }
        }
    }
    
    fclose(fp);
    return speed > 0.0 ? speed : 2.0; // Use default if couldn't determine
}

Result Client::computeMatrixMultiplication(const Task& task) {
    // Start timing
    taskStartTime_ = std::chrono::high_resolution_clock::now();

    Result result;
    result.taskId = task.taskId;
    result.startRow = task.startRow;
    result.endRow = task.endRow;
    result.startCol = task.startCol;
    result.endCol = task.endCol;
    
    // Size for the result tile
    int numRows = task.endRow - task.startRow;
    int numCols = task.endCol - task.startCol;
    result.resultTile.resize(numRows * numCols, 0.0);
    
    // Extract the needed parts of the matrices for this tile
    for (int i = task.startRow; i < task.endRow; i++) {
        for (int j = task.startCol; j < task.endCol; j++) {
            double sum = 0.0;
            for (int k = 0; k < matrixA_.cols(); k++) {
                sum += matrixA_.at(i, k) * matrixB_.at(k, j);
            }
            int localRow = i - task.startRow;
            int localCol = j - task.startCol;
            result.resultTile[localRow * numCols + localCol] = sum;
        }
    }
    
    // Compute the task execution time
    auto endTime = std::chrono::high_resolution_clock::now();
    double taskTimeMs = std::chrono::duration<double, std::milli>(endTime - taskStartTime_).count();
    
    // Add timing info to the result
    result.executionTimeMs = taskTimeMs;
    
    return result;
}

void Client::multiplyRowsSIMD(const Matrix& a, const Matrix& b, std::vector<double>& result, 
                             int startRow, int endRow) {
    // Transpose matrix B for better memory access patterns
    Matrix bTransposed(b.cols(), b.rows());
    for (int i = 0; i < b.rows(); i++) {
        for (int j = 0; j < b.cols(); j++) {
            bTransposed.at(j, i) = b.at(i, j);
        }
    }
    
    int n = a.cols(); // = b.rows()
    int m = b.cols(); // = result cols
    
    // For each assigned row of A
    for (int i = startRow; i < endRow; i++) {
        // For each column of B (result column)
        for (int j = 0; j < m; j++) {
            double sum = 0.0;
            int k = 0;
            
            // Process 4 elements at a time using AVX
            if (n >= 4) {
                // Use AVX SIMD instructions for faster computation
                __m256d sumVec = _mm256_setzero_pd(); // Initialize sum vector to zeros
                
                // Process 4 elements at a time
                for (; k <= n - 4; k += 4) {
                    __m256d aVec = _mm256_loadu_pd(&a.data()[i * n + k]);
                    __m256d bVec = _mm256_loadu_pd(&bTransposed.data()[j * n + k]);
                    sumVec = _mm256_add_pd(sumVec, _mm256_mul_pd(aVec, bVec));
                }
                
                // Extract and sum the four partial sums
                alignas(32) double partialSums[4];
                _mm256_store_pd(partialSums, sumVec);
                sum = partialSums[0] + partialSums[1] + partialSums[2] + partialSums[3];
            }
            
            // Process remaining elements
            for (; k < n; k++) {
                sum += a.at(i, k) * bTransposed.at(j, k);
            }
            
            // Store the result
            result[(i - startRow) * m + j] = sum;
        }
    }
}