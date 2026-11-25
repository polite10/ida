// test_birim.cpp — birim testleri: CRC-32, halka, düşme yuvası, güvence (A6-K33), üst veri denetimi,
// çözücünün sürüm kuralı (A6-K48).
#include "test_destek.hpp"

#include "crc32.hpp"
#include "dusme.hpp"
#include "halka.hpp"
#include "ida/kayit/coz.hpp"

#include <cstring>
#include <thread>

namespace ida::kayit::test {

int test_crc32()
{
    Denetci d;
    // Bilinen denetim değeri: CRC-32("123456789") = 0xCBF43926.
    const char* m = "123456789";
    DOGRULA(d, ic::crc32(reinterpret_cast<const std::uint8_t*>(m), 9) == 0xCBF43926u, "bilinen vektör");
    DOGRULA(d, ic::crc32(nullptr, 0) == 0u, "boş girdi");
    // Akış biçimi ve dilimli yol, bayt bayt hesapla aynı sonucu verir.
    std::vector<std::uint8_t> v(1000);
    for (std::size_t i = 0; i < v.size(); ++i) {
        v[i] = static_cast<std::uint8_t>((i * 131u + 7u) & 0xFFu);
    }
    std::uint32_t bayt_bayt = 0xFFFFFFFFu;
    for (const std::uint8_t b : v) {
        bayt_bayt ^= b;
        for (int k = 0; k < 8; ++k) {
            bayt_bayt = (bayt_bayt & 1u) != 0 ? (bayt_bayt >> 1u) ^ 0xEDB88320u : bayt_bayt >> 1u;
        }
    }
    bayt_bayt ^= 0xFFFFFFFFu;
    DOGRULA(d, ic::crc32(v.data(), v.size()) == bayt_bayt, "dilimli hesap bayt bayt hesapla aynı");
    for (const std::size_t kesim : {std::size_t{0}, std::size_t{1}, std::size_t{7}, std::size_t{8}, std::size_t{513}}) {
        std::uint32_t c = ic::crc32_baslat();
        c = ic::crc32_ekle(c, v.data(), kesim);
        c = ic::crc32_ekle(c, v.data() + kesim, v.size() - kesim);
        DOGRULA(d, ic::crc32_bitir(c) == bayt_bayt, "akış biçimi");
    }
    return d.hata;
}

int test_halka()
{
    Denetci d;
    {
        ic::BaytHalkasi h(4096);
        DOGRULA(d, h.kapasite() == 4096, "kapasite");
        DOGRULA(d, h.ayir(h.azami_yuk() + 1, 0) == nullptr, "çok büyük kayıt reddedilir");
        // Doldur: 100 baytlık kayıtlar (kayıt = 112 bayt).
        int yazilan = 0;
        for (;;) {
            std::uint8_t* p = h.ayir(100, 0);
            if (p == nullptr) {
                break;
            }
            std::memset(p, yazilan & 0xFF, 100);
            h.yayinla();
            ++yazilan;
        }
        DOGRULA(d, yazilan == 4096 / 112, "halka dolana dek yazılır: " + std::to_string(yazilan));
        // Yayınlanmayan ayırma görünmez.
        std::size_t n = 0;
        int okunan = 0;
        while (const std::uint8_t* p = h.oku(n)) {
            DOGRULA(d, n == 100 && p[0] == static_cast<std::uint8_t>(okunan & 0xFF) && p[99] == p[0], "kayıt içeriği");
            h.birak();
            ++okunan;
        }
        DOGRULA(d, okunan == yazilan, "yazılanların hepsi okunur");
        // Pay: boş halkada bile kayıttan sonra `pay` kadar yer kalmalı.
        DOGRULA(d, h.ayir(100, 4096) == nullptr, "pay sağlanamıyorsa yer verilmez");
        DOGRULA(d, h.ayir(100, 0) != nullptr, "paysız yer verilir");
        // Yayınlanmadı: sonraki ayırma aynı yeri kullanır, okuyucu bir şey görmez.
        DOGRULA(d, h.oku(n) == nullptr, "yayınlanmayan kayıt okunmaz");
    }
    {
        // Sarma: değişken boylu kayıtlar, uzun süre; içerik ve sıra korunur.
        ic::BaytHalkasi h(8192);
        std::uint32_t yaz_sira = 0;
        std::uint32_t oku_sira = 0;
        for (int tur = 0; tur < 20000; ++tur) {
            const std::size_t boy = 4 + static_cast<std::size_t>((tur * 37) % 700);
            if (std::uint8_t* p = h.ayir(boy, 0)) {
                std::memcpy(p, &yaz_sira, 4);
                std::memset(p + 4, static_cast<int>(yaz_sira & 0xFFu), boy - 4);
                h.yayinla();
                ++yaz_sira;
            }
            if (tur % 3 != 0) {
                std::size_t n = 0;
                if (const std::uint8_t* p = h.oku(n)) {
                    std::uint32_t s = 0;
                    std::memcpy(&s, p, 4);
                    DOGRULA(d, s == oku_sira, "sıra korunur");
                    DOGRULA(d, n >= 4 && (n == 4 || p[n - 1] == static_cast<std::uint8_t>(s & 0xFFu)), "içerik korunur");
                    h.birak();
                    ++oku_sira;
                }
            }
        }
        DOGRULA(d, yaz_sira > 10000 && oku_sira > 10000, "sarma koşusu ilerledi");
    }
    {
        // İki iş parçacığı: üretici düşenleri sayar; okunan + düşen = denenen, sıra bozulmaz.
        ic::BaytHalkasi h(1u << 14);
        constexpr std::uint32_t kDeneme = 300000;
        std::atomic<bool> bitti{false};
        std::uint32_t dusen = 0;
        std::thread uretici([&] {
            for (std::uint32_t i = 0; i < kDeneme; ++i) {
                const std::size_t boy = 8 + (i % 120u);
                std::uint8_t* p = h.ayir(boy, 0);
                if (p == nullptr) {
                    ++dusen;
                    continue;
                }
                std::memcpy(p, &i, 4);
                std::memcpy(p + 4, &i, 4);
                h.yayinla();
            }
            bitti.store(true, std::memory_order_release);
        });
        std::uint32_t okunan = 0;
        std::uint32_t onceki = 0;
        bool ilk = true;
        bool sira_tamam = true;
        for (;;) {
            std::size_t n = 0;
            const std::uint8_t* p = h.oku(n);
            if (p == nullptr) {
                if (bitti.load(std::memory_order_acquire) && h.oku(n) == nullptr) {
                    break;
                }
                continue;
            }
            std::uint32_t a = 0;
            std::uint32_t b = 0;
            std::memcpy(&a, p, 4);
            std::memcpy(&b, p + 4, 4);
            sira_tamam = sira_tamam && a == b && n == 8 + (a % 120u) && (ilk || a > onceki);
            ilk = false;
            onceki = a;
            ++okunan;
            h.birak();
        }
        uretici.join();
        DOGRULA(d, sira_tamam, "iş parçacıkları arasında içerik ve sıra");
        DOGRULA(d, okunan + dusen == kDeneme,
                "okunan + düşen = denenen: " + std::to_string(okunan) + " + " + std::to_string(dusen));
    }
    return d.hata;
}

int test_dusme_yuvasi()
{
    Denetci d;
    {
        ic::DusmeYuvasi y;
        ic::DusmeKesiti k;
        DOGRULA(d, y.oku(k) && k.sayi == 0, "boş yuva");
        y.dusur(100, 7);
        y.dusur(200, 8);
        y.dusur(300, 9);
        DOGRULA(d, y.oku(k) && k.sayi == 3 && k.ilk_t == 100 && k.son_t == 300 && k.ilk_adim == 7 && k.son_adim == 9,
                "ilk aralık");
        y.onayla(3);
        y.dusur(400, 12);
        y.dusur(500, 13);
        DOGRULA(d, y.oku(k) && k.sayi == 5 && k.ilk_t == 400 && k.son_t == 500 && k.ilk_adim == 12 && k.son_adim == 13,
                "onaydan sonra yeni aralık");
        // Onay gecikirse düşme eski aralığa katılır: aralık gerçeğinden dar olamaz.
        y.dusur(600, 14);
        DOGRULA(d, y.oku(k) && k.sayi == 6 && k.ilk_t == 400 && k.son_t == 600, "onaysız düşme aralığı genişletir");
    }
    {
        // Üretici düşürürken yazıcı okur: bildirilen toplam = düşen; her kesit kendi içinde tutarlı.
        ic::DusmeYuvasi y;
        constexpr std::uint64_t kDusme = 400000;
        std::atomic<bool> bitti{false};
        std::thread uretici([&] {
            for (std::uint64_t i = 1; i <= kDusme; ++i) {
                y.dusur(static_cast<std::int64_t>(i * 10), i);
            }
            bitti.store(true, std::memory_order_release);
        });
        std::uint64_t bildirilen = 0;
        std::uint64_t kayit = 0;
        bool tutarli = true;
        const auto yokla = [&] {
            ic::DusmeKesiti k;
            if (!y.oku(k) || k.sayi == bildirilen) {
                return;
            }
            // Üretici i. düşmede t = 10 i, adım = i yazar: kesit tutarlıysa son = sayı ve ilk ≤ son.
            tutarli = tutarli && k.son_adim == k.sayi && k.son_t == static_cast<std::int64_t>(k.sayi * 10) &&
                      k.ilk_t == static_cast<std::int64_t>(k.ilk_adim * 10) && k.ilk_adim <= k.son_adim &&
                      k.ilk_adim <= bildirilen + 1;  // aralık bildirilmemiş ilk düşmeyi kapsar
            bildirilen = k.sayi;
            y.onayla(k.sayi);
            ++kayit;
        };
        while (!bitti.load(std::memory_order_acquire)) {
            yokla();
        }
        uretici.join();
        yokla();
        DOGRULA(d, tutarli, "kesitler tutarlı ve aralık bildirilmemiş düşmeleri kapsıyor");
        DOGRULA(d, bildirilen == kDusme, "bildirilen toplam = düşen");
        std::printf("  düşme yuvası: %llu düşme %llu kesitte bildirildi\n", static_cast<unsigned long long>(kDusme),
                    static_cast<unsigned long long>(kayit));
    }
    return d.hata;
}

int test_guvence()
{
    Denetci d;
    const auto g = [](Kademe k, std::string_view kip) {
        GuvenceGirdisi x;
        x.kademe = k;
        x.kip = kip;
        return x;
    };
    // A6-K33: kademe ve kipin verdiği en yüksek düzey.
    DOGRULA(d, guvence_belirle(g(Kademe::olaylar, "sil")).duzey == Guvence::izleme, "olaylar → izleme");
    DOGRULA(d, guvence_belirle(g(Kademe::telemetri, "sil")).duzey == Guvence::fizik, "telemetri → fizik");
    DOGRULA(d, guvence_belirle(g(Kademe::tam, "sil")).duzey == Guvence::girdi, "tam + sil → girdi");
    DOGRULA(d, guvence_belirle(g(Kademe::tam, "hil_bariyer")).duzey == Guvence::fizik, "HIL kaydı en çok fizik");
    DOGRULA(d, guvence_belirle(g(Kademe::tam, "hil_1x")).duzey == Guvence::fizik, "HIL 1x en çok fizik");
    DOGRULA(d, guvence_belirle(g(Kademe::tam, "tekrar_fizik")).duzey == Guvence::fizik, "tekrar_fizik → fizik");
    {
        const GuvenceSonucu s = guvence_belirle(g(Kademe::telemetri, "sil"));
        DOGRULA(d, !s.on_eki_son_adim.has_value() && s.nedenler.empty(), "eksiksiz kayıtta önek ve neden yok");
    }
    {
        // İnceleme bulgusu 7: kip × kademe için beklenen düzey Tablo 4.6'nın izin verdiğini aşamaz.
        // Fiziğin koşmadığı kiplerin (`tekrar_girdi_sil`, `tekrar_yeniden_kurma`) kaydı `fizik` güvencesi
        // iddia edemez; `girdi` yalnız `sil` kipinin `tam` kademesindeki kaydından.
        struct Satir {
            std::string_view kip;
            Guvence olaylar;
            Guvence telemetri;
            Guvence tam;
        };
        const Satir tablo[] = {
            {"sil", Guvence::izleme, Guvence::fizik, Guvence::girdi},
            {"hil_1x", Guvence::izleme, Guvence::fizik, Guvence::fizik},
            {"hil_bariyer", Guvence::izleme, Guvence::fizik, Guvence::fizik},
            {"tekrar_fizik", Guvence::izleme, Guvence::fizik, Guvence::fizik},
            {"tekrar_girdi_sil", Guvence::izleme, Guvence::izleme, Guvence::izleme},
            {"tekrar_yeniden_kurma", Guvence::izleme, Guvence::izleme, Guvence::izleme},
            {"bilinmeyen_kip", Guvence::izleme, Guvence::izleme, Guvence::izleme},
            {"", Guvence::izleme, Guvence::izleme, Guvence::izleme},
        };
        for (const Satir& s : tablo) {
            DOGRULA(d, kip_gecerli(s.kip) == (s.kip != "bilinmeyen_kip" && !s.kip.empty()), std::string(s.kip) + ": kip listesi");
            DOGRULA(d, beklenen_duzey(Kademe::olaylar, s.kip) == s.olaylar && beklenen_duzey(Kademe::telemetri, s.kip) == s.telemetri &&
                           beklenen_duzey(Kademe::tam, s.kip) == s.tam,
                    std::string(s.kip) + ": beklenen düzey (Tablo 4.6)");
            const GuvenceSonucu r = guvence_belirle(g(Kademe::tam, s.kip));
            DOGRULA(d, r.duzey == s.tam && r.beklenen == s.tam && r.nedenler.empty(), std::string(s.kip) + ": guvence_belirle aynı düzeyi verir");
        }
        // Düzeyin verilmediği kipte o düzeyin kanallarındaki eksik öneki ya da nedeni etkilemez.
        GuvenceGirdisi x = g(Kademe::tam, "tekrar_girdi_sil");
        x.ilk_eksik_fizik = 10;
        x.ilk_eksik_girdi = 20;
        const GuvenceSonucu r = guvence_belirle(x);
        DOGRULA(d, r.duzey == Guvence::izleme && !r.on_eki_son_adim.has_value() && r.nedenler.empty(), "fizik koşmayan kipte izleme koşulsuz");
    }
    {
        // A6-K38, K53.
        GuvenceGirdisi x = g(Kademe::tam, "tekrar_fizik");
        x.guvencesiz_baslatma = true;
        GuvenceSonucu s = guvence_belirle(x);
        DOGRULA(d, s.duzey == Guvence::yok && s.nedenler == "guvencesiz_baslatma", "güvencesiz başlatma → yok");
        x.guvencesiz_baslatma = false;
        x.canli_mudahale = true;
        s = guvence_belirle(x);
        DOGRULA(d, s.duzey == Guvence::yok && s.nedenler == "canli_mudahale", "canlı müdahale → yok");
    }
    {
        // Önek: gereken kanallardaki ilk eksik adım − 1.
        GuvenceGirdisi x = g(Kademe::telemetri, "hil_bariyer");
        x.ilk_eksik_fizik = 4000;
        const GuvenceSonucu s = guvence_belirle(x);
        DOGRULA(d, s.duzey == Guvence::fizik && s.on_eki_son_adim == std::optional<std::uint64_t>(3999), "önek güvencesi");
        DOGRULA(d, s.nedenler.empty(), "düzey düşmediyse neden yazılmaz");
    }
    {
        // İlk eksik adım 0: o düzey verilmez, bir alt düzeye inilir.
        GuvenceGirdisi x = g(Kademe::telemetri, "sil");
        x.ilk_eksik_fizik = 0;
        GuvenceSonucu s = guvence_belirle(x);
        DOGRULA(d, s.duzey == Guvence::izleme && !s.on_eki_son_adim.has_value() && s.nedenler == "kayit_eksik",
                "fizik 0. adımda eksik → izleme");
        x = g(Kademe::tam, "sil");
        x.ilk_eksik_girdi = 0;
        s = guvence_belirle(x);
        DOGRULA(d, s.duzey == Guvence::fizik && !s.on_eki_son_adim.has_value() && s.nedenler == "kayit_eksik",
                "girdi 0. adımda eksik → fizik (fizik kanalları eksiksiz)");
        x.ilk_eksik_fizik = 10;
        s = guvence_belirle(x);
        DOGRULA(d, s.duzey == Guvence::fizik && s.on_eki_son_adim == std::optional<std::uint64_t>(9),
                "girdi düşer, fizik önekle verilir");
        x.ilk_eksik_fizik = 0;
        s = guvence_belirle(x);
        DOGRULA(d, s.duzey == Guvence::izleme, "ikisi de 0. adımda eksik → izleme");
        x = g(Kademe::tam, "sil");
        x.ilk_eksik_girdi = 77;
        x.ilk_eksik_fizik = 5;
        s = guvence_belirle(x);
        DOGRULA(d, s.duzey == Guvence::girdi && s.on_eki_son_adim == std::optional<std::uint64_t>(76),
                "girdi öneki girdi kanallarından");
    }
    {
        // izleme koşulsuzdur.
        GuvenceGirdisi x = g(Kademe::olaylar, "sil");
        x.ilk_eksik_fizik = 0;
        x.ilk_eksik_girdi = 0;
        const GuvenceSonucu s = guvence_belirle(x);
        DOGRULA(d, s.duzey == Guvence::izleme && s.nedenler.empty(), "olaylar kademesinde eksik izlemeyi düşürmez");
    }
    {
        // Erken kapanış (A6-K12, K31): önek son tam kaydedilen adımdır.
        GuvenceGirdisi x = g(Kademe::telemetri, "sil");
        x.erken_neden = "sinira_ulasti";
        x.erken_son_adim = 500;
        GuvenceSonucu s = guvence_belirle(x);
        DOGRULA(d, s.duzey == Guvence::fizik && s.on_eki_son_adim == std::optional<std::uint64_t>(500), "erken kapanış öneki");
        x.ilk_eksik_fizik = 100;
        s = guvence_belirle(x);
        DOGRULA(d, s.on_eki_son_adim == std::optional<std::uint64_t>(99), "önek ikisinin küçüğü");
        x.ilk_eksik_fizik.reset();
        x.erken_son_adim.reset();
        s = guvence_belirle(x);
        DOGRULA(d, s.duzey == Guvence::izleme && s.nedenler == "sinira_ulasti", "tam kaydedilen adım yoksa düzey düşer");
    }
    {
        // Katalogdaki tekrar düzeyleri: kayit/eksik bütün düzeylerin gereğidir; cekirdek/* yalnız girdi.
        const KanalTanimi* eksik = kanal_bul("kayit/eksik");
        const KanalTanimi* cagri = kanal_bul("cekirdek/cagri");
        const KanalTanimi* durum = kanal_bul("fizik/durum");
        const KanalTanimi* olay = kanal_bul("olay");
        DOGRULA(d, eksik != nullptr && cagri != nullptr && durum != nullptr && olay != nullptr, "kanallar katalogda");
        if (d.hata == 0) {
            DOGRULA(d, eksik_duzeyi_etkiler(*eksik, Guvence::fizik) && eksik_duzeyi_etkiler(*eksik, Guvence::girdi), "kayit/eksik");
            DOGRULA(d, eksik_duzeyi_etkiler(*cagri, Guvence::girdi) && !eksik_duzeyi_etkiler(*cagri, Guvence::fizik), "cekirdek/cagri");
            DOGRULA(d, eksik_duzeyi_etkiler(*durum, Guvence::fizik) && !eksik_duzeyi_etkiler(*durum, Guvence::girdi), "fizik/durum");
            DOGRULA(d, !eksik_duzeyi_etkiler(*olay, Guvence::fizik) && eksik_duzeyi_etkiler(*olay, Guvence::izleme), "olay");
        }
    }
    return d.hata;
}

int test_ust_veri_denetimi()
{
    Denetci d;
    const auto esle = [](const AcilisUstVerisi& u) {
        UstVeri m;
        const auto tablo = acilis_anahtarlari();
        std::size_t i = 0;
        for (const auto& [k, v] : ust_veri_ciftleri(u)) {
            if (!v.empty() || tablo[i].zorunlu) {
                m.emplace(std::string(k), std::string(v));
            }
            ++i;
        }
        return m;
    };
    std::string hata;
    UstVeri tam = esle(ornek_ust_veri());
    tam["a6_surumu"] = "1.0";
    tam["kayit_kademesi"] = "telemetri";
    tam["kamera_kareleri"] = "kapali";
    tam["kayit_duvar_saati"] = "monoton";
    DOGRULA(d, ust_veri_denetle(kUstVeriAcilis, tam, hata), "örnek açılış kaydı geçerli: " + hata);

    // A6 §7 (Y12): `kip = sil` iken `sil_n_bg`'siz açılış kaydı şemadan geçmez.
    UstVeri u = tam;
    u.erase("sil_n_bg");
    DOGRULA(d, !ust_veri_denetle(kUstVeriAcilis, u, hata), "kip = sil iken sil_n_bg zorunlu");
    u["kip"] = "hil_bariyer";
    DOGRULA(d, ust_veri_denetle(kUstVeriAcilis, u, hata), "HIL kipinde sil_n_bg gerekmez: " + hata);
    // `sil_n_bg` = 4294967296 ve 01 reddedilir; 0 ve 4294967295 kabul edilir.
    for (const char* kotu : {"4294967296", "01", "-1", "1.0", "", " 1", "12a", "99999999999"}) {
        u = tam;
        u["sil_n_bg"] = kotu;
        DOGRULA(d, !ust_veri_denetle(kUstVeriAcilis, u, hata), std::string("sil_n_bg reddedilmeli: '") + kotu + "'");
    }
    for (const char* iyi : {"0", "4294967295", "7", "4294967294", "1000000000"}) {
        u = tam;
        u["sil_n_bg"] = iyi;
        DOGRULA(d, ust_veri_denetle(kUstVeriAcilis, u, hata), std::string("sil_n_bg kabul edilmeli: ") + iyi);
    }
    u = tam;
    u["a1_run_id"] = "4294967296";
    DOGRULA(d, !ust_veri_denetle(kUstVeriAcilis, u, hata), "a1_run_id onluk u32 olmalı");
    // Kapalı nesne: bilinmeyen anahtar; eksik zorunlu anahtar.
    u = tam;
    u["bilinmeyen"] = "x";
    DOGRULA(d, !ust_veri_denetle(kUstVeriAcilis, u, hata), "bilinmeyen anahtar reddedilir");
    for (const UstVeriAnahtari& a : acilis_anahtarlari()) {
        if (!a.zorunlu) {
            continue;
        }
        u = tam;
        u.erase(std::string(a.ad));
        DOGRULA(d, !ust_veri_denetle(kUstVeriAcilis, u, hata), "zorunlu anahtar eksik: " + std::string(a.ad));
    }
    // Güncelleme: yalnız kapalı listedeki anahtarlar, boş değer yok, boş kayıt yok.
    UstVeri g;
    g["kart_fw_version"] = "17";
    DOGRULA(d, ust_veri_denetle(kUstVeriGuncelleme, g, hata), "güncelleme geçerli: " + hata);
    g["tohum"] = "1";
    DOGRULA(d, !ust_veri_denetle(kUstVeriGuncelleme, g, hata), "kapalı liste dışı anahtar reddedilir");
    g.clear();
    DOGRULA(d, !ust_veri_denetle(kUstVeriGuncelleme, g, hata), "boş güncelleme reddedilir");
    g["a2_surumu"] = "";
    DOGRULA(d, !ust_veri_denetle(kUstVeriGuncelleme, g, hata), "boş değer reddedilir");
    DOGRULA(d, !ust_veri_denetle("baska_kayit", tam, hata), "bilinmeyen kayıt adı");

    DOGRULA(d, run_id_gecerli("20261125-101500-a1b2c3"), "run_id biçimi");
    for (const char* kotu : {"", "20261125-101500-A1B2C3", "20261125_101500-a1b2c3", "2026112-101500-a1b2c3",
                             "20261125-101500-a1b2c", "20261125-101500-a1b2c3x", "20261125-1015zz-a1b2c3"}) {
        DOGRULA(d, !run_id_gecerli(kotu), std::string("run_id reddedilmeli: ") + kotu);
    }
    const std::string b = bagimlilik_surumleri();
    DOGRULA(d, b.rfind("mcap=2.1.3,zstd=", 0) == 0, "bağımlılık sürümleri: " + b);
    std::printf("  bağımlılıklar: %s\n", b.c_str());
    return d.hata;
}

int test_coz_surum_kurali()
{
    Denetci d;
    const KanalTanimi* k = kanal_bul("kayit/eksik");
    DOGRULA(d, k != nullptr, "kayit/eksik katalogda");
    if (k == nullptr) {
        return d.hata;
    }
    yuk_kayit_eksik y;
    y.kanal = "fizik/durum";
    y.ilk_sim_t_ns = 10;
    y.son_sim_t_ns = 20;
    y.ilk_adim = 1;
    y.son_adim = 2;
    y.kayip_sayisi = 3;
    y.neden = 1;
    std::vector<std::uint8_t> b(boyut(y));
    std::size_t n = 0;
    DOGRULA(d, kodla(y, b.data(), b.size(), n) && n == b.size(), "kodlama");
    std::string json;
    CozmeSonucu s = json_dok(*k->yuk, b, json);
    DOGRULA(d, s.tamam && s.eksik_alan == 0 && s.fazla_bayt == 0 && json == json_dok(y), "tam yük: " + json);

    // A6-K48: büyük `kanal_surumu` (sona alan eklenmiş): bilinen alanlar okunur, fazla baytlar yok sayılır.
    std::vector<std::uint8_t> uzun = b;
    uzun.insert(uzun.end(), {1, 2, 3, 4, 5});
    std::string json2;
    s = json_dok(*k->yuk, uzun, json2);
    DOGRULA(d, s.tamam && s.fazla_bayt == 5 && json2 == json, "sondaki fazla baytlar yok sayılır");

    // Küçük `kanal_surumu`: sondaki eksik alanlar "yok" (null); yalnız alan sınırında.
    const std::vector<std::uint8_t> kisa(b.begin(), b.end() - 1);  // `neden` (u8) yok
    s = json_dok(*k->yuk, kisa, json2);
    DOGRULA(d, s.tamam && s.eksik_alan == 1 && json2.find("\"neden\":null") != std::string::npos,
            "sondaki eksik alan null: " + json2);
    const std::vector<std::uint8_t> kisa2(b.begin(), b.end() - 9);  // `kayip_sayisi` ve `neden` yok
    s = json_dok(*k->yuk, kisa2, json2);
    DOGRULA(d, s.tamam && s.eksik_alan == 2, "iki eksik alan");
    // Alanın ortasında biten yük bozuktur.
    const std::vector<std::uint8_t> yarim(b.begin(), b.end() - 4);
    s = json_dok(*k->yuk, yarim, json2);
    DOGRULA(d, !s.tamam, "alan ortasında biten yük çözülemez");
    // Sayaç üst sınırı aşan metin (kanal: en çok 64) yapısal hatadır.
    std::vector<std::uint8_t> tasan = b;
    tasan[0] = 65;
    tasan[1] = 0;
    s = json_dok(*k->yuk, tasan, json2);
    DOGRULA(d, !s.tamam, "üst sınırı aşan sayaç reddedilir");
    // Alan gezgini adla erişim verir.
    std::string kanal;
    std::uint64_t kayip = 0;
    s = alanlari_gez(*k->yuk, b, [&](const a3::AlanTanimi& a, const AlanDegeri& v) {
        if (a.ad == "kanal") {
            kanal = std::string(v.metin);
        } else if (a.ad == "kayip_sayisi") {
            kayip = v.isaretsiz;
        }
    });
    DOGRULA(d, s.tamam && kanal == "fizik/durum" && kayip == 3, "alan gezgini");
    return d.hata;
}

}  // namespace ida::kayit::test
