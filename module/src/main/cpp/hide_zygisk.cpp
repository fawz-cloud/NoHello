// Isolated translation unit: ELFIO's ET_*/EM_* constexpr constants collide with
// bionic <elf.h>'s macros of the same name, so this file deliberately avoids the
// headers (zygisk/fd_utils/ptrace) that transitively pull <elf.h> in.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include <elfio/elfio.hpp>
#include "log.h"

// Zygisk injection trips a `had_error` flag inside libnativebridge.so's .bss;
// detectors read it as a "Zygote is injected" signal. Locate the mapped ELF,
// resolve .bss, and clear the flag byte. Ported from
// snake-4/Zygisk-Assistant (doHideZygisk).
void doHideZygisk() {
    std::string filePath;
    uintptr_t startAddress = 0;

    std::ifstream maps("/proc/self/maps");
    for (std::string line; std::getline(maps, line);) {
        if (line.find("/libnativebridge.so") == std::string::npos)
            continue;
        unsigned long s = 0, e = 0;
        char perms[8] = {0}, path[512] = {0};
        if (sscanf(line.c_str(), "%lx-%lx %7s %*x %*x:%*x %*u %511s", &s, &e, perms, path) != 4)
            continue;
        std::string p(path);
        // First read-only page holds the ELF header.
        if (strcmp(perms, "r--p") == 0 && p.size() >= 19 &&
            p.compare(p.size() - 19, 19, "/libnativebridge.so") == 0) {
            filePath = p;
            startAddress = static_cast<uintptr_t>(s);
            break;
        }
    }

    if (startAddress == 0)
        return;

    ELFIO::elfio reader;
    if (!reader.load(filePath))
        return;

    uintptr_t bssAddress = 0;
    size_t bssSize = 0;
    for (const auto &sec : reader.sections) {
        if (sec->get_name() == ".bss") {
            bssAddress = startAddress + static_cast<uintptr_t>(sec->get_address());
            bssSize = static_cast<size_t>(sec->get_size());
            break;
        }
    }

    if (bssAddress == 0)
        return;

    auto *pHadError = reinterpret_cast<uint8_t *>(
        memchr(reinterpret_cast<void *>(bssAddress), 0x01, bssSize));
    if (pHadError != nullptr) {
        *pHadError = 0;
        LOGD("#[zygisk::doHideZygisk] libnativebridge.so had_error reset");
    }
}
