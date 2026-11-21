// main.cpp — ida_sil: kartsız koşucu programı (BENIOKU.md).
#include <cstdio>

#include "ida/port.h"
#include "ida/sil/kosucu.hpp"
#include "komut_satiri.hpp"

#ifndef IDA_SIL_GIT
#define IDA_SIL_GIT 0u
#endif

int main(int argc, char** argv)
{
    using namespace ida::sil;
    const KomutSatiri k = komut_satiri_ayristir(argc, argv);
    if (!k.hata.empty()) {
        std::fprintf(stderr, "ida_sil: %s\n\n%s", k.hata.c_str(), kullanim_metni());
        return static_cast<int>(Cikis::Kullanim);
    }
    if (k.yardim) {
        std::fputs(kullanim_metni(), stdout);
        return static_cast<int>(Cikis::Tamam);
    }
    if (k.surum) {
        ida_core_info_t bilgi{};
        ida_core_info(&bilgi);
        std::printf("ida_sil %s (git %08x), A4 port %u.%u, çekirdek sürümü %u, çekirdek %u B\n",
                    kSurum, static_cast<unsigned>(IDA_SIL_GIT),
                    static_cast<unsigned>(bilgi.port_major), static_cast<unsigned>(bilgi.port_minor),
                    static_cast<unsigned>(bilgi.core_version), static_cast<unsigned>(bilgi.core_size));
        return static_cast<int>(Cikis::Tamam);
    }
    Kosucu kosucu(k.ayar);
    const Cikis c = kosucu.calistir();
    if (c != Cikis::Tamam) {
        std::fprintf(stderr, "ida_sil: çıkış %d: %s\n", static_cast<int>(c), cikis_aciklamasi(c));
    }
    return static_cast<int>(c);
}
