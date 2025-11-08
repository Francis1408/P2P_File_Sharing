#ifndef FILE_METADATA_H
#define FILE_METADATA_H

#include <string>

struct FileInfo {
    std::string fileName;
    long long fileSize;
    int blockSize;
    int blockCount;
    std::string checksum;
};

#endif
