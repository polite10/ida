// İDA senaryo kitaplığı (W4-senaryo, dilim 2): çözümlenmiş senaryodan fizik çekirdeğinin
// kurulum yapıları.
//
// Girdi `senaryo_coz`'un geçerli saydığı çözümlenmiş senaryodur (A5-K16); alan adları
// `ida::fizik` yapılarıyla bire bir aynıdır. A5-K32 normalleştirmesini fizik çekirdeği
// kurulumda yapar (`Dunya::kur`); yükleyici dosyadaki değeri aynen taşır.
#pragma once

#include "ida/fizik/kutle_durumu.hpp"
#include "ida/fizik/parametreler.hpp"
#include "ida/senaryo/senaryo.hpp"

#include <array>
#include <optional>
#include <string>

namespace ida::senaryo {

struct FizikKurulumu {
    fizik::AracParametreleri arac{};
    fizik::OrtamParametreleri ortam{};
    fizik::BaslangicDurumu baslangic{};
};

// Çözümlenmiş senaryodan fizik kurulum yapıları. Beklenen bir alan yoksa ya da tipi
// uymuyorsa boş döner ve `hata`'ya yolunu yazar (geçerli çözümlenmiş senaryoda olmaz).
std::optional<FizikKurulumu> fizik_kur(const DogrulanmisSenaryo& cozumlenmis, std::string* hata = nullptr);

// A5-K23 kütle durumları (yuklu = {f1, f2}); fizik çekirdeğinin türetimi kullanılır.
fizik::KutleDurumu kutle_durumu(const FizikKurulumu& k, const std::array<bool, 2>& yuklu);

// A5-K34: temas doğruluk ölçütü ω_c h ölçümleri (bir kütle durumu için). Çarpışma şekli A7
// araç varlığının `sinir_kutusu_govde_m` kutusudur.
struct TemasOlcutu {
    double tekil_azami = 0.0;     // köşe ve alt yüz merkezindeki tekil temasların en büyüğü
    double dort_kose_azami = 0.0; // dört alt köşenin eşzamanlı teması, en büyük kip
    double merkez_dusey = 0.0;    // alt yüz merkezinde düşey temas
};
TemasOlcutu temas_olcutu(const FizikKurulumu& k, const std::array<bool, 2>& yuklu,
                         const std::array<double, 3>& kutu_en_kucuk,
                         const std::array<double, 3>& kutu_en_buyuk, double adim_s = 1.25e-3);

}  // namespace ida::senaryo
