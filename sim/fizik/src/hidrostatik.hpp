// Kapalı üçgen ağlarla hidrostatik (04 §2.4'ün dalgasız hâli).
//
// Düz su yüzeyinde (NED'de z = 0, aşağı pozitif) batık hacim ve batık hacmin merkezi her
// ağ için tam hesaplanır: her üçgen su düzlemiyle kırpılır (Sutherland–Hodgman) ve batık
// parça, düzlem üzerindeki bir P0 noktasıyla işaretli dörtyüzlülere ayrılır. Kapak (ağın su
// düzlemindeki kesiti) P0 ile aynı düzlemde olduğundan katkısı sıfırdır; bu yüzden kapağı
// kurmadan kapalı batık hacim elde edilir (diverjans teoremi). Düz suda bu, ağ üzerindeki
// `−p n dA` basınç integraliyle özdeştir: kaldırma kuvveti ρ g ∇_batık, uygulama noktası batık
// hacmin merkezi.
#pragma once

#include "vek.hpp"

#include <array>
#include <vector>

namespace ida::fizik::ic {

struct KapaliAg {
    std::vector<V> koseler;                      // gövde çerçevesi, O_b'ye göre
    std::vector<std::array<int, 3>> ucgenler;    // dışa bakan, saat yönünün tersi
    double hacim = 0.0;                          // tam hacim
    V merkez{};                                  // tam hacmin merkezi
    V sinir_merkezi{};                           // sınır küresi
    double sinir_yaricapi = 0.0;
};

// Dikdörtgen kutu: merkez, boyutlar (x boy, y genişlik, z yükseklik).
KapaliAg kutu_agi(const V& merkez, const V& boyut);
// Uzun sferoid: merkez, eksen (birim), yarı eksenler a (eksen boyunca), b. Ağ, hacmi
// analitik 4/3 π a b²'ye eşit olacak biçimde merkez etrafında tek düze ölçeklenir.
KapaliAg sferoid_agi(const V& merkez, const V& eksen, double a, double b, int n_boylam, int n_enlem);
// Hacim, merkez ve sınır küresini yeniden hesaplar.
void ag_ozelliklerini_hesapla(KapaliAg& ag);

struct BatikSonuc {
    double hacim = 0.0;              // m³
    V moment{};                      // Σ v · merkez (dünya), m⁴; merkez = moment / hacim
    double en_ust_derinlik = 0.0;    // ağların en üst noktasının derinliği (NED z), m; aşağıya bkz.
};

// Ağ kümesinin p_n, R pozundaki batık hacmi (düz su, z = 0). `kesinlik_derinligi`: bir ağın
// sınır küresinin en üst noktası bu derinlikten daha derindeyse ağ tam batık sayılır ve en üst
// derinliği için sınır küresinden alt sınır verilir; değilse köşelerden tam hesaplanır.
// (En üst derinlik yalnız yüzey sönümü ağırlığında kullanılır; o ağırlık bu derinliğin
// üstünde sıfırdır.)
BatikSonuc batik_hesapla(const std::vector<KapaliAg>& aglar, const V& p_n, const M& R,
                         double kesinlik_derinligi, std::vector<V>& tampon);

}  // namespace ida::fizik::ic
