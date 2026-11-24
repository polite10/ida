// tests/kabul/kopru — emniyet örneği (vaka 8, A1-K56) ve dayanıklılık (vaka 9).
//
// Vaka 8: emniyet geri okuma kayıtları köprüye sensör kümesinden gelir (kopru.hpp çağrı
// sırası); düzenek bunları değişimde ve 100 ms'de bir, değişim anı damgasıyla üretir (taklit).
// Köprünün yükümlülüğü A1-K56/K34: partide en çok bir IDA_SAFETY, kritik üye ertelenmez,
// aynı pencerede kapanma ve açılma olursa o partide kapanmış durum, açılma sonraki partide.
//
// Vaka 9: sabit tohumlu ≥ 100 000 bozuk/rastgele kart çerçevesi; fırlatıcı durumu hiçbir
// zaman genişlemez, çökme yok.
#include <cstdio>

#include "ortak.hpp"
#include "rastgele.hpp"

namespace kabul {
namespace {

const Parti* parti_t(const Duzenek& d, std::int64_t t) {
    for (const auto& p : d.kart.partiler) {
        if (p.tf.t == t) {
            return &p;
        }
    }
    return nullptr;
}

std::int64_t kapanis_t(std::uint64_t b) {
    const std::uint64_t T = ((b + 3u) / 4u) * 4u;  // t_{n−1} < b·h ≤ t_n
    return static_cast<std::int64_t>(T) * kH;
}

bool en_cok_bir_emniyet(const Duzenek& d) {
    for (const auto& p : d.kart.partiler) {
        if (p.emniyet.size() > 1u) {
            return false;
        }
    }
    return true;
}

VAKA("A1-K56.kisitlama_ertelenmez") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    for (std::uint64_t ofset = 0; ofset < 4u; ++ofset) {
        // Atama sınır B'de uygulanır; B % 4 == ofset.
        while (d.b() % 4u != ofset) {
            d.adim();
        }
        const std::uint64_t B = d.b();
        const bool acil = (ofset % 2u) == 0u;
        if (acil) {
            d.emniyet(true, {}, {}, {}, {});
        } else {
            d.emniyet({}, false, {}, {}, {});
        }
        d.ilerle_kadar(kapanis_t(B) + kH);
        const std::uint32_t r = d.emniyet_b(B).emniyet_revizyonu;
        const Parti* p = parti_t(d, kapanis_t(B));
        KONTROL(p != nullptr, "kapanış partisi alındı (sınır " << B << ")");
        if (p != nullptr) {
            KONTROL_ESIT(p->emniyet.size(), std::size_t{1},
                         "A1-K56: kısıtlama kendi partisinde (sınır " << B << ", ofset " << ofset << ")");
            if (!p->emniyet.empty()) {
                const auto& s = p->emniyet.front();
                if (acil) {
                    KONTROL(s.estop == 1u, "A1-K56: acil durdurma ertelenmez (ofset " << ofset << ")");
                } else {
                    KONTROL((s.tube[0] & w::kTupFis) == 0u,
                            "A1-K56: fişin çıkması ertelenmez (ofset " << ofset << ")");
                }
                KONTROL_ESIT(s.safety_rev, r, "A1-K56: safety_rev t_meas anındaki değer");
                KONTROL_ESIT(s.t_meas_ns, static_cast<std::int64_t>(B) * kH, "A1-K56: t_meas değişim anı");
            }
        }
        d.izinli_durum();
        KONTROL(emniyet_esitle(d), "geri alma");
    }
    KONTROL(en_cok_bir_emniyet(d), "A1-K34: partide en çok bir IDA_SAFETY");
}

// Aynı pencerede (4m+1 … 4m+4) iki değişim: değişimler sinir_sonrasi ile B1 ve B2'de uygulanır.
void pencere_vakasi(Duzenek& d, const char* ad, const std::function<void()>& ilk,
                    const std::function<void()>& ikinci,
                    const std::function<bool(const mavlink_ida_safety_t&)>& kapali,
                    bool acilma_sonraki_partide) {
    while (d.b() % 4u != 1u) {
        d.adim();
    }
    const std::uint64_t B1 = d.b() + 4u;  // 4m+1
    const std::uint64_t B2 = B1 + 2u;     // 4m+3
    d.sinir_sonrasi = [&](std::uint64_t b) {
        if (b + 1u == B1) {
            ilk();
        } else if (b + 1u == B2) {
            ikinci();
        }
    };
    const std::int64_t tn = static_cast<std::int64_t>(B1 + 3u) * kH;  // 4m+4
    d.ilerle_kadar(tn + kP + kH);
    d.sinir_sonrasi = nullptr;
    const Parti* p1 = parti_t(d, tn);
    const Parti* p2 = parti_t(d, tn + kP);
    KONTROL(p1 != nullptr && p2 != nullptr, ad << ": partiler alındı");
    if (p1 == nullptr || p2 == nullptr) {
        return;
    }
    KONTROL_ESIT(p1->emniyet.size(), std::size_t{1}, ad << ": A1-K34/K56 partide tek IDA_SAFETY");
    if (!p1->emniyet.empty()) {
        KONTROL(kapali(p1->emniyet.front()),
                ad << ": A1-K56: aynı pencerede açılıp kapanan girdi kısıtlayıcı hâliyle bildirilir");
    }
    if (acilma_sonraki_partide) {
        KONTROL(!p2->emniyet.empty() && !kapali(p2->emniyet.front()),
                ad << ": A1-K56: açılma sonraki partide gönderilir");
    }
}

VAKA("A1-K56.ayni_pencerede_kapanip_acilma") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    // Fiş 0: çıkar (4m+1), takılır (4m+3).
    pencere_vakasi(
        d, "fiş kapan-aç", [&d] { d.emniyet({}, false, {}, {}, {}); },
        [&d] { d.emniyet({}, true, {}, {}, {}); },
        [](const mavlink_ida_safety_t& s) { return (s.tube[0] & w::kTupFis) == 0u; }, true);
    KONTROL(emniyet_esitle(d), "eşitleme");
    // Acil durdurma darbesi.
    pencere_vakasi(
        d, "acil darbe", [&d] { d.emniyet(true, {}, {}, {}, {}); },
        [&d] { d.emniyet(false, {}, {}, {}, {}); },
        [](const mavlink_ida_safety_t& s) { return s.estop == 1u; }, true);
    KONTROL(emniyet_esitle(d), "eşitleme");
    // Pim 1: takılır (kısıtlama, 4m+1), çıkar (4m+3).
    pencere_vakasi(
        d, "pim kapan-aç", [&d] { d.emniyet({}, {}, {}, {}, true); },
        [&d] { d.emniyet({}, {}, {}, {}, false); },
        [](const mavlink_ida_safety_t& s) { return (s.tube[1] & w::kTupPimCikik) == 0u; }, true);
    KONTROL(emniyet_esitle(d), "eşitleme");
    // Ters sıra: önce açılma (fiş 0 çıkarılmış durumdan takılır), sonra kapanma.
    d.emniyet({}, false, {}, {}, {});
    KONTROL(emniyet_esitle(d), "eşitleme (fiş 0 çıkık)");
    pencere_vakasi(
        d, "fiş aç-kapan", [&d] { d.emniyet({}, true, {}, {}, {}); },
        [&d] { d.emniyet({}, false, {}, {}, {}); },
        [](const mavlink_ida_safety_t& s) { return (s.tube[0] & w::kTupFis) == 0u; }, false);
    KONTROL(en_cok_bir_emniyet(d), "A1-K34: partide en çok bir IDA_SAFETY");
}

// ---------------------------------------------------------------- Vaka 9

struct FuzzSonucu {
    std::uint64_t cerceve = 0;
    std::uint64_t ihlal_dolu = 0, ihlal_izin = 0, ihlal_acil = 0;
    std::uint64_t ilk_ihlal = 0;
};

FuzzSonucu fuzz(Duzenek& d, std::uint32_t tohum, std::uint64_t hedef, bool acil_bekleniyor) {
    FuzzSonucu s;
    RastgeleCerceve rc(tohum);
    constexpr std::uint32_t kSinirBasina = 50;
    d.sinir_sonrasi = [&](std::uint64_t) {
        if (d.kart.partiler.empty()) {
            return;
        }
        const GelenTf tf = d.kart.partiler.back().tf;
        for (std::uint32_t i = 0; i < kSinirBasina; ++i) {
            auto b = rc.uret(d.kart, tf);
            if (!b.empty()) {
                d.kart.ham_gonder(std::move(b), static_cast<std::int64_t>(d.b()) * kH);
                ++s.cerceve;
            }
        }
    };
    while (s.cerceve < hedef) {
        const std::uint64_t b = d.b();
        d.adim();
        const auto& e = d.emniyet_b(b);
        const bool dolu = e.dolu[0] && e.dolu[1] && d.k.tup_dolu(0) && d.k.tup_dolu(1);
        const bool izin = !e.izin_acik[0] && !e.izin_acik[1];
        if (!dolu) {
            ++s.ihlal_dolu;
        }
        if (!izin) {
            ++s.ihlal_izin;
        }
        if (acil_bekleniyor && !e.acil_durdurma) {
            ++s.ihlal_acil;
        }
        if ((!dolu || !izin) && s.ilk_ihlal == 0) {
            s.ilk_ihlal = b;
        }
    }
    d.sinir_sonrasi = nullptr;
    return s;
}

void fuzz_raporu(const Duzenek& d, const FuzzSonucu& s) {
    const auto& c = d.k.sayaclar();
    std::printf("çerçeve=%llu rx_crc_err=%llu alan_ret=%llu bag_ret=%llu olay_alinan=%llu "
                "oturum_kopma=%llu oturum_kurulan=%llu\n",
                static_cast<unsigned long long>(s.cerceve),
                static_cast<unsigned long long>(c.rx_crc_err),
                static_cast<unsigned long long>(c.alan_ret),
                static_cast<unsigned long long>(c.bag_ret),
                static_cast<unsigned long long>(c.olay_alinan),
                static_cast<unsigned long long>(c.oturum_kopma),
                static_cast<unsigned long long>(c.oturum_kurulan));
}

VAKA("A1-K109.rastgele_cerceveler_acil") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    KONTROL(izin_ac(d, 0, 450'000'000u) >= 0, "ön koşul: izin (acil durdurma kapatacak)");
    d.emniyet(true, {}, {}, {}, {});
    KONTROL(emniyet_esitle(d), "eşitleme (acil durdurma etkin)");
    KONTROL(!d.k.izin_acik(0) && !d.k.izin_acik(1), "ön koşul: izinler kapalı");
    const FuzzSonucu s = fuzz(d, 0x1DA5EEDu, 100'000u, true);
    fuzz_raporu(d, s);
    KONTROL(s.cerceve >= 100'000u, "en az 100 000 çerçeve");
    KONTROL_ESIT(s.ihlal_dolu, 0u, "fırlatıcı: hiçbir sınırda tüp boşalmaz (ilk ihlal sınırı " << s.ilk_ihlal << ")");
    KONTROL_ESIT(s.ihlal_izin, 0u, "fırlatıcı: hiçbir sınırda izin açılmaz (ilk ihlal " << s.ilk_ihlal << ")");
    KONTROL_ESIT(s.ihlal_acil, 0u, "acil durdurma kart çerçeveleriyle kalkmaz (iskelet §5)");
    KONTROL(d.k.sayaclar().rx_crc_err > 0u && d.k.sayaclar().alan_ret > 0u,
            "düzenek: bozuk ve alanı geçersiz çerçeveler alıcıya ulaştı");
}

VAKA("A1-K109.rastgele_cerceveler_fis") {
    Duzenek d;
    KONTROL(d.kur(), "kurulum");
    d.ilerle(8);
    d.emniyet(false, false, false, true, true);  // iki fiş çıkık, iki pim takılı
    KONTROL(emniyet_esitle(d), "eşitleme");
    const FuzzSonucu s = fuzz(d, 0xB0A7F00Du, 100'000u, false);
    fuzz_raporu(d, s);
    KONTROL(s.cerceve >= 100'000u, "en az 100 000 çerçeve");
    KONTROL_ESIT(s.ihlal_dolu, 0u, "fırlatıcı: tüp boşalmaz (ilk ihlal " << s.ilk_ihlal << ")");
    KONTROL_ESIT(s.ihlal_izin, 0u, "fırlatıcı: izin açılmaz (ilk ihlal " << s.ilk_ihlal << ")");
}

}  // namespace
}  // namespace kabul
