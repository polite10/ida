// İDA senaryo kitaplığı (W4-sonar-sahne): A7 varlık dosyasının (GLB) yüklenmesi ve sonarın
// kullandığı ağların gövde çerçevesine çıkarılması.
//
// Sözleşme: docs/sozlesmeler/A7-sahne.md (1.0). Bu okuyucu A7'nin izin verdiği en küçük glTF alt
// kümesini okur: GLB 2 (JSON + isteğe bağlı BIN parçası), üçgen listesi (`mode` = 4), float32 VEC3
// `POSITION`, indeksli (u8/u16/u32) ya da indekssiz temel öğeler; düğüm dönüşümü TRS ya da birim
// ölçekli `matrix`. Başka her şey A7'nin hata koduyla reddedilir; sessiz atlama yoktur.
//
// Yapılan denetimler (varlık başına; §5 hata kodlarıyla): A7-K03 (`SEMA`, dosya adı), K06
// (`EKSIK_VARLIK`, `OZET_UYUSMAZ`; ayrıştırmadan önce), K10–K12 (`GLTF`, `UZANTI`), K13 (`BIRIM`),
// K14 (`EKSEN`, manifestin eksen alanı), K19 (`ERISILEMEZ`), K20–K21 (`DUGUM`), K23 (`BIRIM`),
// K24 (`TUR`; araca özgü tüp kuralı A5 ister, burada yok), K26 (`KAPALI_DEGIL`, `DISBUKEY_DEGIL`),
// K27 (akustik ağ seçimi), K28 (`MALZEME`), K56 (`TABAN`). Yapılmayanlar: K04–K05 ve şema
// (çözümleyici; A5-K115), K25 ve K52 (yüzerlik; fizik), K29 (bağlantı noktaları; A5 ile), K40–K46
// (türetilmiş parçalar; fizik). Fizik bugün bu okuyucuyu kullanmaz (A7-K50 "tek yükleme" açık konu).
//
// Çerçeve: dönen köşeler varlığın gövde çerçevesindedir (x ileri, y sancak, z aşağı; orijin `REF`):
// `p_b = M · (p_kök − t_REF)`, M = [[0, 0, 1], [−1, 0, 0], [0, −1, 0]] (A7-K14, K20).
#pragma once

#include "ida/senaryo/deger.hpp"
#include "ida/senaryo/senaryo.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ida::senaryo {

// A7 §3.6 hata kodu ve §5'in istediği bağlam (kod, küme, varlık, düğüm indeksi ve adı, ayrıntı).
struct A7Hata {
    std::string kod;          // "OZET_UYUSMAZ", "GLTF", ...
    std::string kume;
    std::string varlik;
    int dugum = -1;           // yoksa −1
    std::string dugum_adi;
    std::string ayrinti;      // beklenen/bulunan
    std::string metin() const;
};

// Bir ağ düğümünün üçgenleri (gövde çerçevesi). `ucgen_malzeme[i]`, `A7Varlik::malzemeler`
// içindeki indistir (A7-K28: üçgenin malzemesi temel öğesinin malzemesidir).
struct A7Ag {
    int dugum = -1;
    std::string ad;
    std::vector<std::array<double, 3>> koseler;
    std::vector<std::array<std::uint32_t, 3>> ucgenler;
    std::vector<std::uint16_t> ucgen_malzeme;
};

struct A7Varlik {
    std::string kimlik;
    std::string tur;                       // arac, torpido, hedef, statik
    std::vector<std::string> malzemeler;   // A7 malzeme kimlikleri (manifestin malzemeler[]'inde var)
    std::vector<A7Ag> akustik;             // A7-K27: akustik[] doluysa SON_*, boşsa COL_* kaynak ağları
    bool akustik_carpismadan = true;       // akustik[] boştu
    std::size_t carpisma_kaynak_ucgen = 0; // bütün COL_* kaynak ağlarının üçgen sayısı
    std::size_t akustik_ucgen = 0;         // `akustik`'teki üçgen sayısı
};

// `manifest`: `assets/<kume>/manifest.json`'un ayrıştırılmış ağacı (çağıran baytları okumuş ve
// A5-K115 özetini denetlemiştir). `kimlik` manifestte yoksa `SEMA`. Varlık dosyası
// `assets/<kume>/<dosya>` yolundan `kaynak` ile okunur (A7-K03).
std::optional<A7Varlik> a7_varlik_yukle(const Deger& manifest, std::string_view kume, std::string_view kimlik,
                                        const DosyaKaynagi& kaynak, A7Hata& hata);

}  // namespace ida::senaryo
