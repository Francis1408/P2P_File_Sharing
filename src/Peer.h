#ifndef PEER_H
#define PEER_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>

/* ######### CLASSE PEER ############ 
  - myPort : Porta do socket que o peer está conectado
  - neighborIP: Endereço IP dos vizinhos
  - neighborPort: Porta do socket do vizinho
  - running: Flag para verificar se o peer está ativo
  
  - serverLoop() : servidor -> escuta as requisições de chuncks de arquivo
  - clientLoop() : cliente -> envia chuncks de mensagem
  - start() : Inicia threads de cliente e servidor
  - handleConnection() : Realiza a troca de mensagens entre peers por meio dos buffers

*/

class Peer {

    public: 
        Peer(int myPort, const std::string& neighborIP, int neighborPort);
        void start();
    
    private:
        int myPort;
        std::string neighborIP;
        int neighborPort;
        std::atomic<bool> running;

        void serverLoop();
        void clientLoop();

        void handleConnection(int clientSock);

};


#endif