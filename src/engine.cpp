#include "engine.h"
#include <iostream>
#include <cstring>
#include <stdexcept>

namespace bitcask {

Engine::Engine(const std::string& dir_path){
    // 暂且固定写死文件名，后续可以扩展为扫描目录下所有文件
    std::string file_path = dir_path + "/0001.data";
    active_file_ = std::make_unique<FileIO>(file_path);
    
    // 启动引擎的第一步：恢复记忆！
    LoadIndexFromDisk();
}

void Engine::LoadIndexFromDisk(){
    uint64_t offset = 0;
    while(1)
        try{
            std::string header_buf = active_file_->Read(offset, 13);
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
            std::string record_buf = active_file_->Read(offset, record_total_size);
            LogRecord record = DecodeLogRecord(record_buf);

            // 5. 将 Key 和 对应的位置信息登记到内存目录
            // 注意：如果是被删除的数据（kDeleted），真实的引擎这里应该从目录里抹除它，这里先简化
            memory_index_[record.key] = LogRecordPos{1, offset, record_total_size};

            // 6. 游标向前推进，准备读下一条
            offset += record_total_size;
        }catch(...){
            break;
        }
    current_offset_ = offset; 
    std::cout << "Engine initialized! Rebuilt " << memory_index_.size() << " keys from disk." << std::endl;
}

void Engine::Put(const std::string& key, const std::string& value) {
    LogRecord record{key, value, LogRecordType::kNormal};
    std::string encoded = EncodeLogRecord(record);

    active_file_->Append(encoded);
    active_file_->Sync();

    memory_index_[key] = LogRecordPos{1, current_offset_, static_cast<uint32_t>(encoded.size())};
    current_offset_ += encoded.size();
}

std::string Engine::Get(const std::string& key) {
    auto it = memory_index_.find(key);
    if (it == memory_index_.end()) {
        return "Not Found";
    }

    LogRecordPos pos = it->second;
    std::string raw_data = active_file_->Read(pos.offset, pos.size);
    LogRecord record = DecodeLogRecord(raw_data);
    
    return record.value;
}
    
}