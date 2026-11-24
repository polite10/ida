// W4-duraklat (EMNİYET; W4-K28 (2)): sınırın duran kipi. Duraklatılmış çalıştırmada aynı sınır
// `sinir_isle` ile (aynı işlev) yeniden işlenir; köprüde ayrı bir artımlı emniyet yolu yoktur.
// Testler W4-bagla-a3'ün iki sol incelemesinin köprü düzeyindeki karşı örnekleridir; her emniyet
// testi önce nötr dışı bir komutun ya da açık bir iznin yürürlükte olduğunu doğrular.
#include "test_destek.hpp"

using namespace ida::kopru;
using namespace ida::kopru::test;

namespace {

constexpr std::uint16_t kThr = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_THR_VALID);
constexpr std::uint16_t kArmed = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_ARMED);
constexpr std::uint16_t kSafe = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_IS_SAFE);
constexpr std::uint16_t kTick = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_SRC_TICK);
constexpr std::uint16_t kAsync = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_ASYNC);

KopruAyari hil()
{
    KopruAyari ka;
    ka.kip = CalistirmaKipi::hil_1x;
    return ka;
}

std::array<std::uint16_t, 8> pwm_hepsi(std::uint16_t v)
{
    std::array<std::uint16_t, 8> p{};
    p.fill(v);
    return p;
}

EmniyetAtama op_acil(std::uint32_t kimlik, bool basili, std::optional<std::uint32_t> rev = std::nullopt)
{
    EmniyetAtama a;
    a.acil_durdurma = basili;
    a.operator_kimligi = kimlik;
    a.beklenen_rev = rev;
    return a;
}

EmniyetAtama op_pim(std::uint32_t kimlik, int tup, bool takili, std::optional<std::uint32_t> rev = std::nullopt)
{
    EmniyetAtama a;
    a.pim[static_cast<std::size_t>(tup)] = takili;
    a.operator_kimligi = kimlik;
    a.beklenen_rev = rev;
    return a;
}

const OperatorSonucu* sonuc(const SinirSonucu& s, std::uint32_t kimlik)
{
    for (const OperatorSonucu& o : s.operator_sonuclari) {
        if (o.kimlik == kimlik) {
            return &o;
        }
    }
    return nullptr;
}

// Duran kipte sınırın bir turu: aynı sınır numarasıyla aynı işlev (zaman ilerlemez).
SinirSonucu duran_tur(SahteDuzenek& z)
{
    SinirSonucu s = z.k.sinir_isle(z.sinir, z.t);
    alisveris(z.k, z.sk, z.t);
    return s;
}

// Fiş takılı, pim çıkık (izin açılabilir; ilk periyot).
void gevsek_hazirla(SahteDuzenek& z)
{
    EmniyetAtama a;
    a.fis = {true, true};
    a.pim = {false, false};
    z.k.emniyet_ata(a);
    (void)z.periyot();
}

// Nötr dışı (1700 µs) yanıt veren sahte kart; altı periyot (yuvalar dolu, komut yürürlükte).
void notr_disi_kur(SahteDuzenek& z)
{
    z.sk.tf_kancasi = [&z](const SahteKart::Tf& tf) { z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1700)); };
    for (int n = 0; n < 6; ++n) {
        (void)z.periyot();
    }
}

std::size_t mesaj_say(const std::vector<std::uint8_t>& v, std::uint32_t msgid)
{
    ida_link_alici_t a{};
    (void)ida_link_alici_kur(&a, IDA_LINK_ROL_KART);
    std::size_t n = 0;
    std::size_t k = 0;
    while (k < v.size()) {
        std::size_t tuk = 0;
        ida_link_cerceve_t c{};
        const ida_link_rc_t rc = ida_link_al(&a, v.data() + k, v.size() - k, &tuk, &c);
        k += tuk;
        if (rc == IDA_LINK_CERCEVE && (msgid == 0u || c.msgid == msgid)) {
            ++n;
        }
        if (tuk == 0u) {
            break;
        }
    }
    return n;
}

}  // namespace

// Sol-1 #1: senaryo sınır b'de acili bırakır (sınır b ilerleyen kipte BİR KEZ işlenir), duraklatmada
// operatör basar → aynı sınırın duran turunda uygulanır, revizyon +1; sürdürmede senaryonun bırakması
// yeniden girmez: basılı kalır. Duran kipte sınır b'nin yeniden işlenmesi zaman çizelgesini yinelemez.
int test_duran_operator_basili_kalir()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    notr_disi_kur(z);
    EmniyetAtama bas;
    bas.acil_durdurma = true;
    z.k.emniyet_ata(bas);
    (void)z.periyot();
    EmniyetAtama birak;
    birak.acil_durdurma = false;
    z.k.emniyet_ata(birak);
    z.sinir_ilerle();   // sınır b: senaryonun bırakması (ilerleyen kip, bir kez)
    DOGRULA(d, !z.k.acil_durdurma(), "senaryo b'de bıraktı");
    const std::uint32_t r = z.k.emniyet_revizyonu();
    z.k.duraklatma_ayarla(true);
    z.k.emniyet_ata(op_acil(31, true));
    const SinirSonucu s1 = duran_tur(z);
    const OperatorSonucu* o = sonuc(s1, 31);
    DOGRULA(d, o != nullptr && o->sonuc == AtamaSonucu::uygulandi && o->degisti, "duran turda operatörün basması uygulandı");
    DOGRULA(d, z.k.acil_durdurma() && z.k.emniyet_revizyonu() == r + 1u && s1.notr, "basılı, revizyon +1, girdi nötr");
    // Aynı sınırın birkaç turu daha: değişiklik yok (senaryonun bırakması yeniden girmez).
    for (int i = 0; i < 5; ++i) {
        const SinirSonucu s = duran_tur(z);
        DOGRULA(d, s.notr && !s.gevsetme_reddedildi && s.operator_sonuclari.empty(), "duran tur yan etkisiz");
    }
    DOGRULA(d, z.k.emniyet_revizyonu() == r + 1u, "turlar revizyonu değiştirmedi");
    z.k.duraklatma_ayarla(false);
    bool notr = true;
    for (int j = 0; j < 40; ++j) {
        z.sinir_ilerle();
        notr = notr && z.sinirlar.back().notr;
    }
    DOGRULA(d, z.k.acil_durdurma() && notr, "sürdürmeden sonra basılı kaldı, itki nötr");
    return d.hata;
}

// Sol-1 #1'in ikinci yarısı ve W4-K3: nötr dışı komut yürürlükteyken duraklatmada bas → bırak (güncel
// revizyonla, ayrı turlarda) → sürdür: basmadan önce kabul edilmiş hiçbir komut geri gelmez; aynı turda
// bırak (eski revizyon) + bas: kısıtlayan önce, bırakma kisitlama_hakim.
int test_duran_bas_birak()
{
    Denetci d;
    SahteDuzenek y;
    DOGRULA(d, y.el_sikis(), "el sıkışma");
    notr_disi_kur(y);
    DOGRULA(d, !y.sinirlar.back().notr && y.sinirlar.back().pwm_us[0] == 1700.0, "ön koşul: nötr dışı komut yürürlükte");
    y.sk.tf_kancasi = nullptr;   // bundan sonra yanıt yok: bekleyen yuvalar basmadan önce gönderilmiş partilerin
    y.sk.oto_yanit = false;
    y.k.duraklatma_ayarla(true);
    y.k.emniyet_ata(op_acil(41, true));
    DOGRULA(d, duran_tur(y).notr, "basıldığı turda girdi nötr");
    y.k.emniyet_ata(op_acil(42, false, y.k.emniyet_revizyonu()));
    const SinirSonucu s = duran_tur(y);
    const OperatorSonucu* o = sonuc(s, 42);
    DOGRULA(d, o != nullptr && o->sonuc == AtamaSonucu::uygulandi && !y.k.acil_durdurma(), "sonraki turda bırakma uygulandı");
    DOGRULA(d, s.notr, "bırakma turunda girdi nötr (eski komut geri gelmedi)");
    y.k.duraklatma_ayarla(false);
    bool notr = true;
    for (int j = 0; j < 12; ++j) {
        y.sinir_ilerle();
        notr = notr && y.sinirlar.back().notr;
    }
    DOGRULA(d, notr, "sürdürmeden sonra eski nötr dışı komut geri gelmedi (W4-K3)");
    // Aynı duran turda bırakma (güncel revizyon) + bas: kısıtlayan önce; bırakma kisitlama_hakim.
    y.k.duraklatma_ayarla(true);
    const std::uint32_t r2 = y.k.emniyet_revizyonu();
    y.k.emniyet_ata(op_acil(52, false, r2));
    y.k.emniyet_ata(op_acil(51, true));
    const SinirSonucu s2 = duran_tur(y);
    DOGRULA(d, s2.operator_sonuclari.size() == 2u && s2.operator_sonuclari.front().kimlik == 51u &&
                   sonuc(s2, 52) != nullptr && sonuc(s2, 52)->sonuc == AtamaSonucu::kisitlama_hakim && y.k.acil_durdurma(),
            "aynı turda kısıtlayan önce; gevşeten kisitlama_hakim (A1-K85 (4))");
    y.k.duraklatma_ayarla(false);
    return d.hata;
}

// Sol-2 #3 (HIL): duraklatmada kart oturumu kapatır (IDA_SESSION_CLOSE). İzin açıkken: sonraki duran
// turda izinler kapanır ve revizyon artar (A1-K85 (1)); aynı turda verilmiş eski revizyonlu gevşetme
// reddedilir (kisitlama_hakim), sonraki turda eski revizyonla gelen de (rev_uyusmuyor).
int test_duran_oturum_sonu()
{
    Denetci d;
    SahteDuzenek z(hil());
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.emniyet_revizyonu();
    bool bir_kez = true;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        if (bir_kez) {
            bir_kez = false;
            (void)z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, tf.seq, tf.t, rev);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kSafe | kArmed), pwm_hepsi(1500), 1);
            return;
        }
        z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600));
    };
    for (int n = 0; n < 3; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, z.k.izin_acik(0) && !z.sinirlar.back().notr, "ön koşul: tüp 0'ın izni açık, nötr dışı komut yürürlükte");
    z.k.duraklatma_ayarla(true);
    const std::uint32_t r0 = z.k.emniyet_revizyonu();
    // Kart oturumu kapatır; aynı turda eski (güncel) revizyonla pim gevşetmesi.
    mavlink_ida_session_close_t kapanis{};
    kapanis.sess = z.sk.sess;
    kapanis.reason = static_cast<std::uint8_t>(IDAW_CLOSE_REASON_SHUTDOWN);
    z.sk.yaz(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, kapanis);
    std::vector<std::uint8_t> w;
    w.swap(z.sk.giden);
    z.k.al(w.data(), w.size(), z.t);
    z.k.emniyet_ata(op_pim(61, 1, false, r0));
    const SinirSonucu s = z.k.sinir_isle(z.sinir, z.t);
    DOGRULA(d, !z.k.izin_acik(0) && !z.k.izin_acik(1), "duran turda izinler kapandı (A1-K23, K85 (1))");
    DOGRULA(d, z.k.emniyet_revizyonu() == r0 + 1u, "oturum sonu revizyonu +1 (A3-K117 (4))");
    DOGRULA(d, s.notr, "itici girdisi nötr (A1-K23)");
    const OperatorSonucu* o = sonuc(s, 61);
    DOGRULA(d, o != nullptr && o->sonuc == AtamaSonucu::kisitlama_hakim, "aynı turdaki eski revizyonlu gevşetme reddedildi");
    z.k.emniyet_ata(op_pim(62, 1, false, r0));
    const SinirSonucu s2 = z.k.sinir_isle(z.sinir, z.t);
    const OperatorSonucu* o2 = sonuc(s2, 62);
    DOGRULA(d, o2 != nullptr && o2->sonuc == AtamaSonucu::rev_uyusmuyor && z.k.emniyet_revizyonu() == r0 + 1u,
            "sonraki turda eski revizyonlu gevşetme rev_uyusmuyor");
    return d.hata;
}

// Sol-2 #3 (HIL): duraklatmada SAFETY_CLOSE (yuvası ileri bir sınırda) ve eşzamansız güvenli çıktı.
// İkisi de sonraki duran turda, fizik ilerlemeden uygulanır: izin kapanır (revizyon +1), yürürlükteki
// nötr dışı komut ve bekleyen yuvalar eşzamansız çıktıyla iptal edilir (A1-K78).
int test_duran_kapanis_esz()
{
    Denetci d;
    SahteDuzenek z(hil());
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    gevsek_hazirla(z);
    const std::uint32_t rev = z.k.emniyet_revizyonu();
    bool bir_kez = true;
    z.sk.tf_kancasi = [&](const SahteKart::Tf& tf) {
        if (bir_kez) {
            bir_kez = false;
            (void)z.sk.olay(IDAW_EVENT_KIND_SAFETY_OPEN, 0, tf.seq, tf.t, rev);
            z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kSafe | kArmed), pwm_hepsi(1500), 1);
            return;
        }
        z.sk.yanit(tf.seq, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1600));
    };
    for (int n = 0; n < 4; ++n) {
        (void)z.periyot();
    }
    DOGRULA(d, z.k.izin_acik(0) && !z.sinirlar.back().notr, "ön koşul: izin açık, nötr dışı komut yürürlükte");
    // Duraklatmadan önceki son parti: yanıtı geldi, yuvası (a_n) ileride.
    z.sk.tf_kancasi = nullptr;
    z.sk.oto_yanit = false;
    z.sinir_ilerle();
    z.sinir_ilerle();
    z.sinir_ilerle();
    z.sinir_ilerle();
    const std::uint32_t son = z.k.parti_kapat(z.t, z.t).value_or(0u);
    alisveris(z.k, z.sk, z.t);
    z.k.duraklatma_ayarla(true);
    const std::uint32_t r0 = z.k.emniyet_revizyonu();
    // Kart, son partinin çağrısında izni kapatır (SAFETY_CLOSE, yuvası a_n'de) ve yanıt verir; sonra
    // ızgara tick'inin eşzamansız güvenli çıktısı.
    (void)z.sk.olay(IDAW_EVENT_KIND_SAFETY_CLOSE, 0, son, z.t, 0);
    z.sk.yanit(son, static_cast<std::uint16_t>(kThr | kArmed), pwm_hepsi(1700), 1);
    z.sk.yanit(son, static_cast<std::uint16_t>(kThr | kSafe | kTick | kAsync), pwm_hepsi(1500));
    std::vector<std::uint8_t> w;
    w.swap(z.sk.giden);
    z.k.al(w.data(), w.size(), z.t);
    const std::uint64_t iptal0 = z.k.sayaclar().yanit_iptal_esz;
    const SinirSonucu s = z.k.sinir_isle(z.sinir, z.t);
    DOGRULA(d, !z.k.izin_acik(0) && z.k.emniyet_revizyonu() == r0 + 1u, "SAFETY_CLOSE yuvasını beklemeden uygulandı; revizyon +1");
    DOGRULA(d, s.notr || (s.pwm_us[0] == 1500.0), "eşzamansız güvenli çıktı uygulandı: girdi nötr");
    DOGRULA(d, z.k.sayaclar().esz_guvenli_uygulanan >= 1u && z.k.sayaclar().yanit_iptal_esz > iptal0,
            "bekleyen (nötr dışı) yuva eşzamansız çıktıyla iptal edildi (A1-K78)");
    DOGRULA(d, z.k.esz_bekleyen_sayisi() == 0u, "bekleyen eşzamansız çıktı tüketildi");
    z.k.duraklatma_ayarla(false);
    bool notr = true;
    for (int j = 0; j < 12; ++j) {
        z.sinir_ilerle();
        notr = notr && (z.sinirlar.back().notr || z.sinirlar.back().pwm_us[0] == 1500.0);
    }
    DOGRULA(d, notr, "sürdürmeden sonra iptal edilen nötr dışı yanıt uygulanmadı");
    return d.hata;
}

// A1-K41: duran kipte daraltıcı komut tele çıkmaz; revizyonu sonraki duran turda artırır (A3-K117
// (1)); ikinci daraltıcı birleşir (A1-K61; düşmez); kip kalkınca ilk sınırda yazılır.
int test_duran_daraltici_tel()
{
    Denetci d;
    SahteDuzenek z(hil());
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    (void)z.periyot();
    z.k.duraklatma_ayarla(true);
    std::vector<std::uint8_t> v;
    z.k.giden_al(v);   // önceki çıkış boşaltıldı
    v.clear();
    const std::uint32_t r0 = z.k.emniyet_revizyonu();
    const std::uint32_t c1 = z.k.komut_basit(IDAW_CMD_TYPE_DISARM, 0, z.t);
    (void)z.k.sinir_isle(z.sinir, z.t);
    const std::uint32_t c2 = z.k.komut_basit(IDAW_CMD_TYPE_DISARM, 0, z.t);
    (void)z.k.sinir_isle(z.sinir, z.t);
    z.k.giden_al(v);
    DOGRULA(d, mesaj_say(v, MAVLINK_MSG_ID_IDA_CMD_SIMPLE) == 0u, "duran kipte daraltıcı tele çıkmadı");
    DOGRULA(d, z.k.emniyet_revizyonu() == r0 + 2u, "iki daraltıcı, iki revizyon (A3-K117 (1)); bekletilmedi");
    const auto s1 = z.k.komut_sonucu(c1);
    DOGRULA(d, s1 && s1->yerel && z.k.sayaclar().komut_birlestirildi == 1u, "ilk DISARM ikinciyle birleşti (A1-K61; düşmedi)");
    (void)c2;
    z.k.duraklatma_ayarla(false);
    v.clear();
    z.sinir_ilerle();
    // sinir_ilerle alışverişi sahte karta verdi: kartın aldığı komutlar.
    DOGRULA(d, !z.sk.komutlar.empty() && z.sk.komutlar.back().type == IDAW_CMD_TYPE_DISARM,
            "kip kalkınca ilk sınırda DISARM yazıldı");
    return d.hata;
}

// Sol-2 #4'ün duran kip karşılığı: ilk TF'den önce duran kipte (HIL) 360 000 eşzamansız güvenli çıktı
// (10 ms ızgarada bir saat); her 1000'de bir duran tur. Bekleyen en çok 1; her tur en yenisini uygular.
int test_duran_esz_sinirli()
{
    Denetci d;
    SahteDuzenek z(hil());
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    z.k.duraklatma_ayarla(true);
    const auto bayrak = static_cast<std::uint16_t>(kThr | kSafe | kTick | kAsync);
    std::size_t en_cok = 0;
    for (int i = 0; i < 360'000; ++i) {
        z.sk.yanit(0u, bayrak, pwm_hepsi(1500));
        if (i % 1000 == 999) {
            alisveris(z.k, z.sk, z.t);
            en_cok = std::max(en_cok, z.k.esz_bekleyen_sayisi());
            (void)duran_tur(z);
            en_cok = std::max(en_cok, z.k.esz_bekleyen_sayisi());
        }
    }
    const Sayaclar& c = z.k.sayaclar();
    DOGRULA(d, z.k.son_parti() == 0u, "ön koşul: ilk TF öncesi");
    DOGRULA(d, en_cok <= 1u, "bekleyen eşzamansız çıktı en çok 1 (" + s(en_cok) + ")");
    DOGRULA(d, c.esz_guvenli_uygulanan == 360u && c.eszamansiz_ilk_parti_oncesi == 360u &&
                   (c.calistirma_gecersiz & kGecEszamansiz) == 0u,
            "her duran turda en yenisi uygulandı, ilk parti öncesi muaf (" + s(c.esz_guvenli_uygulanan) + ")");
    z.k.duraklatma_ayarla(false);
    return d.hata;
}
