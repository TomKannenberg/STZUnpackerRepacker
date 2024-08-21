#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <filesystem>
#include <locale>
#include <locale.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
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

enum FileType {
    File_Dat = 0,
    File_Rel = 1,
    File_GPU = 2,
    File_Language_Dat = 3,
    File_Language_Rel = 4,
    File_Language_GPU = 5,
    File_Max = 6
};

constexpr std::array<char, 256> concat(const char* a, const char* b) {
    std::array<char, 256> result{};
    std::size_t i = 0;
    for (; a[i] != '\0'; ++i) {
        result[i] = a[i];
    }
    std::size_t j = 0;
    for (; b[j] != '\0'; ++j) {
        result[i + j] = b[j];
    }
    result[i + j] = '\0';
    return result;
}

constexpr char frontsymbol[] = "»»";
constexpr char backsymbol[] = "««";

constexpr auto dat_type = concat(backsymbol, ".Dat");
constexpr auto rel_type = concat(backsymbol, ".Rel");
constexpr auto gpu_type = concat(backsymbol, ".GPU");
constexpr auto language_dat = concat(backsymbol, "Language.Dat");
constexpr auto language_rel = concat(backsymbol, "Language.Rel");
constexpr auto language_gpu = concat(backsymbol, "Language.GPU");

constexpr std::array<std::string_view, File_Max> FileTypeStrings = {
    std::string_view(dat_type.data()),
    std::string_view(rel_type.data()),
    std::string_view(gpu_type.data()),
    std::string_view(language_dat.data()),
    std::string_view(language_rel.data()),
    std::string_view(language_gpu.data())
};

FILE_ENTRY entries[6];

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

int someDumbFunction(int input) {
    std::cout << "Idc" << input << std::endl;

    return 0;
}

std::wstring getStzName(const std::filesystem::path& directoryPath) {
    try {
        if (!std::filesystem::is_directory(directoryPath)) {
            std::cerr << ("Provided path is not a directory.");
        }

        for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
            std::string filename = entry.path().filename().string();

            if (filename.empty()) {
                continue;
            }

            size_t firstSymbolPos = filename.find(frontsymbol);
            if (firstSymbolPos == std::string::npos) {
                continue;
            }

            size_t secondSymbolPos = filename.find(backsymbol, firstSymbolPos + 1);
            if (secondSymbolPos == std::string::npos) {
                std::cerr << "Warning: Filename '" << filename << "' does not contain a second occurrence of the symbol." << std::endl;
                continue;
            }

            std::string extractedString = filename.substr(firstSymbolPos + 1, secondSymbolPos - firstSymbolPos - 1);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }
}

void unpackFiles(const std::string& inputFile, const std::string& outputDir) {
    std::ifstream inFile(inputFile, std::ios::binary);
    if (!inFile) {
        std::cerr << "Error: Could not open input file " << inputFile << std::endl;
        return;
    }

    inFile.read(reinterpret_cast<char*>(entries), sizeof(entries));

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

            std::string outPath = outputDir + std::string(frontsymbol) + std::filesystem::path(inputFile).filename().string() + std::string(FileTypeStrings[i]);
            std::cout << frontsymbol << std::endl;
            std::cout << outPath << std::endl;
            std::ofstream outFile(outPath, std::ios::binary);
            if (outFile) {
                outFile.write(uncompressedData.data(), entries[i].uncompressedSize);
                outFile.close();
            }
        }
    }
    inFile.close();
    std::cout << "Unpacking complete." << std::endl;
}

void repackFiles(const std::filesystem::path& inputDir) {
    
    std::wstring outputFile = getStzName(inputDir);

    std::wcout << outputFile << std::endl;
    
    std::ofstream outFile(outputFile, std::ios::binary);
    if (!outFile) {
        std::wcerr << "Error: Could not create output file " << outputFile << std::endl;
        return;
    }

    int currentOffset = sizeof(entries);
    outFile.seekp(currentOffset, std::ios::beg);

    for (int i = 0; i < 6; ++i) {
        std::filesystem::path filePath = inputDir / FileTypeStrings[i];
        std::ifstream inFile(filePath, std::ios::binary);

        if (inFile) {
            
            inFile.seekg(0, std::ios::end);
            std::streamsize fileSize = inFile.tellg();
            inFile.seekg(0, std::ios::beg);

            std::vector<char> uncompressedData(fileSize);

            if (!inFile.read(uncompressedData.data(), fileSize)) {
                std::cerr << "Error: Could not read the file into the vector." << std::endl;
                std::cout << fileSize << std::endl;
            }

            uLongf destLen = compressBound(fileSize);

            std::vector<char> compressedData(destLen);

            int result = compress2((Bytef*)compressedData.data(), &destLen, (const Bytef*)uncompressedData.data(), fileSize, Z_BEST_COMPRESSION);

            if (result != Z_OK) {
                std::cerr << "Error: Failed to compress file " << FileTypeStrings[i] << ". Compression result: " << result << std::endl;
                std::cout << fileSize << std::endl;
                std::cout << destLen << std::endl;
                continue;
            }

            entries[i].offset = currentOffset;
            entries[i].uncompressedSize = fileSize;
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

        removeQuotes(inputFile);

        outputDir = inputFile + "_unpacked\\";

        std::filesystem::create_directories(outputDir);
        unpackFiles(inputFile, outputDir);
    } else if (choice == "r") {
        std::string inputDir, outputFile;
        std::cout << "Enter the input directory path: ";
        std::cin >> inputDir;
        removeQuotes(inputDir);

        inputDir += "\\";

        repackFiles(inputDir);
    } else {
        std::cerr << "Invalid choice. Please enter 'u' to unpack or 'r' to repack." << std::endl;
    }

    goto start;
}