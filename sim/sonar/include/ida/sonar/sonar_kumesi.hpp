// İDA sonar kümesi (W4-sonar): fizik durumundan ve sahneden zaman damgalı Ping1D ve Ping360
// profilleri üretir; sade kademe (iskelet §6, §8; 05 §2.4).
//
// Başsız ve belirlenimcidir: iş parçacığı, saat, dosya ve ağ kullanmaz. Aynı ayar, aynı tohum,
// aynı sahne ve aynı fizik dizisiyle aynı ikili ve platformda bit düzeyinde aynı kayıtları
// üretir. Rastgele çekilişler sensör kümesinin A5 Ek D akışlarıyla yapılır; her sonarın akışları
// kendi adıyla anahtarlıdır, bir sonarın açılıp kapanması ötekilerin dizisini değiştirmez.
//
// Kullanım (her fizik adımında, adım sırasıyla; sensör kümesiyle aynı düzen):
//   kume.adim(dunya.durum(), dunya.adim_no());
//   kume.bosalt(t_n, kayitlar);      // t_teslim ≤ t_n olan kayıtlar, sıralı
//
// Zaman (05 §2.2): ping gönderim anı `t_tx` bir adım sınırıdır; geometri `t_tx` anının
// durumuyla ve sahne pozlarıyla hesaplanır (dur-ve-atla; 05 §2.4.5 md.1). Kayıt, fiziksel
// bitişe (`t_tx + T_dinleme + T_işlem + T_iletim`) eşit ya da sonraki ilk adım sınırında üretilir;
// `t_hazir` = o sınır, `t_teslim` = max(fiziksel bitiş, t_hazir) (W4-K9).
#pragma once

#include "ida/fizik/dunya.hpp"
#include "ida/sensor/ariza.hpp"
#include "ida/sensor/sensor_kumesi.hpp"
#include "ida/sonar/ayarlar.hpp"
#include "ida/sonar/olcum.hpp"
#include "ida/sonar/sahne.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ida::sonar {

using sensor::AdimSonucu;
using sensor::ArizaTutamaci;
using sensor::ArmSonucu;
using sensor::EtkinAriza;
using sensor::KurulumSonucu;
using sensor::SensorArizasi;

struct SonarSayaclari {
    std::uint64_t uretilen = 0;            // kuyruğa giren kayıt
    std::uint64_t kayip = 0;               // kayıp arızasıyla teslim edilmeyen
    std::uint64_t kuyruk_tasma = 0;        // sonar başına 16 kayıt sınırı [T]
    std::uint64_t donma_onceki_cikti_yok = 0;
    std::uint64_t ping = 0;                // atılan ping (bütün sonarlar)
    std::uint64_t isin = 0;                // izlenen ışın
    std::uint64_t havada = 0;              // dönüştürücü havadayken atılan ping (05 §4; eko yok)
};

class SonarKumesi {
public:
    SonarKumesi();
    ~SonarKumesi();
    SonarKumesi(SonarKumesi&&) noexcept;
    SonarKumesi& operator=(SonarKumesi&&) noexcept;
    SonarKumesi(const SonarKumesi&) = delete;
    SonarKumesi& operator=(const SonarKumesi&) = delete;

    // Ayarları doğrular (A5 §3.6, K41, K48, K65, K78; bu kümenin koşulları). Sahne paylaşılır ve
    // değiştirilmez (05 §2.4.1); `adim` çağrıları arasında dışarıdan güncellenebilir.
    KurulumSonucu kur(const SonarKumesiAyari& ayar, std::shared_ptr<const Sahne> sahne);

    // `adim_no` numaralı fizik adım sınırındaki durum (`d.t_ns = adim_no · adim_ns`).
    AdimSonucu adim(const fizik::DurumAnlik& d, std::uint64_t adim_no);

    std::size_t bekleyen() const;
    bool siradaki(std::int64_t t_kadar_ns, SonarOlcumu& cikti);
    void bosalt(std::int64_t t_kadar_ns, std::vector<SonarOlcumu>& cikti);

    // Cihaz yapılandırması (05 §2.4.5 komut tablosu): süren ping tamamlanır, yeni yapılandırma
    // sonraki pingte geçerli olur; bekleyenin yerini en yenisi alır. Aralık dışı alan reddedilir,
    // önceki yapılandırma sürer.
    bool ping360_yapilandir(const std::string& ad, const Ping360Yapilandirmasi& y);
    bool ping1d_yapilandir(const std::string& ad, const Ping1dYapilandirmasi& y);

    // Arıza denetimi (sensör kümesiyle aynı anlam ve RNG yerleri; A5 Tablo 3.10-A, Ek D.4–D.5).
    // Gövdeli: donma, donma_zaman_damgasi, yanlis_gecerlilik, kendini_bildiren, kayip,
    // gec_teslim. Ötekiler yer tutar (arm reddeder; gerekçe iletide).
    ArmSonucu arm(const SensorArizasi& ariza);
    bool disarm(ArizaTutamaci tutamac);
    std::vector<EtkinAriza> active() const;

    bool kuruldu() const;
    std::int64_t zaman_ns() const;
    double ses_hizi() const;               // sahnenin nominal ses hızı (m/s)
    SonarSayaclari sayaclar() const;

private:
    struct Ic;
    std::unique_ptr<Ic> ic_;
};

}  // namespace ida::sonar
