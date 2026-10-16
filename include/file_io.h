#ifndef FILE_IO_H
#define FILE_IO_H

#include <string>

namespace bitcask{

class FileIO{
public:
    explicit FileIO(const std::string& file_name);

    ~FileIO();

    void Append(const std::string& data);

    std::string Read(uint64_t offset, uint32_t n);

    void Sync();
private:
    int fd_;
};

}

#endif