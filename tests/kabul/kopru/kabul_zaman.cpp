// tests/kabul/kopru — sürekli komutun (IDA_ACTUATOR) uygulanması ve zamanlaması (vaka 7).
//
// Gözlem: Kopru::sinir_isle(b)'nin döndürdüğü SinirSonucu, b'de başlayan fizik adımının
// sürekli komutudur (kopru.hpp). Acil durdurma engeli bu katmanda değil sunucudadır (W3-K1;
// sunucu.hpp); o vaka kabul_sunucu.cpp'dedir.
//
// Kipler: HIL 1x (yanıt beklenmez; A1-K75 ile kaçırma ve A1-K78 eşzamansız çıktı bu kipte
// anlamlıdır; SIL'de ASYNC yoktur, A1-K100).
#include "ortak.hpp"

namespace kabul {
namespace {

constexpr double kNotr = 1500.0;

std::uint16_t parti_pwm(std::uint32_t seq) {
    return static_cast<std::uint16_t>(1200u + (seq % 100u) * 5u);
}

bool hep(const Duzenek& d, std::uint64_t b, double deger) {
    return d.hepsi(b, deger);
}

VAKA("A1-K74.uygulama_ani") {
    Duzenek d;
    KONTROL(d.kur(), "kurulum");
    d.kalici_yanitci = [&d](const GelenTf& tf, std::int64_t t) {
        d.kart.yanit_gonder(d.kart.yanit(tf, parti_pwm(tf.seq), 0), t);
    };
    parti_ilerle(d, 40);
    std::uint64_t ilk_A = 0;
    int denetlenen = 0;
    for (const auto& p : d.kart.partiler) {
        const std::uint64_t A = uygulama_siniri(p.tf);
        if (ilk_A == 0) {
            ilk_A = A;
        }
        if (A + 3u >= d.b()) {
            continue;
        }
        ++denetlenen;
        const auto& r = d.sonuc_b(A);
        const double beklenen = parti_pwm(p.tf.seq);
        KONTROL(r.yuva_uygulandi && r.yuva_seq == p.tf.seq,
                "A1-K74: yuva " << p.tf.seq << " tam a_n sınırında (" << A << ") uygulanır");
        KONTROL(hep(d, A, beklenen), "A1-K74: a_n sınırında parti " << p.tf.seq << "'in komutu");
        KONTROL(!hep(d, A - 1u, beklenen),
                "A1-K74: bir adım önce (" << (A - 1u) << ") parti " << p.tf.seq << "'in komutu yok");
        for (std::uint64_t b = A + 1u; b < A + 4u; ++b) {
            KONTROL(hep(d, b, beklenen), "komut sonraki yuvaya kadar yürürlükte (sınır " << b << ")");
        }
    }
    KONTROL(denetlenen >= 30, "yeterli parti denetlendi: " << denetlenen);
    for (std::uint64_t b = 0; b < ilk_A; ++b) {
        KONTROL(hep(d, b, kNotr) && d.sonuc_b(b).notr, "ilk a_n'den önce nötr (sınır " << b << ")");
    }
}

VAKA("A1-K72.gec_yanit") {
    Duzenek d;
    KONTROL(d.kur(), "kurulum");
    parti_ilerle(d, 10);
    const auto s0 = d.k.sayaclar();
    std::uint64_t A = 0;
    d.sonraki_tf_icin([&d, &A](const GelenTf& tf, std::int64_t) {
        A = uygulama_siniri(tf);
        // sinir_isle(A)'dan sonra gelir: t_birak = (A + 1)·h.
        d.kart.yanit_gonder(d.kart.yanit(tf, 1777, 0), static_cast<std::int64_t>(A + 1u) * kH);
    });
    // Akış üretim sırasını korur (A1-K105): sonraki iki partinin yanıtı da gecikmeli yanıtın
    // ardından gelir (kendi a_n'lerine yetişir).
    for (int i = 0; i < 2; ++i) {
        d.sonraki_tf_icin([&d, &A](const GelenTf& tf, std::int64_t) {
            d.kart.yanit_gonder(d.kart.yanit(tf, 1600, 0), static_cast<std::int64_t>(A + 1u) * kH);
        });
    }
    parti_ilerle(d, 12);
    KONTROL(hep(d, A + 4u, 1600.0) && d.sonuc_b(A + 4u).yuva_uygulandi,
            "düzenek: sonraki parti zamanında uygulanır");
    KONTROL(A > 0, "düzenek");
    KONTROL(d.sonuc_b(A).yuva_kacirildi, "A1-K75: HIL 1x'te a_n'de boş yuva kaçırılır");
    KONTROL(hep(d, A, 1600.0), "A1-K75: kaçırılan partide komut değişmez");
    bool uygulandi = false;
    for (std::uint64_t b = 0; b < d.b(); ++b) {
        uygulandi = uygulandi || d.pwm0(b) == 1777.0;
    }
    KONTROL(!uygulandi, "A1-K72: geç yanıt hiçbir sınırda uygulanmaz");
    const auto& s1 = d.k.sayaclar();
    KONTROL_ESIT(s1.yanit_ret_gec, s0.yanit_ret_gec + 1u,
                 "A1-K72, Tablo 3.11a: geç yanıt yanit_ret_gec'e sayılır (yinelenen "
                     << s1.yanit_ret_yinelenen - s0.yanit_ret_yinelenen << ", eski_uretim "
                     << s1.yanit_ret_eski_uretim - s0.yanit_ret_eski_uretim << ", bag_ret "
                     << s1.bag_ret - s0.bag_ret << ", alan_ret " << s1.alan_ret - s0.alan_ret
                     << ", oturum " << s1.yanit_ret_oturum - s0.yanit_ret_oturum << ")");
    KONTROL_ESIT(d.k.sayaclar().parti_kacirildi, s0.parti_kacirildi + 1u, "parti_kacirildi");
}

VAKA("A1-K72.yinelenen_yanit") {
    Duzenek d;
    KONTROL(d.kur(), "kurulum");
    parti_ilerle(d, 10);
    const auto s0 = d.k.sayaclar();
    std::uint64_t A = 0;
    d.sonraki_tf_icin([&d, &A](const GelenTf& tf, std::int64_t t) {
        A = uygulama_siniri(tf);
        d.kart.yanit_gonder(d.kart.yanit(tf, 1650, 0), t);
        d.kart.yanit_gonder(d.kart.yanit(tf, 1750, 0), t);  // aynı parti, yeni out_seq
    });
    parti_ilerle(d, 4);
    std::uint64_t A2 = 0;
    d.sonraki_tf_icin([&d, &A2](const GelenTf& tf, std::int64_t t) {
        A2 = uygulama_siniri(tf);
        mavlink_ida_actuator_t a = d.kart.yanit(tf, 1660, 0);
        d.kart.out_seq_ata(d.kart.out_seq() + 1u);
        a.out_seq = d.kart.out_seq();
        const auto bayt = d.kart.kodla(MAVLINK_MSG_ID_IDA_ACTUATOR, &a, sizeof a);
        d.kart.ham_gonder(bayt, t);
        d.kart.ham_gonder(bayt, t);  // bayt bayt aynı kopya
    });
    parti_ilerle(d, 4);
    KONTROL(hep(d, A, 1650.0), "A1-K72: yuvaya ilk gelen yanıt uygulanır");
    bool ikinci = false;
    for (std::uint64_t b = 0; b < d.b(); ++b) {
        ikinci = ikinci || d.pwm0(b) == 1750.0;
    }
    KONTROL(!ikinci, "A1-K72: yuvası dolu partinin ikinci yanıtı uygulanmaz");
    KONTROL_ESIT(d.k.sayaclar().yanit_ret_yinelenen, s0.yanit_ret_yinelenen + 1u,
                 "yanit_ret_yinelenen (farklı out_seq'li ikinci yanıt)");
    KONTROL(hep(d, A2, 1660.0), "kopyanın ilki uygulanır");
    const auto& s = d.k.sayaclar();
    KONTROL_ESIT((s.yanit_ret_yinelenen + s.yanit_ret_eski_uretim),
                 (s0.yanit_ret_yinelenen + s0.yanit_ret_eski_uretim + 2u),
                 "bayt bayt aynı kopya reddedilir (V5 ya da A1-K72)");
}

VAKA("A1-K77.gecerlilik_siniri") {
    Duzenek d;
    KONTROL(d.kur(), "kurulum");
    parti_ilerle(d, 10);
    const GelenTf son = d.kart.partiler.back().tf;  // son geçerli komutun partisi
    d.kalici_yanitci = [&d](const GelenTf& tf, std::int64_t t) {
        d.kart.yanit_gonder(d.kart.gecersiz_yanit(tf, 0), t);  // THR_VALID = 0
    };
    const auto s0 = d.k.sayaclar();
    const std::uint64_t A0 = uygulama_siniri(son);
    const std::int64_t gecerlilik = 100 * kMs;  // KopruAyari ve kart: valid_for_ns
    const std::uint64_t E = Duzenek::sinir(static_cast<std::int64_t>(A0) * kH + gecerlilik);
    d.ilerle_kadar(static_cast<std::int64_t>(E + 8u) * kH);
    for (std::uint64_t b = A0; b < E; ++b) {
        KONTROL(hep(d, b, 1600.0) && !d.sonuc_b(b).notr,
                "A1-K74: komut a_n + valid_for'dan önce yürürlükte (sınır " << b << ")");
    }
    // EMNİYET okuması: süre a_n + valid_for anında dolar; o anda başlayan adım nötrdür.
    KONTROL(d.sonuc_b(E).notr && hep(d, E, kNotr),
            "A1-K77: a_n + valid_for sınırında (" << E << ") nötr");
    for (std::uint64_t b = E; b < E + 8u; ++b) {
        KONTROL(hep(d, b, kNotr), "A1-K77: nötr kalır (sınır " << b << ")");
    }
    KONTROL_ESIT(d.k.sayaclar().komut_suresi_doldu, s0.komut_suresi_doldu + 1u,
                 "komut_suresi_doldu");
}

VAKA("A1-K74.thr_valid_sifir") {
    Duzenek d;
    KONTROL(d.kur(), "kurulum");
    parti_ilerle(d, 10);
    const auto s0 = d.k.sayaclar();
    std::uint64_t A = 0;
    d.sonraki_tf_icin([&d, &A](const GelenTf& tf, std::int64_t t) {
        A = uygulama_siniri(tf);
        d.kart.yanit_gonder(d.kart.gecersiz_yanit(tf, 0), t);
    });
    d.sonraki_tf();
    d.varsayilan_pwm = 1700;
    parti_ilerle(d, 6);
    KONTROL(d.sonuc_b(A).yuva_uygulandi && !d.sonuc_b(A).yuva_kacirildi,
            "A1-K74: THR_VALID = 0 yuvayı yanıtlanmış sayar");
    KONTROL(hep(d, A, 1600.0) && !d.sonuc_b(A).notr, "A1-K74: yürürlükteki komut değişmez");
    KONTROL(hep(d, A + 4u, 1700.0), "sonraki yuva olağan uygulanır");
    KONTROL_ESIT(d.k.sayaclar().parti_kacirildi, s0.parti_kacirildi, "kaçırma sayılmaz");
}

VAKA("A1-K78.eszamansiz_guvenli") {
    Duzenek d;
    KONTROL(d.kur(), "kurulum");
    parti_ilerle(d, 10);
    const auto s0 = d.k.sayaclar();
    std::uint64_t Bn = 0;
    d.sonraki_tf_icin([&d, &Bn](const GelenTf& tf, std::int64_t t) {
        Bn = Duzenek::sinir(tf.t);
        d.kart.yanit_gonder(d.kart.yanit(tf, 1700, 0), t);
        // Izgara tick'inin güvenli çıktısı (A1-K44): sinir_isle(Bn+1)'den sonra gelir.
        mavlink_ida_actuator_t a{};
        a.batch_seq = tf.seq;
        a.valid_for_ns = d.kart.ayar().valid_for_ns;
        for (auto& p : a.pwm_us) {
            p = 1500;
        }
        a.sess = d.kart.sess();
        a.act_flags = static_cast<std::uint16_t>(w::kThrValid | w::kIsSafe | w::kSrcTick | w::kAsync);
        a.core_rc = w::kRcOk;
        d.kart.yanit_gonder(a, t + 2 * kH);
    });
    d.sonraki_tf();
    d.varsayilan_pwm = 1650;
    parti_ilerle(d, 6);
    KONTROL(Bn > 0, "düzenek");
    KONTROL(hep(d, Bn + 1u, 1600.0), "ASYNC'ten önce komut 1600");
    KONTROL(hep(d, Bn + 2u, kNotr), "A1-K78: ASYNC alımından sonraki ilk sınırda nötr");
    for (std::uint64_t b = Bn + 2u; b < Bn + 12u; ++b) {
        KONTROL(hep(d, b, kNotr),
                "A1-K78: out_seq'i küçük bekleyen yuvalar (n−1, n) iptal; sınır " << b << " nötr");
    }
    KONTROL(hep(d, Bn + 12u, 1650.0), "A1-K78: out_seq'i büyük olağan yanıt kendi a_n'inde");
    KONTROL_ESIT(d.k.sayaclar().esz_guvenli_uygulanan, s0.esz_guvenli_uygulanan + 1u,
                 "esz_guvenli_uygulanan");
    KONTROL(d.k.sayaclar().yanit_iptal_esz >= s0.yanit_iptal_esz + 2u, "yanit_iptal_esz ≥ 2");
}

void oturum_sonu_notr(bool kart_kapatir) {
    Duzenek d;
    KONTROL(hazirla(d), "kurulum");
    KONTROL(izin_ac(d, 0, 450'000'000u) >= 0, "ön koşul: izin");
    parti_ilerle(d, 2);
    const std::uint32_t r0 = rev(d);
    // Kapanış bir yuva sınırı olmayan sınırda (B), bekleyen yuvalar varken.
    std::uint64_t B = d.b() + 1u;
    while (B % 4u != 2u) {
        ++B;
    }
    if (kart_kapatir) {
        d.kart.oturum_kapat_gonder(w::kKapanisShutdown, static_cast<std::int64_t>(B) * kH);
    } else {
        d.sinir_sonrasi = [&d, B](std::uint64_t b) {
            if (b + 1u == B) {
                d.k.oturum_kapat(w::kKapanisShutdown, d.duvar(static_cast<std::int64_t>(b) * kH));
            }
        };
    }
    d.ilerle_kadar(static_cast<std::int64_t>(B - 1u) * kH);
    d.varsayilan_pwm = 1700;  // yeni oturumun komutları
    d.ilerle_kadar(static_cast<std::int64_t>(B + 40u) * kH);
    d.sinir_sonrasi = nullptr;
    const char* kim = kart_kapatir ? "kart kapattı" : "sim kapattı";
    KONTROL(hep(d, B - 1u, 1600.0), "kapanıştan önce komut yürürlükte (" << kim << ")");
    KONTROL(hep(d, B, kNotr), "A1-K23: oturum sonunda sonraki sınırdan itibaren nötr (" << kim << ")");
    KONTROL(!d.emniyet_b(B).izin_acik[0], "A1-K23: izinler kapanır (" << kim << ")");
    KONTROL_ESIT(d.emniyet_b(B).emniyet_revizyonu, r0 + 1u, "A1-K106 md.4 (" << kim << ")");
    bool eski = false;
    for (std::uint64_t b = B; b < B + 40u; ++b) {
        eski = eski || d.pwm0(b) == 1600.0;
    }
    KONTROL(!eski, "A1-K23: eski oturumun bekleyen yuvaları silinir, uygulanmaz (" << kim << ")");
}

VAKA("A1-K23.oturum_kapaninca_notr") {
    oturum_sonu_notr(true);
    oturum_sonu_notr(false);
}

VAKA("A1-K23.t_bag_asimi") {
    Duzenek d;  // HIL 1x: T_bağ uygulanır (SIL'de uygulanmaz, A1-K100)
    KONTROL(d.kur(), "kurulum");
    parti_ilerle(d, 10);
    const std::int64_t t_son = d.kart.partiler.back().tf.t;  // kartın son çerçevesi bu TF'nin yanıtı
    d.varsayilan_acik = false;
    d.kart.el_sikisma_yanitla = false;
    const std::uint32_t r0 = rev(d);
    const std::int64_t t_bag = 500 * kMs;
    d.ilerle_kadar(t_son + t_bag + 20 * kP);
    std::uint64_t B_son = 0;
    for (std::uint64_t b = Duzenek::sinir(t_son); b < d.b(); ++b) {
        if (d.durum_b(b) != ida::kopru::OturumDurumu::kosuyor) {
            B_son = b;
            break;
        }
    }
    KONTROL(B_son != 0, "A1-K23: T_bağ aşımında oturum biter");
    const std::int64_t t_bitis = static_cast<std::int64_t>(B_son) * kH;
    KONTROL(t_bitis >= t_son + t_bag - kP && t_bitis <= t_son + t_bag + kP,
            "A1-K23: oturum T_bağ (500 ms) sonra biter; gözlenen " << (t_bitis - t_son) / kMs << " ms");
    if (B_son != 0) {
        KONTROL(hep(d, B_son, kNotr), "oturum sonunda nötr");
        KONTROL_ESIT(d.emniyet_b(B_son).emniyet_revizyonu, r0 + 1u, "A1-K106 md.4");
        KONTROL(!d.emniyet_b(B_son).izin_acik[0] && !d.emniyet_b(B_son).izin_acik[1], "izinler kapalı");
    }
}

}  // namespace
}  // namespace kabul
