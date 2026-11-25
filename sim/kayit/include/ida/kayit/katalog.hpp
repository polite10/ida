// ida/kayit/katalog.hpp — A6 kanal ve üst veri kataloğunun derlenmiş tabloları (W5-kayit).
//
// Tablolar ve gömülü baytlar derleme zamanında `docs/sozlesmeler/sema/a6-kanallar.json`'dan
// üretilir (`sim/kayit/arac/uret_kayit.py`); bu başlık yalnız tiplerini bildirir. Yük alan
// tabloları A3'ün tablo tipleriyle (`ida::a3::AlanTanimi`, `YukTanimi`) verilir (W5-K36).
//
// W5-K37: `kamera/kare` kanalı ve `yuk_kamera_kare` bu dilimde tablolarda yoktur.
#pragma once

#include "ida/a3/katalog.hpp"

#include <cstdint>
#include <span>
#include <string_view>

namespace ida::kayit {

// A6 §2: kayıt kademesi. Sıra anlamlıdır: bir kanal kendi kademesinde ve üstündekilerde yazılır (A6-K16).
enum class Kademe : std::uint8_t { olaylar = 0, telemetri = 1, tam = 2 };

// Katalogdaki `kosul`: kanalın yazıldığı ek koşul.
enum class KanalKosulu : std::uint8_t {
    yok,
    cekirdek_pc_kipleri,  // yalnız `sil`, `tekrar_girdi_sil`, `tekrar_yeniden_kurma` kiplerinde
};

// Katalogdaki `tekrar_duzeyi` (Tablo 3.3 "Tekrar" sütunu), bit maskesi.
namespace tekrar {
inline constexpr std::uint8_t izleme = 1u << 0;
inline constexpr std::uint8_t fizik = 1u << 1;
inline constexpr std::uint8_t girdi = 1u << 2;
inline constexpr std::uint8_t yeniden_kurma = 1u << 3;
}  // namespace tekrar

// MCAP Schema kaydı: her (yük, kanal_surumu) için bir tane (A6 §3.2).
struct SemaTanimi {
    std::string_view ad;          // "ida.<yük adı>.v<kanal_surumu>"
    const a3::YukTanimi* yuk;
    std::uint16_t kanal_surumu;
    std::string_view veri;        // yükün alan listesinin kanonik JSON metni (A6-K07)
};

struct KanalTanimi {
    std::string_view ad;            // MCAP `topic`
    std::uint16_t kanal_surumu;
    const a3::YukTanimi* yuk;
    std::uint16_t sema;             // semalar() içindeki sıra
    Kademe kademe;                  // yazıldığı en düşük kademe
    std::string_view kademe_adi;    // katalogdaki metin (kanal `metadata`'sı)
    std::string_view zaman_anlami;  // `log_time` anlamı (A3-K26)
    bool a3_ortak;
    std::uint8_t tekrar_duzeyleri;  // tekrar:: bitleri
    KanalKosulu kosul;
    std::uint16_t indeks;           // kanallar() içindeki sıra (= Kanal değeri)
};

// `AlanTanimi::yok` kodları A3'tekilerdir (ida/a3/katalog.hpp: 0…7); A6'ya özgü yüklerde ek olarak
// 8 = 0xFFFF (u16) vardır.
inline constexpr std::uint8_t kYokKodu0xFFFF = 8;

enum class UstVeriBicimi : std::uint8_t { serbest, onluk_u32 };

// Üst veri kaydının bir anahtarı (`x_ida_ust_veri` + `$defs/ust_veri_*`).
struct UstVeriAnahtari {
    std::string_view ad;
    bool zorunlu;
    UstVeriBicimi bicim;
    std::uint8_t en_az_uzunluk;  // şemadaki `minLength`; yoksa 0
};

struct EkTanimi {
    std::string_view ad;
    std::string_view ortam_turu;
    bool zorunlu;
};

// Üst veri kayıtlarının adları (A6 §3.5).
inline constexpr std::string_view kUstVeriAcilis = "ida_kayit";
inline constexpr std::string_view kUstVeriGuncelleme = "ida_kayit_guncelleme";
inline constexpr std::string_view kUstVeriKapanis = "ida_kayit_kapanis";
// Yazan sürümün kataloğunu taşıyan ek (A6-K50); yazıcı kendisi ekler.
inline constexpr std::string_view kKatalogEki = "a6_kanallar.json";

std::span<const std::uint8_t> katalog_baytlari() noexcept;  // a6-kanallar.json, değiştirilmeden
std::span<const a3::YukTanimi> yukler() noexcept;
std::span<const SemaTanimi> semalar() noexcept;
std::span<const KanalTanimi> kanallar() noexcept;
std::span<const UstVeriAnahtari> acilis_anahtarlari() noexcept;
std::span<const UstVeriAnahtari> guncelleme_anahtarlari() noexcept;
std::span<const UstVeriAnahtari> kapanis_anahtarlari() noexcept;
std::span<const EkTanimi> ekler() noexcept;

// Yardımcılar (katalog.cpp).
const KanalTanimi* kanal_bul(std::string_view ad) noexcept;
std::string_view kademe_adi(Kademe k) noexcept;
bool kademe_coz(std::string_view ad, Kademe& k) noexcept;
// A6 §3.5 `kip` değerleri; çekirdeği PC'de çağıran kipler (`cekirdek/*` kanallarının koşulu).
bool kip_gecerli(std::string_view kip) noexcept;
bool kip_cekirdek_pc(std::string_view kip) noexcept;
// Kanal bu kademe ve kipte yazılır mı (A6-K16 ve katalogdaki koşul).
bool kanal_etkin(const KanalTanimi& k, Kademe kademe, std::string_view kip) noexcept;

}  // namespace ida::kayit
