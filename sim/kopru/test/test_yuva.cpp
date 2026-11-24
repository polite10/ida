// Kabul 3: aktüatör zamanlaması (A1-K72–K78; iskelet §4). Sahte kart yanıtları denetler.
#include "test_destek.hpp"

using namespace ida::kopru;
using namespace ida::kopru::test;

namespace {

constexpr std::uint16_t kThr = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_THR_VALID | IDAW_ACT_FLAGS_ARMED);

std::array<std::uint16_t, 8> pwm_seq(std::uint32_t seq)
{
    std::array<std::uint16_t, 8> p{};
    p.fill(1500);
    p[0] = static_cast<std::uint16_t>(1500u + (seq % 300u));
    return p;
}

}  // namespace

int test_yuva_zamanlama()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    std::uint32_t gec_seq = 0;
    std::uint32_t yinelenen_seq = 0;
    std::uint32_t gecersiz_seq = 0;
    bool kes = false;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        if (kes || tf.seq == gec_seq) {
            return;   // yanıt yok (geç gelecek ya da hiç)
        }
        if (tf.seq == gecersiz_seq) {
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(IDAW_ACT_FLAGS_ARMED), {}, 0, 3);   // BAD_INPUT
            return;
        }
        z.sk.yanit(tf.seq, kThr, pwm_seq(tf.seq));
        if (tf.seq == yinelenen_seq) {
            z.sk.yanit(tf.seq, kThr, pwm_seq(tf.seq + 7u));
        }
    };
    std::vector<std::uint32_t> seqler;
    std::vector<std::uint64_t> a_adim;
    for (int n = 0; n < 20; ++n) {
        const std::uint32_t s_ = z.periyot();
        seqler.push_back(s_);
        a_adim.push_back(z.sinir + kLactAdim);
    }
    // (a) Her yanıt tam a_n = t_n + L_act'te başlayan adımda uygulanır, önceki adımda değil.
    std::uint32_t hata_adim = 0;
    for (std::size_t j = 0; j + 2 < seqler.size(); ++j) {
        const std::uint64_t a = a_adim[j];
        if (z.sinirlar.size() <= a) {
            break;
        }
        if (z.sinirlar[a].pwm_us[0] != pwm_seq(seqler[j])[0] || !z.sinirlar[a].yuva_uygulandi ||
            z.sinirlar[a].yuva_seq != seqler[j]) {
            ++hata_adim;
        }
        if (j > 0 && z.sinirlar[a - 1].pwm_us[0] != pwm_seq(seqler[j - 1])[0]) {
            ++hata_adim;
        }
    }
    DOGRULA(d, hata_adim == 0u, "yanıt yalnız a_n adımında uygulanır (" + s(hata_adim) + " hata)");
    DOGRULA(d, z.sinirlar[a_adim[0] - 1].notr, "ilk yanıttan önce nötr");

    // (b) Geç yanıt: a_m'den sonra gelen yanıt reddedilir, komut değişmez; parti kaçırıldı.
    gec_seq = z.k.son_parti() + 1u;
    const std::uint32_t ss = z.periyot();
    DOGRULA(d, ss == gec_seq, "geç yanıtlı parti");
    const std::uint64_t a_m = z.sinir + kLactAdim;
    (void)z.periyot();
    (void)z.periyot();   // a_m bu periyodun son sınırında işlendi (yanıtsız)
    const SinirSonucu& sm = z.sinirlar[a_m];
    DOGRULA(d, sm.yuva_kacirildi && sm.yuva_seq == gec_seq, "a_m'de yuva boş: kaçırıldı (A1-K75)");
    DOGRULA(d, sm.pwm_us[0] == pwm_seq(gec_seq - 1u)[0], "komut değişmedi");
    z.sk.yanit(gec_seq, kThr, pwm_seq(gec_seq));
    alisveris(z.k, z.sk, z.t);
    DOGRULA(d, z.k.sayaclar().yanit_ret_gec == 1u, "geç yanıt reddedildi (A1-K72)");
    DOGRULA(d, z.k.sayaclar().parti_kacirildi == 1u, "parti_kacirildi 1");
    DOGRULA(d, (z.k.sayaclar().calistirma_gecersiz & (kGecKacirilanParti | kGecGecYanit)) ==
                   (kGecKacirilanParti | kGecGecYanit),
            "çalıştırma geçersiz (A1-K79)");

    // (c) Yinelenen yanıt reddedilir; THR_VALID = 0 yanıt yuvayı doldurur, komutu değiştirmez.
    gec_seq = 0;
    yinelenen_seq = z.k.son_parti() + 1u;
    gecersiz_seq = yinelenen_seq + 1u;
    std::vector<std::uint64_t> b_adim;
    std::vector<std::uint32_t> b_seq;
    for (int n = 0; n < 6; ++n) {
        b_seq.push_back(z.periyot());
        b_adim.push_back(z.sinir + kLactAdim);
    }
    DOGRULA(d, z.k.sayaclar().yanit_ret_yinelenen == 1u, "yinelenen yanıt reddedildi");
    DOGRULA(d, z.sinirlar[b_adim[0]].pwm_us[0] == pwm_seq(b_seq[0])[0], "ilk yanıt uygulandı, yinelenen değil");
    DOGRULA(d, z.sinirlar[b_adim[1]].yuva_seq == b_seq[1] && !z.sinirlar[b_adim[1]].yuva_kacirildi &&
                   z.sinirlar[b_adim[1]].pwm_us[0] == pwm_seq(b_seq[0])[0],
            "THR_VALID = 0: yuva doldu, komut değişmedi");

    // (d) Süre dolumu: yanıtlar kesilir; son komut a_k + valid_for'da nötre düşer (A1-K77).
    const std::uint64_t kac_bas = z.k.sayaclar().parti_kacirildi;
    kes = true;
    const std::uint64_t son_a = b_adim.back();
    for (int n = 0; n < 30; ++n) {
        (void)z.periyot();
    }
    const std::uint64_t bitis = son_a + 80u;   // 100 ms / 1,25 ms
    DOGRULA(d, !z.sinirlar[bitis - 1].notr && z.sinirlar[bitis - 1].pwm_us[0] == pwm_seq(b_seq.back())[0],
            "kullanım süresi içinde son komut sürer");
    DOGRULA(d, z.sinirlar[bitis].notr && z.sinirlar[bitis].pwm_us[0] == 1500.0, "a_k + 100 ms'de nötr");
    DOGRULA(d, z.k.sayaclar().komut_suresi_doldu == 1u, "komut_suresi_doldu 1");
    DOGRULA(d, z.k.sayaclar().parti_kacirildi > kac_bas, "yanıtsız partiler kaçırıldı");
    return d.hata;
}

int test_eszamansiz_guvenli()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    for (int n = 0; n < 5; ++n) {
        (void)z.periyot();
    }
    // Yuvalarda iki yanıt bekliyor (son iki parti); ızgara tick'inin güvenli çıktısı gelir.
    const std::uint32_t son = z.k.son_parti();
    std::array<std::uint16_t, 8> notr{};
    notr.fill(1500);
    z.sk.yanit(son, static_cast<std::uint16_t>(IDAW_ACT_FLAGS_THR_VALID | IDAW_ACT_FLAGS_IS_SAFE |
                                               IDAW_ACT_FLAGS_SRC_TICK | IDAW_ACT_FLAGS_ASYNC),
               notr);
    alisveris(z.k, z.sk, z.t);
    z.sinir_ilerle();
    const SinirSonucu& s0 = z.sinirlar[z.sinir];
    DOGRULA(d, !s0.notr && s0.pwm_us[0] == 1500.0, "eşzamansız çıktı alımdan sonraki ilk sınırda uygulandı");
    DOGRULA(d, z.k.sayaclar().esz_guvenli_uygulanan == 1u, "esz_guvenli_uygulanan 1");
    DOGRULA(d, z.k.sayaclar().yanit_iptal_esz == 2u, "önceden üretilmiş iki yanıt iptal (" +
                                                         s(z.k.sayaclar().yanit_iptal_esz) + ")");
    for (int j = 0; j < 12; ++j) {
        z.sinir_ilerle();
    }
    bool hep_notr = true;
    for (std::uint64_t b = z.sinir - 12; b <= z.sinir; ++b) {
        hep_notr = hep_notr && z.sinirlar[b].pwm_us[0] == 1500.0;
    }
    DOGRULA(d, hep_notr, "iptal edilen yanıtlar sonradan uygulanmadı");
    DOGRULA(d, z.k.sayaclar().parti_kacirildi == 0u, "iptal kaçırma sayılmaz");
    DOGRULA(d, (z.k.sayaclar().calistirma_gecersiz & kGecEszamansiz) != 0u, "çalıştırma geçersiz (A1-K79)");
    return d.hata;
}

// W4-K26 (1) (EMNİYET): SIL'de bekleme noktasında işlenen eşzamansız güvenli çıktı ve oturum
// sonu, sil_kisitlayan_uygula ile aynı sınırda yürürlüğe girer; HIL'de çağrı etkisizdir ve
// A1-K78/K23 olağan yoluyla (alımdan sonraki ilk sınır) uygulanır.
int test_sil_kisit_erken()
{
    Denetci d;
    std::array<std::uint16_t, 8> notr{};
    notr.fill(1500);
    const auto bayrak = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_THR_VALID | IDAW_ACT_FLAGS_IS_SAFE |
                                                   IDAW_ACT_FLAGS_SRC_TICK | IDAW_ACT_FLAGS_ASYNC);
    for (const CalistirmaKipi kip : {CalistirmaKipi::sil, CalistirmaKipi::hil_1x}) {
        const bool sil = kip == CalistirmaKipi::sil;
        const std::string ad = sil ? "SIL: " : "HIL 1x: ";
        KopruAyari a;
        a.kip = kip;
        SahteDuzenek z(a);
        z.sk.pwm.fill(1600);   // yürürlükteki komut nötr değil
        DOGRULA(d, z.el_sikis(), ad + "el sıkışma");
        for (int n = 0; n < 5; ++n) {
            (void)z.periyot();
        }
        const std::uint32_t son = z.k.son_parti();
        z.sk.yanit(son, bayrak, notr);
        alisveris(z.k, z.sk, z.t);
        const KisitSonucu k = z.k.sil_kisitlayan_uygula(z.sinir);
        if (sil) {
            DOGRULA(d, k.degisti && !k.notr && k.pwm_us[0] == 1500.0,
                    ad + "eşzamansız güvenli çıktı bekleme noktasının sınırında yürürlükte");
            DOGRULA(d, z.k.sayaclar().esz_guvenli_uygulanan == 1u && z.k.sayaclar().yanit_iptal_esz == 2u,
                    ad + "A1-K78 iptali aynı anda (" + s(z.k.sayaclar().yanit_iptal_esz) + ")");
        } else {
            DOGRULA(d, !k.degisti && z.k.sayaclar().esz_guvenli_uygulanan == 0u, ad + "sil_kisitlayan_uygula etkisiz");
            z.sinir_ilerle();
            DOGRULA(d, z.k.sayaclar().esz_guvenli_uygulanan == 1u && z.k.sayaclar().yanit_iptal_esz == 2u &&
                           z.sinirlar[z.sinir].pwm_us[0] == 1500.0,
                    ad + "A1-K78: alımdan sonraki ilk sınırda uygulandı");
        }
        // Kartın SESSION_CLOSE'u (A1-K23).
        mavlink_ida_session_close_t c{};
        c.sess = z.k.sess();
        c.reason = static_cast<std::uint8_t>(IDAW_CLOSE_REASON_SAFE_STATE);
        z.sk.yaz(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, c);
        alisveris(z.k, z.sk, z.t);
        DOGRULA(d, z.k.sayaclar().oturum_kopma == 1u, ad + "oturum bitti (sahte kartla yeni oturum hemen kurulur)");
        const KisitSonucu k2 = z.k.sil_kisitlayan_uygula(z.sinir);
        if (sil) {
            DOGRULA(d, k2.degisti && k2.notr, ad + "oturum sonu: iticiler bekleme noktasının sınırında nötr, izinler kapalı");
        } else {
            DOGRULA(d, !k2.degisti, ad + "oturum sonu: sil_kisitlayan_uygula etkisiz");
            z.sinir_ilerle();
            DOGRULA(d, z.sinirlar[z.sinir].notr, ad + "A1-K23: sonraki sınırdan itibaren nötr");
        }
    }
    return d.hata;
}

// W4-K6: ilk parti kapanmadan (batch_seq = 0) gelen eşzamansız güvenli çıktı uygulanır ama
// A1-K79 geçersizliğine sayılmaz; parti kapandıktan sonra gelen sayılır, batch_seq = 0 iddia
// etse de.
int test_eszamansiz_ilk_parti()
{
    Denetci d;
    constexpr std::uint16_t kEsz = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_THR_VALID | IDAW_ACT_FLAGS_IS_SAFE |
                                                              IDAW_ACT_FLAGS_SRC_TICK | IDAW_ACT_FLAGS_ASYNC);
    std::array<std::uint16_t, 8> notr{};
    notr.fill(1500);
    {
        SahteDuzenek z;
        DOGRULA(d, z.el_sikis(), "el sıkışma");
        z.sk.yanit(0, kEsz, notr);   // ilk TF'den önce (ızgara tick'i)
        alisveris(z.k, z.sk, z.t);
        z.sinir_ilerle();
        DOGRULA(d, z.k.sayaclar().esz_guvenli_uygulanan == 1u, "ilk parti öncesi çıktı uygulandı (A1-K78)");
        DOGRULA(d, z.k.sayaclar().eszamansiz_ilk_parti_oncesi == 1u, "ayrı sayaçta");
        DOGRULA(d, (z.k.sayaclar().calistirma_gecersiz & kGecEszamansiz) == 0u, "geçersiz kılmaz (W4-K6)");
        for (int n = 0; n < 4; ++n) {
            (void)z.periyot();
        }
        z.sk.yanit(z.k.son_parti(), kEsz, notr);   // ilk parti kapandıktan sonra
        alisveris(z.k, z.sk, z.t);
        z.sinir_ilerle();
        DOGRULA(d, z.k.sayaclar().esz_guvenli_uygulanan == 2u, "ikinci çıktı uygulandı");
        DOGRULA(d, (z.k.sayaclar().calistirma_gecersiz & kGecEszamansiz) != 0u, "parti kapandıktan sonra geçersiz kılar");
    }
    {
        SahteDuzenek z;
        DOGRULA(d, z.el_sikis(), "el sıkışma");
        for (int n = 0; n < 4; ++n) {
            (void)z.periyot();
        }
        z.sk.yanit(0, kEsz, notr);   // batch_seq = 0 iddiası, ama parti kapanmış
        alisveris(z.k, z.sk, z.t);
        z.sinir_ilerle();
        DOGRULA(d, z.k.sayaclar().esz_guvenli_uygulanan == 1u, "çıktı uygulandı");
        DOGRULA(d, z.k.sayaclar().eszamansiz_ilk_parti_oncesi == 0u, "muaf sayılmadı");
        DOGRULA(d, (z.k.sayaclar().calistirma_gecersiz & kGecEszamansiz) != 0u,
                "batch_seq = 0 iddiası parti kapandıktan sonra sayımdan kaçamaz");
    }
    return d.hata;
}
