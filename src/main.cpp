#include <iostream>
#include "engine.h"

int main() {
    bitcask::Engine db(".");

    // 1. 写入数据
    db.Put("user_004", "Alice");
    std::cout << "Put user_004: " << db.Get("user_004") << std::endl;

    // 2. 删除数据
    bool success = db.Delete("user_004");
    std::cout << "Delete user_004 status: " << (success ? "True" : "False") << std::endl;

    // 3. 再次查询，应该找不到
    std::cout << "Get user_004 after delete: " << db.Get("user_004") << std::endl;

    return 0;
}