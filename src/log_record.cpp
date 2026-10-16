#include "log_record.h"
#include <cstring>

namespace bitcask{

std::string EncodeLogRecord(const LogRecord& record){
    uint32_t key_size = record.key.size();
    uint32_t value_size = record.value.size();
    uint32_t total_size = 13 + key_size + value_size;

    std::string buffer(total_size, 0);
    char *ptr = buffer.data();

    uint32_t crc = 0;
    std::memcpy(ptr, &crc, 4);
    ptr += 4;

    uint8_t type=static_cast<uint8_t>(record.type);
    std::memcpy(ptr, &type, 1);
    ptr += 1;

    std::memcpy(ptr, &key_size, 4);
    ptr += 4;

    std::memcpy(ptr, &value_size, 4);
    ptr += 4;

    std::memcpy(ptr, record.key.data(), key_size);
    ptr += key_size;
    std::memcpy(ptr, record.value.data(), value_size);

    return buffer;
}

LogRecord DecodeLogRecord(const std::string& buffer) {
    LogRecord record;
    const char* ptr = buffer.data();

    ptr += 4;

    record.type = static_cast<LogRecordType>(*ptr);
    ptr += 1;

    uint32_t key_size = 0;
    std::memcpy(&key_size, ptr, 4);
    ptr += 4;

    uint32_t value_size = 0;
    std::memcpy(&value_size, ptr, 4);
    ptr += 4;

    record.key = std::string(ptr, key_size);
    ptr += key_size;
    record.value = std::string(ptr, value_size);

    return record;
}

}