#include "serialize.h"
#include <cstring>
#include <string>
#include <map>
#include <ostream>
#include <fstream>

struct Message {
    int id = 0;
    std::string str;
    std::map<int, std::string> store;
};

namespace serialize {
    template <class Archive>
    void serialize(Archive& ar, Message& msg) {
        ar & msg.id & msg.str & msg.store;
    }
}

int main(int argc, char* argv[]) {
    Message msg;
    msg.id = 1;
    msg.str = "test serialze";
    msg.store[11] = "123";
    msg.store[22] = "456";

    // 序列化
    serialize::IArchive iar;
    iar & msg;
    std::ofstream ofs("./ser.bin");
    ofs.write(iar.get(), iar.len());
    ofs.close();
    
    // 反序列化
    std::ifstream ifs("./ser.bin");
    char buf[1024] = {0};
    size_t size = ifs.read(buf, 1024).gcount();
    ifs.close();
    serialize::OArchive oar(buf, size);

    Message msgCopy;
    oar & msgCopy;
    return 0;
}