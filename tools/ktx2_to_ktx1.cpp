#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <map>
#include <string>
#include <stdint.h>

const uint8_t KTX2_IDENTIFIER[12] = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

const uint8_t KTX1_IDENTIFIER[12] = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

// GL constants
constexpr uint32_t GL_R11F_G11F_B10F = 0x8C3A; // 35898 decimal

struct KTX2LevelIndex {
    uint64_t byteOffset;
    uint64_t byteLength;
    uint64_t uncompressedByteLength;
};

struct KTX2Header {
    uint8_t identifier[12];
    uint32_t vkFormat;
    uint32_t typeSize;
    uint32_t pixelWidth;
    uint32_t pixelHeight;
    uint32_t pixelDepth;
    uint32_t layerCount;
    uint32_t faceCount;
    uint32_t levelCount;
    uint32_t supercompressionScheme;

    // Index
    uint32_t dfdByteOffset;
    uint32_t dfdByteLength;
    uint32_t kvdByteOffset;
    uint32_t kvdByteLength;

    uint64_t sgdByteOffset;
    uint64_t sgdByteLength;
};

struct KTX1Header {
    uint8_t identifier[12];
    uint32_t endianness;
    uint32_t glType;
    uint32_t glTypeSize;
    uint32_t glFormat;
    uint32_t glInternalFormat;
    uint32_t glBaseInternalFormat;
    uint32_t pixelWidth;
    uint32_t pixelHeight;
    uint32_t pixelDepth;
    uint32_t numberOfArrayElements;
    uint32_t numberOfFaces;
    uint32_t numberOfMipmapLevels;
    uint32_t bytesOfKeyValueData;
};

struct MipmapLevel {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t sizePerFace;
    std::vector<std::vector<uint8_t>> faceData;
};

class KTX2Converter {
private:
    std::vector<uint8_t> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file: " + filename);
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            throw std::runtime_error("Error reading file");
        }

        return buffer;
    }

    void validateKTX2Header(const KTX2Header& header) {
        if (memcmp(header.identifier, KTX2_IDENTIFIER, 12) != 0) {
            throw std::runtime_error("Invalid KTX2 file identifier");
        }

        std::cout << "KTX2 header validation passed\n";
    }

    void printHeaderDetails(const KTX2Header& header) {
        std::cout << "KTX2 Header Details:\n";
        std::cout << "VkFormat: " << header.vkFormat << "\n";
        std::cout << "Dimensions: " << header.pixelWidth << "x" << header.pixelHeight;
        if (header.pixelDepth > 0) std::cout << "x" << header.pixelDepth;
        std::cout << std::endl;

        std::cout << "Faces: " << header.faceCount << "\n";
        std::cout << "Layers: " << header.layerCount << "\n";
        std::cout << "Mipmap Levels: " << header.levelCount << "\n";

        if (header.kvdByteLength > 0) {
            std::cout << "Key-Value Data: " << header.kvdByteLength << " bytes at offset " << header.kvdByteOffset << "\n";
        } else {
            std::cout << "No Key-Value Data\n";
        }

        if (header.faceCount == 6) {
            std::cout << "Texture Type: Cubemap\n";
        } else if (header.pixelDepth > 0) {
            std::cout << "Texture Type: 3D\n";
        } else if (header.pixelHeight > 0) {
            std::cout << "Texture Type: 2D\n";
        } else {
            std::cout << "Texture Type: 1D\n";
        }
    }

    KTX1Header createKTX1Header(const KTX2Header& ktx2Header, const size_t mipmapLevelCount) {
        KTX1Header ktx1Header = {};
        
        memcpy(ktx1Header.identifier, KTX1_IDENTIFIER, 12);
        
        // Endianness (little-endian)
        ktx1Header.endianness = 0x04030201;
        
        ktx1Header.glType = GL_R11F_G11F_B10F;
        ktx1Header.glTypeSize = 1;
        ktx1Header.glFormat = GL_R11F_G11F_B10F;
        ktx1Header.glInternalFormat = GL_R11F_G11F_B10F;
        ktx1Header.glBaseInternalFormat = GL_R11F_G11F_B10F;
        
        ktx1Header.pixelWidth = ktx2Header.pixelWidth;
        // For Cubemap:
        if (ktx2Header.faceCount == 6) {
            ktx1Header.pixelHeight = ktx2Header.pixelWidth;
            ktx1Header.pixelDepth = 0;
        } else {
            ktx1Header.pixelHeight = ktx2Header.pixelHeight > 0 ? ktx2Header.pixelHeight : 1;
            ktx1Header.pixelDepth = ktx2Header.pixelDepth > 0 ? ktx2Header.pixelDepth : 1;
        }
        ktx1Header.numberOfArrayElements = 0;
        
        ktx1Header.numberOfFaces = ktx2Header.faceCount;
        ktx1Header.numberOfMipmapLevels = mipmapLevelCount;
        // Key-value data bytes (will be set later)
        ktx1Header.bytesOfKeyValueData = 0;
        std::cout << "KTX1 Header Format Values:" << std::endl;
        std::cout << "glType: " << ktx1Header.glType << " (GL_R11F_G11F_B10F)" << std::endl;
        std::cout << "glTypeSize: " << ktx1Header.glTypeSize << std::endl;
        std::cout << "glFormat: " << ktx1Header.glFormat << " (GL_R11F_G11F_B10F)" << std::endl;
        std::cout << "glInternalFormat: " << ktx1Header.glInternalFormat << " (GL_R11F_G11F_B10F)" << std::endl;
        std::cout << "glBaseInternalFormat: " << ktx1Header.glBaseInternalFormat << " (GL_R11F_G11F_B10F)" << std::endl;
        std::cout << "Dimensions: " << ktx1Header.pixelWidth << "x" << ktx1Header.pixelHeight << "x" << ktx1Header.pixelDepth << std::endl;
        std::cout << "Array Elements: " << ktx1Header.numberOfArrayElements << std::endl;
        std::cout << "Faces: " << ktx1Header.numberOfFaces << std::endl;
        std::cout << "Mipmap Levels: " << ktx1Header.numberOfMipmapLevels << std::endl;
        std::cout << "Expected imageType: ";
        if (ktx1Header.pixelHeight == 1) {
            std::cout << "1D (0)" << std::endl;
        } else if (ktx1Header.pixelDepth == 1) {
            std::cout << "2D (1)" << std::endl;
        } else {
            std::cout << "3D (2)" << std::endl;
        }

        std::cout << "Expected imageViewType: ";
        if (ktx1Header.numberOfFaces == 6) {
            if (ktx1Header.numberOfArrayElements == 0) {
                std::cout << "CUBE (3)" << std::endl;
            } else {
                std::cout << "CUBE_ARRAY (6)" << std::endl;
            }
        } else if (ktx1Header.pixelDepth > 1) {
            std::cout << "3D (2)" << std::endl;
        } else if (ktx1Header.numberOfArrayElements > 0) {
            if (ktx1Header.pixelHeight == 1) {
                std::cout << "1D_ARRAY (4)" << std::endl;
            } else {
                std::cout << "2D_ARRAY (5)" << std::endl;
            }
        } else {
            if (ktx1Header.pixelHeight == 1) {
                std::cout << "1D (0)" << std::endl;
            } else {
                std::cout << "2D (1)" << std::endl;
            }
        }
        return ktx1Header;
    }

    std::map<std::string, std::vector<uint8_t>> parseKTX2KeyValueData(const std::vector<uint8_t>& fileData, uint32_t kvdOffset, uint32_t kvdLength) {
        std::map<std::string, std::vector<uint8_t>> keyValueData;
        
        if (kvdLength == 0) {
            return keyValueData;
        }

        size_t endOffset = kvdOffset + kvdLength;
        size_t offset = kvdOffset;
        while (offset < endOffset) {
            // Check for sufficient bytes for the KV entry header
            if (offset + 8 > endOffset) {
                std::cerr << "Warning: Not enough bytes left for KV entry header" << std::endl;
                break;
            }

            // Read the key-value entry header
            uint32_t keyAndValueByteLength = *reinterpret_cast<const uint32_t*>(&fileData[offset]);
            offset += 4;
            uint32_t keyByteLength = *reinterpret_cast<const uint32_t*>(&fileData[offset]);
            offset += 4;
            
            // Validate key and value byte lengths
            if (keyByteLength > keyAndValueByteLength) {
                std::cerr << "Warning: Key length (" << keyByteLength << ") > total length (" << keyAndValueByteLength << ")" << std::endl;
                break;
            }
            
            // Check if we have enough bytes for the key and value
            if (offset + keyAndValueByteLength > endOffset) {
                std::cerr << "Warning: Not enough bytes left for KV entry data" << std::endl;
                break;
            }
            
            // Extract the key (null-terminated string)
            std::string key(reinterpret_cast<const char*>(&fileData[offset]), keyByteLength - 1);
            offset += keyByteLength;
            
            // Extract the value
            uint32_t valueByteLength = keyAndValueByteLength - keyByteLength;
            std::vector<uint8_t> value(&fileData[offset], &fileData[offset + valueByteLength]);
            offset += valueByteLength;
            uint32_t padding = (4 - (keyAndValueByteLength & 3)) & 3;
            offset += padding;
            keyValueData[key] = value;
            std::cout << "Found KTX2 key-value pair: key='" << key << "', value size=" << valueByteLength << " bytes" << std::endl;
        }

        return keyValueData;
    }

    std::vector<uint8_t> createKTX1KeyValueData(const std::map<std::string, std::vector<uint8_t>>& kvData) {
        std::vector<uint8_t> ktx1KvData;
        for (const auto& [key, value] : kvData) {
            std::vector<uint8_t> ktx1Entry = createKTX1KeyValueEntry(key, value);
            ktx1KvData.insert(ktx1KvData.end(), ktx1Entry.begin(), ktx1Entry.end());
        }

        return ktx1KvData;
    }
    
    std::vector<uint8_t> createKTX1KeyValueEntry(const std::string& key, const std::vector<uint8_t>& value) {
        uint32_t keySize = static_cast<uint32_t>(key.size() + 1); // Include null terminator
        uint32_t valueSize = static_cast<uint32_t>(value.size());
        uint32_t totalSize = keySize + valueSize;
        uint32_t paddedSize = (totalSize + 3) & ~3; // Align to 4 bytes
        uint32_t paddingSize = paddedSize - totalSize;
        std::vector<uint8_t> kvEntry(4 + paddedSize);
        // Write totalSize at the beginning
        memcpy(kvEntry.data(), &totalSize, 4);
        
        // Write key (including null terminator)
        memcpy(kvEntry.data() + 4, key.c_str(), keySize);
        // Write value
        memcpy(kvEntry.data() + 4 + keySize, value.data(), valueSize);
        // Add padding if needed
        memset(kvEntry.data() + 4 + totalSize, 0, paddingSize);

        return kvEntry;
    }
    
    std::vector<MipmapLevel> extractMipmapLevels(const KTX2Header& header, const std::vector<uint8_t>& fileData, const std::vector<KTX2LevelIndex>& levelIndices) {
        uint32_t levelCount = header.levelCount;
        uint32_t faceCount = header.faceCount > 0 ? header.faceCount : 1;
        std::vector<MipmapLevel> mipmapLevels(levelCount);

        for (uint32_t i = 0; i < levelCount; i++) {
            uint32_t width = std::max(1u, header.pixelWidth >> i);
            uint32_t height = header.pixelHeight > 0 ? std::max(1u, header.pixelHeight >> i) : 1;
            uint32_t depth = header.pixelDepth > 0 ? std::max(1u, header.pixelDepth >> i) : 1;
            mipmapLevels[i].width = width;
            mipmapLevels[i].height = height;
            mipmapLevels[i].depth = depth;
            uint64_t byteOffset = levelIndices[i].byteOffset;
            uint64_t byteLength = levelIndices[i].byteLength;
            uint32_t sizePerFace = static_cast<uint32_t>(byteLength / faceCount);
            mipmapLevels[i].sizePerFace = sizePerFace;
            mipmapLevels[i].faceData.resize(faceCount);

            for (uint32_t face = 0; face < faceCount; face++) {
                uint64_t faceOffset = byteOffset + face * sizePerFace;
                if (faceOffset + sizePerFace <= fileData.size()) {
                    mipmapLevels[i].faceData[face].assign(
                    fileData.begin() + faceOffset,
                    fileData.begin() + faceOffset + sizePerFace);
                
                } else {
                    std::cerr << "Warning: Face data extends beyond file end, padding with zeros" << std::endl;
                    mipmapLevels[i].faceData[face].resize(sizePerFace, 0);
                }
            }
            std::cout << "Extracted level " << i << ": " << width << "x" << height << ", " << sizePerFace << " bytes per face" << std::endl;
        }
        
        return mipmapLevels;
    }

public:
    void convertKTX2toKTX1(const std::string& inputFilename, const std::string& outputFilename) {
        std::cout << "Converting " << inputFilename << " to KTX1..." << std::endl;
        std::vector<uint8_t> fileData = readFile(inputFilename);
        if (fileData.size() < sizeof(KTX2Header)) {
            throw std::runtime_error("File too small to contain a valid KTX2 header");
        }

        KTX2Header* ktx2Header = reinterpret_cast<KTX2Header*>(fileData.data());
        validateKTX2Header(*ktx2Header);

        printHeaderDetails(*ktx2Header);
        std::map<std::string, std::vector<uint8_t>> keyValueData;
        if (ktx2Header->kvdByteLength > 0) {
            keyValueData = parseKTX2KeyValueData(fileData, ktx2Header->kvdByteOffset, ktx2Header->kvdByteLength);
        }

        // Preserve spherical harmonics data if found
        bool hasSphericalHarmonics = keyValueData.find("sh") != keyValueData.end();
        if (hasSphericalHarmonics) {
            std::cout << "Found spherical harmonics data - will preserve in output" << std::endl;
        }

        // Calculate the size needed for level indices
        size_t levelIndicesSize = ktx2Header->levelCount * sizeof(KTX2LevelIndex);
        size_t levelIndicesOffset = sizeof(KTX2Header);
        // Validate that we have enough data for level indices
        if (levelIndicesOffset + levelIndicesSize > fileData.size()) {
            throw std::runtime_error("Not enough data for level indices");
        }

        std::vector<KTX2LevelIndex> levelIndices;
        if (ktx2Header->levelCount > 0) {
            KTX2LevelIndex* levelIndexPtr = reinterpret_cast<KTX2LevelIndex*>(fileData.data() + levelIndicesOffset);

            levelIndices.assign(levelIndexPtr, levelIndexPtr + ktx2Header->levelCount);
            
            // Debug print level indices
            for (size_t i = 0; i < levelIndices.size(); ++i) {
                std::cout << "Level " << i << ": Offset=" << levelIndices[i].byteOffset << ", Length=" << levelIndices[i].byteLength << std::endl;
            }
        }

        std::vector<MipmapLevel> mipmapLevels = extractMipmapLevels(*ktx2Header, fileData, levelIndices);
        KTX1Header ktx1Header = createKTX1Header(*ktx2Header, mipmapLevels.size());
        std::vector<uint8_t> ktx1KvdData;
        if (hasSphericalHarmonics) {
            ktx1KvdData = createKTX1KeyValueData(keyValueData);
            ktx1Header.bytesOfKeyValueData = static_cast<uint32_t>(ktx1KvdData.size());
            std::cout << "Added " << ktx1KvdData.size() << " bytes of key-value data to KTX1 file" << std::endl;
        }

        std::ofstream outFile(outputFilename, std::ios::binary);
        if (!outFile.is_open()) {
            throw std::runtime_error("Could not create output file: " + outputFilename);
        }

        outFile.write(reinterpret_cast<char*>(&ktx1Header), sizeof(KTX1Header));
        if (!ktx1KvdData.empty()) {
            outFile.write(reinterpret_cast<char*>(ktx1KvdData.data()), ktx1KvdData.size());
        }

        for (const auto& level : mipmapLevels) {
            uint32_t imageSizeForLevel = level.sizePerFace;
            outFile.write(reinterpret_cast<char*>(&imageSizeForLevel), sizeof(uint32_t));

            for (const auto& faceData : level.faceData) {
                outFile.write(reinterpret_cast<const char*>(faceData.data()), faceData.size());
            }
        }

        outFile.close();
        std::cout << "Conversion complete: " << inputFilename << " -> " << outputFilename << std::endl;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.ktx2> <output.ktx1>" << std::endl;
        return 1;
    }
    try {
        KTX2Converter converter;
        converter.convertKTX2toKTX1(argv[1], argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}