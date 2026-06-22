#include <switch.h>
#include <string>
#include "fs.hpp"

#define INNER_HEAP_SIZE 0x80000

extern "C" {
    u32 __nx_applet_type = AppletType_None;
    u32 __nx_fs_num_sessions = 1;

    size_t nx_inner_heap_size = INNER_HEAP_SIZE;
    char nx_inner_heap[INNER_HEAP_SIZE];

    void __libnx_initheap(void) {
        void *addr = nx_inner_heap;
        size_t size = nx_inner_heap_size;

        extern char *fake_heap_start;
        extern char *fake_heap_end;

        fake_heap_start = (char *) addr;
        fake_heap_end = (char *) addr + size;
    }

    void __appInit(void) {
        if (R_FAILED(smInitialize())) {
            fatalThrow(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));
        }

        Result rc = setsysInitialize();
        if (R_SUCCEEDED(rc)) {
            SetSysFirmwareVersion fw;
            rc = setsysGetFirmwareVersion(&fw);
            if (R_SUCCEEDED(rc))
                hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
            setsysExit();
        }

        rc = fsInitialize();
        if (R_FAILED(rc)) {
            fatalThrow(MAKERESULT(Module_Libnx, LibnxError_InitFail_FS));
        }

        fsdevMountSdmc();
    }

    void __appExit(void) {
        smExit();
        fsExit();
    }
}

bool IsEmuNand() {
    splInitialize();
    u64 out;
    splGetConfig((SplConfigItem)65007, &out);
    splExit();
    return (out != 0);
}

int main() {
    std::vector<std::string> entries;
    bool isEmunand = IsEmuNand();

    Result rc = fs::ParseConfig(entries, isEmunand);
    if (R_FAILED(rc)) {
        fs::Log("Result failed: %u", R_DESCRIPTION(rc));
        return rc;
    }

    std::string env, del;
    if (isEmunand) {
        env = ".emu.bak";
        del = ".sys.bak";
    } else {
        env = ".sys.bak";
        del = ".emu.bak";
    }

    fs::EditContent(entries, env, del);

    return 0;
}
