#ifndef LOG_RECORD_H
#define LOG_RECORD_H

#include<string>
#include<cstdint>

namespace bitcask{


enum class LogRecordType{
    kNormal = 1,
    kDeleted = 0
};

struct LogRecord{
    std::string key;
    std::string value;
    LogRecordType type{LogRecordType::kNormal};
};

struct LogRecordPos{
    uint32_t file_id;
    uint64_t offset;
    uint32_t size;
};

std::string EncodeLogRecord(const LogRecord& record);

LogRecord DecodeLogRecord(const std::string& buffer);

}

#endif