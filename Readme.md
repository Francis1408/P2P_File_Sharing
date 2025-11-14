# Peer-to-Peer (P2P) System

Francisco Abreu Gonçalves

Pedro Veloso Inácio de Oliveira

-------------

## 1. Intoduction 
This project aimed studying the protocol P2P symetric, as well as the features and protocols of file transfering between systems.

## 2. Structure

The main ideis from a Peer-to-Peer (P2P) is that each peer perform either as a server or as a client. Inside the [Peer file](./src/Peer.cpp) constructor, a socket number, a list of neighboors and a path do find the files chunks are passed as arguments.

```cpp
Peer::Peer(int myPort, const std::vector<NeighborInfo>& neighbors std::string metadataPath)
: myPort(myPort),
    neighbors(neighbors),
    running(true),
    metadataPath(std::move(metadataPath)),
    downloadRoot("downloads")

```
In order to perform symetrically as a server and a client, two threads for both methods are created,

```cpp
void Peer::start() {
// Cria e incia as threads de cliente e servidor
std::thread serverThread(&Peer::serverLoop, this);
std::thread clientThread(&Peer::clientLoop, this);
serverThread.join();
clientThread.join();
}

```

### 2.1. Server

This project uses **TCP Sockets**, from POSIX/BSD libraries, in order to create a server process which listens to sockets using the methods *bind*, *listen* and *accept*. The peers take the type *sockaddr_in*, that contains socket infos (socket number, IP address, etc) to address them.

The server listens from the sockets messages defined in the binary protocol inside the [Protocol file](./src/Protocol.cpp). The most relevant messages are:

- GET_METADATA: The server sends through socket the metadate to the requesting client. This metadate can be stored localy or remotely. The data is serialized in a payload with the struct 
```cpp
std::vector<std::uint8_t>
```
```cpp
std::vector<std::uint8_t> blockData((std::istreambuf_iterator<char(blockFile)), std::istreambuf_iterator<char>());

    std::vector<std::uint8_t> response(sizeof(std::uint32_t) + blockData.size());
    std::uint32_t indexNetwork = htonl(static_cast<std::uint32_t>(blockIndex));
    std::memcpy(response.data(), &indexNetwork, sizeof(indexNetwork));
    if (!blockData.empty()) {
        std::memcpy(response.data() + sizeof(indexNetwork), blockData.data(), blockData.size());
    }

    if (!Protocol::sendMessage(clientSock, Protocol::MessageType::BLOCK_DATA, response)) {
        std::cerr << "[Servidor " << myPort << "] Falha ao enviar bloco " << blockIndex << std::endl;
    } else {
        std::cout << "[Servidor " << myPort << "] Cliente " << clientIP << ":" << clientPort << " Requisitou bloco " << blockIndex << std::endl;
    }

```

- REQUEST_BLOCK: When the client has possesion of the metadata, it sends this message in order to ask the missing chunks. The required chunk is identified by the *blockIndex*, which is a payload argument. By the time that the chunk is found, the server returns it to the client.

```cpp
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
```

### 2.2. Client

At the client side, there is a the socket creation to connect with the neighbors. Initially, the message GET_METADATA is sent in order to know which chunks each neighboor has. Thus, with the methods *findNextMissingBlock()* and *requestBlockFromNeighbor()*, the system retrieves the missing chunks. The struct *ownedBlocks* is used to retain information about the owned chunks of each peer.

```cpp
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
```

### 2.3. File Chunking 

The system uses a chunking process inside the [FileProcessor file](./src/FileProcessor.cpp), during the file's metadata creation.
This reading happens in fixed sized chunks, defined by the parameter *blockSize*. This values dictates how many bytes consists each block. This pieces are stored locally inside the generated /download folder. The block names follow the pattern: **block_0.bin**, **block_1.bin**, **block_2.bin**, ...
During thr block reading, the content is also sent in order to make the implementation fo the **SHA-256**. The*checksum* role is to secure the files integrity.

```cpp
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

```

## 3. How to run

To see the system working, run the following terminal commands:

```shell
$ make
$ make meta FILE_TO_SHARE=<file_path> BLOCK_SIZE=<block_size>
```
This will compile the files and prepare the file metadata

Finally, simply execute the run command:

```shell
$ make run TEST=<test_file_path>
```
The execution file must contain this pattern:

```text
[--meta <file.meta>] <socket> <neightbor1_ip> <neightbor1_socket>[<neightbor2_ip>  <neightbor2_socket> ...]
```
Each peer must be labeled between SEEDER or LEECHER.

Example:
```text
SEEDER 5000 metadata/small.txt.meta 127.0.0.1 5001 127.0.0.1 5002
LEECHER 5001 127.0.0.1 5000 127.0.0.1 5002
LEECHER 5002 127.0.0.1 5000 127.0.0.1 5003
LEECHER 5003 127.0.0.1 5001 127.0.0.1 5002
```