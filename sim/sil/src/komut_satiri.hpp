// komut_satiri.hpp — ida_sil komut satırı (BENIOKU.md "Komut satırı").
#pragma once

#include <string>

#include "ida/sil/kosucu.hpp"

namespace ida::sil {

struct KomutSatiri {
    Ayarlar ayar;
    bool surum = false;   // --surum
    bool yardim = false;  // --yardim, -h
    std::string hata;     // boş değilse kullanım hatası
};

KomutSatiri komut_satiri_ayristir(int argc, const char* const* argv);

// --yardim metni.
const char* kullanim_metni();

}  // namespace ida::sil
