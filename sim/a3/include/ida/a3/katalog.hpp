// ida/a3/katalog.hpp — A3 kanal kataloğunun derlenmiş tabloları (W4-a3).
//
// Tablolar ve gömülü baytlar derleme zamanında `docs/sozlesmeler/sema/a3-kanallar.json`'dan
// üretilir (`sim/a3/arac/uret_katalog.py`); bu başlık yalnız tiplerini bildirir. Sunucu kataloğu
// bayt bayt gömer (A3-K16) ve `GET /api/v1/katalog` ile aynı baytları döndürür.
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

namespace ida::a3 {

// A3 §3.1 tel tipleri.
enum class Tip : std::uint8_t { u8, u16, u32, u64, i32, i64, f32, f64, metin, bayt_dizisi, dizi };

struct DegerAdi {
    std::uint32_t kod;
    std::string_view ad;
};

struct AlanTanimi {
    std::string_view ad;
    Tip tip;
    std::uint16_t adet;      // 0: tekil alan; >0: `adet` öğeli sabit dizi (A3 §3.1 `T[n]`)
    Tip sayac;               // metin, bayt_dizisi, dizi: uzunluk/öğe sayacı tipi
    std::uint32_t azami;     // metin, bayt_dizisi, dizi: üst sınır
    const AlanTanimi* oge;   // dizi: öğe alanları
    std::uint16_t oge_sayisi;
    const DegerAdi* degerler;  // katalogdaki `degerler` (kod → ad); yoksa nullptr
    std::uint16_t deger_sayisi;
    // Katalogdaki `yok_degeri`: 0 tanımsız, 1 NaN, 2 INT64_MIN, 3 2^64-1, 4 0xFFFFFFFF, 5 255, 6 -1, 7 0
    std::uint8_t yok;
};

struct YukTanimi {
    std::string_view ad;
    const AlanTanimi* alanlar;
    std::uint16_t alan_sayisi;
};

// A3 §2: kanal sınıfı (düşürme kuralı).
enum class Sinif : std::uint8_t { durum, dizi, olay };

struct KanalTanimi {
    std::string_view ad;
    std::uint16_t kanal_id;
    std::uint16_t kanal_surumu;
    const YukTanimi* yuk;
    Sinif sinif;
    std::uint8_t azami_hz;      // yalnız durum sınıfı; diğerlerinde 0
    std::uint8_t varsayilan_hz; // yalnız durum sınıfı; diğerlerinde 0
    std::uint32_t kuyruk_siniri;// dizi ve olay sınıfı; durumda 0
    bool zorunlu;               // A3-K29
    bool yer_gercegi;           // A3-K28 bit3
    std::string_view anahtar[2];
    std::uint8_t anahtar_sayisi;
    std::string_view zaman_anlami;
    std::uint16_t indeks;       // kanallar() içindeki sıra
};

struct OlayTuru {
    std::uint16_t kod;
    std::string_view ad;
    std::string_view varsayilan_onem;
    std::string_view ayrinti_semasi;  // $defs içindeki ad
};

struct HataKodu {
    std::string_view kod;
    std::uint16_t http;
};

struct UcTanimi {
    std::string_view yontem;
    std::string_view yol;          // `{ad}` parçaları değişkendir
    std::string_view istek_semasi; // $defs içindeki ad; gövdesiz uçta boş
    bool asenkron;
    bool durum_degistirir;
};

std::span<const std::uint8_t> katalog_baytlari() noexcept;        // a3-kanallar.json, değiştirilmeden
std::span<const std::uint8_t> komut_katalogu_baytlari() noexcept; // protocol/komutlar.json, değiştirilmeden
std::span<const YukTanimi> yukler() noexcept;
std::span<const KanalTanimi> kanallar() noexcept;
std::span<const OlayTuru> olay_turleri() noexcept;
std::span<const HataKodu> hata_kodlari() noexcept;
std::span<const UcTanimi> uclar() noexcept;
std::span<const std::pair<std::uint32_t, std::string_view>> gecerlilik_nedenleri() noexcept;

// Yardımcılar (katalog.cpp).
const KanalTanimi* kanal_bul(std::uint16_t kanal_id) noexcept;
const KanalTanimi* kanal_bul(std::string_view ad) noexcept;
const OlayTuru* olay_turu_bul(std::uint16_t kod) noexcept;
// Katalogdaki HTTP durumu; bilinmeyen kodda 0.
std::uint16_t hata_http(std::string_view kod) noexcept;
// Yükün alanındaki `degerler` tablosunda kodun adı; yoksa boş.
std::string_view deger_adi(std::string_view yuk, std::string_view alan, std::uint32_t kod) noexcept;
// Ada göre kod; yoksa false.
bool deger_kodu(std::string_view yuk, std::string_view alan, std::string_view ad, std::uint32_t& kod) noexcept;
const YukTanimi* yuk_bul(std::string_view ad) noexcept;

}  // namespace ida::a3
