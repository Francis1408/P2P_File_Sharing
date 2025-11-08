#include "Peer.h"
#include "FileProcessor.h"

#include <iostream>
#include <vector>

namespace {
constexpr std::size_t DEFAULT_BLOCK_SIZE = 1024;

void printUsage(const char* binaryName) {
    std::cerr << "Uso:\n"
              << "  " << binaryName << " --create-meta <arquivo> [tamanho_bloco]\n"
              << "  " << binaryName << " [--meta <arquivo.meta>] <minha_porta> <ip_vizinho1> <porta_vizinho1> [<ip_vizinho2> <porta_vizinho2> ...]\n";
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
                      << "Arquivo original: " << result.content.info.fileName << " (" << result.content.info.fileSize << " bytes)\n"
                      << "Blocos gerados: " << result.content.info.blockCount << " de tamanho " << result.content.info.blockSize << " bytes\n"
                      << "Checksum (SHA-256): " << result.content.info.checksum << "\n"
                      << "Pasta dos blocos: " << result.content.blocksDirectory << "\n"
                      << "Arquivo .meta: " << result.metadataPath << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Erro ao gerar metadata: " << e.what() << "\n";
            return 1;
        }

        return 0;
    }

    std::string metadataPath;
    int argIndex = 1;
    while (argIndex < argc) {
        std::string arg = argv[argIndex];
        if (arg == "--meta") {
            if (argIndex + 1 >= argc) {
                printUsage(argv[0]);
                return 1;
            }
            metadataPath = argv[argIndex + 1];
            argIndex += 2;
        } else {
            break;
        }
    }

    // A porta do peer e pelo menos um vizinho (IP e porta) são necessários.
    if (argIndex >= argc) {
        printUsage(argv[0]);
        return 1;
    }

    int myPort = std::stoi(argv[argIndex++]);

    int remaining = argc - argIndex;
    if (remaining < 2 || remaining % 2 != 0) {
        printUsage(argv[0]);
        return 1;
    }

    std::vector<NeighborInfo> neighbors;
    for (int i = argIndex; i < argc; i += 2) {
        NeighborInfo neighbor;
        neighbor.ip = argv[i];
        neighbor.port = std::stoi(argv[i + 1]);
        neighbors.push_back(neighbor);
    }

    try {
        Peer peer(myPort, neighbors, metadataPath);
        peer.start();
    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << std::endl;
    }

    return 0;
}
