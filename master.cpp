#include "master.h"
#include <algorithm>
#include <cstring>

Master::Master(int port)
    : port_(port), running_(false), computationStarted_(false),
      matrixA_(1, 1), matrixB_(1, 1), resultMatrix_(1, 1),
      nextTaskId_(0), completedTasks_(0), totalTasks_(0) {}

Master::~Master()
{
    stop();
}

void Master::start()
{
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0)
    {
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

    if (bind(serverSocket_, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        std::cerr << "Error binding socket\n";
        close(serverSocket_);
        return;
    }

    // Listen for connections
    if (listen(serverSocket_, 10) < 0)
    {
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

void Master::stop()
{
    if (!running_)
        return;

    running_ = false;
    close(serverSocket_);

    // Wake up any waiting threads
    taskCV_.notify_all();

    // Send shutdown to all clients
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto &client : clientThreads_)
    {
        NetworkMessage::sendMessage(client.first, SHUTDOWN, {});
        client.second.join();
    }
    clientThreads_.clear();
}

void Master::startComputation()
{
    if (computationStarted_)
    {
        std::cerr << "Computation already started\n";
        return;
    }

    // Lock to check if we have clients
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        if (clientThreads_.empty())
        {
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

void Master::setMatrices(const Matrix &a, const Matrix &b)
{
    // Verify matrices can be multiplied
    if (a.cols() != b.rows())
    {
        std::cerr << "Invalid matrix dimensions for multiplication\n";
        return;
    }

    matrixA_ = a;
    matrixB_ = b;
    resultMatrix_ = Matrix(a.rows(), b.cols());

    // Create tiled tasks
    createTiledTasks();
}

void Master::createTiledTasks()
{
    int rows = matrixA_.rows();
    int cols = matrixB_.cols();
    int common = matrixA_.cols(); // = matrixB_.rows()

    // Calculate number of tiles in each dimension
    int rowTiles = (rows + TILE_SIZE - 1) / TILE_SIZE;
    int colTiles = (cols + TILE_SIZE - 1) / TILE_SIZE;

    // Reset counters
    totalTasks_ = 0;
    completedTasks_ = 0;
    nextTaskId_ = 0;

    std::lock_guard<std::mutex> lock(taskMutex_);

    // Create tasks for each tile
    for (int i = 0; i < rowTiles; i++)
    {
        int startRow = i * TILE_SIZE;
        int endRow = std::min(startRow + TILE_SIZE, rows);

        for (int j = 0; j < colTiles; j++)
        {
            Task task;
            task.taskId = nextTaskId_++;
            task.startRow = startRow;
            task.endRow = endRow;
            task.startCol = j * TILE_SIZE;
            task.endCol = std::min(task.startCol + TILE_SIZE, cols);
            task.matrixSize = common;

            taskQueue_.push(task);
            totalTasks_++;
        }
    }

    std::cout << "Created " << totalTasks_ << " tiled tasks" << std::endl;
}

bool Master::isComplete() const
{
    return completedTasks_ >= totalTasks_ && computationStarted_;
}

Matrix Master::getResult() const
{
    return resultMatrix_;
}

int Master::getClientCount() const
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return clientThreads_.size();
}

void Master::acceptConnections()
{
    while (running_)
    {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        int clientSocket = accept(serverSocket_, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (clientSocket < 0)
        {
            if (running_)
            {
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

// Modify this function to handle task distribution more efficiently
void Master::handleClient(int clientSocket, struct sockaddr_in clientAddr)
{

    // Store the client's IP address for fair distribution tracking
    std::string clientIp = inet_ntoa(clientAddr.sin_addr);

    // Wait for CPU information
    auto [msgType, cpuInfoData] = NetworkMessage::receiveMessage(clientSocket);

    if (msgType == CPU_INFO && cpuInfoData.size() >= sizeof(double))
    {
        double cpuSpeed;
        std::memcpy(&cpuSpeed, cpuInfoData.data(), sizeof(double));

        // Store client performance info
        {
            std::lock_guard<std::mutex> lock(perfMutex_);
            clientPerformance_[clientSocket].cpuSpeed = cpuSpeed;
            clientPerformance_[clientSocket].performanceRatio = cpuSpeed; // Initially based on CPU speed
        }

        std::cout << "Client " << clientIp << " reported CPU speed: " << cpuSpeed << " GHz\n";
    }

    // Send matrices A and B to the client
    std::vector<char> matrixAData = NetworkMessage::serializeMatrix(matrixA_);
    NetworkMessage::sendMessage(clientSocket, MATRIX_DATA, matrixAData);

    std::vector<char> matrixBData = NetworkMessage::serializeMatrix(matrixB_);
    NetworkMessage::sendMessage(clientSocket, MATRIX_DATA, matrixBData);

    // Initialize task count for this client
    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        clientTaskCounts_[clientSocket] = 0;
    }

    while (running_)
    {
        // Receive message from client
        auto [msgType, payload] = NetworkMessage::receiveMessage(clientSocket);

        if (msgType == TASK_REQUEST)
        {
            // Client is asking for work
            Task task;
            bool hasTask = false;

            {
                std::unique_lock<std::mutex> lock(taskMutex_);

                // Wait until computation has started
                taskCV_.wait(lock, [this]()
                             { return computationStarted_ || !running_; });

                // If we're shutting down, exit
                if (!running_)
                    break;

                // Get client task count
                int &clientTaskCount = clientTaskCounts_[clientSocket];

                if (!taskQueue_.empty())
                {
                    // Get client performance ratio
                    double perfRatio = 1.0;
                    {
                        std::lock_guard<std::mutex> perfLock(perfMutex_);
                        perfRatio = clientPerformance_[clientSocket].performanceRatio;
                    }

                    // Check if this client should get a task based on its performance
                    bool shouldAssignTask = true;

                    // Only do load balancing if we have multiple clients
                    if (clientTaskCounts_.size() > 1)
                    {
                        // Calculate weighted task count (tasks / performance)
                        double weightedTaskCount = clientTaskCount / perfRatio;

                        // Check if any client has a higher weighted task count
                        for (const auto &[otherSocket, otherCount] : clientTaskCounts_)
                        {
                            if (otherSocket == clientSocket)
                                continue;

                            double otherPerfRatio = 1.0;
                            {
                                std::lock_guard<std::mutex> perfLock(perfMutex_);
                                if (clientPerformance_.find(otherSocket) != clientPerformance_.end())
                                {
                                    otherPerfRatio = clientPerformance_[otherSocket].performanceRatio;
                                }
                            }

                            double otherWeightedCount = otherCount / otherPerfRatio;

                            // If this client already has more work relative to its performance,
                            // and there are enough tasks for everyone, don't give it more work yet
                            if (weightedTaskCount > otherWeightedCount &&
                                taskQueue_.size() <= clientTaskCounts_.size())
                            {
                                shouldAssignTask = false;
                                break;
                            }
                        }
                    }

                    if (shouldAssignTask)
                    {
                        task = taskQueue_.front();
                        taskQueue_.pop();
                        hasTask = true;
                        clientTaskCount++; // Increment task count for this client
                    }
                }
            }

            if (hasTask)
            {
                // Send task to client
                std::vector<char> taskData = NetworkMessage::serializeTask(task);
                NetworkMessage::sendMessage(clientSocket, TASK_RESPONSE, taskData);

                std::cout << "Assigned task " << task.taskId << " to client "
                          << clientIp << " (socket " << clientSocket << ")" << std::endl;
            }
            else if (isComplete())
            {
                // No more tasks, send shutdown
                NetworkMessage::sendMessage(clientSocket, SHUTDOWN, {});
                break;
            }
            else
            {
                // No tasks currently, but computation not complete
                NetworkMessage::sendMessage(clientSocket, NO_WORK, {});

                // Wait a bit before client retries
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        else if (msgType == COMPUTATION_RESULT)
        {
            // Received computation result
            Result result = NetworkMessage::deserializeResult(payload);

            // Update performance metrics based on execution time
            updateClientPerformance(clientSocket, result.executionTimeMs);

            // Decrement task count when result is received
            {
                std::lock_guard<std::mutex> lock(taskMutex_);
                clientTaskCounts_[clientSocket]--;
            }

            processResult(result);
        }
        else if (msgType == CLIENT_DISCONNECT)
        {
            // Client is disconnecting
            std::cout << "Client disconnected: " << clientIp << std::endl;
            break;
        }
    }

    close(clientSocket);

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = clientThreads_.find(clientSocket);
        if (it != clientThreads_.end())
        {
            it->second.detach(); // Detach the thread
            clientThreads_.erase(it);
        }

        {
            std::lock_guard<std::mutex> perfLock(perfMutex_);
            clientPerformance_.erase(clientSocket);
        }

        std::cout << "Connected clients: " << clientThreads_.size() << std::endl;
    }
}

void Master::updateClientPerformance(int clientSocket, double taskTimeMs)
{
    std::lock_guard<std::mutex> lock(perfMutex_);

    ClientInfo &info = clientPerformance_[clientSocket];
    info.lastTaskTime = taskTimeMs;

    // Update performance ratio based on recent task time
    // We want higher ratio for faster clients
    if (taskTimeMs > 0)
    {
        // Blend old ratio with new measurement (exponential smoothing)
        const double alpha = 0.3;              // Smoothing factor
        double newRatio = 1000.0 / taskTimeMs; // Normalize: tasks per second
        info.performanceRatio = (1 - alpha) * info.performanceRatio + alpha * newRatio;
    }

    std::cout << "Client " << clientSocket << " performance ratio updated to: "
              << info.performanceRatio << std::endl;
}

void Master::processResult(const Result &result)
{
    // Update result matrix with the computed tile
    int resultCols = resultMatrix_.cols();
    int tileWidth = result.endCol - result.startCol;

    for (int row = result.startRow; row < result.endRow; row++)
    {
        for (int col = result.startCol; col < result.endCol; col++)
        {
            int localRow = row - result.startRow;
            int localCol = col - result.startCol;
            int tileIdx = localRow * tileWidth + localCol;

            resultMatrix_.at(row, col) = result.resultTile[tileIdx];
        }
    }

    completedTasks_++;
    std::cout << "Completed task " << result.taskId
              << " (" << completedTasks_ << "/" << totalTasks_ << ")" << std::endl;

    if (isComplete())
    {
        std::cout << "Matrix multiplication complete!" << std::endl;
    }
}

void Master::redistributeWork()
{
    // Logic to redistribute work when clients join/leave
    // This would rebalance remaining tasks in the queue
}