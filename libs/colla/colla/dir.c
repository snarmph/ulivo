#include "dir.h"
#include "tracelog.h"

#ifdef _WIN32
#include "win32_slim.h"
#include <stdlib.h>
#include <assert.h>

#include "strstream.h"

typedef struct {
    dir_entry_t cur;
    dir_entry_t next;
    HANDLE handle;
} _dir_internal_t;

static dir_entry_t _fillDirEntry(WIN32_FIND_DATAW *data) {
    return (dir_entry_t) {
        .type = 
            data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? 
            FS_TYPE_DIR : FS_TYPE_FILE,
        .name = strFromWCHAR(data->cFileName, 0)
    };
}

static _dir_internal_t _getFirst(const wchar_t *path) {
    _dir_internal_t res = {0};
    WIN32_FIND_DATAW data = {0};

    res.handle = FindFirstFileW(path, &data);

    if(res.handle != INVALID_HANDLE_VALUE) {
        res.next = _fillDirEntry(&data);
    }

    return res;
}

static void _getNext(_dir_internal_t *ctx) {
    WIN32_FIND_DATAW data = {0};

    BOOL result = FindNextFileW(ctx->handle, &data);
    if(!result) {
        if(GetLastError() == ERROR_NO_MORE_FILES) {
            FindClose(ctx->handle);
            ctx->handle = NULL;
            return;
        }
    }
    ctx->next = _fillDirEntry(&data);
}

dir_t dirOpen(const char *path) {
    DWORD n = GetFullPathName(path, 0, NULL, NULL);
    str_ostream_t out = ostrInitLen(n + 3);
    n = GetFullPathName(path, n, out.buf, NULL);
    assert(n > 0);
    out.len += n;
    switch(ostrBack(out)) {
    case '\\':
    case '/':
    case ':':
        // directory ends in path separator
        // NOP
        break;
    default:
        ostrPutc(&out, '\\');
    }
    ostrPutc(&out, '*');

    _dir_internal_t *dir = malloc(sizeof(_dir_internal_t));
    if(dir) {
        wchar_t *wpath = strToWCHAR(ostrAsStr(out));
        assert(wpath);
        *dir = _getFirst(wpath);
        free(wpath);
    }
    ostrFree(out);

    return dir;
}

void dirClose(dir_t ctx) {
    free(ctx);
}

bool dirValid(dir_t ctx) {
    _dir_internal_t *dir = (_dir_internal_t*)ctx;
    return dir->handle != INVALID_HANDLE_VALUE;
}

dir_entry_t *dirNext(dir_t ctx) {
    _dir_internal_t *dir = (_dir_internal_t*)ctx;
    strFree(dir->cur.name);
    if(!dir->handle) return NULL;
    dir->cur = dir->next;
    _getNext(dir);
    return &dir->cur;
}

void dirCreate(const char *path) {
    CreateDirectoryA(path, NULL);
}

#else

#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

// taken from https://sites.uclouvain.be/SystInfo/usr/include/dirent.h.html
// hopefully shouldn't be needed
#ifndef DT_DIR
    #define DT_DIR 4
#endif
#ifndef DT_REG
    #define DT_REG 8
#endif

typedef struct {
    DIR *dir;
    dir_entry_t next;
} _dir_internal_t;

dir_t dirOpen(const char *path) {
    _dir_internal_t *ctx = (_dir_internal_t *)calloc(1, sizeof(_dir_internal_t));
    if(ctx) ctx->dir = opendir(path);
    return ctx;
}

void dirClose(dir_t ctx) {
    if(ctx) {
        _dir_internal_t *in = (_dir_internal_t *)ctx;
        closedir(in->dir);
        free(in);
    }
}

bool dirValid(dir_t ctx) {
    _dir_internal_t *dir = (_dir_internal_t*)ctx;
    return dir->dir != NULL;
}

dir_entry_t *dirNext(dir_t ctx) {
    if(!ctx) return NULL;
    _dir_internal_t *in = (_dir_internal_t *)ctx;
    strFree(in->next.name);
    struct dirent *dp = readdir(in->dir);
    if(!dp) return NULL;
    
    switch(dp->d_type) {
    case DT_DIR: in->next.type = FS_TYPE_DIR; break;
    case DT_REG: in->next.type = FS_TYPE_FILE; break;
    default: in->next.type = FS_TYPE_UNKNOWN; break;
    }

    in->next.name = strFromStr(dp->d_name);
    return &in->next;
}

void dirCreate(const char *path) {
    mkdir(path, 0700);
}

#endif

#include <stdio.h>

bool dirRemove(const char *path) {
    dir_t dir = dirOpen(path);
    if (!dirValid(dir)) return false;
    dir_entry_t *it = NULL;
    while((it = dirNext(dir))) {
        if (it->type == FS_TYPE_FILE) {
            str_t file_path = strFromFmt("%s/%s", path, it->name.buf);
            if (remove(file_path.buf)) {
                err("couldn't remove %s > %s", file_path.buf, strerror(errno));
            }
            strFree(file_path);
        }
        else if (it->type == FS_TYPE_DIR) {
            if (strcmp(it->name.buf, ".") == 0 || strcmp(it->name.buf, "..") == 0) {
                continue;
            }
            str_t new_path = strFromFmt("%s/%s", path, it->name.buf);
            info("new path: %s--%s -> %s", path, it->name.buf, new_path.buf);
            if (!dirRemove(new_path.buf)) {
                err("couldn't delete folder %s", new_path.buf);
                break;
            }
            strFree(new_path);
        }
        else {
            err("%d -> %s", it->type, it->name.buf);
        }
    }
    dirClose(dir);
#ifdef _WIN32
    return RemoveDirectoryA(path);
#else
    return rmdir(path) == 0;
#endif
}
