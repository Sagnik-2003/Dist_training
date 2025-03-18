#pragma once
#include "common.h"
#include <map>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

class Master {
public:
    Master(int port);
    ~Master();
    
    void start();
    void stop();
    
    // Set matrices for multiplication
    void setMatrices(const Matrix& a, const Matrix& b);
    
    // Check if computation is complete
    bool isComplete() const;
    
    // Get the result matrix
    Matrix getResult() const;

private:
    // Server socket
    int serverSocket_;
    int port_;
    std::atomic<bool> running_;
    
    // Input and output matrices
    Matrix matrixA_;
    Matrix matrixB_;
    Matrix resultMatrix_;
    
    // Tracking tasks and clients
    std::map<int, std::thread> clientThreads_;
    std::mutex clientsMutex_;
    
    std::queue<Task> taskQueue_;
    std::mutex taskMutex_;
    std::condition_variable taskCV_;
    
    std::map<int, Result> results_;
    std::mutex resultsMutex_;
    
    std::atomic<int> nextTaskId_;
    std::atomic<int> completedTasks_;
    std::atomic<int> totalTasks_;
    
    // Connection handling methods
    void acceptConnections();
    void handleClient(int clientSocket, struct sockaddr_in clientAddr);
    
    // Task management
    Task createTask();
    void processResult(const Result& result);
    
    // Calculate how to divide work based on available clients
    void redistributeWork();
};