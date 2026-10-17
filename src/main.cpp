#include <iostream>
#include <filesystem>
#include "engine.h"

int main() {
    bitcask::Engine db(".");
    std::string data_file = "./0001.data";

    // 1. 暴力修改：把同一个 Key 的值反复修改 1000 次
    // 在旧版里，这会产生 1000 条历史废弃数据
    std::cout << "Writing 1000 historical versions of a key..." << std::endl;
    for (int i = 0; i < 1000; i++) {
        db.Put("boss_key", "value_" + std::to_string(i));
    }

    // 打印此时被撑满的磁盘文件大小
    auto size_before = std::filesystem::file_size(data_file);
    std::cout << "[Before Merge] File Size: " << size_before << " bytes" << std::endl;

    // 2. 召唤终极魔法：执行垃圾回收
    db.Merge();

    // 打印压缩后的磁盘文件大小
    auto size_after = std::filesystem::file_size(data_file);
    std::cout << "[After Merge]  File Size: " << size_after << " bytes" << std::endl;
    
    // 3. 验证数据正确性：虽然换了皮，依然能读出最后一次写入的值
    std::cout << "Verification: boss_key = " << db.Get("boss_key") << std::endl;

    return 0;
}