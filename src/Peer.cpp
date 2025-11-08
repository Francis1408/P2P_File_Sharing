#include "Peer.h"

#include "Protocol.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <unistd.h>
#include <vector>

// Tamanho do chunck armazenado pelo peer
#define BUFFER_SIZE 1024

// Construtor atualizado para aceitar uma lista de vizinhos e metadata opcional
Peer::Peer(int myPort, const std::vector<NeighborInfo>& neighbors, std::string metadataPath)
    : myPort(myPort),
      neighbors(neighbors),
      running(true),
      metadataPath(std::move(metadataPath)),
      downloadRoot("downloads") {
    if (!this->metadataPath.empty()) {
        try {
            localMetadata = FileProcessor::loadMetadataFile(this->metadataPath);
            fileInfo = localMetadata->info;
            ownedBlocks.assign(fileInfo.blockCount, true);
            std::cout << "[Peer " << myPort << "] Metadata local carregada de " << this->metadataPath << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[Peer " << myPort << "] Falha ao carregar metadata: " << e.what() << "\n";
        }
    }
}

void Peer::start() {
    // Cria e incia as threads de cliente e servidor
    std::thread serverThread(&Peer::serverLoop, this);
    std::thread clientThread(&Peer::clientLoop, this);

    serverThread.join();
    clientThread.join();
}

void Peer::serverLoop() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error(std::string("ERRO ao criar socket servidor: ") + std::strerror(errno));
    }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(myPort);

    // Permite a reutilização do endereço para evitar erros de "Address already in use"
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        throw std::runtime_error(std::string("ERRO ao configurar SO_REUSEADDR: ") + std::strerror(errno));
    }

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        throw std::runtime_error(std::string("ERRO ao atrelar o endereço ao socket: ") + std::strerror(errno));
    }

    listen(sockfd, 5);
    std::cout << "[Servidor " << myPort << "] Aguardando conexões...\n";

    while (running) {
        sockaddr_in cli_addr{};
        socklen_t clilen = sizeof(cli_addr);
        int newsock = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

        if (newsock >= 0) {
            std::thread(&Peer::handleConnection, this, newsock).detach();
        }
    }

    close(sockfd);
}

void Peer::handleConnection(int clientSock) {
    Protocol::MessageType type;
    std::vector<std::uint8_t> payload;
    if (!Protocol::receiveMessage(clientSock, type, payload)) {
        std::cerr << "[Servidor " << myPort << "] Falha ao ler mensagem do cliente" << std::endl;
        close(clientSock);
        return;
    }

    switch (type) {
        case Protocol::MessageType::GET_METADATA:
            handleGetMetadata(clientSock);
            break;
        case Protocol::MessageType::REQUEST_BLOCK:
            handleRequestBlock(clientSock, payload);
            break;
        default:
            std::cout << "[Servidor " << myPort << "] Tipo de mensagem não suportado: "
                      << static_cast<int>(type) << std::endl;
            break;
    }

    close(clientSock);
}

void Peer::clientLoop() {
    sleep(2); // Espera os outros peers subirem

    while (running) {
        for (const auto& neighbor : neighbors) {
            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) {
                std::cerr << "ERRO ao criar socket cliente" << std::endl;
                continue;
            }

            sockaddr_in serv_addr{};
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(neighbor.port);
            inet_pton(AF_INET, neighbor.ip.c_str(), &serv_addr.sin_addr);

            if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
                std::cout << "[Cliente " << myPort << "] Falha na conexão com "
                          << neighbor.ip << ":" << neighbor.port << std::endl;
                close(sockfd);
                continue;
            }

            std::cout << "[Cliente " << myPort << "] Conectado com "
                      << neighbor.ip << ":" << neighbor.port << std::endl;

            if (!Protocol::sendMessage(sockfd, Protocol::MessageType::GET_METADATA, {})) {
                std::cerr << "[Cliente " << myPort << "] Falha ao enviar GET_METADATA" << std::endl;
                close(sockfd);
                continue;
            }

            Protocol::MessageType responseType;
            std::vector<std::uint8_t> payload;
            if (!Protocol::receiveMessage(sockfd, responseType, payload)) {
                std::cerr << "[Cliente " << myPort << "] Falha ao receber resposta" << std::endl;
                close(sockfd);
                continue;
            }

            if (responseType == Protocol::MessageType::METADATA_RESPONSE) {
                std::string metadataText(payload.begin(), payload.end());
                try {
                    remoteMetadata = FileProcessor::parseMetadataString(metadataText);
                    const auto& info = remoteMetadata->info;
                    fileInfo = info;
                    {
                        std::lock_guard<std::mutex> lock(ownedBlocksMutex);
                        if (ownedBlocks.empty() || ownedBlocks.size() != static_cast<std::size_t>(info.blockCount)) {
                            ownedBlocks.assign(info.blockCount, localMetadata.has_value());
                            if (!localMetadata) {
                                std::fill(ownedBlocks.begin(), ownedBlocks.end(), false);
                            }
                        }
                    }
                    std::cout << "[Cliente " << myPort << "] Metadata recebida de "
                              << neighbor.ip << ":" << neighbor.port << " -> arquivo "
                              << info.fileName << ", blocos: " << info.blockCount
                              << ", checksum: " << info.checksum << std::endl;
                    if (!localMetadata) {
                        int nextBlock = findNextMissingBlock();
                        if (nextBlock >= 0) {
                            requestBlockFromNeighbor(neighbor, nextBlock);
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[Cliente " << myPort << "] Falha ao interpretar metadata: "
                              << e.what() << std::endl;
                }
            } else if (responseType == Protocol::MessageType::ERROR) {
                std::string errorMsg(payload.begin(), payload.end());
                std::cerr << "[Cliente " << myPort << "] Erro remoto: " << errorMsg << std::endl;
            } else {
                std::cout << "[Cliente " << myPort << "] Tipo de resposta inesperado: "
                          << static_cast<int>(responseType) << std::endl;
            }

            close(sockfd);
        }
        sleep(5); // Espera 5 segundos antes de tentar se conectar a todos os vizinhos novamente
    }
}

void Peer::handleGetMetadata(int clientSock) {
    const FileProcessor::MetadataContent* metadataToSend = nullptr;
    if (localMetadata) {
        metadataToSend = &*localMetadata;
    } else if (remoteMetadata) {
        metadataToSend = &*remoteMetadata;
    }

    if (!metadataToSend) {
        sendErrorMessage(clientSock, "Peer não possui metadata disponível");
        return;
    }

    auto serialized = FileProcessor::serializeMetadata(*metadataToSend);
    std::vector<std::uint8_t> payload(serialized.begin(), serialized.end());
    if (!Protocol::sendMessage(clientSock, Protocol::MessageType::METADATA_RESPONSE, payload)) {
        std::cerr << "[Servidor " << myPort << "] Falha ao enviar metadata" << std::endl;
    } else {
        std::cout << "[Servidor " << myPort << "] Metadata enviada para cliente" << std::endl;
    }
}

void Peer::handleRequestBlock(int clientSock, const std::vector<std::uint8_t>& payload) {
    if (payload.size() < sizeof(std::uint32_t)) {
        sendErrorMessage(clientSock, "Payload REQUEST_BLOCK inválido");
        return;
    }

    std::uint32_t blockIndexNetwork;
    std::memcpy(&blockIndexNetwork, payload.data(), sizeof(blockIndexNetwork));
    int blockIndex = static_cast<int>(ntohl(blockIndexNetwork));

    if (fileInfo.blockCount == 0) {
        sendErrorMessage(clientSock, "Peer não possui informação de blocos disponível");
        return;
    }

    if (blockIndex < 0 || blockIndex >= fileInfo.blockCount) {
        sendErrorMessage(clientSock, "Índice de bloco inválido");
        return;
    }

    namespace fs = std::filesystem;
    fs::path blockPath;
    if (localMetadata) {
        blockPath = fs::path(localMetadata->blocksDirectory) /
                    ("block_" + std::to_string(blockIndex) + ".bin");
    } else {
        if (!remoteMetadata || !hasBlock(blockIndex)) {
            sendErrorMessage(clientSock, "Bloco ainda não disponível");
            return;
        }
        blockPath = ensureDownloadDir() /
                    ("block_" + std::to_string(blockIndex) + ".bin");
    }

    std::ifstream blockFile(blockPath, std::ios::binary);
    if (!blockFile) {
        sendErrorMessage(clientSock, "Bloco não encontrado");
        return;
    }

    std::vector<std::uint8_t> blockData((std::istreambuf_iterator<char>(blockFile)),
                                        std::istreambuf_iterator<char>());

    std::vector<std::uint8_t> response(sizeof(std::uint32_t) + blockData.size());
    std::uint32_t indexNetwork = htonl(static_cast<std::uint32_t>(blockIndex));
    std::memcpy(response.data(), &indexNetwork, sizeof(indexNetwork));
    if (!blockData.empty()) {
        std::memcpy(response.data() + sizeof(indexNetwork), blockData.data(), blockData.size());
    }

    if (!Protocol::sendMessage(clientSock, Protocol::MessageType::BLOCK_DATA, response)) {
        std::cerr << "[Servidor " << myPort << "] Falha ao enviar bloco " << blockIndex << std::endl;
    } else {
        std::cout << "[Servidor " << myPort << "] Servindo bloco " << blockIndex << std::endl;
    }
}

void Peer::sendErrorMessage(int clientSock, const std::string& message) {
    std::vector<std::uint8_t> payload(message.begin(), message.end());
    if (!Protocol::sendMessage(clientSock, Protocol::MessageType::ERROR, payload)) {
        std::cerr << "[Servidor " << myPort << "] Falha ao enviar mensagem de erro" << std::endl;
    }
}

bool Peer::requestBlockFromNeighbor(const NeighborInfo& neighbor, int blockIndex) {
    if (!remoteMetadata) {
        return false;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "[Cliente " << myPort << "] ERRO ao criar socket para bloco" << std::endl;
        return false;
    }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(neighbor.port);
    inet_pton(AF_INET, neighbor.ip.c_str(), &serv_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        std::cout << "[Cliente " << myPort << "] Falha ao conectar para solicitar bloco "
                  << blockIndex << std::endl;
        close(sockfd);
        return false;
    }

    std::uint32_t indexNetwork = htonl(static_cast<std::uint32_t>(blockIndex));
    std::vector<std::uint8_t> payload(sizeof(indexNetwork));
    std::memcpy(payload.data(), &indexNetwork, sizeof(indexNetwork));

    if (!Protocol::sendMessage(sockfd, Protocol::MessageType::REQUEST_BLOCK, payload)) {
        std::cerr << "[Cliente " << myPort << "] Falha ao enviar REQUEST_BLOCK" << std::endl;
        close(sockfd);
        return false;
    }

    Protocol::MessageType responseType;
    std::vector<std::uint8_t> responsePayload;
    if (!Protocol::receiveMessage(sockfd, responseType, responsePayload)) {
        std::cerr << "[Cliente " << myPort << "] Falha ao receber bloco" << std::endl;
        close(sockfd);
        return false;
    }

    bool success = false;
    if (responseType == Protocol::MessageType::BLOCK_DATA) {
        if (responsePayload.size() < sizeof(std::uint32_t)) {
            std::cerr << "[Cliente " << myPort << "] Payload BLOCK_DATA inválido" << std::endl;
        } else {
            std::uint32_t idxNetwork;
            std::memcpy(&idxNetwork, responsePayload.data(), sizeof(idxNetwork));
            int receivedIndex = static_cast<int>(ntohl(idxNetwork));
            std::vector<std::uint8_t> blockBytes(responsePayload.begin() + sizeof(idxNetwork),
                                                 responsePayload.end());
            if (saveReceivedBlock(receivedIndex, blockBytes)) {
                success = true;
            }
        }
    } else if (responseType == Protocol::MessageType::ERROR) {
        std::string errorMsg(responsePayload.begin(), responsePayload.end());
        std::cerr << "[Cliente " << myPort << "] Erro ao requisitar bloco: " << errorMsg << std::endl;
    } else {
        std::cout << "[Cliente " << myPort << "] Resposta inesperada ao requisitar bloco: "
                  << static_cast<int>(responseType) << std::endl;
    }

    close(sockfd);

    if (success && !localMetadata) {
        int nextBlock = findNextMissingBlock();
        if (nextBlock >= 0) {
            requestBlockFromNeighbor(neighbor, nextBlock);
        } else if (hasAllBlocks()) {
            tryAssembleFile();
        }
    }

    return success;
}

bool Peer::saveReceivedBlock(int blockIndex, const std::vector<std::uint8_t>& data) {
    if (!remoteMetadata) {
        return false;
    }

    auto targetDir = ensureDownloadDir();

    std::filesystem::path blockPath = targetDir / ("block_" + std::to_string(blockIndex) + ".bin");
    std::ofstream output(blockPath, std::ios::binary);
    if (!output) {
        std::cerr << "[Cliente " << myPort << "] Não foi possível salvar bloco em "
                  << blockPath << std::endl;
        return false;
    }
    if (!data.empty()) {
        output.write(reinterpret_cast<const char*>(data.data()), data.size());
    }
    output.flush();
    output.close();

    {
        std::lock_guard<std::mutex> lock(ownedBlocksMutex);
        if (static_cast<std::size_t>(blockIndex) >= ownedBlocks.size()) {
            ownedBlocks.resize(blockIndex + 1, false);
        }
        ownedBlocks[blockIndex] = true;
    }

    std::cout << "[Cliente " << myPort << "] Bloco " << blockIndex
              << " salvo em " << blockPath << std::endl;

    if (hasAllBlocks()) {
        tryAssembleFile();
    }

    return true;
}

int Peer::findNextMissingBlock() const {
    std::lock_guard<std::mutex> lock(ownedBlocksMutex);
    for (std::size_t i = 0; i < ownedBlocks.size(); ++i) {
        if (!ownedBlocks[i]) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::filesystem::path Peer::ensureDownloadDir() const {
    namespace fs = std::filesystem;
    fs::path targetDir = fs::path(downloadRoot) / (remoteMetadata ? remoteMetadata->info.fileName : "unknown");
    fs::create_directories(targetDir);
    return targetDir;
}

bool Peer::hasBlock(int blockIndex) const {
    std::lock_guard<std::mutex> lock(ownedBlocksMutex);
    return blockIndex >= 0 && static_cast<std::size_t>(blockIndex) < ownedBlocks.size() && ownedBlocks[blockIndex];
}

bool Peer::hasAllBlocks() const {
    std::lock_guard<std::mutex> lock(ownedBlocksMutex);
    if (ownedBlocks.empty()) return false;
    return std::all_of(ownedBlocks.begin(), ownedBlocks.end(), [](bool owned) { return owned; });
}

void Peer::tryAssembleFile() {
    if (!remoteMetadata || fileAssembled) {
        return;
    }
    if (findNextMissingBlock() >= 0) {
        return;
    }

    auto targetDir = ensureDownloadDir();
    namespace fs = std::filesystem;
    fs::path outputPath = targetDir / ("complete_" + remoteMetadata->info.fileName);

    std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "[Cliente " << myPort << "] Não foi possível criar arquivo final em "
                  << outputPath << std::endl;
        return;
    }

    for (int i = 0; i < remoteMetadata->info.blockCount; ++i) {
        fs::path blockPath = targetDir / ("block_" + std::to_string(i) + ".bin");
        std::ifstream blockFile(blockPath, std::ios::binary);
        if (!blockFile) {
            std::cerr << "[Cliente " << myPort << "] Bloco faltando durante montagem: "
                      << blockPath << std::endl;
            return;
        }
        std::vector<char> buffer((std::istreambuf_iterator<char>(blockFile)),
                                 std::istreambuf_iterator<char>());
        if (!buffer.empty()) {
            output.write(buffer.data(), buffer.size());
        }
    }

    output.flush();
    output.close();

    try {
        auto checksum = FileProcessor::computeFileChecksum(outputPath.string());
        if (checksum == remoteMetadata->info.checksum) {
            std::cout << "[Cliente " << myPort << "] Download completo! Arquivo reconstituído em "
                      << outputPath << " (checksum OK)" << std::endl;
        } else {
            std::cerr << "[Cliente " << myPort << "] Checksum divergente: esperado "
                      << remoteMetadata->info.checksum << ", obtido " << checksum << std::endl;
        }
        fileAssembled = true;
    } catch (const std::exception& e) {
        std::cerr << "[Cliente " << myPort << "] Falha ao calcular checksum: "
                  << e.what() << std::endl;
    }
}
