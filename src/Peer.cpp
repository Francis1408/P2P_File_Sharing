#include "Peer.h"
#include <iostream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <arpa/inet.h>

// Tamanho do chunck armazenado pelo peer
#define BUFFER_SIZE 1024

// Construtor atualizado para aceitar uma lista de vizinhos
Peer::Peer(int myPort, const std::vector<NeighborInfo>& neighbors)
    : myPort(myPort), neighbors(neighbors), running(true) {}

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
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    read(clientSock, buffer, BUFFER_SIZE);

    std::cout << "[Servidor " << myPort << "] Recebido: " << buffer << "\n";

    std::string response = "Oi, sou o peer na porta " + std::to_string(myPort);
    write(clientSock, response.c_str(), response.size());

    close(clientSock);
}

void Peer::clientLoop() {
    sleep(2); // Espera os outros peers subirem

    while (running) {
        for (const auto& neighbor : neighbors) {
            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) {
                std::cerr << "ERRO ao criar socket cliente\n";
                continue; // Tenta o próximo vizinho
            }

            sockaddr_in serv_addr{};
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(neighbor.port);
            inet_pton(AF_INET, neighbor.ip.c_str(), &serv_addr.sin_addr);

            if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
                std::cout << "[Cliente " << myPort << "] Falha na conexão com " << neighbor.ip << ":" << neighbor.port << "\n";
                close(sockfd);
                continue; // Tenta o próximo vizinho
            }

            std::cout << "[Cliente " << myPort << "] Conectado com " << neighbor.ip << ":" << neighbor.port << "\n";
            
            std::string msg = "Olá vizinho, sou o peer " + std::to_string(myPort);
            write(sockfd, msg.c_str(), msg.size());

            char buffer[BUFFER_SIZE];
            memset(buffer, 0, BUFFER_SIZE);
            read(sockfd, buffer, BUFFER_SIZE);
            std::cout << "[Cliente " << myPort << "] Resposta de " << neighbor.ip << ":" << neighbor.port << ": " << buffer << "\n";

            close(sockfd);
        }
        sleep(5); // Espera 5 segundos antes de tentar se conectar a todos os vizinhos novamente
    }
}