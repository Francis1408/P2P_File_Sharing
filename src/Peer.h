#ifndef PEER_H
#define PEER_H

#include <atomic>
#include <filesystem>
#include <mutex>
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
    std::string downloadRoot;
    bool fileAssembled = false;

    // Gerenciamento de arquivos e blocos
    FileInfo fileInfo;
    std::vector<bool> ownedBlocks;

    std::optional<FileProcessor::MetadataContent> localMetadata;
    std::optional<FileProcessor::MetadataContent> remoteMetadata;
    mutable std::mutex ownedBlocksMutex;

    void serverLoop();
    void clientLoop();
    void handleConnection(int clientSock);
    void handleGetMetadata(int clientSock);
    void handleRequestBlock(int clientSock, const std::vector<std::uint8_t>& payload);
    void sendErrorMessage(int clientSock, const std::string& message);

    bool requestBlockFromNeighbor(const NeighborInfo& neighbor, int blockIndex);
    bool saveReceivedBlock(int blockIndex, const std::vector<std::uint8_t>& data);
    int findNextMissingBlock() const;
    void tryAssembleFile();
    std::filesystem::path ensureDownloadDir() const;
    bool hasBlock(int blockIndex) const;
    bool hasAllBlocks() const;
};

#endif
