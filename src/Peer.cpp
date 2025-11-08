#include "Peer.h"

#include "Protocol.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>

// Tamanho do chunck armazenado pelo peer
#define BUFFER_SIZE 1024

// Construtor atualizado para aceitar uma lista de vizinhos e metadata opcional
Peer::Peer(int myPort, const std::vector<NeighborInfo>& neighbors, std::string metadataPath)
    : myPort(myPort),
      neighbors(neighbors),
      running(true),
      metadataPath(std::move(metadataPath)) {
    if (!this->metadataPath.empty()) {
        try {
            localMetadata = FileProcessor::loadMetadataFile(this->metadataPath);
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
                    std::cout << "[Cliente " << myPort << "] Metadata recebida de "
                              << neighbor.ip << ":" << neighbor.port << " -> arquivo "
                              << info.fileName << ", blocos: " << info.blockCount
                              << ", checksum: " << info.checksum << std::endl;
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
    if (!localMetadata) {
        sendErrorMessage(clientSock, "Peer não possui metadata configurada");
        return;
    }

    auto serialized = FileProcessor::serializeMetadata(*localMetadata);
    std::vector<std::uint8_t> payload(serialized.begin(), serialized.end());
    if (!Protocol::sendMessage(clientSock, Protocol::MessageType::METADATA_RESPONSE, payload)) {
        std::cerr << "[Servidor " << myPort << "] Falha ao enviar metadata" << std::endl;
    } else {
        std::cout << "[Servidor " << myPort << "] Metadata enviada para cliente" << std::endl;
    }
}

void Peer::sendErrorMessage(int clientSock, const std::string& message) {
    std::vector<std::uint8_t> payload(message.begin(), message.end());
    if (!Protocol::sendMessage(clientSock, Protocol::MessageType::ERROR, payload)) {
        std::cerr << "[Servidor " << myPort << "] Falha ao enviar mensagem de erro" << std::endl;
    }
}
