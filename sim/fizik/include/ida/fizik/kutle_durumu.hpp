// İDA fizik çekirdeği: türetilmiş kütle durumları (A5-K23, A5-K24; iskelet §5).
#pragma once

#include "ida/fizik/parametreler.hpp"

#include <array>

namespace ida::fizik {

// Bir kütle durumunun O_b'ye göre büyüklükleri. Torpido tüpten ayrılana kadar aracın bağlı
// kütlesidir; aracın eklenmiş kütlesi ve sönümü yük durumundan bağımsızdır (iskelet §5).
struct KutleDurumu {
    std::array<bool, kFirlaticiSayisi> yuklu{};  // f1, f2
    double kutle = 0.0;     // kg
    Vek3 r_g{};             // m
    double hacim = 0.0;     // m³ (tam batık deplasman ∇)
    Vek3 r_b{};             // m (tam batık yüzerlik merkezi)
    Mat3 atalet{};          // kg·m², O_b'ye göre
};

// A5-K23 sırasıyla türetir: (1) boş araç; (2) trim kütlesi (A5-K24); (3) her dolu tüp için
// torpido (tüp konumunda nokta kütle + düzgün yoğunluklu sferoid ataleti, paralel eksen;
// hacmi 4/3 π a b² yüzerlik merkezine katkısıyla; torpidonun kendi r_g ofseti ihmal).
KutleDurumu kutle_durumu_turet(const AracParametreleri& arac,
                               const std::array<bool, kFirlaticiSayisi>& yuklu);

}  // namespace ida::fizik
