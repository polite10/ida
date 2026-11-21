// İDA sensör kümesi (W4-sensor): fizik durumundan zaman damgalı ölçüm üretir (05 §2.1–§2.3,
// §2.6, §2.7). Kamera ve sonar (Ping1D, Ping360) bu kümede değildir.
//
// Başsız ve belirlenimcidir: iş parçacığı, saat, dosya ve ağ kullanmaz. Aynı ayar, aynı tohum
// ve aynı fizik dizisiyle aynı ikili ve platformda bit düzeyinde aynı kayıtları üretir.
//
// Kullanım (her fizik adımında, adım sırasıyla):
//   kume.dis_durum_ver(d);           // yetkili emniyet durumu, sızıntı (değişince)
//   kume.adim(dunya.durum(), dunya.adim_no());
//   kume.bosalt(t_n, kayitlar);      // t_teslim ≤ t_n olan kayıtlar, sıralı
//
// `adim` ilk çağrıda herhangi bir adım numarasıyla başlayabilir; sonra her çağrı bir önceki
// numaradan tam bir büyük olmalıdır (fizik adımlarının hiçbiri atlanmaz). Örneklemeler fizik
// adım sınırlarına bağlanır (05 §2.2: örnekleme olayı en yakın sınıra, tamamlanma olayı
// fiziksel bitişe eşit ya da sonraki ilk sınıra).
#pragma once

#include "ida/fizik/dunya.hpp"
#include "ida/sensor/ariza.hpp"
#include "ida/sensor/ayarlar.hpp"
#include "ida/sensor/olcum.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ida::sensor {

// Sim sunucusunun yetkili durumu (iskelet §5; 05 §2.3.8). Sensör kümesi bunu değiştirmez,
// yalnız geri okuma ölçümü üretir.
struct EmniyetDurumu {
    bool acil_durdurma = false;                      // true: basılı
    std::array<bool, 2> fis{false, false};           // true: takılı (f1, f2)
    std::array<bool, 2> pim{true, true};             // true: takılı
    std::array<bool, 2> dolu{true, true};            // tüp dolu
    std::array<bool, 2> izin_acik{false, false};     // fırlatıcının süreli izni açık
    std::uint32_t emniyet_revizyonu = 1;             // iskelet §3.2
};

struct DisDurum {
    EmniyetDurumu emniyet{};
    std::uint16_t islak_problar = 0;                 // bit i: prob i ıslak (yer gerçeği)
};

enum class AdimSonucu {
    tamam,
    kurulmadi,
    sira_disi,      // adim_no bir öncekinden tam bir büyük değil
    tutarsiz,       // d.adim_no ya da d.t_ns, adim_no ile uyuşmuyor
};

struct KurulumSonucu {
    bool tamam = false;
    std::string hata;                    // tamam değilse
    std::vector<std::string> uyarilar;   // kapsam dışı sensörler vb.
};

struct Sayaclar {
    std::uint64_t kuyruk_tasma = 0;            // sensör başına 64 kayıt sınırı (05 §3)
    std::uint64_t donma_onceki_cikti_yok = 0;  // A5 Tablo 3.10-A
    std::uint64_t nan_reddi = 0;               // 05 §4: model çıktısı sonlu değil
    std::uint64_t uretilen = 0;                // kuyruğa giren kayıt
    std::uint64_t kayip = 0;                   // kayıp arızasıyla teslim edilmeyen
    std::uint64_t aralik_reddi = 0;            // arıza zinciri sonrası temsil edilemeyen değer
};

class SensorKumesi {
public:
    SensorKumesi();
    ~SensorKumesi();
    SensorKumesi(SensorKumesi&&) noexcept;
    SensorKumesi& operator=(SensorKumesi&&) noexcept;
    SensorKumesi(const SensorKumesi&) = delete;
    SensorKumesi& operator=(const SensorKumesi&) = delete;

    // Ayarları doğrular (A5-K41–K49 ve bu kümenin koşulları), açılış hatalarını çeker.
    // Hata olursa küme kurulmamış kalır. Yeniden çağrılırsa her şey sıfırlanır.
    KurulumSonucu kur(const SensorKumesiAyari& ayar);

    // Sonraki `adim` çağrısının sınırında geçerli olan yetkili dış durum.
    void dis_durum_ver(const DisDurum& d);

    // `adim_no` numaralı fizik adım sınırındaki durum (`d.t_ns = adim_no · adim_ns`).
    AdimSonucu adim(const fizik::DurumAnlik& d, std::uint64_t adim_no);

    // Kuyruktaki kayıtlar (05 §3 drain): `t_teslim ≤ t_kadar_ns` olanlar
    // (t_teslim, sensor_id, inst, seq) sırasıyla.
    std::size_t bekleyen() const;
    bool siradaki(std::int64_t t_kadar_ns, Olcum& cikti);
    void bosalt(std::int64_t t_kadar_ns, std::vector<Olcum>& cikti);

    // Arıza denetimi (05 §3 IFaultControl). Arıza `max(zaman_ns, sonraki adım sınırı)` anındaki
    // ya da sonraki ilk adım sınırında başlar; `disarm` sonraki adım sınırında kaldırır.
    // Kayıt, t_b ≤ t_meas < t_bitis ise (ve zaman deseni etkinse) arızadan etkilenir.
    ArmSonucu arm(const SensorArizasi& ariza);
    bool disarm(ArizaTutamaci tutamac);
    std::vector<EtkinAriza> active() const;

    bool kuruldu() const;
    std::int64_t zaman_ns() const;     // son işlenen adım sınırı
    Sayaclar sayaclar() const;

private:
    struct Ic;
    std::unique_ptr<Ic> ic_;
};

}  // namespace ida::sensor
