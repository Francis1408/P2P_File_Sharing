#include "Peer.h"
#include "FileProcessor.h"
#include <iostream>
#include <vector>

namespace {
constexpr std::size_t DEFAULT_BLOCK_SIZE = 1024;

void printUsage(const char* binaryName) {
    std::cerr << "Uso:\n"
              << "  " << binaryName << " --create-meta <arquivo> [tamanho_bloco]\n"
              << "  " << binaryName << " <minha_porta> <ip_vizinho1> <porta_vizinho1> [<ip_vizinho2> <porta_vizinho2> ...]\n";
}
}

int main(int argc, char* argv[]) {
    if (argc >= 2 && std::string(argv[1]) == "--create-meta") {
        if (argc < 3) {
            printUsage(argv[0]);
            return 1;
        }

        try {
            std::size_t blockSize = DEFAULT_BLOCK_SIZE;
            if (argc >= 4) {
                blockSize = static_cast<std::size_t>(std::stoul(argv[3]));
            }
            auto result = FileProcessor::createFileMetadata(argv[2], blockSize);
            std::cout << "Metadata gerada com sucesso!\n"
                      << "Arquivo original: " << result.info.fileName << " (" << result.info.fileSize << " bytes)\n"
                      << "Blocos gerados: " << result.info.blockCount << " de tamanho " << result.info.blockSize << " bytes\n"
                      << "Checksum (SHA-256): " << result.info.checksum << "\n"
                      << "Pasta dos blocos: " << result.blocksDirectory << "\n"
                      << "Arquivo .meta: " << result.metadataPath << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Erro ao gerar metadata: " << e.what() << "\n";
            return 1;
        }

        return 0;
    }

    // A porta do peer e pelo menos um vizinho (IP e porta) são necessários.
    // O número de argumentos para vizinhos deve ser par.
    if (argc < 4 || (argc - 2) % 2 != 0) {
        printUsage(argv[0]);
        return 1;
    }

    int myPort = std::stoi(argv[1]);
    
    std::vector<NeighborInfo> neighbors;
    for (int i = 2; i < argc; i += 2) {
        NeighborInfo neighbor;
        neighbor.ip = argv[i];
        neighbor.port = std::stoi(argv[i + 1]);
        neighbors.push_back(neighbor);
    }

    try {
        Peer peer(myPort, neighbors);
        peer.start();
    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << std::endl;
    }

    return 0;
}
