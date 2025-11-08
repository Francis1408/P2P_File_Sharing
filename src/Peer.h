#ifndef PEER_H
#define PEER_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <utility> // Para std::pair

// Estrutura para armazenar informações do vizinho
struct NeighborInfo {
    std::string ip;
    int port;
};

// Estrutura para metadados do arquivo
struct FileInfo {
    std::string fileName;
    long long fileSize;
    int blockSize;
    int blockCount;
    std::string checksum;
};

class Peer {
public:
    // Construtor atualizado para aceitar uma lista de vizinhos
    Peer(int myPort, const std::vector<NeighborInfo>& neighbors);
    void start();

private:
    int myPort;
    std::vector<NeighborInfo> neighbors;
    std::atomic<bool> running;

    // Gerenciamento de arquivos e blocos
    FileInfo fileInfo;
    std::vector<bool> ownedBlocks;

    void serverLoop();
    void clientLoop();
    void handleConnection(int clientSock);
};

#endif