#include <switch.h>
#include <fstream>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <cstdarg>
#include "result.hpp"
#include "fs.hpp"

namespace fs {

    #define CONTENTS "sdmc:/atmosphere/contents/"
    constexpr char FileLogPath[] = "/config/sys-env/log.txt";

    void Log(const char *format, ...) {
        va_list args;
        va_start(args, format);
        FILE *file = fopen(FileLogPath, "a");

        if (file) {
            vfprintf(file, format, args);
            fprintf(file, "\n");
            fclose(file);
        }

        va_end(args);
    }

    bool EndsWith(const std::string &str, const std::string &suffix) {
        if (str.length() < suffix.length()) {
            return false;
        }

        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    }

    bool CheckVersion(u32 currentVer, const ProgramEntry &entry) {
        if (!entry.hasVersionCheck) {
            return true;
        }

        switch (entry.op) {
            case OP_EQ: return currentVer == entry.version;
            case OP_GE: return currentVer >= entry.version;
            case OP_LE: return currentVer <= entry.version;
            case OP_GT: return currentVer >  entry.version;
            case OP_LT: return currentVer <  entry.version;
            default:    return false;
        }
    }

    Result EditContent(std::vector<ProgramEntry> &matchList, std::string &env, std::string &del, u32 currentVer) {
        DIR *dir = opendir(CONTENTS);
        if (!dir) {
            return SYSENV_RC(SysEnvResult_OpenContentsFailed);
        }

        dirent *entry;

        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type != DT_DIR) {
                continue;
            }

            std::string name = entry->d_name;
            if (name == "." || name == "..") {
                continue;
            }

            std::string modified = name;

            if (EndsWith(modified, del)) {
                modified.erase(modified.length() - del.length());
            }

            // 查找匹配的 ProgramEntry
            // 同时尝试去掉本侧环境后缀再匹配，以恢复已被阻止的目录
            std::string matchName = modified;
            if (EndsWith(matchName, env)) {
                matchName.erase(matchName.length() - env.length());
            }

            ProgramEntry *matched = nullptr;
            for (auto &pe : matchList) {
                if (name == pe.id || modified == pe.id || matchName == pe.id) {
                    matched = &pe;
                    break;
                }
            }

            if (matched != nullptr) {
                bool shouldBlock;
                if (matched->hasVersionCheck) {
                    // 有版本条件：固件不符合条件时才阻止
                    shouldBlock = !CheckVersion(currentVer, *matched);
                } else {
                    // 无版本条件：保持原有行为，始终阻止
                    shouldBlock = true;
                }

                if (shouldBlock) {
                    if (!EndsWith(modified, env)) {
                        modified += env;
                    }
                } else {
                    // 不应阻止：去掉环境后缀以恢复加载
                    if (EndsWith(modified, env)) {
                        modified.erase(modified.length() - env.length());
                    }
                }
            }

            std::string oldPath = std::string(CONTENTS) + name;
            std::string newPath = std::string(CONTENTS) + modified;

            if (oldPath != newPath) {
                if (rename(oldPath.c_str(), newPath.c_str()) != 0) {
                    closedir(dir);
                    return SYSENV_RC(SysEnvResult_RenameFailed);
                }
            }
        }

        closedir(dir);
        R_SUCCEED();
    }

    Result FindConfigHeader(std::ifstream &file, std::string &envString) {
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            R_SUCCEED_IF(line == envString);
        }

        return SYSENV_RC(SysEnvResult_HeaderMissing);
    }

    // 解析一行中的版本条件部分，格式: " = >=16.0.0" 或 " = 16.0.0"
    Result ParseVersionCondition(const std::string &condition, ProgramEntry &entry) {
        std::string cond = condition;

        // 去掉前导空格
        size_t pos = 0;
        while (pos < cond.size() && cond[pos] == ' ') {
            pos++;
        }
        cond = cond.substr(pos);

        if (cond.empty()) {
            return SYSENV_RC(SysEnvResult_InvalidVersionFormat);
        }

        // 检测运算符
        u8 op;
        size_t verStart;
        if (cond.compare(0, 2, ">=") == 0) {
            op = OP_GE;
            verStart = 2;
        } else if (cond.compare(0, 2, "<=") == 0) {
            op = OP_LE;
            verStart = 2;
        } else if (cond[0] == '>') {
            op = OP_GT;
            verStart = 1;
        } else if (cond[0] == '<') {
            op = OP_LT;
            verStart = 1;
        } else if (cond[0] == '=') {
            op = OP_EQ;
            verStart = 1;
        } else {
            // 无运算符，默认精确匹配（兼容 "16.0.0" 直接写的情况）
            op = OP_EQ;
            verStart = 0;
        }

        std::string verStr = cond.substr(verStart);
        // 去掉版本号前的空格
        pos = 0;
        while (pos < verStr.size() && verStr[pos] == ' ') {
            pos++;
        }
        verStr = verStr.substr(pos);

        // 解析 MAJOR.MINOR.MICRO
        u32 major = 0, minor = 0, micro = 0;
        int dots = 0;
        std::string part;
        for (char c : verStr) {
            if (c >= '0' && c <= '9') {
                part += c;
            } else if (c == '.') {
                if (part.empty()) {
                    return SYSENV_RC(SysEnvResult_InvalidVersionFormat);
                }
                if (dots == 0) {
                    major = std::stoul(part);
                } else if (dots == 1) {
                    minor = std::stoul(part);
                }
                part.clear();
                dots++;
            } else {
                return SYSENV_RC(SysEnvResult_InvalidVersionFormat);
            }
        }
        if (!part.empty()) {
            micro = std::stoul(part);
        }

        if (dots > 2) {
            return SYSENV_RC(SysEnvResult_InvalidVersionFormat);
        }

        entry.hasVersionCheck = true;
        entry.op = op;
        entry.version = MAKEHOSVERSION(major, minor, micro);

        R_SUCCEED();
    }

    void ReadConfigEntries(std::ifstream &file, std::vector<ProgramEntry> &entries) {
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (!line.empty() && line[0] == '[') {
                break;
            }

            if (line.empty()) {
                continue;
            }

            ProgramEntry entry;
            entry.hasVersionCheck = false;
            entry.op = OP_EQ;
            entry.version = 0;

            // 查找 '=' 分隔符，拆分程序 ID 和版本条件
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                entry.id = line.substr(0, eqPos);
                // 去掉程序 ID 尾部空格
                while (!entry.id.empty() && entry.id.back() == ' ') {
                    entry.id.pop_back();
                }

                std::string cond = line.substr(eqPos + 1);
                Result rc = ParseVersionCondition(cond, entry);
                if (R_FAILED(rc)) {
                    // 解析失败：跳过此行，记录日志
                    Log("Failed to parse version condition: %s", line.c_str());
                    continue;
                }
            } else {
                entry.id = line;
            }

            if (!entry.id.empty()) {
                entries.push_back(entry);
            }
        }
    }

    #define PATH "sdmc:/config/sys-env/config.ini"
    Result EnsureConfigExists() {
        std::ifstream file(PATH);
        R_SUCCEED_IF(file.good());

        if (mkdir("sdmc:/config", 0777) != 0 && errno != EEXIST) {
            return SYSENV_RC(SysEnvResult_CreateDirectoryFailed);
        }

        if (mkdir("sdmc:/config/sys-env", 0777) != 0 && errno != EEXIST) {
            return SYSENV_RC(SysEnvResult_CreateDirectoryFailed);
        }

        std::ofstream config(PATH);
        if (!config.is_open()) {
            return SYSENV_RC(SysEnvResult_CreateFileFailed);
        }

        std::ofstream log(FileLogPath);
        if (!log.is_open()) {
            return SYSENV_RC(SysEnvResult_CreateFileFailed);
        }

        return SYSENV_RC(SysEnvResult_EmptyConfig);
    }

    Result ParseConfig(std::vector<ProgramEntry> &entries, bool emuNand) {
        R_TRY(EnsureConfigExists());

        std::ifstream file(PATH);
        if (!file.is_open()) {
            return SYSENV_RC(SysEnvResult_CreateFileFailed);
        }

        std::string blackListHeader;
        if (emuNand) {
          blackListHeader = "[EmuNand]";
        }  else {
            blackListHeader = "[SysNand]";
        }

        R_TRY(FindConfigHeader(file, blackListHeader));

        ReadConfigEntries(file, entries);

        if (entries.empty()) {
            return SYSENV_RC(SysEnvResult_ConfigNotFound);
        }

        R_SUCCEED();
    }

}
