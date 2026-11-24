// tests/kabul/kopru — vakaların ortak yardımcıları.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "duzenek.hpp"
#include "kontrol.hpp"

namespace kabul {

struct OlayPlani {
    std::uint8_t kind = w::kOlaySafetyOpen;
    std::uint8_t tube = 0;
    std::uint32_t hold_ns = 200'000'000u;
    std::function<void(mavlink_ida_event_t&)> degistir;   // gönderimden önce alan değişikliği
};

struct YanitPlani {
    bool gonder = true;
    std::uint16_t pwm = 1600;
    std::uint16_t flags = static_cast<std::uint16_t>(w::kThrValid | w::kArmed);
    std::uint8_t core_rc = w::kRcOk;
    bool olaylari_say = true;         // n_events = olay sayısı (false: 0)
    bool yanit_once = false;          // true: yanıt olaylardan önce gönderilir (sıra ters)
    std::int64_t olay_gecikme = 0;    // olayların bırakılma gecikmesi (sim zamanı)
    std::int64_t yanit_gecikme = 0;
    std::function<void(mavlink_ida_actuator_t&)> degistir;
};

struct GidenOlaylar {
    GelenTf tf;
    std::vector<mavlink_ida_event_t> olaylar;
    std::uint32_t out_seq = 0;
};

// Sıradaki TF'ye verilecek yanıtı ve olayları kurar ve o TF'ye kadar ilerler.
inline GidenOlaylar olaylarla_yanitla(Duzenek& d, const std::vector<OlayPlani>& olaylar,
                                      const YanitPlani& y = YanitPlani{}) {
    auto sonuc = std::make_shared<GidenOlaylar>();
    d.sonraki_tf_icin([&d, olaylar, y, sonuc](const GelenTf& tf, std::int64_t t) {
        sonuc->tf = tf;
        for (const auto& p : olaylar) {
            mavlink_ida_event_t e = d.kart.olay(p.kind, p.tube, tf, p.hold_ns);
            if (p.degistir) {
                p.degistir(e);
            }
            sonuc->olaylar.push_back(e);
        }
        mavlink_ida_actuator_t a = d.kart.yanit(
            tf, y.pwm, static_cast<std::uint8_t>(y.olaylari_say ? olaylar.size() : 0u));
        a.act_flags = y.flags;
        a.core_rc = y.core_rc;
        if ((y.flags & w::kThrValid) == 0) {
            a.valid_for_ns = 0;
            for (auto& p : a.pwm_us) {
                p = 0;
            }
        }
        if (y.degistir) {
            y.degistir(a);
        }
        if (y.yanit_once && y.gonder) {
            sonuc->out_seq = d.kart.yanit_gonder(a, t + y.yanit_gecikme);
        }
        for (const auto& e : sonuc->olaylar) {
            d.kart.olay_gonder(e, t + y.olay_gecikme);
        }
        if (!y.yanit_once && y.gonder) {
            sonuc->out_seq = d.kart.yanit_gonder(a, t + y.yanit_gecikme);
        }
    });
    d.sonraki_tf();
    return *sonuc;
}

// Kart, sim'in güncel emniyet revizyonunu ve durumunu taşıyan IDA_SAFETY'yi alana kadar
// ilerler (A1-K56: değişimde gönderilir).
inline bool emniyet_esitle(Duzenek& d) {
    for (int i = 0; i < 64; ++i) {
        d.sonraki_tf();
        const auto ed = d.k.dis_durum().emniyet;
        if (!d.kart.partiler.empty() && !d.kart.partiler.back().emniyet.empty() &&
            d.kart.son_rev() == ed.emniyet_revizyonu) {
            return true;
        }
    }
    return false;
}

// Olayın kartın aldığı ilk sonucunu bekler (en çok ~40 parti).
inline std::optional<mavlink_ida_event_result_t> sonuc_bekle(Duzenek& d, std::uint64_t id) {
    for (int i = 0; i < 40; ++i) {
        auto r = d.kart.sonuc(id);
        if (r) {
            return r;
        }
        d.sonraki_tf();
    }
    return d.kart.sonuc(id);
}

inline int sonuc_kodu(Duzenek& d, std::uint64_t id) {
    const auto r = sonuc_bekle(d, id);
    return r ? static_cast<int>(r->result) : -1;
}

// Kur, el sık, izin veren duruma getir ve kartın bunu görmesini bekle.
inline bool hazirla(Duzenek& d, bool izinli = true) {
    if (!d.kur()) {
        return false;
    }
    d.ilerle(8);
    if (izinli) {
        d.izinli_durum();
    }
    return emniyet_esitle(d);
}

// Tüpün iznini SAFETY_OPEN ile açar; uygulanma anını döndürür (−1: açılamadı).
inline std::int64_t izin_ac(Duzenek& d, std::uint8_t tube, std::uint32_t hold_ns = 200'000'000u) {
    OlayPlani p;
    p.kind = w::kOlaySafetyOpen;
    p.tube = tube;
    p.hold_ns = hold_ns;
    const GidenOlaylar g = olaylarla_yanitla(d, {p});
    const auto r = sonuc_bekle(d, g.olaylar.at(0).event_id);
    if (!r || r->result != w::kSonucApplied) {
        return -1;
    }
    return r->t_apply_ns;
}

inline std::uint32_t rev(const Duzenek& d) {
    return d.k.dis_durum().emniyet.emniyet_revizyonu;
}

inline OlayPlani acma(std::uint8_t tube, std::uint32_t hold_ns = 200'000'000u) {
    OlayPlani p;
    p.kind = w::kOlaySafetyOpen;
    p.tube = tube;
    p.hold_ns = hold_ns;
    return p;
}

inline OlayPlani kapama(std::uint8_t tube) {
    OlayPlani p;
    p.kind = w::kOlaySafetyClose;
    p.tube = tube;
    p.hold_ns = 0;
    return p;
}

inline OlayPlani atis(std::uint8_t tube) {
    OlayPlani p;
    p.kind = w::kOlayFire;
    p.tube = tube;
    p.hold_ns = 0;
    return p;
}

// Uygulama anı a_n'nin fizik sınırı (A1 §2: a_n = t_n + L_act).
inline std::uint64_t uygulama_siniri(const GelenTf& tf) {
    return Duzenek::sinir(tf.t + kLact);
}

// Bir olayı (kartın A1-K80 yinelemesi gibi aynı yükle) sıradaki TF'de yeniden gönderir; o
// TF'nin yanıtı olağandır ve olayı saymaz (n_events = 0).
inline void yinele(Duzenek& d, const mavlink_ida_event_t& e) {
    d.sonraki_tf_icin([&d, e](const GelenTf& tf, std::int64_t t) {
        d.kart.olay_gonder(e, t);
        d.kart.yanit_gonder(d.kart.yanit(tf, d.varsayilan_pwm, 0), t);
    });
    d.sonraki_tf();
}

// n parti boyunca ilerler.
inline void parti_ilerle(Duzenek& d, int n) {
    for (int i = 0; i < n; ++i) {
        d.sonraki_tf();
    }
}

inline std::uint64_t olay_sayaci(const Duzenek& d, std::uint8_t kod) {
    return d.k.sayaclar().olay_sonuc.at(kod);
}

}  // namespace kabul
