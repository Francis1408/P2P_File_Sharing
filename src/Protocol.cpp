#include "Protocol.h"

#include <arpa/inet.h>
#include <unistd.h>

#include <cstring>

namespace {

constexpr std::size_t HEADER_SIZE = 5; // 1 byte type + 4 bytes payload size

bool writeAll(int fd, const void* buffer, std::size_t bytes) {
    const std::uint8_t* data = static_cast<const std::uint8_t*>(buffer);
    std::size_t total = 0;
    while (total < bytes) {
        ssize_t written = ::write(fd, data + total, bytes - total);
        if (written <= 0) {
            return false;
        }
        total += static_cast<std::size_t>(written);
    }
    return true;
}

bool readAll(int fd, void* buffer, std::size_t bytes) {
    std::uint8_t* data = static_cast<std::uint8_t*>(buffer);
    std::size_t total = 0;
    while (total < bytes) {
        ssize_t readBytes = ::read(fd, data + total, bytes - total);
        if (readBytes <= 0) {
            return false;
        }
        total += static_cast<std::size_t>(readBytes);
    }
    return true;
}

} // namespace

namespace Protocol {

bool sendMessage(int sockfd, MessageType type, const std::vector<std::uint8_t>& payload) {
    std::uint8_t header[HEADER_SIZE];
    header[0] = static_cast<std::uint8_t>(type);
    std::uint32_t payloadSize = htonl(static_cast<std::uint32_t>(payload.size()));
    std::memcpy(header + 1, &payloadSize, sizeof(payloadSize));

    if (!writeAll(sockfd, header, HEADER_SIZE)) {
        return false;
    }

    if (!payload.empty()) {
        if (!writeAll(sockfd, payload.data(), payload.size())) {
            return false;
        }
    }

    return true;
}

bool receiveMessage(int sockfd, MessageType& type, std::vector<std::uint8_t>& payload) {
    std::uint8_t header[HEADER_SIZE];
    if (!readAll(sockfd, header, HEADER_SIZE)) {
        return false;
    }

    type = static_cast<MessageType>(header[0]);
    std::uint32_t payloadSizeNetwork;
    std::memcpy(&payloadSizeNetwork, header + 1, sizeof(payloadSizeNetwork));
    std::uint32_t payloadSize = ntohl(payloadSizeNetwork);

    payload.resize(payloadSize);
    if (payloadSize > 0) {
        if (!readAll(sockfd, payload.data(), payloadSize)) {
            return false;
        }
    }

    return true;
}

} // namespace Protocol
