/*
 * Copyright (c) Lightos_
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <switch.h>
#include <string>
#include "fs.hpp"
#include "utils.hpp"

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
    splGetConfig(IS_EMUMMC_CONFIG, &out);
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
