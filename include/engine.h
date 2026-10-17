#ifndef ENGINE_H
#define ENGINE_H

#include <string>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <map>
#include "log_record.h"
#include "file_io.h"

namespace bitcask {

class Engine{
public:
    explicit Engine(const std::string& dir_path);

    ~Engine() = default;

    void Put(const std::string& key,const std::string& value);

    std::string Get(const std::string& key);

    bool Delete(const std::string& key);

    void Merge();
private:
    void LoadIndexFromDisk();

    std::unique_ptr<FileIO> active_file_;
    std::unordered_map<std::string,LogRecordPos> memory_index_;
    uint64_t current_offset_{0};

    std::string dir_path_;

    std::map<uint32_t,std::unique_ptr<FileIO>> older_files_;
    uint32_t active_file_id_{1};

    static constexpr uint64_t kMaxFileSize = 10 * 1024;
};


}

#endif