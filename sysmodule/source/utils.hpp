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

#pragma once

#include <switch.h>

constexpr u32 SysEnvModule = 0x42A;
constexpr SplConfigItem IS_EMUMMC_CONFIG = (SplConfigItem)65007;

enum SysEnvResult {
    SysEnvResult_EmptyConfig = 0,
    SysEnvResult_ConfigNotFound,
    SysEnvResult_HeaderMissing,
    SysEnvResult_CreateDirectoryFailed,
    SysEnvResult_CreateFileFailed,
    SysEnvResult_OpenContentsFailed,
    SysEnvResult_RenameFailed,
};

#define SYSENV_RC(x) MAKERESULT(SysEnvModule, x)

#define R_RETURN(rc) \
    do {             \
        return (rc); \
    } while (0)

#define R_SUCCEED()  \
    do {             \
        return 0;    \
    } while (0)

#define R_TRY(rc)            \
    do {                     \
        Result _r = (rc);    \
        if (R_FAILED(_r)) {  \
            return _r;       \
        }                    \
    } while (0)

#define R_UNLESS(cond, rc) \
    do {                   \
        if (!(cond)) {     \
            return (rc);   \
        }                  \
    } while (0)

#define R_SUCCEED_IF(cond) \
    do {                   \
        if ((cond)) {      \
            return 0;      \
        }                  \
    } while (0)

#define R_THROW(rc) \
    do {            \
        return (rc);\
    } while (0)
