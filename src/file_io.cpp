#include "file_io.h"
#include <fcntl.h>    // 提供 open 的宏定义 (如 O_CREAT)
#include <unistd.h>   // 提供 write, close, fsync
#include <stdexcept>

namespace bitcask {

FileIO::FileIO(const std::string& file_name){
    // 调用 Linux 底层 open
    // O_RDWR: 读写模式
    // O_CREAT: 如果文件不存在就自动创建
    // O_APPEND: 所有的 write 操作强制追加到文件最末尾（这是 Bitcask 快如闪电的灵魂）
    // 0644: 文件的默认权限
    fd_ = ::open(file_name.c_str(), O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd_ < 0) {
        throw std::runtime_error("Failed to open data file: " + file_name);
    }
}

FileIO::~FileIO() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

void FileIO::Append(const std::string& data) {
    // 直接把打包好的字节流怼进文件里
    ssize_t bytes_written = ::write(fd_, data.data(), data.size());
    if (bytes_written != static_cast<ssize_t>(data.size())) {
        throw std::runtime_error("Failed to write to data file");
    }
}

void FileIO::Sync() {
    // 数据库防丢数据的关键：绕过 Page Cache，强制操作系统把数据刷落物理磁盘
    if (fd_ >= 0) {
        ::fsync(fd_);
    }
}

std::string FileIO::Read(uint64_t offset, uint32_t n) {
    std::string buffer(n, 0); // 准备一个 n 字节的空容器
    // pread：直接去指定的 offset 抓取 n 字节，不影响其他人的读写
    ssize_t bytes_read = ::pread(fd_, buffer.data(), n, offset);
    if (bytes_read != static_cast<ssize_t>(n)) {
        throw std::runtime_error("Failed to read complete data from file");
    }
    return buffer;
}

}