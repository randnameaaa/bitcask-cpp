#include <iostream>
#include "engine.h"

int main() {
    // 初始化引擎（指向当前目录，它会自动去读 0001.data 恢复记忆）
    bitcask::Engine db(".");

    // 我们可以直接验证一下，上次程序退出前写进去的 user_001 还在不在？
    std::cout << "Read old key (user_001): " << db.Get("user_001") << std::endl;

    // 写入新数据
    db.Put("user_002", "Jerry");
    db.Put("user_003", "Spike");

    std::cout << "Read new key (user_002): " << db.Get("user_002") << std::endl;

    return 0;
}