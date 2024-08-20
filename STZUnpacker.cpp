#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <zlib.h>
#ifdef _WIN32
#include <windows.h>
#endif

#pragma comment(lib, "zlib.lib")

struct FILE_ENTRY {
    int offset;
    int uncompressedSize;
    int compressedSize;
};

FILE_ENTRY entries[6];

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void unpackFiles(const std::string& inputFile, const std::string& outputDir) {
    std::ifstream inFile(inputFile, std::ios::binary);
    if (!inFile) {
        std::cerr << "Error: Could not open input file " << inputFile << std::endl;
        return;
    }

    // Read the file entries metadata
    inFile.read(reinterpret_cast<char*>(entries), sizeof(entries));

    std::ofstream stzinfo(outputDir + "/file.stzinfo");
    if (!stzinfo) {
        std::cerr << "Error: Could not create .stzinfo file" << std::endl;
        return;
    }

    for (int i = 0; i < 6; ++i) {
        if (entries[i].compressedSize > 0) {
            std::vector<char> compressedData(entries[i].compressedSize);
            inFile.seekg(entries[i].offset, std::ios::beg);
            inFile.read(compressedData.data(), entries[i].compressedSize);

            std::vector<char> uncompressedData(entries[i].uncompressedSize);
            uLongf destLen = entries[i].uncompressedSize;
            int result = uncompress((Bytef*)uncompressedData.data(), &destLen, (const Bytef*)compressedData.data(), entries[i].compressedSize);

            if (result != Z_OK) {
                std::cerr << "Error: Failed to decompress file " << i << std::endl;
                continue;
            }

            std::ofstream outFile(outputDir + "/file" + std::to_string(i) + ".bin", std::ios::binary);
            if (outFile) {
                outFile.write(uncompressedData.data(), entries[i].uncompressedSize);
                outFile.close();
            }

            stzinfo << "file" << i << ".bin " << entries[i].uncompressedSize << " " << entries[i].compressedSize << std::endl;
        } else {
            stzinfo << "file" << i << ".bin 0 0" << std::endl;
        }
    }

    stzinfo.close();
    inFile.close();
    std::cout << "Unpacking complete." << std::endl;
}

void repackFiles(const std::string& stzInfoPath, const std::string& outputFile) {
    std::ifstream stzinfo(stzInfoPath);
    if (!stzinfo) {
        std::cerr << "Error: Could not open .stzinfo file" << std::endl;
        return;
    }

    std::vector<std::string> fileNames(6);
    std::vector<int> uncompressedSizes(6), compressedSizes(6);
    std::filesystem::path stzDir = std::filesystem::path(stzInfoPath).parent_path();

    for (int i = 0; i < 6; ++i) {
        stzinfo >> fileNames[i] >> uncompressedSizes[i] >> compressedSizes[i];
    }



    stzinfo.close();

    std::ofstream outFile(outputFile, std::ios::binary);
    if (!outFile) {
        std::cerr << "Error: Could not create output file " << outputFile << std::endl;
        return;
    }

    int currentOffset = sizeof(entries);
    outFile.seekp(currentOffset, std::ios::beg);

    for (int i = 0; i < 6; ++i) {
        if (uncompressedSizes[i] > 0) {
            std::filesystem::path filePath = stzDir / fileNames[i];
            std::ifstream inFile(filePath, std::ios::binary);
            if (!inFile) {
                std::cerr << "Error: Could not open file " << fileNames[i] << std::endl;
                return;
            }

            std::vector<char> uncompressedData(uncompressedSizes[i]);
            inFile.read(uncompressedData.data(), uncompressedSizes[i]);
            inFile.close();

            uLongf destLen = compressBound(uncompressedSizes[i]);
            std::vector<char> compressedData(destLen);

            int result = compress2((Bytef*)compressedData.data(), &destLen, (const Bytef*)uncompressedData.data(), uncompressedSizes[i], Z_BEST_COMPRESSION);

            if (result != Z_OK) {
                std::cerr << "Error: Failed to compress file " << fileNames[i] << std::endl;
                continue;
            }

            entries[i].offset = currentOffset;
            entries[i].uncompressedSize = uncompressedSizes[i];
            entries[i].compressedSize = destLen;

            outFile.seekp(entries[i].offset, std::ios::beg);
            outFile.write(compressedData.data(), entries[i].compressedSize);
            currentOffset += destLen;
            int remainder = destLen % 64;
            currentOffset += (64 - remainder);
        } else {
            entries[i].offset = currentOffset;
            entries[i].uncompressedSize = 0;
            entries[i].compressedSize = 0;
        }
    }


    outFile.seekp(currentOffset, std::ios::beg);
    constexpr uint32_t nullfooteralign = 32;
    int nullfootersize = 32 - (currentOffset % 32);
    std::vector<char> nullBytes(nullfootersize, 0);
    outFile.write(nullBytes.data(), nullfootersize);

    outFile.seekp(0, std::ios::beg);
    outFile.write(reinterpret_cast<const char*>(entries), sizeof(entries));

    outFile.close();
    std::cout << "Repacking complete." << std::endl;
}

void removeQuotes(std::string& str) {
    if (str.size() >= 2 && str[0] == '"' && str[str.size() - 1] == '"') {
        str = str.substr(1, str.size() - 2);
    }
}

int main() {
start:
    std::string choice;
    std::cout << "Do you want to unpack or repack? (u/r): ";
    std::cin >> choice;

    clearScreen();

    if (choice == "u") {
        std::string inputFile, outputDir;
        std::cout << "Enter the input file path: ";
        std::cin >> inputFile;
        std::cout << "Enter the output directory: ";
        std::cin >> outputDir;

        removeQuotes(inputFile);
        removeQuotes(outputDir);

        std::filesystem::create_directories(outputDir);
        unpackFiles(inputFile, outputDir);
    } else if (choice == "r") {
        std::string stzInfoPath, outputFile;
        std::cout << "Enter the .stzinfo file path: ";
        std::cin >> stzInfoPath;
        std::cout << "Enter the output file path: ";
        std::cin >> outputFile;

        removeQuotes(stzInfoPath);
        removeQuotes(outputFile);

        repackFiles(stzInfoPath, outputFile);
    } else {
        std::cerr << "Invalid choice. Please enter 'u' to unpack or 'r' to repack." << std::endl;
    }

    goto start;
}