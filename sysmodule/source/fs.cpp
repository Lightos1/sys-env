#include <switch.h>
#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <dirent.h>
#include <algorithm>
#include <sys/stat.h>
#include <cstdarg>

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

    void EditContent(std::vector<std::string> &matchList, std::string &env, std::string &del) {
        DIR *dir = opendir(CONTENTS);
        if (!dir) {
            return;
        }

        dirent *entry;

        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type != DT_DIR) continue;

            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;

            std::string modified = name;

            if (EndsWith(modified, del)) {
                modified.erase(modified.length() - del.length());
            }

            bool inList = std::find(matchList.begin(), matchList.end(), name) != matchList.end() || std::find(matchList.begin(), matchList.end(), modified) != matchList.end();

            if (inList) {
                if (!EndsWith(modified, env)) {
                    modified += env;
                }
            }

            std::string oldPath = std::string(CONTENTS) + name;
            std::string newPath = std::string(CONTENTS) + modified;

            if (oldPath != newPath) {
                rename(oldPath.c_str(), newPath.c_str());
            }
        }

        closedir(dir);
    }

    Result FindConfigHeader(std::ifstream &file, std::string &envString) {
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line == envString) {
                return 0;
            }
        }
        return 1;
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
    void EnsureConfigExists() {
        std::ifstream file(PATH);
        if (file.good()) {
            return;
        }

        mkdir("sdmc:/config", 0777);
        mkdir("sdmc:/config/sys-env", 0777);

        std::ofstream createFile(PATH);
        std::ofstream createLog(FileLogPath);
        createFile.close();
        createLog.close();
    }

    Result ParseConfig(std::vector<std::string> &entries, bool emuNand) {
        EnsureConfigExists();

        std::ifstream file(PATH);
        if (!file.is_open()) {
            return 1;
        }

        std::string blackListHeader;
        if (emuNand) {
          blackListHeader = "[EmuNand]";
        }  else {
            blackListHeader = "[SysNand]";
        }

        Result rc = FindConfigHeader(file, blackListHeader);
        if (R_FAILED(rc)) {
            return rc;
        }

        ReadConfigEntries(file, entries);

        if (!entries.empty()) {
            return 0;
        }

        return -1;
    }

}
