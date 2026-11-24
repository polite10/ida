// tests/kabul/kopru — W4-K15 (EMNİYET): olay kümesi sayıyla kapanır; sayılmamış genişletici
// olay protokol ihlalidir. Dayanak: A1-K73 (yuva n = ACTUATOR(n) + tam n_events farklı
// event_id), A1-K118, Tablo 4.6b, A1-K87, A1-K23, H12; mimar kararı W4-K15
// (docs/gorevler/W4-ortak.md).
//
// Vaka adları kararın maddeleriyle: W4-K15.<madde>.<durum>. HIL 1x (T_bağ ve kaçırma bu kipte).
#include <cstdio>

#include "ortak.hpp"

namespace kabul {
namespace {

constexpr std::uint8_t kRcBadInput = w::kRcBadInput;

bool oturum_bitti(const Duzenek& d, std::uint16_t s0) {
    return d.k.durum() != ida::kopru::OturumDurumu::kosuyor || d.k.sess() != s0;
}

bool oturum_suruyor(const Duzenek& d, std::uint16_t s0) {
    return d.k.durum() == ida::kopru::OturumDurumu::kosuyor && d.k.sess() == s0;
}

void kapanis_yaz(const Duzenek& d, const char* ad) {
    std::printf("%s: kartın aldığı son SESSION_CLOSE nedeni = %u (0: alınmadı), oturum_kopma = %llu\n",
                ad, d.kart.son_kapanis_nedeni,
                static_cast<unsigned long long>(d.k.sayaclar().oturum_kopma));
}

// Tam yuvaya (ACTUATOR(n), n_events = 0) aynı batch_seq'li yeni kimlikli genişletici olay.
void tam_yuvaya_yeni(std::uint8_t tur) {
    const char* ad = (tur == w::kOlayFire) ? "FIRE" : "SAFETY_OPEN";
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    if (tur == w::kOlayFire) {
        KONTROL(izin_ac(d, 0, 450'000'000u) >= 0, "ön koşul: izin 0");
    }
    const std::uint16_t s0 = d.k.sess();
    const std::uint64_t rs0 = olay_sayaci(d, w::kSonucRejSafe);
    std::uint64_t A = 0;
    std::uint64_t Btf = 0;
    d.sonraki_tf_icin([&](const GelenTf& tf, std::int64_t t) {
        A = uygulama_siniri(tf);
        Btf = Duzenek::sinir(tf.t);
        d.kart.yanit_gonder(d.kart.yanit(tf, 1600, 0), t);  // küme: 0 olay; yuva tam
        const std::uint8_t tup = (tur == w::kOlayFire) ? 0u : 1u;
        const mavlink_ida_event_t e = d.kart.olay(tur, tup, tf, 200'000'000u);
        d.kart.olay_gonder(e, t + 2 * kH);                   // a_n'den önce, yanıttan sonra
    });
    d.sonraki_tf();
    d.ilerle_kadar(static_cast<std::int64_t>(A + 40u) * kH);
    kapanis_yaz(d, ad);
    KONTROL(oturum_bitti(d, s0), "W4-K15 (3): tam yuvaya yeni kimlikli " << ad
                                     << " → oturum protokol hatasıyla biter");
    KONTROL(olay_sayaci(d, w::kSonucRejSafe) > rs0, "W4-K15 (3): " << ad << " REJ_SAFE alır");
    for (std::uint64_t b = Btf; b <= A + 40u; ++b) {
        if (tur == w::kOlayFire) {
            KONTROL(d.emniyet_b(b).dolu[0], "W4-K15: FIRE uygulanmaz; tüp 0 dolu (sınır " << b << ")");
        } else {
            KONTROL(!d.emniyet_b(b).izin_acik[1],
                    "W4-K15: SAFETY_OPEN uygulanmaz; izin 1 kapalı (sınır " << b << ")");
        }
    }
    KONTROL(!d.emniyet_b(A + 40u).izin_acik[0] && !d.emniyet_b(A + 40u).izin_acik[1],
            "A1-K23: oturum sonunda iki tüpün izni kapalı");
}

VAKA("W4-K15.3.tam_yuvaya_yeni_open") {
    tam_yuvaya_yeni(w::kOlaySafetyOpen);
}

VAKA("W4-K15.3.tam_yuvaya_yeni_fire") {
    tam_yuvaya_yeni(w::kOlayFire);
}

VAKA("W4-K15.3.sayiyi_asan_genisletici") {
    // İki farklı SAFETY_OPEN yanıttan önce gelir; yanıt n_events = 1 der.
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    const std::uint16_t s0 = d.k.sess();
    const std::uint64_t rs0 = olay_sayaci(d, w::kSonucRejSafe);
    std::uint64_t A = 0;
    std::uint64_t Btf = 0;
    d.sonraki_tf_icin([&](const GelenTf& tf, std::int64_t t) {
        A = uygulama_siniri(tf);
        Btf = Duzenek::sinir(tf.t);
        d.kart.olay_gonder(d.kart.olay(w::kOlaySafetyOpen, 0, tf, 200'000'000u), t);
        d.kart.olay_gonder(d.kart.olay(w::kOlaySafetyOpen, 1, tf, 200'000'000u), t);
        d.kart.yanit_gonder(d.kart.yanit(tf, 1600, 1), t);
    });
    d.sonraki_tf();
    d.ilerle_kadar(static_cast<std::int64_t>(A + 40u) * kH);
    kapanis_yaz(d, "sayıyı aşan");
    KONTROL(oturum_bitti(d, s0), "W4-K15 (3): sayıyı aşan genişletici olay → oturum biter");
    KONTROL(olay_sayaci(d, w::kSonucRejSafe) > rs0, "W4-K15 (3): REJ_SAFE");
    for (std::uint64_t b = Btf; b <= A + 40u; ++b) {
        KONTROL(!d.emniyet_b(b).izin_acik[0] && !d.emniyet_b(b).izin_acik[1],
                "W4-K15: hiçbir izin açılmaz (sınır " << b << ")");
    }
}

VAKA("W4-K15.1.eksik_kume_open_uygulanmaz") {
    // Yanıt 2 olay sayar; SAFETY_OPEN(1) gelir, SAFETY_CLOSE(0) kaybolur: küme hiç tamamlanmaz.
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    std::uint64_t A = 0;
    std::uint64_t Btf = 0;
    d.sonraki_tf_icin([&](const GelenTf& tf, std::int64_t t) {
        A = uygulama_siniri(tf);
        Btf = Duzenek::sinir(tf.t);
        d.kart.olay_gonder(d.kart.olay(w::kOlaySafetyOpen, 1, tf, 200'000'000u), t);
        (void)d.kart.olay(w::kOlaySafetyClose, 0, tf);  // kimlik tüketildi, çerçeve kayboldu
        d.kart.yanit_gonder(d.kart.yanit(tf, 1600, 2), t);
    });
    d.sonraki_tf();
    d.ilerle_kadar(static_cast<std::int64_t>(A + 80u) * kH);
    for (std::uint64_t b = Btf; b <= A + 80u; ++b) {
        KONTROL(!d.emniyet_b(b).izin_acik[1],
                "W4-K15 (1): küme tamamlanmadan SAFETY_OPEN hiçbir sınırda uygulanmaz (sınır " << b << ")");
    }
}

VAKA("W4-K15.1.eksik_kume_close_beklemez") {
    // Yanıt 2 olay sayar; SAFETY_CLOSE(0) gelir, SAFETY_OPEN(1) kaybolur.
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    KONTROL(izin_ac(d, 0, 450'000'000u) >= 0, "ön koşul: izin 0");
    std::uint64_t A = 0;
    std::uint64_t Btf = 0;
    d.sonraki_tf_icin([&](const GelenTf& tf, std::int64_t t) {
        A = uygulama_siniri(tf);
        Btf = Duzenek::sinir(tf.t);
        (void)d.kart.olay(w::kOlaySafetyOpen, 1, tf, 200'000'000u);  // kayboldu
        d.kart.olay_gonder(d.kart.olay(w::kOlaySafetyClose, 0, tf), t);
        d.kart.yanit_gonder(d.kart.yanit(tf, 1600, 2), t);
    });
    d.sonraki_tf();
    d.ilerle_kadar(static_cast<std::int64_t>(A + 80u) * kH);
    KONTROL(!d.emniyet_b(A).izin_acik[0],
            "W4-K15 (1), A1-K87: eksik kümede SAFETY_CLOSE beklemeden uygulanır (a_n'de izin 0 kapalı)");
    for (std::uint64_t b = Btf; b <= A + 80u; ++b) {
        KONTROL(!d.emniyet_b(b).izin_acik[1], "izin 1 açılmaz (sınır " << b << ")");
    }
}

VAKA("W4-K15.3.is_safe_yanit_sonra_fire") {
    // Yanıt n_events = 1, IS_SAFE = 1 (nötr) + SAFETY_OPEN(1); ardından aynı batch_seq'le yeni
    // kimlikli FIRE(0): yanıt sonradan tutarsızlaşamaz; FIRE uygulanmaz, oturum biter.
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    KONTROL(izin_ac(d, 0, 450'000'000u) >= 0, "ön koşul: izin 0");
    const std::uint16_t s0 = d.k.sess();
    std::uint64_t A = 0;
    std::uint64_t Btf = 0;
    d.sonraki_tf_icin([&](const GelenTf& tf, std::int64_t t) {
        A = uygulama_siniri(tf);
        Btf = Duzenek::sinir(tf.t);
        d.kart.olay_gonder(d.kart.olay(w::kOlaySafetyOpen, 1, tf, 200'000'000u), t);
        mavlink_ida_actuator_t a = d.kart.yanit(tf, 1500, 1);
        a.act_flags = static_cast<std::uint16_t>(w::kThrValid | w::kIsSafe | w::kArmed);
        d.kart.yanit_gonder(a, t);
        d.kart.olay_gonder(d.kart.olay(w::kOlayFire, 0, tf), t + 2 * kH);
    });
    d.sonraki_tf();
    d.ilerle_kadar(static_cast<std::int64_t>(A + 40u) * kH);
    kapanis_yaz(d, "IS_SAFE + FIRE");
    KONTROL(oturum_bitti(d, s0), "W4-K15 (3): tam kümeye yeni kimlikli FIRE → oturum biter");
    for (std::uint64_t b = Btf; b <= A + 40u; ++b) {
        KONTROL(d.emniyet_b(b).dolu[0], "W4-K15: FIRE uygulanmaz; tüp 0 dolu (sınır " << b << ")");
    }
    KONTROL(!d.emniyet_b(A + 40u).izin_acik[0] && !d.emniyet_b(A + 40u).izin_acik[1],
            "A1-K23: oturum sonunda izinler kapalı");
}

VAKA("W4-K15.3.ayni_kimlik_yineleme_ihlal_degil") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    const std::uint16_t s0 = d.k.sess();
    const std::uint64_t rs0 = olay_sayaci(d, w::kSonucRejSafe);
    std::uint64_t A = 0;
    std::uint64_t id = 0;
    d.sonraki_tf_icin([&](const GelenTf& tf, std::int64_t t) {
        A = uygulama_siniri(tf);
        const mavlink_ida_event_t e = d.kart.olay(w::kOlaySafetyOpen, 1, tf, 200'000'000u);
        id = e.event_id;
        d.kart.olay_gonder(e, t);
        d.kart.yanit_gonder(d.kart.yanit(tf, 1600, 1), t);
        d.kart.olay_gonder(e, t + 2 * kH);  // aynı yükle yineleme (A1-K80)
    });
    d.sonraki_tf();
    d.ilerle_kadar(static_cast<std::int64_t>(A + 8u) * kH);
    KONTROL(oturum_suruyor(d, s0), "W4-K15 (3), A1-K81: aynı event_id yinelemesi ihlal değildir");
    KONTROL(d.emniyet_b(A).izin_acik[1], "SAFETY_OPEN a_n'de uygulanır");
    const auto r = sonuc_bekle(d, id);
    KONTROL(r && r->result == w::kSonucApplied, "ilk sonuç APPLIED");
    KONTROL_ESIT(olay_sayaci(d, w::kSonucRejSafe), rs0, "REJ_SAFE yok");
}

VAKA("W4-K15.3.sayiyi_asan_safety_close") {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    KONTROL(izin_ac(d, 0, 450'000'000u) >= 0, "ön koşul: izin 0");
    const std::uint16_t s0 = d.k.sess();
    std::uint64_t A = 0;
    std::uint64_t id = 0;
    d.sonraki_tf_icin([&](const GelenTf& tf, std::int64_t t) {
        A = uygulama_siniri(tf);
        d.kart.yanit_gonder(d.kart.yanit(tf, 1600, 0), t);
        const mavlink_ida_event_t e = d.kart.olay(w::kOlaySafetyClose, 0, tf);
        id = e.event_id;
        d.kart.olay_gonder(e, t + 2 * kH);
    });
    d.sonraki_tf();
    d.ilerle_kadar(static_cast<std::int64_t>(A + 8u) * kH);
    KONTROL(oturum_suruyor(d, s0), "W4-K15 (3): sayıyı aşan SAFETY_CLOSE ihlal değildir; oturum sürer");
    KONTROL(!d.emniyet_b(A).izin_acik[0], "W4-K15 (3), A1-K87: SAFETY_CLOSE uygulanır");
    const auto r = sonuc_bekle(d, id);
    KONTROL(r && r->result == w::kSonucApplied, "SAFETY_CLOSE APPLIED");
}

VAKA("W4-K15.2.bekleyen_yanit_t_bag_yenilemez") {
    // Kart yalnız doğrulanamayan (olayını hiç göndermediği, n_events = 1) yanıtlar gönderir:
    // bekleyen yanıt canlılığı yenilemez, T_bağ son geçerli çerçeveden sayılır.
    Duzenek d;
    KONTROL(d.kur(), "kurulum");
    parti_ilerle(d, 10);
    const std::int64_t t_son = d.kart.partiler.back().tf.t;
    d.kart.el_sikisma_yanitla = false;
    d.kalici_yanitci = [&d](const GelenTf& tf, std::int64_t t) {
        d.kart.yanit_gonder(d.kart.yanit(tf, 1600, 1), t);
    };
    const std::int64_t t_bag = 500 * kMs;
    d.ilerle_kadar(t_son + t_bag + 20 * kP);
    std::uint64_t B_son = 0;
    for (std::uint64_t b = Duzenek::sinir(t_son); b < d.b(); ++b) {
        if (d.durum_b(b) != ida::kopru::OturumDurumu::kosuyor) {
            B_son = b;
            break;
        }
    }
    KONTROL(B_son != 0, "W4-K15 (2): yalnız bekleyen yanıtlar varken T_bağ dolar, oturum biter");
    if (B_son != 0) {
        const std::int64_t t_bitis = static_cast<std::int64_t>(B_son) * kH;
        KONTROL(t_bitis >= t_son + t_bag - kP && t_bitis <= t_son + t_bag + kP,
                "W4-K15 (2), A1-K23: oturum son geçerli çerçeveden T_bağ sonra biter; gözlenen "
                    << (t_bitis - t_son) / kMs << " ms");
    }
}

VAKA("W4-K15.4.bad_input_olay_beklemez") {
    // Hata dönüşü (BAD_INPUT, THR_VALID = 0) n_events = 1 dese de olay beklemeden tamamlanır ve
    // canlılığı yeniler (A1-K73).
    Duzenek d;
    KONTROL(d.kur(), "kurulum");
    parti_ilerle(d, 10);
    // Düzenek denetimi: aynı biçimde, olay saymayan (n_events = 0) BAD_INPUT yanıtı kabul edilir.
    std::uint64_t A0 = 0;
    d.sonraki_tf_icin([&d, &A0](const GelenTf& tf, std::int64_t t) {
        A0 = uygulama_siniri(tf);
        mavlink_ida_actuator_t a = d.kart.gecersiz_yanit(tf, 0);
        a.core_rc = kRcBadInput;
        d.kart.yanit_gonder(a, t);
    });
    d.sonraki_tf();
    d.ilerle_kadar(static_cast<std::int64_t>(A0) * kH);
    KONTROL(d.sonuc_b(A0).yuva_uygulandi && !d.sonuc_b(A0).yuva_kacirildi,
            "düzenek: n_events = 0 BAD_INPUT yanıtı yuvayı tamamlar (Tablo 4.6a)");
    parti_ilerle(d, 4);
    const std::uint16_t s0 = d.k.sess();
    const std::uint64_t kacan0 = d.k.sayaclar().parti_kacirildi;
    std::uint64_t A_ilk = 0;
    d.kalici_yanitci = [&d, &A_ilk](const GelenTf& tf, std::int64_t t) {
        if (A_ilk == 0) {
            A_ilk = uygulama_siniri(tf);
        }
        mavlink_ida_actuator_t a = d.kart.gecersiz_yanit(tf, 1);
        a.core_rc = kRcBadInput;
        d.kart.yanit_gonder(a, t);
    };
    d.ilerle_kadar(d.t_sonraki() + 700 * kMs);
    KONTROL(A_ilk != 0 && d.sonuc_b(A_ilk).yuva_uygulandi && !d.sonuc_b(A_ilk).yuva_kacirildi,
            "W4-K15 (4): BAD_INPUT yanıtı olay beklemeden yuvayı tamamlar");
    KONTROL_ESIT(d.k.sayaclar().parti_kacirildi, kacan0, "kaçırılan parti yok");
    KONTROL(oturum_suruyor(d, s0), "W4-K15 (4): BAD_INPUT yanıtları canlılığı yeniler (700 ms > T_bağ)");
}

}  // namespace
}  // namespace kabul
