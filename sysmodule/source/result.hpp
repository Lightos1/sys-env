#pragma once

#include <switch.h>

constexpr u32 SysEnvModule = 0x42A;

enum SysEnvResult {
    SysEnvResult_EmptyConfig = 1,
    SysEnvResult_ConfigNotFound = 0,
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
