// rastgele.cpp — bkz. rastgele.hpp.
#include "rastgele.hpp"

#include "ozet.hpp"

#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#else
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/random.h>
#endif
#endif

namespace ida::a3 {

bool kripto_rastgele(std::uint8_t* hedef, std::size_t n)
{
#if defined(_WIN32)
    while (n > 0) {
        const ULONG parca = n > 0x10000u ? 0x10000u : static_cast<ULONG>(n);
        if (BCryptGenRandom(nullptr, hedef, parca, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
            return false;
        }
        hedef += parca;
        n -= parca;
    }
    return true;
#else
    while (n > 0) {
        const std::size_t parca = n > 256u ? 256u : n;  // getentropy en çok 256 bayt
        if (getentropy(hedef, parca) != 0) {
            return false;
        }
        hedef += parca;
        n -= parca;
    }
    return true;
#endif
}

std::string rastgele_onaltilik(std::size_t bayt)
{
    std::vector<std::uint8_t> b(bayt);
    if (!kripto_rastgele(b.data(), b.size())) {
        return {};
    }
    return ozet::onaltilik(b.data(), b.size());
}

}  // namespace ida::a3
