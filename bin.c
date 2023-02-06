#include <windows.h>
extern int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCCH lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar);
extern int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWCH lpWideCharStr, int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte, LPCCH lpDefaultChar, LPBOOL lpUsedDefaultChar);
#define COLLA_IMPL
#define COLLA_NO_THREADS
#define COLLA_NO_NET
#include "libs/colla/colla.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        info("bin.c lets you write a header for a binary file");
        fatal("usage: bin.c <input> <output>");
    }
    vec(uint8_t) buf = fileReadWhole(argv[1]);
    str_ostream_t ostr = ostrInit();
    ostrPuts(&ostr, "#pragma once\n\n");
    ostrPuts(&ostr, "#include <stdint.h>\n\n");
    ostrPrintf(&ostr, "#define BIN_LEN %u\n\n", vecLen(buf));
    ostrPuts(&ostr, "const uint8_t bin_data[BIN_LEN] = { \n\t");
    uint32 len = vecLen(buf);
    for (uint32 i = 0; i < len; ++i) {
        ostrPrintf(&ostr, "0x%02x, ", buf[i]);
        if ((i + 1) % 4 == 0) ostrPuts(&ostr, "  ");
        if ((i + 1) % 8 == 0) ostrPuts(&ostr, "\n\t");
    }
    if ((len + 1) % 8 != 0) ostrPuts(&ostr, "\n");
    ostrPuts(&ostr, "};\n");
    if (!fileWriteWholeText(argv[2], ostrAsView(ostr))) {
        fatal("couldn't write to file");
    }
}