#define TESLA_INIT_IMPL
#include <exception_wrap.hpp>
#include <tesla.hpp>

#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

namespace {

    constexpr const char * const ConfigDir    = "/config/sys-env";
    constexpr const char * const ConfigPath   = "/config/sys-env/config.ini";
    constexpr const char * const ContentsPath = "/atmosphere/contents";

    constexpr const char * const HeaderSys = "[SysNand]";
    constexpr const char * const HeaderEmu = "[EmuNand]";

    constexpr const char * const SuffixSysBak = ".sys.bak";
    constexpr const char * const SuffixEmuBak = ".emu.bak";

    constexpr u64 TeslaProgramId = 0x420000000007E51AULL;
    constexpr u64 SelfProgramId  = 0x420000000000042AULL;

    struct ContentEntry {
        std::string id;
        std::string dirName;
    };

    void RStrip(std::string &line) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
    }

    bool EndsWith(const std::string &str, const std::string &suffix) {
        if (str.length() < suffix.length()) {
            return false;
        }
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    }

    std::string StripBakSuffix(const std::string &dirName) {
        if (EndsWith(dirName, SuffixSysBak)) {
            return dirName.substr(0, dirName.length() - std::strlen(SuffixSysBak));
        }
        if (EndsWith(dirName, SuffixEmuBak)) {
            return dirName.substr(0, dirName.length() - std::strlen(SuffixEmuBak));
        }
        return dirName;
    }

    void ReadConfig(std::vector<std::string> &sysList, std::vector<std::string> &emuList) {
        sysList.clear();
        emuList.clear();

        std::ifstream file(ConfigPath);
        if (!file.is_open()) {
            return;
        }

        std::vector<std::string> *current = nullptr;
        std::string line;
        while (std::getline(file, line)) {
            RStrip(line);
            if (line.empty()) {
                continue;
            }

            if (line[0] == '[') {
                if (line == HeaderSys) {
                    current = &sysList;
                } else if (line == HeaderEmu) {
                    current = &emuList;
                } else {
                    current = nullptr;
                }
                continue;
            }

            if (current) {
                current->push_back(line);
            }
        }
    }

    bool WriteConfig(const std::vector<std::string> &sysList, const std::vector<std::string> &emuList) {
        mkdir("/config", 0777);
        mkdir(ConfigDir, 0777);

        std::ofstream file(ConfigPath, std::ios::trunc);
        if (!file.is_open()) {
            return false;
        }

        file << HeaderSys << "\n";
        for (const auto &id : sysList) {
            file << id << "\n";
        }
        file << "\n";
        file << HeaderEmu << "\n";
        for (const auto &id : emuList) {
            file << id << "\n";
        }

        return true;
    }

    bool HasExefsNsp(const std::string &dirName) {
        std::string path = std::string(ContentsPath) + "/" + dirName + "/exefs.nsp";
        struct stat st;
        return stat(path.c_str(), &st) == 0;
    }

    std::vector<ContentEntry> ScanContents() {
        std::vector<ContentEntry> entries;

        DIR *dir = opendir(ContentsPath);
        if (!dir) {
            return entries;
        }

        struct dirent *d;
        while ((d = readdir(dir)) != nullptr) {
            if (d->d_name[0] == '.') {
                continue;
            }
            if (d->d_type != DT_DIR) {
                continue;
            }

            const std::string dirName = d->d_name;
            const std::string id = StripBakSuffix(dirName);
            if (id.length() != 16) {
                continue;
            }

            const u64 pid = std::strtoull(id.c_str(), nullptr, 16);
            if (pid == TeslaProgramId || pid == SelfProgramId) {
                continue;
            }

            if (HasExefsNsp(dirName)) {
                continue;
            }

            entries.push_back({ id, dirName });
        }
        closedir(dir);

        std::sort(entries.begin(), entries.end(), [](const ContentEntry &a, const ContentEntry &b) {
            return a.id < b.id;
        });

        return entries;
    }

}

class GuiEnv : public tsl::Gui {
  public:
    explicit GuiEnv(bool emuNand) : m_emuNand(emuNand) {}

    tsl::elm::Element *createUI() override {
        const char *title = m_emuNand ? "EmuNand" : "SysNand";
        auto *frame = new tsl::elm::OverlayFrame("sys-env", title);

        ReadConfig(m_sysList, m_emuList);
        m_contents = ScanContents();

        auto *list = new tsl::elm::List();

        if (m_contents.empty()) {
            list->addItem(new tsl::elm::CategoryHeader("No titles found in /atmosphere/contents"));
            frame->setContent(list);
            return frame;
        }

        list->addItem(new tsl::elm::CategoryHeader("Blocked titles   (toggle to block on boot)"));

        std::vector<std::string> &blocked = m_emuNand ? m_emuList : m_sysList;

        for (const auto &entry : m_contents) {
            const std::string label = entry.id;

            const bool isBlocked =
                std::find(blocked.begin(), blocked.end(), entry.id) != blocked.end();

            auto *item = new tsl::elm::ToggleListItem(label, isBlocked, "Blocked", "Allowed");
            const std::string id = entry.id;
            item->setStateChangedListener([this, id](bool state) {
                this->SetBlocked(id, state);
            });
            list->addItem(item);
        }

        frame->setContent(list);
        return frame;
    }

  private:
    void SetBlocked(const std::string &id, bool blocked) {
        std::vector<std::string> &target = m_emuNand ? m_emuList : m_sysList;

        auto it = std::find(target.begin(), target.end(), id);
        if (blocked) {
            if (it == target.end()) {
                target.push_back(id);
            }
        } else {
            if (it != target.end()) {
                target.erase(it);
            }
        }

        WriteConfig(m_sysList, m_emuList);
    }

    bool m_emuNand;
    std::vector<std::string> m_sysList;
    std::vector<std::string> m_emuList;
    std::vector<ContentEntry> m_contents;
};

class GuiMain : public tsl::Gui {
  public:
    tsl::elm::Element *createUI() override {
        auto *frame = new tsl::elm::OverlayFrame("sys-env", VERSION);
        auto *list = new tsl::elm::List();

        list->addItem(new tsl::elm::CategoryHeader("Edit blocked titles per environment"));

        auto *sysItem = new tsl::elm::ListItem("SysNand");
        sysItem->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<GuiEnv>(false);
                return true;
            }
            return false;
        });
        list->addItem(sysItem);

        auto *emuItem = new tsl::elm::ListItem("EmuNand");
        emuItem->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<GuiEnv>(true);
                return true;
            }
            return false;
        });
        list->addItem(emuItem);

        frame->setContent(list);
        return frame;
    }
};

class SysEnvOverlay : public tsl::Overlay {
  public:
    std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<GuiMain>();
    }
};

int main(int argc, char **argv) {
    return tsl::loop<SysEnvOverlay>(argc, argv);
}
