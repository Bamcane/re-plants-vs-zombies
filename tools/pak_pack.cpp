// pak_pack.cpp
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <zstd.h>

using namespace std;

const uint32_t PAK_MAGIC = 0xBAC04AC0;
const uint8_t FILEFLAGS_END = 0x80;

void list_files(const string& base, const string& path, vector<string>& files) {
    DIR* dir = opendir(path.c_str());
    if (!dir) return;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        string full = path + "/" + entry->d_name;
        string rel = full.substr(base.length() + 1);
        struct stat st;
        if (stat(full.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                list_files(base, full, files);
            } else if (S_ISREG(st.st_mode)) {
                files.push_back(rel);
            }
        }
    }
    closedir(dir);
}

int64_t get_file_time(const string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return static_cast<int64_t>(st.st_mtime);
    }
    return static_cast<int64_t>(time(nullptr));
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <input_dir> <output.pak>\n";
        return 1;
    }

    string inputDir = argv[1];
    string outputFile = argv[2];
    if (inputDir.back() == '/') inputDir.pop_back();

    vector<string> files;
    list_files(inputDir, inputDir, files);

    if (files.empty()) {
        cerr << "No files found in " << inputDir << "\n";
        return 1;
    }

    vector<uint8_t> headerBuf;
    vector<uint8_t> dataBuf;

    // Header: magic + version
    uint32_t magic = PAK_MAGIC;
    uint32_t version = 1;
    headerBuf.insert(headerBuf.end(), reinterpret_cast<uint8_t*>(&magic), reinterpret_cast<uint8_t*>(&magic) + 4);
    headerBuf.insert(headerBuf.end(), reinterpret_cast<uint8_t*>(&version), reinterpret_cast<uint8_t*>(&version) + 4);

    for (const string& relPath : files) {
        string fullPath = inputDir + "/" + relPath;
        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;

        string normPath = relPath;
        for (char& c : normPath) if (c == '\\') c = '/';

        // 读取原始数据
        ifstream fin(fullPath, ios::binary);
        vector<uint8_t> srcData(st.st_size);
        if (st.st_size > 0) {
            fin.read(reinterpret_cast<char*>(srcData.data()), st.st_size);
        }
        fin.close();

        vector<uint8_t> finalData;
        uint8_t flags = 0x00; // 默认未压缩
        int srcSize = static_cast<int>(st.st_size);
        int storedSize = srcSize;
        size_t compressedSize = 0;

        // 尝试压缩
        size_t maxDstSize = ZSTD_compressBound(st.st_size);
        vector<uint8_t> compressed(maxDstSize);
        compressedSize = ZSTD_compress(
            compressed.data(), maxDstSize,
            srcData.data(), st.st_size,
            6
        );

        if (!ZSTD_isError(compressedSize) && compressedSize < static_cast<size_t>(st.st_size)) {
            // 压缩成功且更小：使用压缩数据
            compressed.resize(compressedSize);
            finalData = std::move(compressed);
            flags = 0x01; // 标记为已压缩
            storedSize = static_cast<int>(compressedSize);
            cout << "Added (compressed): " << normPath << " (" << st.st_size << " -> " << compressedSize << " bytes)\n";
        } else {
            // 不压缩：使用原始数据
            finalData = std::move(srcData);
            storedSize = srcSize;
            cout << "Added (uncompressed): " << normPath << " (" << st.st_size << " bytes)\n";
        }

        // 写 entry: flags, name, src_size, stored_size, time
        uint8_t nameLen = static_cast<uint8_t>(normPath.size());
        int64_t fileTime = get_file_time(fullPath);

        headerBuf.push_back(flags);
        headerBuf.push_back(nameLen);
        headerBuf.insert(headerBuf.end(), normPath.begin(), normPath.end());
        headerBuf.insert(headerBuf.end(), reinterpret_cast<uint8_t*>(&srcSize), reinterpret_cast<uint8_t*>(&srcSize) + 4);
        headerBuf.insert(headerBuf.end(), reinterpret_cast<uint8_t*>(&storedSize), reinterpret_cast<uint8_t*>(&storedSize) + 4);
        headerBuf.insert(headerBuf.end(), reinterpret_cast<uint8_t*>(&fileTime), reinterpret_cast<uint8_t*>(&fileTime) + 8);

        // 追加实际存储的数据（压缩 or 原始）
        dataBuf.insert(dataBuf.end(), finalData.begin(), finalData.end());
    }

    headerBuf.push_back(FILEFLAGS_END);

    // 合并
    vector<uint8_t> finalBuf;
    finalBuf.insert(finalBuf.end(), headerBuf.begin(), headerBuf.end());
    finalBuf.insert(finalBuf.end(), dataBuf.begin(), dataBuf.end());

    // XOR 0xF7 加密整个文件（包括 header 和压缩数据）
    for (uint8_t& b : finalBuf) {
        b ^= 0xF7;
    }

    ofstream fout(outputFile, ios::binary);
    if (!fout) {
        cerr << "Cannot write to " << outputFile << "\n";
        return 1;
    }
    fout.write(reinterpret_cast<char*>(finalBuf.data()), finalBuf.size());
    fout.close();

    cout << "Packed " << files.size() << " files into " << outputFile << " (with zstd)\n";
    return 0;
}