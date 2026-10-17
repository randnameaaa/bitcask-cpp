#include <iostream>
#include "engine.h"

int main() {
    bitcask::Engine db(".");

    std::cout << "Writing massive data to trigger file rollover..." << std::endl;
    for (int i = 0; i < 1000; i++) {
        std::string key = "user_" + std::to_string(i);
        // 构造一个稍微大一点的虚假字符串（长达 100 字节），加速文件写满
        std::string val(100, 'A'); 
        db.Put(key, val);
    }

    // 测试一：跨越文件的极限读取（user_10 肯定在早期的文件里，user_999 肯定在最新的活跃文件里）
    std::cout << "Get user_10 (old file): " << db.Get("user_10").size() << " bytes" << std::endl;
    std::cout << "Get user_999 (active file): " << db.Get("user_999").size() << " bytes" << std::endl;

    return 0;
}