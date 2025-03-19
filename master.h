#pragma once
#include "common.h"
#include <map>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

// Define tile size for matrix multiplication
#define TILE_SIZE 64

class Master {
public:
    Master(int port);
    ~Master();
    
    void start();
    void stop();
    
    // Set matrices for multiplication
    void setMatrices(const Matrix& a, const Matrix& b);
    
    // Begin computation after clients have connected
    void startComputation();
    
    // Check if computation is complete
    bool isComplete() const;
    
    // Get the result matrix
    Matrix getResult() const;
    
    // Get current number of connected clients
    int getClientCount() const;

private:
    // Server socket
    int serverSocket_;
    int port_;
    std::atomic<bool> running_;
    std::atomic<bool> computationStarted_;
    
    // Input and output matrices
    Matrix matrixA_;
    Matrix matrixB_;
    Matrix resultMatrix_;
    
    // Tracking tasks and clients
    std::map<int, std::thread> clientThreads_;
    mutable std::mutex clientsMutex_;
    // Track how many tasks each client is currently processing
    std::map<int, int> clientTaskCounts_; // <socket, task count>
    
    std::queue<Task> taskQueue_;
    std::mutex taskMutex_;
    std::condition_variable taskCV_;
    
    std::map<int, Result> results_;
    std::mutex resultsMutex_;
    
    std::atomic<int> nextTaskId_;
    std::atomic<int> completedTasks_;
    std::atomic<int> totalTasks_;

    // Client performance tracking
    struct ClientInfo {
        double cpuSpeed;        // GHz
        double lastTaskTime;    // ms
        double performanceRatio; // Higher is better
    };
    std::map<int, ClientInfo> clientPerformance_;
    std::mutex perfMutex_;
    
    // Calculate client performance ratio
    void updateClientPerformance(int clientSocket, double taskTimeMs);
    
    // Connection handling methods
    void acceptConnections();
    void handleClient(int clientSocket, struct sockaddr_in clientAddr);
    
    // Task management
    void processResult(const Result& result);
    
    // Calculate how to divide work based on available clients
    void redistributeWork();

    // Tile management
    void createTiledTasks();
};