#pragma once
#include <switch.h>
#include <vector>

namespace fs {

    void Log(const char *log, ...);
    void EditContent(std::vector<std::string> &matchList, std::string &env, std::string &del);
    Result ParseConfig(std::vector<std::string> &entries, bool emuNand);

}
