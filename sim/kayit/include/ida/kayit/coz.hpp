// ida/kayit/coz.hpp — tablo güdümlü yük çözücü (A6-K08, K48).
//
// İleti yükünü kataloğun alan tablosuna göre (elle yazılmış alan sırası olmadan) çözer. Okuyucu
// `kayit/eksik` kayıtlarını, `ida_kayit_dok` ve testler ileti değerlerini bununla okur. Üretilen
// kodlayıcılardan bağımsız bir gerçekleştirimdir: gidiş-dönüş testlerinde kodlayıcıyı sınar.
//
// Sürüm kuralı (A6-K48; A3-K19 ile aynı): bildiğinden büyük `kanal_surumu`'lu iletide sondaki fazla
// baytlar yok sayılır; küçük `kanal_surumu`'lu iletide sondaki eksik alanlar "yok" sayılır. Eksiklik
// yalnız üst düzey bir alan sınırında kabul edilir; alanın ortasında biten yük bozuktur.
#pragma once

#include "ida/a3/katalog.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace ida::kayit {

struct CozmeSonucu {
    bool tamam = false;          // yük yapısal olarak çözüldü
    std::size_t eksik_alan = 0;  // sonda bulunmayan üst düzey alan sayısı (eski `kanal_surumu`)
    std::size_t fazla_bayt = 0;  // sonda yok sayılan bayt (yeni `kanal_surumu`)
};

// Yükü A3'ün `json_dok` biçiminde döker (32 bit ve altı tamsayılar sayı, 64 bit tamsayılar onluk
// dize, "yok" değeri ve bulunmayan alan null, sonlu olmayan kayan nokta dize). Bozuksa `cikti`
// tanımsızdır.
CozmeSonucu json_dok(const a3::YukTanimi& yuk, std::span<const std::uint8_t> veri, std::string& cikti);

// Üst düzey bir alanın değeri (sabit dizi ve `dizi` alanları ziyaret edilmez).
struct AlanDegeri {
    enum class Tur : std::uint8_t { isaretsiz, isaretli, kayan, metin, baytlar };
    Tur tur = Tur::isaretsiz;
    std::uint64_t isaretsiz = 0;
    std::int64_t isaretli = 0;
    double kayan = 0;
    std::string_view metin;                 // kaynağı gösterir
    std::span<const std::uint8_t> baytlar;  // kaynağı gösterir
};
using AlanZiyaretcisi = std::function<void(const a3::AlanTanimi& alan, const AlanDegeri& deger)>;

// Yükün üst düzey tekil alanlarını sırayla ziyaret eder (bulunmayan sondaki alanlar ziyaret edilmez).
CozmeSonucu alanlari_gez(const a3::YukTanimi& yuk, std::span<const std::uint8_t> veri, const AlanZiyaretcisi& z);

}  // namespace ida::kayit
