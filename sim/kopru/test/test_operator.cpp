// W4-bagla-a3 (EMNİYET): operatör (A3 paneli) emniyet isteklerinin köprüdeki yolu. İstek, senaryo
// atamalarının kullandığı aynı `emniyet_ata` → `sinir_isle` yolundan girer; fark yalnız revizyon
// muhasebesidir (A3-K117 (1), (6): istek başına) ve gevşeten isteğin A3-K95 (4) değerlendirmesidir.
#include "test_destek.hpp"

#include <atomic>
#include <cstdlib>
#include <new>

// W4-K28 (3): canlı yığın baytı sayacı (yalnız bu sınama programında; MSVC'de değiştirilmez).
// Her bloğun başında boyu ve sayılıp sayılmadığı saklanır; `g_say` açıkken ayrılan bloklar
// serbest bırakılınca düşülür.
namespace {
std::atomic<long long> g_canli_bayt{0};
std::atomic<bool> g_bayt_say{false};
}  // namespace
#if !defined(_MSC_VER)
namespace {
constexpr std::size_t kBaslik = alignof(std::max_align_t) > 16u ? alignof(std::max_align_t) : 16u;
}  // namespace
static void* baslikli_ayir(std::size_t n) noexcept
{
    auto* p = static_cast<unsigned char*>(std::malloc(n + kBaslik));
    if (p == nullptr) {
        return nullptr;
    }
    const bool say = g_bayt_say.load(std::memory_order_relaxed);
    reinterpret_cast<std::size_t*>(p)[0] = n;
    reinterpret_cast<std::size_t*>(p)[1] = say ? 1u : 0u;
    if (say) {
        g_canli_bayt.fetch_add(static_cast<long long>(n), std::memory_order_relaxed);
    }
    return p + kBaslik;
}
void* operator new(std::size_t n)
{
    void* p = baslikli_ayir(n);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}
void* operator new[](std::size_t n) { return ::operator new(n); }
// nothrow biçimleri başlıklı düzenle eşlenir (libc++ std::stable_sort tamponu nothrow new kullanır).
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return baslikli_ayir(n); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return baslikli_ayir(n); }
void operator delete(void* q) noexcept
{
    if (q == nullptr) {
        return;
    }
    auto* p = static_cast<unsigned char*>(q) - kBaslik;
    if (reinterpret_cast<std::size_t*>(p)[1] != 0u) {
        g_canli_bayt.fetch_sub(static_cast<long long>(reinterpret_cast<std::size_t*>(p)[0]), std::memory_order_relaxed);
    }
    std::free(p);
}
void operator delete[](void* q) noexcept { ::operator delete(q); }
void operator delete(void* q, std::size_t) noexcept { ::operator delete(q); }
void operator delete[](void* q, std::size_t) noexcept { ::operator delete(q); }
void operator delete(void* q, const std::nothrow_t&) noexcept { ::operator delete(q); }
void operator delete[](void* q, const std::nothrow_t&) noexcept { ::operator delete(q); }
#endif

using namespace ida::kopru;
using namespace ida::kopru::test;

namespace {

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

}  // namespace

// Kısıtlayan istek: istek başına bir revizyon; durum değişmese de artar (A3-K92); gevşeten istek
// revizyonla değerlendirilir (A3-K118) ve uygulanınca bir artırır (A3-K117 (6)).
int test_operator_emniyet()
{
    Denetci d;
    SahteDuzenek z;
    const std::uint32_t r0 = z.k.emniyet_revizyonu();
    // İki kısıtlayan istek aynı sınırda: biri durumu değiştirir, öteki değiştirmez.
    z.k.emniyet_ata(op_acil(11, true));
    z.k.emniyet_ata(op_acil(12, true));
    z.sinir_ilerle();
    const SinirSonucu& s1 = z.sinirlar.back();
    DOGRULA(d, z.k.acil_durdurma(), "acil durdurma basıldı");
    DOGRULA(d, z.k.emniyet_revizyonu() == r0 + 2u, "istek başına bir revizyon (A3-K117 (1))");
    const OperatorSonucu* a = sonuc(s1, 11);
    const OperatorSonucu* b = sonuc(s1, 12);
    DOGRULA(d, a != nullptr && a->sonuc == AtamaSonucu::uygulandi && a->degisti && a->kisitlayan, "ilk istek uygulandı, durum değişti");
    DOGRULA(d, b != nullptr && b->sonuc == AtamaSonucu::uygulandi && !b->degisti, "ikinci istek uygulandı, durum değişmedi (A3-K92)");
    DOGRULA(d, !s1.gevsetme_reddedildi, "senaryo gevşetmesi yok: senaryo kaydı yazılmaz");

    // Gevşeten, bayat revizyonla: rev_uyusmuyor; durum değişmez, revizyon artmaz.
    const std::uint32_t r1 = z.k.emniyet_revizyonu();
    z.k.emniyet_ata(op_acil(13, false, r1 - 1u));
    z.sinir_ilerle();
    const OperatorSonucu* c = sonuc(z.sinirlar.back(), 13);
    DOGRULA(d, c != nullptr && c->sonuc == AtamaSonucu::rev_uyusmuyor, "bayat revizyonlu gevşetme reddedildi");
    DOGRULA(d, z.k.acil_durdurma() && z.k.emniyet_revizyonu() == r1, "durum ve revizyon değişmedi");
    // Revizyonsuz gevşetme de reddedilir.
    z.k.emniyet_ata(op_acil(14, false));
    z.sinir_ilerle();
    const OperatorSonucu* c2 = sonuc(z.sinirlar.back(), 14);
    DOGRULA(d, c2 != nullptr && c2->sonuc == AtamaSonucu::rev_uyusmuyor && z.k.acil_durdurma(), "revizyonsuz gevşetme reddedildi");

    // Aynı sınırda kısıtlayan + gevşeten: gevşeten `kisitlama_hakim` (A3-K95 (4)).
    z.k.emniyet_ata(op_acil(15, false, r1));
    z.k.emniyet_ata(op_pim(16, 0, true));
    z.sinir_ilerle();
    const OperatorSonucu* e = sonuc(z.sinirlar.back(), 15);
    const OperatorSonucu* f = sonuc(z.sinirlar.back(), 16);
    DOGRULA(d, f != nullptr && f->sonuc == AtamaSonucu::uygulandi, "kısıtlayan uygulandı");
    DOGRULA(d, e != nullptr && e->sonuc == AtamaSonucu::kisitlama_hakim && z.k.acil_durdurma(), "gevşeten kisitlama_hakim");
    // Sıra: gevşeten önce verildiği hâlde kısıtlayanın arkasında beklemez, kısıtlayan önce işlenir.
    DOGRULA(d, z.sinirlar.back().operator_sonuclari.size() == 2u &&
                   z.sinirlar.back().operator_sonuclari.front().kimlik == 16u,
            "kısıtlayan sonucu önce (A1-K85 adım 2, gevşeten adım 4)");

    // Güncel revizyonla gevşetme uygulanır ve bir artırır.
    const std::uint32_t r2 = z.k.emniyet_revizyonu();
    z.k.emniyet_ata(op_acil(17, false, r2));
    z.sinir_ilerle();
    const OperatorSonucu* g = sonuc(z.sinirlar.back(), 17);
    DOGRULA(d, g != nullptr && g->sonuc == AtamaSonucu::uygulandi && g->degisti && g->rev_sonra == r2 + 1u,
            "gevşetme uygulandı, revizyon +1");
    DOGRULA(d, !z.k.acil_durdurma(), "acil durdurma serbest");
    return d.hata;
}

// Senaryo ataması ile operatör isteği aynı sınırda (A1-K85; W4-bagla-a3 kapsam 4): kısıtlayan
// önce; senaryonun gevşetmesi operatörün kısıtlamasıyla reddedilir; operatörün gevşetmesi
// senaryonun kısıtlamasıyla reddedilir. Senaryonun gevşetmesi uygulanınca operatörün aynı
// revizyonu taşıyan gevşetmesi bayat olur (her biri taşıdığı revizyona göre).
int test_operator_ve_senaryo()
{
    Denetci d;
    SahteDuzenek z;
    EmniyetAtama sk;
    sk.acil_durdurma = true;
    z.k.emniyet_ata(sk);
    z.sinir_ilerle();
    // Senaryo gevşetir, operatör aynı sınırda kısıtlar.
    EmniyetAtama sg;
    sg.acil_durdurma = false;
    z.k.emniyet_ata(sg);
    z.k.emniyet_ata(op_pim(21, 1, true));
    z.sinir_ilerle();
    DOGRULA(d, z.sinirlar.back().gevsetme_reddedildi && z.k.acil_durdurma(), "senaryonun gevşetmesi reddedildi");
    // Senaryo kısıtlar, operatör aynı sınırda gevşetir.
    const std::uint32_t r = z.k.emniyet_revizyonu();
    EmniyetAtama sk2;
    sk2.fis[0] = false;
    z.k.emniyet_ata(sk2);
    z.k.emniyet_ata(op_acil(22, false, r));
    z.sinir_ilerle();
    const OperatorSonucu* a = sonuc(z.sinirlar.back(), 22);
    DOGRULA(d, a != nullptr && a->sonuc == AtamaSonucu::kisitlama_hakim && z.k.acil_durdurma(), "operatörün gevşetmesi kisitlama_hakim");
    DOGRULA(d, z.k.emniyet_revizyonu() == r + 1u, "senaryonun kısıtlaması tek işlem");
    // İkisi de gevşetir: önce senaryo (uygulanır, +1), sonra operatör (taşıdığı revizyon bayat).
    const std::uint32_t r3 = z.k.emniyet_revizyonu();
    EmniyetAtama sg2;
    sg2.acil_durdurma = false;
    z.k.emniyet_ata(sg2);
    z.k.emniyet_ata(op_pim(23, 0, false, r3));
    z.sinir_ilerle();
    const OperatorSonucu* b = sonuc(z.sinirlar.back(), 23);
    DOGRULA(d, !z.sinirlar.back().gevsetme_reddedildi && !z.k.acil_durdurma(), "senaryonun gevşetmesi uygulandı");
    DOGRULA(d, b != nullptr && b->sonuc == AtamaSonucu::rev_uyusmuyor, "operatörün gevşetmesi bayat revizyonla reddedildi");
    DOGRULA(d, z.k.emniyet_revizyonu() == r3 + 1u, "yalnız senaryonun gevşetmesi revizyonu artırdı");
    return d.hata;
}

// (W4-K27 (1)'in artımlı işlem testi W4-duraklat'ta test_duran.cpp'ye taşındı: duran kip sinir_isle'dir.)

// W4-K27 (3): duraklatmada kart güvenli çıktıları sınır işlenmeden gelir; bekleyen bellek sınırlı
// (yalnız en yeni). Bir saatlik HIL ızgarası eşdeğeri: 360 000 eşzamansız güvenli çıktı.
int test_esz_sinirli()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    for (int n = 0; n < 3; ++n) {
        (void)z.periyot();
    }
    std::array<std::uint16_t, 8> notr{};
    notr.fill(1500);
    const auto bayrak = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_THR_VALID | IDAW_ACT_FLAGS_IS_SAFE |
                                                   IDAW_ACT_FLAGS_SRC_TICK | IDAW_ACT_FLAGS_ASYNC);
    std::size_t en_cok = 0;
    for (int i = 0; i < 360'000; ++i) {
        z.sk.yanit(z.k.son_parti(), bayrak, notr);
        if (i % 1000 == 999) {
            alisveris(z.k, z.sk, z.t);
            en_cok = std::max(en_cok, z.k.esz_bekleyen_sayisi());
        }
    }
    alisveris(z.k, z.sk, z.t);
    en_cok = std::max(en_cok, z.k.esz_bekleyen_sayisi());
    DOGRULA(d, en_cok == 1u, "bekleyen eşzamansız çıktı en çok 1 (" + std::to_string(en_cok) + ")");
    DOGRULA(d, z.k.sayaclar().esz_birlesen == 359'999u,
            "geçersiz kılınanlar sayıldı (" + std::to_string(z.k.sayaclar().esz_birlesen) + ")");
    z.sinir_ilerle();
    DOGRULA(d, z.k.sayaclar().esz_guvenli_uygulanan == 1u && z.k.esz_bekleyen_sayisi() == 0u,
            "sürdürmede en yenisi uygulandı");
    return d.hata;
}

// W4-K28 (3) (EMNİYET; sol-2 #4): ilk parti (ilk TF ve olağan yanıt) öncesinde gelen batch_seq = 0
// eşzamansız çıktılar A1-K79'dan muaftır; muafiyet yalnız tutulan tek çıktının yanında saklanır.
// 360 000 çıktı (10 ms ızgarada bir saat) boyunca köprünün canlı belleği büyümez; en yenisi
// uygulanır ve muaf sayılır (geçersizlik biti kurulmaz).
int test_esz_ilk_parti_oncesi()
{
    Denetci d;
    SahteDuzenek z;
    DOGRULA(d, z.el_sikis(), "el sıkışma");
    std::array<std::uint16_t, 8> notr{};
    notr.fill(1500);
    const auto bayrak = static_cast<std::uint16_t>(IDAW_ACT_FLAGS_THR_VALID | IDAW_ACT_FLAGS_IS_SAFE |
                                                   IDAW_ACT_FLAGS_SRC_TICK | IDAW_ACT_FLAGS_ASYNC);
    std::size_t en_cok = 0;
    long long isinma = 0;
    long long en_cok_artis = 0;
    g_canli_bayt.store(0);
    for (int i = 0; i < 360'000; ++i) {
        if (i == 10'000) {
            g_bayt_say.store(true);   // ısınmadan sonra: tamponlar oturdu
            isinma = g_canli_bayt.load();
        }
        z.sk.yanit(0u, bayrak, notr);
        if (i % 1000 == 999) {
            alisveris(z.k, z.sk, z.t);
            en_cok = std::max(en_cok, z.k.esz_bekleyen_sayisi());
            en_cok_artis = std::max(en_cok_artis, g_canli_bayt.load() - isinma);
        }
    }
    alisveris(z.k, z.sk, z.t);
    g_bayt_say.store(false);
    en_cok = std::max(en_cok, z.k.esz_bekleyen_sayisi());
    DOGRULA(d, z.k.son_parti() == 0u, "ön koşul: hiç parti kapanmadı (ilk TF öncesi)");
    DOGRULA(d, en_cok == 1u, "bekleyen eşzamansız çıktı en çok 1 (" + std::to_string(en_cok) + ")");
    DOGRULA(d, z.k.sayaclar().esz_birlesen == 359'999u,
            "geçersiz kılınanlar sayıldı (" + std::to_string(z.k.sayaclar().esz_birlesen) + ")");
#if !defined(_MSC_VER)
    // Kimlik başına küme düğümü olsaydı 350 000 düğüm (onlarca bayt/düğüm) MB düzeyinde büyürdü.
    DOGRULA(d, en_cok_artis < 64 * 1024,
            "köprünün canlı belleği sabit (ısınmadan sonra en çok artış " + std::to_string(en_cok_artis) + " B)");
#else
    (void)en_cok_artis;
#endif
    z.sinir_ilerle();
    const Sayaclar& c = z.k.sayaclar();
    DOGRULA(d, c.esz_guvenli_uygulanan == 1u && c.eszamansiz_ilk_parti_oncesi == 1u &&
                   (c.calistirma_gecersiz & kGecEszamansiz) == 0u,
            "en yenisi uygulandı ve ilk parti öncesi muaf sayıldı (A1-K79 biti kurulmadı)");
    return d.hata;
}
