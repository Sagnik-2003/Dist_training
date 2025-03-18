#include "client.h"
#include <iostream>
#include <thread>
#include <chrono>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <master_ip> <master_port>\n";
        return 1;
    }
    
    std::string masterIp = argv[1];
    int masterPort = std::stoi(argv[2]);
    
    // Create client
    Client client(masterIp, masterPort);
    
    // Connect to master
    if (!client.connect()) {
        std::cerr << "Failed to connect to master server\n";
        return 1;
    }
    
    // Start processing
    client.start();
    
    // Run until user decides to stop
    std::cout << "Client started. Press Enter to disconnect...\n";
    std::cin.get();
    
    // Stop and disconnect
    client.stop();
    client.disconnect();
    
    std::cout << "Client disconnected\n";
    
    return 0;
}