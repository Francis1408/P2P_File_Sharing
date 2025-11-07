#include "Peer.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Uso: " << argv[0] << " <minha_porta> <ip_vizinho> <porta_vizinho>\n";
        return 1;
    }

    int myPort = std::stoi(argv[1]);
    std::string neighborIP = argv[2];
    int neighborPort = std::stoi(argv[3]);

    try {
        Peer peer(myPort, neighborIP, neighborPort);
        peer.start();
    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << std::endl;
    }

    return 0;
}