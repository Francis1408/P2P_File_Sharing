#include "Peer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

// Tamanho do chunck armazenado pelo peer
#define BUFFER_SIZE 1024


// Construtor
Peer::Peer(int myPort, const std::string& neighborIP, int neighborPort)
    : myPort(myPort), neighborIP(neighborIP), neighborPort(neighborPort), running(true) {}


void Peer::start() {

    // Cria e incia as threads de cliente e servidor
    std::thread serverThread(&Peer::serverLoop, this);
    std::thread clientThread(&Peer::clientLoop, this);

    serverThread.join();
    clientThread.join();
}

void Peer::serverLoop() {

// ########### CRIA O SOCKET ####################
/* socket(int __domain, int __type, int __protocol)

    domain(AF_INET) -> Utiliza protocolo IPV4
    type(SOCK_STREAM) -> Utiliza protocolo TCP  */

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
//##############################################
    if (sockfd < 0) throw std::runtime_error("ERRO ao criar socket servidor");

    // Prepara a estrutura sockaddr_in que guarda o endereço de IP e a porta do vizinho
    sockaddr_in serv_addr{};

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(myPort);

// ####### ATRELA O ADDR AO SOCKET #############
    int bind_success = bind(sockfd, (struct sockaddr * ) &serv_addr, sizeof(serv_addr));
//##############################################

    if(bind_success < 0) 
        throw std::runtime_error("ERRO ao atrelar o endereço ao socket");

// ####### ESCUTA CONEXÕES NO SOCKET ###########
/* listen(int __fd, int __n)

    fd(sockfd) -> Escuta a porta sockfd
    n(5) -> Suporta até 5 conexões  */
    listen(sockfd, 5);
//##############################################
std::cout << "[Servidor " << myPort << "] Aguardando conexões...\n";


    // LOOP PRINCIPAL DE EXECUÇÃO 
    while (running) {

        // Limpa qualquer informação dentro do endereço do cliente
        sockaddr_in cli_addr{};
        socklen_t clilen = sizeof(cli_addr);

    // ######### BLOQUEIA A EXECUÇÃO ATÉ QUE ALGUEM SE CONCTE ##################
        // Cria um socket dedicado à conversa entre peers 
        int newsock = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    //#########################################################################

        // Caso a requisição seja aceita, repassa o chunck para o solicitante
        if (newsock >= 0)
            std::thread(&Peer::handleConnection, this, newsock).detach();
        
    }

    // Fecha a conexão
    close(sockfd);
}

void Peer::handleConnection(int clientSock) {

    // Prepara o buffer para ler a mensagem enviada pelo peer solicitante
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE); // Limpa o buffer
    read(clientSock, buffer, BUFFER_SIZE); // Lê o texto 

    std::cout << "[Servidor " << myPort << "] Recebido: " << buffer << "\n";

    // Envia a resposta para o vizinho de que recebeu a mensagem
    std::string response = "Oi, sou o peer na porta " + std::to_string(myPort);
    write(clientSock, response.c_str(), response.size());

    close(clientSock);
}

void Peer::clientLoop() {

    sleep(2); // espera o outro peer subir

    while (running) {

    // ########### CRIA O SOCKET ####################
    /* socket(int __domain, int __type, int __protocol)

        domain(AF_INET) -> Utiliza protocolo IPV4
        type(SOCK_STREAM) -> Utiliza protocolo TCP  */

        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    //##############################################
        if (sockfd < 0) {
            std::cerr << "ERRO ao criar socket cliente\n";
            sleep(3);
            continue;
        }

        // Prepara a estrutura sockaddr_in que guarda o endereço de IP e a porta do servidor
        sockaddr_in serv_addr{};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(neighborPort); // Porta do servidor 
        // Transforma uma string de IP em número binário que o socket entende
        inet_pton(AF_INET, neighborIP.c_str(), &serv_addr.sin_addr);


    // ########### CONECTA-SE AO SERVIDOR ####################
    /* connect(int __fd, const struct *__addr, socklen_t __len)
        
        fd(sockfd) = Socket a se conectar
        addr(serv_addr) = Endereço do socket
        len() = Tamanho do endereço do socket
    */
        int connect_att = connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    //##############################################
    
        if(connect_att < 0) {
            std::cout << "[Cliente " << myPort << "] Falha na conexão, tentando novamente...\n";
            close(sockfd);
            sleep(3);
            continue;
        }

        // Envia mensagem para o vizinho
        std::string msg = "Olá vizinho, sou o peer " + std::to_string(myPort);
        write(sockfd, msg.c_str(), msg.size());

        // Lê a reposta do vizinho
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        read(sockfd, buffer, BUFFER_SIZE);
        std::cout << "[Cliente " << myPort << "] Resposta: " << buffer << "\n";

        close(sockfd);
        sleep(5);

    }
}