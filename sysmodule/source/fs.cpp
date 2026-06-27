#include <switch.h>
#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <dirent.h>
#include <algorithm>
#include <sys/stat.h>
#include <cstdarg>
#include "result.hpp"

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

    std::string StripSuffix(const std::string &str, const std::string &suffix) {
        if (EndsWith(str, suffix)) {
            return str.substr(0, str.length() - suffix.length());
        }
        return str;
    }

    bool IsInMatchList(const std::vector<std::string> &matchList, const std::string &name, const std::string &modified) {
        return std::find(matchList.begin(), matchList.end(), name) != matchList.end() || std::find(matchList.begin(), matchList.end(), modified) != matchList.end();
    }

    std::string ResolveTargetName(const std::vector<std::string> &matchList, const std::string &name, const std::string &modified, const std::string &env, const std::string &del) {
        if (IsInMatchList(matchList, name, modified)) {
            if (!EndsWith(modified, env)) {
                return modified + env;
            }
        } else {
            if (EndsWith(name, env)) {
                return StripSuffix(name, env);
            }
            if (EndsWith(name, del)) {
                return StripSuffix(name, del);
            }
        }
        return modified;
    }

    Result RenameEntry(const std::string &oldPath, const std::string &newPath) {
        if (oldPath == newPath) {
            R_SUCCEED();
        }

        if (rename(oldPath.c_str(), newPath.c_str()) != 0) {
            return SYSENV_RC(SysEnvResult_RenameFailed);
        }

        R_SUCCEED();
    }

    Result ProcessEntry(const std::string &name, std::vector<std::string> &matchList, std::string &env, std::string &del) {
        std::string modified = StripSuffix(name, env);
        std::string target = ResolveTargetName(matchList, name, modified, env, del);

        std::string oldPath = std::string(CONTENTS) + name;
        std::string newPath = std::string(CONTENTS) + target;

        return RenameEntry(oldPath, newPath);
    }

    Result EditContent(std::vector<std::string> &matchList, std::string &env, std::string &del) {
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

            Result rc = ProcessEntry(name, matchList, env, del);
            if (R_FAILED(rc)) {
                closedir(dir);
                return rc;
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

    void ReadConfigEntries(std::ifstream &file, std::vector<std::string> &entries) {
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

            entries.push_back(line);
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

    Result ParseConfig(std::vector<std::string> &entries, bool emuNand) {
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

        R_SUCCEED();
    }

}
