#include "engine.h"
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <vector>
#include <algorithm>

namespace bitcask {

std::string GetFileName(const std::string& dir, uint32_t file_id) {
    std::ostringstream oss;
    oss << dir << "/" << std::setfill('0') << std::setw(4) << file_id << ".data";
    return oss.str();
}

Engine::Engine(const std::string& dir_path) : dir_path_(dir_path) {
    
    if (!std::filesystem::exists(dir_path_)) {
        std::filesystem::create_directories(dir_path_);
    }

    LoadIndexFromDisk();
}

void Engine::LoadIndexFromDisk() {
    std::vector<uint32_t> file_ids;
    
    // 1. 智能扫描目录，揪出所有 .data 文件的 ID
    for (const auto& entry : std::filesystem::directory_iterator(dir_path_)) {
        if (entry.path().extension() == ".data") {
            std::string filename = entry.path().stem().string();
            try {
                file_ids.push_back(std::stoul(filename));
            } catch (...) {} // 忽略类似 merge.data 这样的非纯数字文件
        }
    }

    // 2. 如果是空目录，直接初始化第一代文件
    if (file_ids.empty()) {
        active_file_id_ = 1;
        active_file_ = std::make_unique<FileIO>(GetFileName(dir_path_, active_file_id_));
        current_offset_ = 0;
        return;
    }

    // 3. 从小到大排序，严格按照时间线回放数据
    std::sort(file_ids.begin(), file_ids.end());
    
    for (uint32_t id : file_ids) {
        auto file_io = std::make_unique<FileIO>(GetFileName(dir_path_, id));
        uint64_t offset = 0;

        while (true) {
            try {
                std::string header_buf = file_io->Read(offset, 13);
                if (header_buf.size() < 13) break;

                const char* ptr = header_buf.data();
                ptr += 5;
                uint32_t key_size, value_size;
                std::memcpy(&key_size, ptr, 4);
                ptr += 4;
                std::memcpy(&value_size, ptr, 4);

                uint32_t record_total_size = 13 + key_size + value_size;
                std::string record_buf = file_io->Read(offset, record_total_size);
                LogRecord record = DecodeLogRecord(record_buf);

                if (record.type == LogRecordType::kNormal) {
                    memory_index_[record.key] = LogRecordPos{id, offset, record_total_size};
                } else if (record.type == LogRecordType::kDeleted) {
                    memory_index_.erase(record.key);
                }
                offset += record_total_size;
            } catch (...) { break; }
        }
        older_files_[id] = std::move(file_io);
        current_offset_ = offset; 
    }

    // 4. 把 ID 最大的那个文件从 older_files_ 里抽出来当做活跃文件
    active_file_id_ = file_ids.back();
    active_file_ = std::move(older_files_[active_file_id_]);
    older_files_.erase(active_file_id_);
}

void Engine::Put(const std::string& key, const std::string& value) {
    LogRecord record{key, value, LogRecordType::kNormal};
    std::string encoded = EncodeLogRecord(record);

    if (current_offset_ + encoded.size() > kMaxFileSize) {
        active_file_->Sync(); // 安全落地
        older_files_[active_file_id_] = std::move(active_file_); // 降级为只读历史文件
        
        active_file_id_++; // ID自增
        active_file_ = std::make_unique<FileIO>(GetFileName(dir_path_, active_file_id_)); // 开启新纪元
        current_offset_ = 0;
    }

    active_file_->Append(encoded);
    active_file_->Sync();

    memory_index_[key] = LogRecordPos{active_file_id_, current_offset_, static_cast<uint32_t>(encoded.size())};
    current_offset_ += encoded.size();
}

std::string Engine::Get(const std::string& key) {
    auto it = memory_index_.find(key);
    if (it == memory_index_.end()) {
        return "Not Found";
    }

    LogRecordPos pos = it->second;
    std::string raw_data;
    if (pos.file_id == active_file_id_) {
        // 数据在正在写的活跃文件里
        raw_data = active_file_->Read(pos.offset, pos.size);
    } else {
        // 数据在被冻结的只读文件里
        if (older_files_.find(pos.file_id) == older_files_.end()) return "Data Corruption: File Missing";
        raw_data = older_files_[pos.file_id]->Read(pos.offset, pos.size);
    }

    LogRecord record = DecodeLogRecord(raw_data);
    return record.value;
}

bool Engine::Delete(const std::string& key) {
    if(memory_index_.find(key)==memory_index_.end()){
        return false;
    }

    LogRecord record{key, "", LogRecordType::kDeleted};
    std::string encoded = EncodeLogRecord(record);

    // 墓碑数据同样会触发文件滚动
    if (current_offset_ + encoded.size() > kMaxFileSize) {
        active_file_->Sync();
        older_files_[active_file_id_] = std::move(active_file_);
        active_file_id_++;
        active_file_ = std::make_unique<FileIO>(GetFileName(dir_path_, active_file_id_));
        current_offset_ = 0;
    }

    active_file_->Append(encoded);
    active_file_->Sync();

    memory_index_.erase(key);
    current_offset_ += encoded.size();

    return true;
}

void Engine::Merge() {
    if (older_files_.empty()) return; // 没有历史文件，不用折腾

    // 选定历史文件里最老的 ID，作为合并后的“终极归宿”
    uint32_t merge_id = older_files_.begin()->first;
    std::string merge_file = dir_path_ + "/merge.data";
    auto merge_io = std::make_unique<FileIO>(merge_file);
    uint64_t merge_offset = 0;

    // 1. 遍历内存目录，只要是活着的、并且在老文件里的数据，全部揪出来
    for (auto& [key, pos] : memory_index_) {
        if (pos.file_id == active_file_id_) continue; // 活跃文件还没满，不参与合并

        // 读出纯净的旧数据
        std::string raw_data = older_files_[pos.file_id]->Read(pos.offset, pos.size);
        
        // 写进新的合并文件里
        merge_io->Append(raw_data);
        
        // 关键：更新这把钥匙的门牌号，指向全新的归宿
        pos.file_id = merge_id;
        pos.offset = merge_offset;
        merge_offset += pos.size;
    }

    merge_io->Sync();
    merge_io.reset(); // 释放文件指针

    // 2. 霸道清理：把 older_files_ 里所有的旧文件无情删掉
    for (auto& [id, file] : older_files_) {
        file.reset();
        std::filesystem::remove(GetFileName(dir_path_, id));
    }
    older_files_.clear();

    // 3. 移花接木：把 merge.data 改名为我们选定的那一个老文件
    std::string new_file_name = GetFileName(dir_path_, merge_id);
    std::filesystem::rename(merge_file, new_file_name);
    
    // 重新挂载回历史记录里，继续发光发热
    older_files_[merge_id] = std::make_unique<FileIO>(new_file_name);

    std::cout << "Merge Completed! All old data compacted into file ID: " << merge_id << std::endl;
}

}