#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <vector>

namespace Protocol {

enum class MessageType : std::uint8_t {
    GET_METADATA = 1,
    METADATA_RESPONSE = 2,
    REQUEST_BLOCK = 3,
    BLOCK_DATA = 4,
    ERROR = 5
};

bool sendMessage(int sockfd, MessageType type, const std::vector<std::uint8_t>& payload);
bool receiveMessage(int sockfd, MessageType& type, std::vector<std::uint8_t>& payload);

} // namespace Protocol

#endif
