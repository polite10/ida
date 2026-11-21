// Sensör arıza zinciri arayüzü (05 §2.7, §3 IFaultControl; A5 Tablo 3.10-A, A5-K64–K70).
//
// Alan adları A5 `olaylar[i]` anahtarlarıyla aynıdır. Arıza, ölçüm zincirinin temel kaydının
// kopyası üzerinde çalışır; temel ölçüm dizisi arıza açık ya da kapalı aynıdır.
#pragma once

#include <cstdint>
#include <string>

namespace ida::sensor {

// Sıra 05 §2.7 / A5 bileşim sırasıdır. Bu dilimde gövdesi olanlar: ofset … gec_teslim.
// Durumlu üçü (motor_takilmasi, dvl_isin_kaybi, guc_cevrimi) arayüzde yer tutar: arm reddeder.
enum class ArizaTuru : std::uint8_t {
    ofset = 0,
    olcek = 1,
    suruklenme_dogrusal = 2,
    suruklenme_ustel = 3,
    gurultu_artisi = 4,
    sicrama = 5,
    donma = 6,
    donma_zaman_damgasi = 7,
    yanlis_gecerlilik = 8,
    kendini_bildiren = 9,
    kayip = 10,
    gec_teslim = 11,
    motor_takilmasi = 12,
    dvl_isin_kaybi = 13,
    guc_cevrimi = 14,
};

enum class ZamanDeseniTuru : std::uint8_t { surekli, bernoulli, gilbert_elliott };

struct ZamanDeseni {                  // olaylar[i].zaman_deseni
    ZamanDeseniTuru tur = ZamanDeseniTuru::surekli;
    double p = 0.0;                   // bernoulli
    std::int64_t t_iyi_ns = 0;        // gilbert_elliott
    std::int64_t t_kotu_ns = 0;
};

struct ArizaSiddeti {                 // olaylar[i].siddet (kanal birimi)
    double ofset = 0.0;
    double olcek = 1.0;
    double oran = 0.0;                // birim/s
    double son_deger = 0.0;
    std::int64_t tau_ns = 0;
    double k = 1.0;
    double p = 0.0;
    double genlik = 0.0;
    std::int64_t gecikme_ns = 0;
    std::int64_t titresim_ns = 0;
};

struct SensorArizasi {
    std::string kimlik;               // olaylar[i].kimlik; RNG alt sistemi `ariza/<kimlik>`
    std::string hedef;                // arac.sensorler anahtarı (sensör adı)
    ArizaTuru tur = ArizaTuru::ofset;
    std::int64_t zaman_ns = 0;        // başlangıç: t ≥ zaman_ns olan ilk adım sınırı
    std::int64_t sure_ns = 0;         // 0: süresiz (yalnız disarm kaldırır)
    ArizaSiddeti siddet{};
    std::uint32_t kanal_maskesi = 0xFFFFFFFFu;  // A5-K69: bu sürümde yalnız bütün kanallar
    bool sira_boz = false;
    ZamanDeseni zaman_deseni{};
};

using ArizaTutamaci = std::uint32_t;  // 0 geçersiz

struct ArmSonucu {
    bool tamam = false;
    ArizaTutamaci tutamac = 0;
    std::string hata;                 // tamam değilse (A5 kural numarasıyla)
};

struct EtkinAriza {
    ArizaTutamaci tutamac = 0;
    SensorArizasi tanim{};
    std::int64_t t_baslangic_ns = 0;  // uygulandığı adım sınırı t_b
    std::int64_t t_bitis_ns = 0;      // kalktığı adım sınırı; süresizde INT64_MAX
};

}  // namespace ida::sensor
