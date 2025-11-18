// pak_unpack.cpp
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <zstd.h>

using namespace std;

const uint32_t PAK_MAGIC = 0xBAC04AC0;
const uint8_t FILEFLAGS_END = 0x80;

bool create_directories(const string& path) {
    size_t pos = 0;
    do {
        pos = path.find('/', pos + 1);
        string subdir = path.substr(0, pos);
        if (!subdir.empty() && mkdir(subdir.c_str(), 0755) != 0) {
            if (errno != EEXIST) return false;
        }
    } while (pos != string::npos);
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <input.pak> <output_dir>\n";
        return 1;
    }

    string pakFile = argv[1];
    string outDir = argv[2];
    if (outDir.back() != '/') outDir += '/';

    ifstream fin(pakFile, ios::binary);
    if (!fin) {
        cerr << "Cannot open " << pakFile << "\n";
        return 1;
    }

    fin.seekg(0, ios::end);
    size_t fileSize = fin.tellg();
    fin.seekg(0);

    vector<uint8_t> buffer(fileSize);
    fin.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    fin.close();

    // XOR 0xF7 解密整个文件
    for (size_t i = 0; i < fileSize; ++i) {
        buffer[i] ^= 0xF7;
    }

    size_t pos = 0;

    if (pos + 8 > fileSize) {
        cerr << "File too small\n";
        return 1;
    }

    uint32_t magic = *reinterpret_cast<const uint32_t*>(buffer.data() + pos); pos += 4;
    uint32_t version = *reinterpret_cast<const uint32_t*>(buffer.data() + pos); pos += 4;

    if (magic != PAK_MAGIC) {
        cerr << "Invalid magic number\n";
        return 1;
    }

    if (version != 0) {
        cerr << "Unsupported version: " << version << "\n";
        return 1;
    }

    struct Entry {
        uint8_t flags;
        string name;
        int originalSize;
        int64_t time;
        size_t storedSize; // 实际存储在 PAK 中的字节数（压缩 or 原始）
    };

    vector<Entry> entries;

    // 第一遍：解析头
    while (pos < fileSize) {
        if (pos + 1 > fileSize) break;

        uint8_t flags = buffer[pos++];
        if (flags & FILEFLAGS_END) break;

        uint8_t nameLen = buffer[pos++];
        if (pos + nameLen + 4 + 4 + 8 > fileSize) {
            cerr << "Truncated entry header\n";
            return 1;
        }

        string name(reinterpret_cast<const char*>(buffer.data() + pos), nameLen);
        pos += nameLen;

        int srcSize = *reinterpret_cast<const int*>(buffer.data() + pos); pos += 4;
        int storedSizeInt = *reinterpret_cast<const int*>(buffer.data() + pos); pos += 4;
        int64_t fileTime = *reinterpret_cast<const int64_t*>(buffer.data() + pos); pos += 8;

        for (char& c : name) {
            if (c == '\\') c = '/';
        }

        entries.push_back({flags, name, srcSize, fileTime, static_cast<size_t>(storedSizeInt)});
    }

    size_t dataStart = pos;
    size_t dataOffset = 0;

    // 第二遍：提取文件
    for (const auto& e : entries) {
        if (dataStart + dataOffset + e.storedSize > buffer.size()) {
            cerr << "Data overflow for: " << e.name << "\n";
            continue;
        }

        const uint8_t* srcData = buffer.data() + dataStart + dataOffset;
        vector<uint8_t> outputData(e.originalSize ? e.originalSize : 1);

        if (e.flags & 0x01) {
            // 已压缩：需要解压
            size_t result = ZSTD_decompress(
                outputData.data(),
                outputData.size(),
                srcData,
                e.storedSize
            );
            if (ZSTD_isError(result) || result != static_cast<size_t>(e.originalSize)) {
                cerr << "ZSTD decompression failed for " << e.name << ": "
                     << ZSTD_getErrorName(result) << "\n";
                continue;
            }
        } else {
            // 未压缩：直接复制
            if (e.storedSize != static_cast<size_t>(e.originalSize)) {
                cerr << "Size mismatch in uncompressed file: " << e.name << "\n";
                continue;
            }
            memcpy(outputData.data(), srcData, e.storedSize);
        }

        // 写出文件
        string fullPath = outDir + e.name;
        if (!create_directories(fullPath)) {
            cerr << "Failed to create directories for: " << fullPath << "\n";
            continue;
        }

        ofstream fout(fullPath, ios::binary);
        if (!fout) {
            cerr << "Cannot write file: " << fullPath << "\n";
            continue;
        }

        fout.write(reinterpret_cast<const char*>(outputData.data()), e.originalSize);
        fout.close();

        cout << "Extracted: " << e.name << " (" << e.originalSize << " bytes"
             << (e.flags & 0x01 ? ", compressed" : ", raw") << ")\n";

        dataOffset += e.storedSize;
    }

    cout << "Unpacked " << entries.size() << " files from " << pakFile << "\n";
    return 0;
}