// Asset access: reads directly from the unmodified ISO (ISO9660) -> CPK -> file, with CRILAYLA decompression.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdio>

using Bytes = std::vector<uint8_t>;

class Cpk {
public:
    bool open(FILE* f, uint64_t base);
    bool contains(const std::string& name) const { return idx.count(name) != 0; }
    Bytes read(const std::string& name);
    // raw stored range, for streaming uncompressed audio/video
    bool locate(const std::string& name, uint64_t& off, uint64_t& size) const;
    std::vector<std::string> names() const;
private:
    struct Ent { uint64_t off; uint32_t size, ext; };
    std::unordered_map<std::string, Ent> idx;
    FILE* fp = nullptr;
    uint64_t base = 0;
};

class AssetFS {
public:
    // path = .iso file; or a directory containing DATA.CPK / STRM.CPK
    bool mount(const std::string& path);
    Bytes read(const std::string& name);
    bool exists(const std::string& name) const;
    std::vector<std::string> names() const;
    std::string error;
private:
    FILE* fp = nullptr;
    Cpk data, strm;
};
extern AssetFS g_fs;
