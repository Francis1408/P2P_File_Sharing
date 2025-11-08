#ifndef FILE_PROCESSOR_H
#define FILE_PROCESSOR_H

#include <cstddef>
#include <string>

#include "FileMetadata.h"

namespace FileProcessor {

struct MetadataCreationResult {
    FileInfo info;
    std::string blocksDirectory;
    std::string metadataPath;
};

MetadataCreationResult createFileMetadata(const std::string& sourceFile,
                                          std::size_t blockSize,
                                          const std::string& blocksRoot = "blocks",
                                          const std::string& metadataRoot = "metadata");

}

#endif
