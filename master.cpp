#include "master.h"
#include <algorithm>
#include <cstring>

Master::Master(int port) 
    : port_(port), running_(false), computationStarted_(false),
      matrixA_(1, 1), matrixB_(1, 1), resultMatrix_(1, 1),
      nextTaskId_(0), completedTasks_(0), totalTasks_(0) {}

Master::~Master() {
    stop();
}

void Master::start() {
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        std::cerr << "Error creating socket\n";
        return;
    }
    
    // Enable address reuse
    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind socket
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);
    
    if (bind(serverSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Error binding socket\n";
        close(serverSocket_);
        return;
    }
    
    // Listen for connections
    if (listen(serverSocket_, 10) < 0) {
        std::cerr << "Error listening\n";
        close(serverSocket_);
        return;
    }
    
    running_ = true;
    std::cout << "Master server started on port " << port_ << std::endl;
    std::cout << "Waiting for clients to connect...\n";
    std::cout << "Connected clients: 0\n";
    
    // Start accepting client connections in a separate thread
    std::thread acceptThread(&Master::acceptConnections, this);
    acceptThread.detach();
}

void Master::stop() {
    if (!running_) return;
    
    running_ = false;
    close(serverSocket_);
    
    // Wake up any waiting threads
    taskCV_.notify_all();
    
    // Send shutdown to all clients
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& client : clientThreads_) {
        NetworkMessage::sendMessage(client.first, SHUTDOWN, {});
        client.second.join();
    }
    clientThreads_.clear();
}

void Master::startComputation() {
    if (computationStarted_) {
        std::cerr << "Computation already started\n";
        return;
    }
    
    // Lock to check if we have clients
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        if (clientThreads_.empty()) {
            std::cerr << "No clients connected. Cannot start computation.\n";
            return;
        }
        
        std::cout << "Starting computation with " << clientThreads_.size() << " connected clients\n";
    }

    // Set computation flag
    computationStarted_ = true;
    
    // Notify all waiting clients that computation has started
    taskCV_.notify_all();
}

void Master::setMatrices(const Matrix& a, const Matrix& b) {
    // Verify matrices can be multiplied
    if (a.cols() != b.rows()) {
        std::cerr << "Invalid matrix dimensions for multiplication\n";
        return;
    }
    
    matrixA_ = a;
    matrixB_ = b;
    resultMatrix_ = Matrix(a.rows(), b.cols());
    
    // Calculate total number of tasks (one per row for simplicity)
    totalTasks_ = a.rows();
    completedTasks_ = 0;
    
    // Create initial tasks
    for (int i = 0; i < a.rows(); i++) {
        Task task;
        task.taskId = nextTaskId_++;
        task.startRow = i;
        task.endRow = i + 1;
        task.matrixSize = a.cols();
        
        std::lock_guard<std::mutex> lock(taskMutex_);
        taskQueue_.push(task);
    }
    
    // Tasks will be distributed once startComputation() is called
}

bool Master::isComplete() const {
    return completedTasks_ >= totalTasks_ && computationStarted_;
}

Matrix Master::getResult() const {
    return resultMatrix_;
}

int Master::getClientCount() const {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return clientThreads_.size();
}

void Master::acceptConnections() {
    while (running_) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        
        int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            if (running_) {
                std::cerr << "Error accepting connection\n";
            }
            continue;
        }
        
        std::cout << "New client connected: " << inet_ntoa(clientAddr.sin_addr) << std::endl;
        
        // Create a thread to handle this client
        std::thread clientThread(&Master::handleClient, this, clientSocket, clientAddr);
        
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clientThreads_[clientSocket] = std::move(clientThread);
            std::cout << "Connected clients: " << clientThreads_.size() << std::endl;
        }
    }
}

void Master::handleClient(int clientSocket, struct sockaddr_in clientAddr) {
    // Send matrices A and B to the client
    std::vector<char> matrixAData = NetworkMessage::serializeMatrix(matrixA_);
    NetworkMessage::sendMessage(clientSocket, MATRIX_DATA, matrixAData);
    
    std::vector<char> matrixBData = NetworkMessage::serializeMatrix(matrixB_);
    NetworkMessage::sendMessage(clientSocket, MATRIX_DATA, matrixBData);
    
    while (running_) {
        // Receive message from client
        auto [msgType, payload] = NetworkMessage::receiveMessage(clientSocket);
        
        if (msgType == TASK_REQUEST) {
            // Client is asking for work
            Task task;
            bool hasTask = false;
            
            {
                std::unique_lock<std::mutex> lock(taskMutex_);
                
                // Wait until computation has started and there are tasks available
                taskCV_.wait(lock, [this]() { 
                    return computationStarted_ || !running_; 
                });
                
                // If we're shutting down, exit
                if (!running_) break;
                
                if (!taskQueue_.empty()) {
                    task = taskQueue_.front();
                    taskQueue_.pop();
                    hasTask = true;
                }
            }
            
            if (hasTask) {
                // Send task to client
                std::vector<char> taskData = NetworkMessage::serializeTask(task);
                NetworkMessage::sendMessage(clientSocket, TASK_RESPONSE, taskData);
            } else if (isComplete()) {
                // No more tasks, send shutdown
                NetworkMessage::sendMessage(clientSocket, SHUTDOWN, {});
                break;
            } else {
                // No tasks currently, but computation not complete
                NetworkMessage::sendMessage(clientSocket, NO_WORK, {});
                
                // Wait a bit before client retries
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } 
        else if (msgType == COMPUTATION_RESULT) {
            // Received computation result
            Result result = NetworkMessage::deserializeResult(payload);
            processResult(result);
        }
        else if (msgType == CLIENT_DISCONNECT) {
            // Client is disconnecting
            std::cout << "Client disconnected: " << inet_ntoa(clientAddr.sin_addr) << std::endl;
            break;
        }
    }
    
    close(clientSocket);
    
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = clientThreads_.find(clientSocket);
        if (it != clientThreads_.end()) {
            it->second.detach();  // Detach the thread
            clientThreads_.erase(it);
            std::cout << "Connected clients: " << clientThreads_.size() << std::endl;
        }
    }
}

void Master::processResult(const Result& result) {
    // Update result matrix with the computed rows
    for (int row = result.startRow; row < result.endRow; row++) {
        for (int col = 0; col < resultMatrix_.cols(); col++) {
            int idx = (row - result.startRow) * resultMatrix_.cols() + col;
            resultMatrix_.at(row, col) = result.resultRows[idx];
        }
    }
    
    completedTasks_++;
    std::cout << "Completed task " << result.taskId 
              << " (" << completedTasks_ << "/" << totalTasks_ << ")" << std::endl;
              
    if (isComplete()) {
        std::cout << "Matrix multiplication complete!" << std::endl;
    }
}

void Master::redistributeWork() {
    // Logic to redistribute work when clients join/leave
    // This would rebalance remaining tasks in the queue
}