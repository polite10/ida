// tests/kabul/kopru — emniyet geri okumasının parti düzeyinde denetimi (A1-K56; W4-K4, W4-K5).
//
// Bir kısıtlama (t_kisit) ve ardından bir gevşetme (t_gevset) yetkili durumda uygulanır.
// Beklenen (mimar kararları W4-K4, W4-K5; docs/gorevler/W4-ortak.md):
//   (a) kısıtlama, uygulandığı pencereyi kapatan partide görünür (A1-K56: ertelenmez);
//   (b) gevşetme, t_gevset + anahtar_gecikmesi_ns'ten sonraki ilk partiden önce hiçbir
//       partide görünmez; o partide ya da en geç bir sonrakinde görünür (W4-K5);
//   (c) her örnekteki (durum, revizyon) çifti sim'in yetkili geçmişinde var olmuştur (W4-K4);
//   (d) revizyon partiler arasında azalmaz.
#pragma once

#include <cstdint>
#include <functional>
#include <variant>
#include <vector>

#include "ida/sensor/ayarlar.hpp"
#include "kontrol.hpp"
#include "sahte_kart.hpp"

namespace kabul {

// A5 firlatici_io profilinin anahtar gecikmesi (05 §2.3.8). Sensör ayarından okunur; İDA-0
// sensör listesinde geri okuma yoksa (olmamalı) profilin belgelenmiş değeri kullanılır.
inline constexpr std::int64_t kAnahtarGecikmesiProfil = 10'000'000;

inline std::int64_t anahtar_gecikmesi_ns() {
    for (const auto& s : ida::sensor::ida0_sensorleri()) {
        if (const auto* f = std::get_if<ida::sensor::FirlaticiGeriOkumaAyar>(&s.ayar)) {
            return f->anahtar_gecikmesi_ns;
        }
    }
    return kAnahtarGecikmesiProfil;
}

struct GeriOkumaBeklentisi {
    const char* ad = "";
    // Örneğin denetlenen alanı kısıtlayan değerde mi (true) gevşek mi (false).
    std::function<bool(const mavlink_ida_safety_t&)> kisitli;
    std::int64_t t_kisit = 0;
    std::int64_t t_gevset = 0;
    std::int64_t gecikme = 0;
    // (alan kısıtlı mı, revizyon) çifti sim'in yetkili geçmişinde var olmuş mu.
    std::function<bool(bool, std::uint32_t)> gecmiste;
};

inline void geri_okuma_denetle(const std::vector<const Parti*>& partiler,
                               const GeriOkumaBeklentisi& b) {
    // (c), (d)
    std::uint32_t onceki_rev = 0;
    bool azaldi = false;
    for (const Parti* p : partiler) {
        for (const auto& s : p->emniyet) {
            const bool k = b.kisitli(s);
            KONTROL(b.gecmiste(k, s.safety_rev),
                    b.ad << ": W4-K4: (durum, revizyon) çifti yetkili geçmişte yok: parti t="
                         << p->tf.t << ", " << (k ? "kısıtlı" : "gevşek") << ", rev " << s.safety_rev);
            azaldi = azaldi || s.safety_rev < onceki_rev;
            onceki_rev = s.safety_rev;
        }
    }
    KONTROL(!azaldi, b.ad << ": revizyon partiler arasında azalmaz");

    // (a)
    std::size_t ik = partiler.size();
    for (std::size_t i = 0; i < partiler.size(); ++i) {
        if (partiler[i]->tf.t >= b.t_kisit) {
            ik = i;
            break;
        }
    }
    KONTROL(ik < partiler.size(), b.ad << ": kısıtlamanın partisi alındı");
    if (ik >= partiler.size()) {
        return;
    }
    const Parti& pk = *partiler[ik];
    KONTROL(pk.emniyet.size() == 1u && b.kisitli(pk.emniyet.front()),
            b.ad << ": A1-K56: kısıtlama uygulandığı pencereyi kapatan partide (t=" << pk.tf.t
                 << ") görünür");

    // (b)
    const std::int64_t T = b.t_gevset + b.gecikme;
    std::size_t iy = partiler.size();
    for (std::size_t i = ik; i < partiler.size(); ++i) {
        if (partiler[i]->tf.t >= T) {
            iy = i;
            break;
        }
    }
    KONTROL(iy + 1u < partiler.size(), b.ad << ": gevşetmeden sonraki partiler alındı");
    if (iy + 1u >= partiler.size()) {
        return;
    }
    for (std::size_t i = ik; i < iy; ++i) {
        for (const auto& s : partiler[i]->emniyet) {
            KONTROL(b.kisitli(s), b.ad << ": W4-K5: gevşetme t_gevset + anahtar_gecikmesi ("
                                       << T << " ns) sonrasındaki ilk partiden önce görünmez; "
                                       << "görüldüğü parti t=" << partiler[i]->tf.t);
        }
    }
    bool goruldu = false;
    for (std::size_t i = iy; i <= iy + 1u; ++i) {
        for (const auto& s : partiler[i]->emniyet) {
            goruldu = goruldu || !b.kisitli(s);
        }
    }
    KONTROL(goruldu, b.ad << ": W4-K5: gevşetme t=" << partiler[iy]->tf.t
                          << " partisinde ya da bir sonrakinde görünür");
}

}  // namespace kabul
