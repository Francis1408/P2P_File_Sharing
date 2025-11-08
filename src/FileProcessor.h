#ifndef FILE_PROCESSOR_H
#define FILE_PROCESSOR_H

#include <cstddef>
#include <string>

#include "FileMetadata.h"

namespace FileProcessor {

struct MetadataContent {
    FileInfo info;
    std::string blocksDirectory;
};

struct MetadataCreationResult {
    MetadataContent content;
    std::string metadataPath;
};

MetadataCreationResult createFileMetadata(const std::string& sourceFile,
                                          std::size_t blockSize,
                                          const std::string& blocksRoot = "blocks",
                                          const std::string& metadataRoot = "metadata");

MetadataContent loadMetadataFile(const std::string& metadataPath);
MetadataContent parseMetadataString(const std::string& data);
std::string serializeMetadata(const MetadataContent& content);
std::string computeFileChecksum(const std::string& filePath);

}

#endif
