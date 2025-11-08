#include "Peer.h"
#include <iostream>
#include <vector>

int main(int argc, char* argv[]) {
    // A porta do peer e pelo menos um vizinho (IP e porta) são necessários.
    // O número de argumentos para vizinhos deve ser par.
    if (argc < 4 || (argc - 2) % 2 != 0) {
        std::cerr << "Uso: " << argv[0] << " <minha_porta> <ip_vizinho1> <porta_vizinho1> [<ip_vizinho2> <porta_vizinho2> ...]\n";
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