#ifndef PEER_H
#define PEER_H

#include <atomic>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "FileProcessor.h"

// Estrutura para armazenar informações do vizinho
struct NeighborInfo {
    std::string ip;
    int port;
};

class Peer {
public:
    // Construtor atualizado para aceitar uma lista de vizinhos e metadata opcional
    Peer(int myPort, const std::vector<NeighborInfo>& neighbors, std::string metadataPath = "");
    void start();

private:
    int myPort;
    std::vector<NeighborInfo> neighbors;
    std::atomic<bool> running;
    std::string metadataPath;

    // Gerenciamento de arquivos e blocos
    FileInfo fileInfo;
    std::vector<bool> ownedBlocks;

    std::optional<FileProcessor::MetadataContent> localMetadata;
    std::optional<FileProcessor::MetadataContent> remoteMetadata;

    void serverLoop();
    void clientLoop();
    void handleConnection(int clientSock);
    void handleGetMetadata(int clientSock);
    void sendErrorMessage(int clientSock, const std::string& message);
};

#endif
