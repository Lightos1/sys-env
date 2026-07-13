#define TESLA_INIT_IMPL
#include <exception_wrap.hpp>
#include <tesla.hpp>

#include <cJSON.h>

#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

namespace {

    constexpr const char* const CONFIG_DIR    = "/config/sys-env";
    constexpr const char* const CONFIG_PATH   = "/config/sys-env/config.ini";
    constexpr const char* const CONTENTS_PATH = "/atmosphere/contents";

    constexpr const char* const HEADER_SYS = "[SysNand]";
    constexpr const char* const HEADER_EMU = "[EmuNand]";

    constexpr u64 TESLA_PROGRAM_ID = 0x420000000007E51AULL;
    constexpr u64 SELF_PROGRAM_ID  = 0x420000000000042AULL;

    struct ContentEntry {
        std::string id;
        std::string name;
    };

    void rstrip(std::string& line) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
    }

    void readConfig(std::vector<std::string>& sysList, std::vector<std::string>& emuList) {
        sysList.clear();
        emuList.clear();

        std::ifstream file(CONFIG_PATH);
        if (!file.is_open())
            return;

        std::vector<std::string>* current = nullptr;
        std::string line;
        while (std::getline(file, line)) {
            rstrip(line);
            if (line.empty())
                continue;

            if (line[0] == '[') {
                if (line == HEADER_SYS)
                    current = &sysList;
                else if (line == HEADER_EMU)
                    current = &emuList;
                else
                    current = nullptr;
                continue;
            }

            if (current)
                current->push_back(line);
        }
    }

    bool writeConfig(const std::vector<std::string>& sysList, const std::vector<std::string>& emuList) {
        mkdir("/config", 0777);
        mkdir(CONFIG_DIR, 0777);

        std::ofstream file(CONFIG_PATH, std::ios::trunc);
        if (!file.is_open())
            return false;

        file << HEADER_SYS << "\n";
        for (const auto& id : sysList)
            file << id << "\n";
        file << "\n";
        file << HEADER_EMU << "\n";
        for (const auto& id : emuList)
            file << id << "\n";

        return true;
    }

    std::string readToolboxName(const std::string& id) {
        std::string path = std::string(CONTENTS_PATH) + "/" + id + "/toolbox.json";
        FILE* fp = std::fopen(path.c_str(), "rb");
        if (!fp)
            return "";

        std::fseek(fp, 0, SEEK_END);
        const long size = std::ftell(fp);
        if (size <= 0 || size > 8192) {
            std::fclose(fp);
            return "";
        }
        std::fseek(fp, 0, SEEK_SET);

        std::string buf(static_cast<size_t>(size), '\0');
        const size_t read = std::fread(buf.data(), 1, size, fp);
        std::fclose(fp);
        if (read != static_cast<size_t>(size))
            return "";

        std::string name;
        cJSON* json = cJSON_ParseWithLength(buf.data(), size);
        if (json) {
            cJSON* nameItem = cJSON_GetObjectItem(json, "name");
            if (nameItem && cJSON_IsString(nameItem) && nameItem->valuestring)
                name = nameItem->valuestring;
            cJSON_Delete(json);
        }
        return name;
    }

    std::vector<ContentEntry> scanContents() {
        std::vector<ContentEntry> entries;

        DIR* dir = opendir(CONTENTS_PATH);
        if (!dir)
            return entries;

        struct dirent* d;
        while ((d = readdir(dir)) != nullptr) {
            if (d->d_name[0] == '.')
                continue;
            if (d->d_type != DT_DIR)
                continue;

            const std::string id = d->d_name;
            if (id.length() != 16)
                continue;

            const u64 pid = std::strtoull(id.c_str(), nullptr, 16);
            if (pid == TESLA_PROGRAM_ID || pid == SELF_PROGRAM_ID)
                continue;

            entries.push_back({ id, readToolboxName(id) });
        }
        closedir(dir);

        std::sort(entries.begin(), entries.end(), [](const ContentEntry& a, const ContentEntry& b) {
            if (a.name.empty() != b.name.empty())
                return !a.name.empty();
            if (!a.name.empty())
                return a.name < b.name;
            return a.id < b.id;
        });

        return entries;
    }

} // namespace

class GuiEnv : public tsl::Gui {
  public:
    explicit GuiEnv(bool emuNand) : m_emuNand(emuNand) {}

    tsl::elm::Element* createUI() override {
        const char* title = m_emuNand ? "EmuNand" : "SysNand";
        auto* frame = new tsl::elm::OverlayFrame("sys-env", title);

        readConfig(m_sysList, m_emuList);
        m_contents = scanContents();

        auto* list = new tsl::elm::List();

        if (m_contents.empty()) {
            list->addItem(new tsl::elm::CategoryHeader("No titles found in /atmosphere/contents"));
            frame->setContent(list);
            return frame;
        }

        list->addItem(new tsl::elm::CategoryHeader("Blocked titles   (toggle to block on boot)"));

        std::vector<std::string>& blocked = m_emuNand ? m_emuList : m_sysList;

        for (const auto& entry : m_contents) {
            const std::string label = entry.name.empty()
                ? entry.id
                : entry.name + ult::DIVIDER_SYMBOL + entry.id;

            const bool isBlocked =
                std::find(blocked.begin(), blocked.end(), entry.id) != blocked.end();

            auto* item = new tsl::elm::ToggleListItem(label, isBlocked, "Blocked", "Allowed");
            const std::string id = entry.id;
            item->setStateChangedListener([this, id](bool state) {
                this->setBlocked(id, state);
            });
            list->addItem(item);
        }

        frame->setContent(list);
        return frame;
    }

  private:
    void setBlocked(const std::string& id, bool blocked) {
        std::vector<std::string>& target = m_emuNand ? m_emuList : m_sysList;

        auto it = std::find(target.begin(), target.end(), id);
        if (blocked) {
            if (it == target.end())
                target.push_back(id);
        } else {
            if (it != target.end())
                target.erase(it);
        }

        writeConfig(m_sysList, m_emuList);
    }

    bool m_emuNand;
    std::vector<std::string> m_sysList;
    std::vector<std::string> m_emuList;
    std::vector<ContentEntry> m_contents;
};

class GuiMain : public tsl::Gui {
  public:
    tsl::elm::Element* createUI() override {
        auto* frame = new tsl::elm::OverlayFrame("sys-env", VERSION);
        auto* list = new tsl::elm::List();

        list->addItem(new tsl::elm::CategoryHeader("Edit blocked titles per environment"));

        auto* sysItem = new tsl::elm::ListItem("SysNand");
        sysItem->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<GuiEnv>(false);
                return true;
            }
            return false;
        });
        list->addItem(sysItem);

        auto* emuItem = new tsl::elm::ListItem("EmuNand");
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

int main(int argc, char** argv) {
    return tsl::loop<SysEnvOverlay>(argc, argv);
}
