#include "engine.h"
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <iomanip>
#include <sstream>

namespace bitcask {

std::string GetFileName(const std::string& dir, uint32_t file_id) {
    std::ostringstream oss;
    oss << dir << "/" << std::setfill('0') << std::setw(4) << file_id << ".data";
    return oss.str();
}

Engine::Engine(const std::string& dir_path) : dir_path_(dir_path) {
    
    LoadIndexFromDisk();
}

void Engine::LoadIndexFromDisk() {
    uint32_t file_id = 1;
    while(true) {
        std::string file_path = GetFileName(dir_path_, file_id);
        if (!std::filesystem::exists(file_path)) {
            // 如果连 0001.data 都不存在，活跃文件 ID 依然是 1
            active_file_id_ = std::max(1u, file_id - 1);
            break;
        }
        auto file_io = std::make_unique<FileIO>(file_path);
        uint64_t offset = 0;
        while(true)
            try{
                std::string header_buf = file_io->Read(offset, 13);
                if (header_buf.size() < 13) break; // 读到文件末尾了，退出循环

                // 2. 从头部解析出 Key 和 Value 的长度
                const char* ptr = header_buf.data();
                ptr += 5; // 跳过 4字节CRC 和 1字节Type
                uint32_t key_size, value_size;
                std::memcpy(&key_size, ptr, 4);
                ptr += 4;
                std::memcpy(&value_size, ptr, 4);

                // 3. 计算这条数据在磁盘上的总长度
                uint32_t record_total_size = 13 + key_size + value_size;

                // 4. 把整条数据完整读出来，解包拿到 Key
                std::string record_buf = file_io->Read(offset, record_total_size);
                LogRecord record = DecodeLogRecord(record_buf);

                // 5. 将 Key 和 对应的位置信息登记到内存目录
                if (record.type == LogRecordType::kNormal) {
                    memory_index_[record.key] = LogRecordPos{file_id, offset, record_total_size};
                } else if (record.type == LogRecordType::kDeleted) {
                    // 如果读到墓碑，说明这个数据被删了，从目录里抹除它
                    memory_index_.erase(record.key);
                }

                // 6. 游标向前推进，准备读下一条
                offset += record_total_size;
            }catch(...){
                break;
            }
        current_offset_ = offset; 
        older_files_[file_id] = std::move(file_io); // 先全放进老文件里
        file_id++;
    }

    if (older_files_.empty()) {
        // 目录为空，凭空创建 0001.data
        active_file_ = std::make_unique<FileIO>(GetFileName(dir_path_, 1));
        current_offset_ = 0;
    } else {
        // 把找到的最后一个文件从老文件集合里抽离出来，作为当前活跃文件
        active_file_ = std::move(older_files_[active_file_id_]);
        older_files_.erase(active_file_id_);
    }
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
    if(memory_index_.empty()) return ;

    std::string old_file = dir_path_ + "/0001.data";
    std::string merge_file = dir_path_ + "/merge.data";

    auto merge_io = std::make_unique<FileIO>(merge_file);
    uint64_t merge_offset = 0;

    for(auto &[key, pos] : memory_index_) {
        std::string raw_data = active_file_->Read(pos.offset, pos.size);
        
        merge_io->Append(raw_data);

        pos.offset = merge_offset;
        merge_offset += pos.size;
    }

    merge_io->Sync();

    active_file_.reset();
    merge_io.reset();

    std::filesystem::remove(old_file);
    std::filesystem::rename(merge_file, old_file);

    active_file_ = std::make_unique<FileIO>(old_file);
    current_offset_ = merge_offset;
}
    
}