// test_yazim.cpp — yazım yolu: kabul 4 (üretici beklemez, eksik kayıt), kabul 7 (sınırlar), parça
// kapatma süresi (enjekte edilen saatle), kapanış, hata kapanışı, güncelleme.
#include "test_destek.hpp"

#include "ida/kayit/coz.hpp"
#include "mcap_kitaplik_okuma.hpp"
#include "sinama_uretim.hpp"
#include "zamanlama_olcutu.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <map>
#include <set>
#include <thread>

namespace ida::kayit::test {

namespace {

std::string yol_metni(const std::filesystem::path& p)
{
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

// Yerel dosyayı saran, sınama için davranışı değiştirilebilen yazım hedefi.
struct HedefAyari {
    std::int64_t bosaltma_gecikmesi_ms = 0;  // her `bosalt` ve büyük `yaz` çağrısında uyku (yavaş disk)
    std::uint64_t hata_esigi = 0;            // 0: yok; toplam bayt bu eşiği aşacaksa yazım hata verir
    Dosya::Sonuc hata_turu = Dosya::Sonuc::disk_dolu;
    bool kesmeden_sonra_yazilir = true;      // false: hata kalıcıdır (kuyruk da yazılamaz)
    std::shared_ptr<std::atomic<std::uint64_t>> yazilan = std::make_shared<std::atomic<std::uint64_t>>(0);
    std::shared_ptr<std::atomic<bool>> hata_verdi = std::make_shared<std::atomic<bool>>(false);
};

class SinamaHedefi final : public Dosya {
public:
    SinamaHedefi(std::unique_ptr<Dosya> ic, HedefAyari a) : ic_(std::move(ic)), a_(std::move(a)) {}
    Sonuc yaz(const std::uint8_t* veri, std::size_t n) override
    {
        if (a_.bosaltma_gecikmesi_ms > 0 && n >= 4096) {
            std::this_thread::sleep_for(std::chrono::milliseconds(a_.bosaltma_gecikmesi_ms));
        }
        if (a_.hata_verdi->load() && !(kesildi_ && a_.kesmeden_sonra_yazilir)) {
            return a_.hata_turu;
        }
        if (a_.hata_esigi != 0 && !a_.hata_verdi->load() && a_.yazilan->load() + n > a_.hata_esigi) {
            a_.hata_verdi->store(true);
            // Gerçek bir dolu disk gibi: baytların bir kısmı yazılmış olabilir.
            (void)ic_->yaz(veri, n / 2);
            return a_.hata_turu;
        }
        a_.yazilan->fetch_add(n);
        return ic_->yaz(veri, n);
    }
    Sonuc bosalt() override
    {
        if (a_.bosaltma_gecikmesi_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(a_.bosaltma_gecikmesi_ms));
        }
        if (a_.hata_verdi->load() && !(kesildi_ && a_.kesmeden_sonra_yazilir)) {
            return a_.hata_turu;
        }
        return ic_->bosalt();
    }
    Sonuc kes(std::uint64_t boy) override
    {
        kesildi_ = true;
        return ic_->kes(boy);
    }
    void kapat() override { ic_->kapat(); }

private:
    std::unique_ptr<Dosya> ic_;
    HedefAyari a_;
    bool kesildi_ = false;
};

DosyaFabrikasi hedef_fabrikasi(const HedefAyari& a)
{
    return [a](const std::filesystem::path& yol, std::string& hata) -> std::unique_ptr<Dosya> {
        auto ic = yerel_dosya_olustur(yol, hata);
        if (!ic) {
            return nullptr;
        }
        return std::make_unique<SinamaHedefi>(std::move(ic), a);
    };
}

struct Yuzdelik {
    double p50 = 0;
    double p99 = 0;
    double en_kotu = 0;
};

Yuzdelik yuzdelik(std::vector<std::uint32_t>& ns)
{
    Yuzdelik y;
    if (ns.empty()) {
        return y;
    }
    std::sort(ns.begin(), ns.end());
    y.p50 = ns[ns.size() / 2];
    y.p99 = ns[std::min(ns.size() - 1, ns.size() * 99 / 100)];
    y.en_kotu = ns.back();
    return y;
}

std::int64_t simdi_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Dosyadaki bir kanalın iletileri ve o kanal için bildirilen kayıp toplamı.
struct KanalSayimi {
    std::uint64_t ileti = 0;
    std::uint64_t kayip = 0;
};
std::map<std::string, KanalSayimi> kanal_sayimi(Okuyucu& o)
{
    std::map<std::string, KanalSayimi> m;
    for (const KanalOzeti& k : o.ozet().kanallar) {
        if (k.konu != "kayit/eksik") {
            m[k.konu].ileti = k.ileti_sayisi;
        }
    }
    for (const EksikKaydi& e : o.ozet().eksikler) {
        m[std::string(e.kanal->ad)].kayip += e.kayip_sayisi;
    }
    return m;
}

}  // namespace

// Kabul 4: yazıcı durdurulmuş, disk yavaş ve halka doluyken üretici çağrısının süresi sağlıklı
// koşudan ayırt edilemez; düşen her ileti sayılır ve `kayit/eksik`'te görünür.
int test_uretici_beklemez()
{
    Denetci d;
    constexpr int kCagri = 100000;
    constexpr std::int64_t kTakilma_ms = 50;
    struct Senaryo {
        const char* ad;
        bool is_parcacigi;
        std::size_t halka_bayt;
        std::int64_t gecikme_ms;
    };
    const Senaryo senaryolar[] = {
        {"saglikli", true, std::size_t{128} << 20, 0},
        {"yazici_durdu", false, std::size_t{1} << 20, 0},
        {"yavas_disk", true, std::size_t{1} << 20, kTakilma_ms},
    };
    std::map<std::string, Yuzdelik> sonuc;
    FizikDurumu fd;
    int no = 0;
    for (const Senaryo& s : senaryolar) {
        GeciciDizin dizin("beklemez_" + std::to_string(no++));
        Ayar a;
        a.dizin = dizin.yol();
        a.kademe = Kademe::telemetri;
        a.ureticiler.push_back(UreticiAyari{"dunya", s.halka_bayt});
        a.is_parcacigi = s.is_parcacigi;
        HedefAyari h;
        h.bosaltma_gecikmesi_ms = s.gecikme_ms;
        a.dosya_fabrikasi = hedef_fabrikasi(h);
        std::string hata;
        const OrnekEkler ekler = ornek_ekler();
        const std::vector<Ek> ek = ekler.ekler();
        auto y = Yazici::ac(a, ornek_ust_veri(kOrnekRunId, "hil_bariyer"), ek, hata);
        DOGRULA(d, y != nullptr, std::string(s.ad) + ": yazıcı açıldı: " + hata);
        if (!y) {
            continue;
        }
        Uretici& u = y->uretici(0);
        std::vector<std::uint32_t> sure(kCagri);
        std::set<std::uint64_t> dusen_adimlar;
        std::uint64_t ilk_dusen = 0;
        std::uint64_t son_dusen = 0;
        for (int i = 0; i < kCagri; ++i) {
            const auto adim = static_cast<std::uint64_t>(i);
            fd.guncelle(adim);
            const std::int64_t t0 = simdi_ns();
            const YazmaSonucu r = u.yaz<Kanal::fizik_durum>(static_cast<std::int64_t>(i) * 5'000'000, adim, fd.y);
            const std::int64_t t1 = simdi_ns();
            sure[static_cast<std::size_t>(i)] = static_cast<std::uint32_t>(std::min<std::int64_t>(t1 - t0, 0xFFFFFFFFll));
            if (r == YazmaSonucu::dustu_halka) {
                if (dusen_adimlar.empty()) {
                    ilk_dusen = adim;
                }
                son_dusen = adim;
                dusen_adimlar.insert(adim);
            } else {
                DOGRULA(d, r == YazmaSonucu::yazildi, std::string(s.ad) + ": beklenmeyen sonuç");
            }
        }
        sonuc[s.ad] = yuzdelik(sure);
        const UreticiSayaclari us = u.sayaclar();
        const KapanisSonucu ks = y->kapat(ornek_kapanis());
        DOGRULA(d, ks.dosya_kapandi, std::string(s.ad) + ": dosya kapandı: " + ks.hata);
        // İşlevsel ölçütler (her ortamda): hiçbir ileti sessizce kaybolmaz.
        DOGRULA(d, us.uretilen == static_cast<std::uint64_t>(kCagri) && us.uretilen == us.kuyruklanan + us.dusen_halka + us.dusen_gecersiz &&
                       us.dusen_halka == dusen_adimlar.size(),
                std::string(s.ad) + ": üretilen = kuyruklanan + düşen");
        if (!s.is_parcacigi || s.gecikme_ms > 0) {
            DOGRULA(d, us.dusen_halka > 0, std::string(s.ad) + ": halka doldu ve ileti düştü (sınama anlamlı)");
        }
        AcmaHatasi ah = AcmaHatasi::yok;
        const auto o = Okuyucu::ac(y->yol(), ah);
        DOGRULA(d, o != nullptr && o->ozet().saglam(), std::string(s.ad) + ": dosya sağlam");
        if (!o) {
            continue;
        }
        const std::map<std::string, KanalSayimi> sayim = kanal_sayimi(*o);
        const KanalSayimi fs = sayim.count("fizik/durum") != 0 ? sayim.at("fizik/durum") : KanalSayimi{};
        DOGRULA(d, fs.ileti == us.kuyruklanan && fs.kayip == us.dusen_halka && fs.ileti + fs.kayip == us.uretilen,
                std::string(s.ad) + ": yazılan + düşen = üretilen (" + std::to_string(fs.ileti) + " + " + std::to_string(fs.kayip) + ")");
        if (!dusen_adimlar.empty()) {
            // `kayit/eksik` aralıkları düşen iletilerle sayıca ve aralıkça eşleşir.
            std::uint64_t ilk = UINT64_MAX;
            std::uint64_t son = 0;
            for (const EksikKaydi& e : o->ozet().eksikler) {
                DOGRULA(d, e.kanal->ad == "fizik/durum" && e.neden == 0, std::string(s.ad) + ": eksik kaydı kanalı ve nedeni");
                DOGRULA(d, e.ilk_adim <= e.son_adim && e.kayip_sayisi <= e.son_adim - e.ilk_adim + 1 &&
                               e.ilk_sim_t_ns == static_cast<std::int64_t>(e.ilk_adim) * 5'000'000 &&
                               e.son_sim_t_ns == static_cast<std::int64_t>(e.son_adim) * 5'000'000,
                        std::string(s.ad) + ": eksik kaydı aralığı tutarlı");
                DOGRULA(d, dusen_adimlar.count(e.ilk_adim) == 1 && dusen_adimlar.count(e.son_adim) == 1,
                        std::string(s.ad) + ": aralığın uçları düşen iletilerdir");
                ilk = std::min(ilk, e.ilk_adim);
                son = std::max(son, e.son_adim);
            }
            DOGRULA(d, ilk == ilk_dusen && son == son_dusen, std::string(s.ad) + ": eksik aralıklarının birleşimi düşenleri kapsar");
            DOGRULA(d, o->ozet().guvence == Guvence::fizik && o->ozet().guvence_on_eki_son_adim == std::optional<std::uint64_t>(ilk_dusen - 1),
                    std::string(s.ad) + ": önek güvencesi ilk eksik adımdan önce biter");
        }
        std::printf("  %-13s p50 %.0f ns  p99 %.0f ns  en kötü %.0f ns  (çağrı %d, düşen %llu, eksik kaydı %zu)\n", s.ad,
                    sonuc[s.ad].p50, sonuc[s.ad].p99, sonuc[s.ad].en_kotu, kCagri, static_cast<unsigned long long>(us.dusen_halka),
                    o->ozet().eksikler.size());
    }
    // Zamanlama ölçütleri (W4-K24, W4-K29: yalnız ortak yardımcıdan).
    if (sonuc.size() == 3) {
        const Yuzdelik& sg = sonuc["saglikli"];
        for (const char* ad : {"yazici_durdu", "yavas_disk"}) {
            const Yuzdelik& y = sonuc[ad];
            char olcum[200];
            std::snprintf(olcum, sizeof olcum, "p99 %.0f ns (sağlıklı %.0f ns; sınır 3x + 1 µs)", y.p99, sg.p99);
            DOGRULA(d, ida::test::zamanlama(y.p99 <= 3.0 * sg.p99 + 1000.0, std::string("üretici çağrısı p99, ") + ad, olcum),
                    std::string(ad) + ": p99 sağlıklı koşudan ayırt edilemez olmalı");
            std::snprintf(olcum, sizeof olcum, "en kötü %.0f ns (yazım hedefinin takılması %lld ms; sınır yarısı)", y.en_kotu,
                          static_cast<long long>(kTakilma_ms));
            DOGRULA(d, ida::test::zamanlama(y.en_kotu < static_cast<double>(kTakilma_ms) * 0.5e6, std::string("üretici çağrısı en kötü, ") + ad, olcum),
                    std::string(ad) + ": üretici yazım hedefini beklememeli");
        }
    }
    return d.hata;
}

// A6-K06: üretici çağrısı bellek ayırmaz (yazıldı, süzüldü, düştü ve kapalı yollarının hepsinde).
int test_uretici_ayirmaz()
{
    Denetci d;
    if (!ayirma_sayimi_var()) {
        std::printf("  sanitizer'lı derleme: ayırma sayımı yapılmadı (ölçüm host-debug/host-release'te)\n");
        return 0;
    }
    Duzenek dz("ayirmaz", Kademe::telemetri, "hil_bariyer", [](Ayar& a) { a.ureticiler[0].halka_bayt = 1u << 16; });
    DOGRULA(d, dz.y != nullptr, "yazıcı açıldı: " + dz.hata);
    if (!dz.y) {
        return d.hata;
    }
    FizikDurumu fd;
    fd.guncelle(1);
    Depo depo;
    a3::yuk_olay olay;
    ornek_doldur(olay, depo, 1);
    a3::yuk_imu imu;
    ornek_doldur(imu, depo, 2);
    yuk_a1_blok blok;
    ornek_doldur(blok, depo, 3);
    yuk_hil_bariyer bariyer;
    bariyer.karar = 1;
    yuk_cekirdek_cagri cagri;  // bu kademede süzülür
    ornek_doldur(cagri, depo, 4);
    yuk_hil_oturum uzun;       // kodlanamaz: `ayrinti` üst sınırı aşıyor
    const std::string cok_uzun(400, 'x');
    uzun.ayrinti = cok_uzun;

    Uretici& u = dz.u();
    std::array<int, 5> sayim{};  // YazmaSonucu değerine göre; sabit boylu (sayım sırasında ayırma olmasın)
    ayirma_sayimini_baslat();
    for (int i = 0; i < 4000; ++i) {
        const auto adim = static_cast<std::uint64_t>(i);
        const std::int64_t t = static_cast<std::int64_t>(i) * 1000;
        const YazmaSonucu r[] = {
            u.yaz<Kanal::fizik_durum>(t, adim, fd.y),
            u.yaz<Kanal::olay>(t, adim, olay),
            u.yaz<Kanal::sensor_imu>(t, adim, imu),
            u.yaz<Kanal::a1_kart_sim>(t, adim, blok),
            u.yaz<Kanal::hil_bariyer>(t, adim, bariyer),
            u.yaz<Kanal::cekirdek_cagri>(t, adim, cagri),
            u.yaz<Kanal::hil_oturum>(t, adim, uzun),
            u.yaz<Kanal::olay>(-1, adim, olay),
            u.yaz<Kanal::olay>(t, adim, olay, 12345),
        };
        (void)u.duvar_ns();
        (void)u.sayaclar();
        for (const YazmaSonucu s : r) {
            ++sayim[static_cast<std::size_t>(s)];
        }
    }
    const std::uint64_t ayirma_sayisi = ayirma_sayimini_bitir();
    DOGRULA(d, ayirma_sayisi == 0, "üretici yolunda bellek ayırma sayısı: " + std::to_string(ayirma_sayisi));
    const int yazildi = sayim[static_cast<std::size_t>(YazmaSonucu::yazildi)];
    const int suzuldu = sayim[static_cast<std::size_t>(YazmaSonucu::suzuldu)];
    const int dustu_halka = sayim[static_cast<std::size_t>(YazmaSonucu::dustu_halka)];
    const int dustu_gecersiz = sayim[static_cast<std::size_t>(YazmaSonucu::dustu_gecersiz)];
    // Halka dolduktan sonra kodlanamayan ileti de halka taşması olarak düşer; negatif zamanlı her
    // ileti ve halka dolmadan önceki kodlanamayanlar `dustu_gecersiz`'dir.
    DOGRULA(d, yazildi > 0 && suzuldu == 4000 && dustu_halka > 0 && dustu_gecersiz > 4000,
            "yolların hepsi sınandı: yazıldı " + std::to_string(yazildi) + ", süzüldü " + std::to_string(suzuldu) +
                ", halka taşması " + std::to_string(dustu_halka) + ", geçersiz " + std::to_string(dustu_gecersiz));
    (void)dz.kapat();
    // Kapalı dosyada da ayırma yok.
    ayirma_sayimini_baslat();
    int kapali = 0;
    for (int i = 0; i < 1000; ++i) {
        kapali += dz.u().yaz<Kanal::olay>(1, 1, olay) == YazmaSonucu::kapali ? 1 : 0;
    }
    const std::uint64_t ayirma = ayirma_sayimini_bitir();
    DOGRULA(d, kapali == 1000 && ayirma == 0, "kapalı dosyada üretici yolu");
    return d.hata;
}

// A6-K06, K30: düşen her ileti `kayit/eksik`'te; yazılan + düşen = üretilen (belirlenimci, tek iş parçacığı).
int test_eksik_sayim()
{
    Denetci d;
    Duzenek dz("eksik_sayim", Kademe::telemetri, "hil_bariyer", [](Ayar& a) { a.ureticiler[0].halka_bayt = 16384; });
    DOGRULA(d, dz.y != nullptr, "yazıcı açıldı: " + dz.hata);
    if (!dz.y) {
        return d.hata;
    }
    Uretici& u = dz.u();
    FizikDurumu fd;
    Depo depo;
    a3::yuk_olay olay;
    olay.ayrinti = "{}";
    struct Beklenen {
        std::set<std::uint64_t> yazilan;
        std::set<std::uint64_t> dusen;
    };
    std::map<std::string, Beklenen> beklenen;
    const auto fizik_yaz = [&](std::uint64_t adim) {
        fd.guncelle(adim);
        const YazmaSonucu r = u.yaz<Kanal::fizik_durum>(static_cast<std::int64_t>(adim) * 1000, adim, fd.y);
        (r == YazmaSonucu::yazildi ? beklenen["fizik/durum"].yazilan : beklenen["fizik/durum"].dusen).insert(adim);
        return r;
    };
    const auto olay_yaz = [&](std::uint64_t adim) {
        olay.adim_no = adim;
        const YazmaSonucu r = u.yaz<Kanal::olay>(static_cast<std::int64_t>(adim) * 1000, adim, olay);
        (r == YazmaSonucu::yazildi ? beklenen["olay"].yazilan : beklenen["olay"].dusen).insert(adim);
        return r;
    };
    // 1) Telemetri halkayı doldurur ve düşmeye başlar; olay iletileri ayrılan payla yine girer.
    std::uint64_t adim = 0;
    for (; adim < 60; ++adim) {
        (void)fizik_yaz(adim);
    }
    DOGRULA(d, !beklenen["fizik/durum"].dusen.empty() && !beklenen["fizik/durum"].yazilan.empty(), "telemetri düşmeye başladı");
    if (d.hata != 0) {
        return d.hata;  // sonraki denetimler düşen iletilere dayanır
    }
    DOGRULA(d, olay_yaz(adim) == YazmaSonucu::yazildi && olay_yaz(adim + 1) == YazmaSonucu::yazildi,
            "telemetri düşerken olay iletisi halkaya girer (pay)");
    const std::size_t ilk_dusen_sayisi = beklenen["fizik/durum"].dusen.size();
    // 2) Yazıcı boşaltır ve düşmeleri bildirir; sonra yeniden taşma (ikinci aralık).
    DOGRULA(d, dz.y->isle(), "tur iş yaptı");
    adim = 100;
    for (; adim < 105; ++adim) {
        DOGRULA(d, fizik_yaz(adim) == YazmaSonucu::yazildi, "boşaltmadan sonra yer var");
    }
    for (; adim < 200; ++adim) {
        (void)fizik_yaz(adim);
        (void)olay_yaz(adim);
    }
    DOGRULA(d, beklenen["fizik/durum"].dusen.size() > ilk_dusen_sayisi && !beklenen["olay"].dusen.empty(),
            "ikinci taşma: telemetri ve (halka tümüyle dolunca) olay düştü");
    dz.y->isle();
    // 3) Halkaya sığmayacak kadar büyük ileti ve kodlanamayan ileti.
    yuk_a1_blok blok;
    const std::vector<std::uint8_t> buyuk(60000, 0x5A);
    blok.bayt = buyuk;
    DOGRULA(d, u.yaz<Kanal::a1_kart_sim>(300'000, 300, blok) == YazmaSonucu::dustu_halka, "halkaya sığmayan ileti düşer");
    beklenen["a1/kart_sim"].dusen.insert(300);
    yuk_hil_oturum oturum;
    const std::string uzun(300, 'u');
    oturum.ayrinti = uzun;  // en çok 256
    DOGRULA(d, u.yaz<Kanal::hil_oturum>(301'000, 301, oturum) == YazmaSonucu::dustu_gecersiz, "kodlanamayan ileti düşer");
    oturum.ayrinti = "tamam";
    DOGRULA(d, u.yaz<Kanal::hil_oturum>(302'000, 302, oturum) == YazmaSonucu::yazildi, "kodlanamayan iletiden sonra halka sağlam");
    beklenen["hil/oturum"].yazilan.insert(302);

    const UreticiSayaclari us = u.sayaclar();
    const KapanisSonucu ks = dz.kapat();
    DOGRULA(d, ks.dosya_kapandi, "dosya kapandı: " + ks.hata);
    std::uint64_t toplam_yazilan = 0;
    std::uint64_t toplam_dusen = 0;
    for (const auto& [kanal, b] : beklenen) {
        toplam_yazilan += b.yazilan.size();
        toplam_dusen += b.dusen.size();
    }
    toplam_dusen += 1;  // kodlanamayan hil/oturum
    DOGRULA(d, us.uretilen == toplam_yazilan + toplam_dusen && us.kuyruklanan == toplam_yazilan &&
                   us.dusen_halka + us.dusen_gecersiz == toplam_dusen && us.dusen_gecersiz == 1,
            "üretici sayaçları: üretilen = kuyruklanan + düşen");
    DOGRULA(d, ks.sayaclar.yazilan_uretici_iletisi == toplam_yazilan && ks.sayaclar.bildirilen_dusen == toplam_dusen &&
                   ks.sayaclar.yazilan_ileti == toplam_yazilan + ks.sayaclar.eksik_kaydi && ks.sayaclar.yazimda_kaybolan == 0,
            "yazıcı sayaçları: yazılan + bildirilen düşen = üretilen");

    const auto o = dz.oku();
    DOGRULA(d, o != nullptr && o->ozet().saglam(), "dosya sağlam");
    if (!o) {
        return d.hata;
    }
    // Dosyadaki iletiler: tam olarak yazılanlar (adım = log_time / 1000), her biri bir kez.
    std::map<std::string, std::set<std::uint64_t>> okunan;
    std::size_t eksik_iletisi = 0;
    for (const OkunanIleti& i : hepsini_oku(*o)) {
        if (i.kanal == "kayit/eksik") {
            ++eksik_iletisi;
            continue;
        }
        DOGRULA(d, okunan[i.kanal].insert(static_cast<std::uint64_t>(i.log_time / 1000)).second, i.kanal + ": ileti iki kez");
    }
    for (const auto& [kanal, b] : beklenen) {
        DOGRULA(d, okunan[kanal] == b.yazilan, kanal + ": dosyadaki iletiler yazılanlarla aynı");
    }
    // `kayit/eksik` kayıtları: kanal başına sayı ve aralık.
    std::map<std::string, std::set<std::uint64_t>> kapsanan;
    std::map<std::string, std::uint64_t> kayip;
    for (const EksikKaydi& e : o->ozet().eksikler) {
        const std::string kanal(e.kanal->ad);
        kayip[kanal] += e.kayip_sayisi;
        const Beklenen& b = beklenen[kanal];
        if (kanal == "hil/oturum") {
            DOGRULA(d, e.neden == 1 && e.ilk_adim == 301 && e.son_adim == 301 && e.kayip_sayisi == 1 &&
                           e.ilk_sim_t_ns == 301'000,
                    "kodlanamayan ileti yazma_hatasi nedeniyle bildirilir");
            continue;
        }
        DOGRULA(d, e.neden == 0, kanal + ": neden halka_tasmasi");
        // Aralıktaki düşen ileti sayısı bildirilen sayıya eşit (aralık tam ve dar).
        std::uint64_t aralikta = 0;
        for (const std::uint64_t a : b.dusen) {
            if (a >= e.ilk_adim && a <= e.son_adim) {
                ++aralikta;
                kapsanan[kanal].insert(a);
            }
        }
        DOGRULA(d, aralikta == e.kayip_sayisi && b.dusen.count(e.ilk_adim) == 1 && b.dusen.count(e.son_adim) == 1,
                kanal + ": eksik kaydı aralığı düşenlerle eşleşir (" + std::to_string(e.ilk_adim) + ".." + std::to_string(e.son_adim) + ")");
        DOGRULA(d, e.ilk_sim_t_ns == static_cast<std::int64_t>(e.ilk_adim) * 1000 && e.son_sim_t_ns == static_cast<std::int64_t>(e.son_adim) * 1000,
                kanal + ": eksik kaydı zamanları");
    }
    for (const auto& [kanal, b] : beklenen) {
        if (kanal == "hil/oturum") {
            DOGRULA(d, kayip[kanal] == 1, "hil/oturum kaybı");
            continue;
        }
        DOGRULA(d, kayip[kanal] == b.dusen.size() && kapsanan[kanal] == b.dusen, kanal + ": düşen her ileti kayit/eksik'te");
    }
    DOGRULA(d, eksik_iletisi == o->ozet().eksikler.size() && eksik_iletisi == ks.sayaclar.eksik_kaydi && eksik_iletisi >= 5,
            "eksik kaydı sayısı: " + std::to_string(eksik_iletisi));
    // Kapanış kaydı ve önek güvencesi (A6-K33): fizik düzeyinin kanallarındaki ilk eksik adım.
    const std::uint64_t ilk_fizik = *beklenen["fizik/durum"].dusen.begin();
    DOGRULA(d, ks.kapanis_kaydi.at("kayit_eksik") == "true" && ks.kapanis_kaydi.at("ilk_eksik_adim") == std::to_string(ilk_fizik) &&
                   ks.kapanis_kaydi.at("tekrar_guvencesi") == "fizik" &&
                   ks.kapanis_kaydi.at("guvence_on_eki_son_adim") == std::to_string(ilk_fizik - 1),
            "kapanış kaydı: eksik ve önek");
    DOGRULA(d, o->ozet().guvence == Guvence::fizik && o->ozet().guvence_on_eki_son_adim == std::optional<std::uint64_t>(ilk_fizik - 1) &&
                   o->ozet().ilk_eksik_fizik == std::optional<std::uint64_t>(ilk_fizik),
            "okuyucu: önek güvencesinin sınırı ilk eksik adım");
    return d.hata;
}

// Birden çok üretici iş parçacığı ve yazıcı iş parçacığı birlikte: hiçbir ileti kaybolmaz, yinelenmez.
int test_es_zamanli_ureticiler()
{
    Denetci d;
    GeciciDizin dizin("es_zamanli");
    Ayar a;
    a.dizin = dizin.yol();
    a.kademe = Kademe::telemetri;
    constexpr int kUretici = 4;
    constexpr std::uint64_t kIleti = 40000;
    for (int i = 0; i < kUretici; ++i) {
        a.ureticiler.push_back(UreticiAyari{"u" + std::to_string(i), std::size_t{1} << 16});
    }
    a.parca_hedefi_bayt = 65536;
    const OrnekEkler ekler = ornek_ekler();
    const std::vector<Ek> ek = ekler.ekler();
    std::string hata;
    auto y = Yazici::ac(a, ornek_ust_veri(kOrnekRunId, "hil_bariyer"), ek, hata);
    DOGRULA(d, y != nullptr, "yazıcı açıldı: " + hata);
    if (!y) {
        return d.hata;
    }
    const Kanal kanallari[kUretici] = {Kanal::sensor_imu, Kanal::sensor_temel_imu, Kanal::sensor_gercek_imu, Kanal::hil_tf_yazim};
    std::vector<std::thread> is;
    for (int i = 0; i < kUretici; ++i) {
        is.emplace_back([&, i] {
            Uretici& u = y->uretici(static_cast<std::size_t>(i));
            a3::yuk_imu imu;
            yuk_hil_tf_yazim tf;
            for (std::uint64_t n = 0; n < kIleti; ++n) {
                const auto t = static_cast<std::int64_t>(n) * 1000 + i;
                if (kanallari[i] == Kanal::hil_tf_yazim) {
                    tf.batch_seq = static_cast<std::uint32_t>(n);
                    (void)u.yaz<Kanal::hil_tf_yazim>(t, n, tf);
                } else {
                    imu.seq = static_cast<std::uint32_t>(n);
                    if (i == 0) {
                        (void)u.yaz<Kanal::sensor_imu>(t, n, imu);
                    } else if (i == 1) {
                        (void)u.yaz<Kanal::sensor_temel_imu>(t, n, imu);
                    } else {
                        (void)u.yaz<Kanal::sensor_gercek_imu>(t, n, imu);
                    }
                }
                // Patlamalı üretim: her patlama halkayı taşırır, aradaki duraklamada yazıcı boşaltır; böylece
                // yazım ve düşme birbirine karışır (birden çok eksik aralığı).
                if (n % 2000 == 1999) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        });
    }
    for (auto& t : is) {
        t.join();
    }
    std::array<UreticiSayaclari, kUretici> us{};
    for (int i = 0; i < kUretici; ++i) {
        us[static_cast<std::size_t>(i)] = y->uretici(static_cast<std::size_t>(i)).sayaclar();
    }
    const KapanisSonucu ks = y->kapat(ornek_kapanis());
    DOGRULA(d, ks.dosya_kapandi, "dosya kapandı: " + ks.hata);
    AcmaHatasi ah = AcmaHatasi::yok;
    const auto o = Okuyucu::ac(y->yol(), ah);
    DOGRULA(d, o != nullptr && o->ozet().saglam(), "dosya sağlam");
    if (!o) {
        return d.hata;
    }
    const std::map<std::string, KanalSayimi> sayim = kanal_sayimi(*o);
    std::uint64_t toplam_dusen = 0;
    for (int i = 0; i < kUretici; ++i) {
        const std::string ad(kanallar()[static_cast<std::size_t>(kanallari[i])].ad);
        const UreticiSayaclari& s = us[static_cast<std::size_t>(i)];
        const KanalSayimi ks2 = sayim.count(ad) != 0 ? sayim.at(ad) : KanalSayimi{};
        DOGRULA(d, s.uretilen == kIleti && s.uretilen == s.kuyruklanan + s.dusen_halka, ad + ": üretici sayaçları");
        DOGRULA(d, ks2.ileti == s.kuyruklanan && ks2.kayip == s.dusen_halka, ad + ": yazılan + düşen = üretilen (" +
                                                                                 std::to_string(ks2.ileti) + " + " + std::to_string(ks2.kayip) + ")");
        toplam_dusen += s.dusen_halka;
    }
    // Sıra numaraları boşluksuz ve tekil; iletiler yinelenmez; okuma sırası doğru.
    std::set<std::uint32_t> siralar;
    std::set<std::pair<std::string, std::int64_t>> anahtarlar;
    std::int64_t onceki_t = -1;
    std::uint32_t onceki_s = 0;
    bool sirali = true;
    std::size_t n = 0;
    Ileti i;
    while (o->sonraki(i)) {
        sirali = sirali && (i.log_time > onceki_t || (i.log_time == onceki_t && i.sequence > onceki_s));
        onceki_t = i.log_time;
        onceki_s = i.sequence;
        siralar.insert(i.sequence);
        if (i.kanal->ad != "kayit/eksik") {
            DOGRULA(d, anahtarlar.emplace(std::string(i.kanal->ad), i.log_time).second, "ileti yinelendi");
        }
        ++n;
    }
    DOGRULA(d, sirali && o->okuma_hatasi().empty(), "okuma sırası");
    DOGRULA(d, siralar.size() == n && n == o->ozet().ileti_sayisi && *siralar.begin() == 1 && *siralar.rbegin() == n,
            "sequence boşluksuz ve tekil");
    std::printf("  %d üretici x %llu ileti: yazılan %llu, düşen %llu, eksik kaydı %zu, parça %llu\n", kUretici,
                static_cast<unsigned long long>(kIleti), static_cast<unsigned long long>(ks.sayaclar.yazilan_uretici_iletisi),
                static_cast<unsigned long long>(toplam_dusen), o->ozet().eksikler.size(),
                static_cast<unsigned long long>(ks.sayaclar.kapanan_parca));
    return d.hata;
}

// A6-K04: açık parça en geç 1 s duvar saatinde kapatılır (enjekte edilen saatle; gerçek zaman beklenmez).
int test_parca_suresi()
{
    Denetci d;
    Duzenek dz("parca_suresi", Kademe::telemetri, "hil_bariyer");
    DOGRULA(d, dz.y != nullptr, "yazıcı açıldı: " + dz.hata);
    if (!dz.y) {
        return d.hata;
    }
    a3::yuk_calistirma_durum y;
    const auto yaz = [&](int adet, int taban) {
        for (int i = 0; i < adet; ++i) {
            DOGRULA(d, dz.u().yaz<Kanal::calistirma_durum>((taban + i) * 1000LL, static_cast<std::uint64_t>(taban + i), y) == YazmaSonucu::yazildi, "yaz");
        }
    };
    const auto diskteki = [&](bool& kapanmis, bool& bozuk) {
        const auto o = dz.oku();
        kapanmis = o ? o->ozet().kapanmis : true;
        bozuk = o ? o->ozet().bozuk : true;
        return o ? o->ozet().ileti_sayisi : std::uint64_t{9999};
    };
    bool kapanmis = false;
    bool bozuk = false;
    yaz(10, 0);
    dz.y->isle();
    DOGRULA(d, dz.y->sayaclar().kapanan_parca == 0, "süre dolmadan parça kapanmaz");
    DOGRULA(d, diskteki(kapanmis, bozuk) == 0 && !kapanmis && !bozuk, "açık parçanın iletileri henüz dosyada değil");
    dz.saat.ilerlet(999'999'999);
    dz.y->isle();
    DOGRULA(d, dz.y->sayaclar().kapanan_parca == 0, "1 s'den 1 ns önce parça açık");
    dz.saat.ilerlet(1);
    dz.y->isle();
    DOGRULA(d, dz.y->sayaclar().kapanan_parca == 1 && dz.y->sayaclar().yazilan_ileti == 10, "1 s dolunca parça kapanır");
    DOGRULA(d, diskteki(kapanmis, bozuk) == 10 && !kapanmis && !bozuk, "kapanan parça dosyada (yazıcı açıkken okunur)");
    // Süre açık parçaya ilk iletinin girdiği andan sayılır.
    dz.saat.ilerlet(5'000'000'000);
    dz.y->isle();
    DOGRULA(d, dz.y->sayaclar().kapanan_parca == 1, "boş parça kapatılmaz");
    yaz(5, 100);
    dz.y->isle();
    dz.saat.ilerlet(600'000'000);
    yaz(5, 200);
    dz.y->isle();
    DOGRULA(d, dz.y->sayaclar().kapanan_parca == 1, "0,6 s: açık");
    dz.saat.ilerlet(400'000'000);
    dz.y->isle();
    DOGRULA(d, dz.y->sayaclar().kapanan_parca == 2 && dz.y->sayaclar().yazilan_ileti == 20, "ilk iletiden 1 s sonra kapanır");
    DOGRULA(d, diskteki(kapanmis, bozuk) == 20, "ikinci parça dosyada");
    yaz(3, 300);
    const KapanisSonucu ks = dz.kapat();
    DOGRULA(d, ks.dosya_kapandi && ks.sayaclar.kapanan_parca == 3 && ks.sayaclar.yazilan_ileti == 23, "kapanış son parçayı yazar");
    DOGRULA(d, diskteki(kapanmis, bozuk) == 23 && kapanmis && !bozuk, "kapanmış dosya");
    return d.hata;
}

// A6-K03: parça hedefi 1 MiB sıkıştırmasız veri; bir kayıt kadar aşılabilir.
int test_parca_hedefi()
{
    Denetci d;
    Duzenek dz("parca_hedefi", Kademe::tam, "hil_bariyer");
    DOGRULA(d, dz.y != nullptr, "yazıcı açıldı: " + dz.hata);
    if (!dz.y) {
        return d.hata;
    }
    DOGRULA(d, dz.ayar.parca_hedefi_bayt == 1048576u && dz.ayar.parca_kapatma_ns == 1'000'000'000, "varsayılan parça ayarları");
    // 6 MiB'lik A1 bloğu (her biri 32 KiB); sıkıştırılamayan veri.
    Rastgele r(7);
    std::vector<std::uint8_t> veri(32768);
    yuk_a1_blok blok;
    std::uint64_t toplam = 0;
    for (int i = 0; i < 192; ++i) {
        for (auto& b : veri) {
            b = static_cast<std::uint8_t>(r.ham());
        }
        blok.bayt = veri;
        DOGRULA(d, dz.u().yaz<Kanal::a1_sim_kart>(i * 1000LL, static_cast<std::uint64_t>(i), blok) == YazmaSonucu::yazildi, "yaz");
        toplam += boyut(blok);
        dz.y->isle();
    }
    const KapanisSonucu ks = dz.kapat();
    DOGRULA(d, ks.dosya_kapandi, "dosya kapandı");
    const KitaplikDokumu kd = kitaplik_ile_oku(yol_metni(dz.y->yol()));
    DOGRULA(d, kd.acildi && kd.parcalar.size() == ks.sayaclar.kapanan_parca, "parça sayısı tutarlı");
    DOGRULA(d, kd.parcalar.size() >= 6 && kd.parcalar.size() <= 7, "6 MiB veri ≈ 6 parça: " + std::to_string(kd.parcalar.size()));
    for (std::size_t i = 0; i < kd.parcalar.size(); ++i) {
        const bool son = i + 1 == kd.parcalar.size();
        DOGRULA(d, kd.parcalar[i].acik_boy <= 1048576u + 32768u + 4096u, "parça hedefi en çok bir kayıt kadar aşılır");
        DOGRULA(d, son || kd.parcalar[i].acik_boy >= 1048576u, "son parça dışındakiler hedefe ulaşmıştır");
        // Sıkıştırmadan kazanç olmayan parça sıkıştırmasız yazılabilir (A6-K03).
        DOGRULA(d, kd.parcalar[i].sikistirma.empty() || kd.parcalar[i].sikistirma == "zstd", "sıkıştırma");
    }
    std::printf("  %llu bayt yük, %zu parça\n", static_cast<unsigned long long>(toplam), kd.parcalar.size());
    return d.hata;
}

// A6-K01: aynı adlı dosya varsa yazıcı onu ezmez; çalıştırma başlamaz.
int test_dosya_ezilmez()
{
    Denetci d;
    GeciciDizin dizin("ezilmez");
    const std::filesystem::path yol = dizin.yol() / (std::string(kOrnekRunId) + ".mcap");
    const std::vector<std::uint8_t> eski = {'e', 's', 'k', 'i', ' ', 'k', 'a', 'y', 'i', 't'};
    DOGRULA(d, dosya_yaz(yol, eski), "var olan dosya hazırlandı");
    SahteSaat saat;
    const Ayar a = elle_ayar(dizin, Kademe::telemetri, saat);
    const OrnekEkler ekler = ornek_ekler();
    const std::vector<Ek> ek = ekler.ekler();
    std::string hata;
    auto y = Yazici::ac(a, ornek_ust_veri(), ek, hata);
    DOGRULA(d, y == nullptr && hata.find("dosya var") != std::string::npos, "var olan dosya açılmaz: " + hata);
    DOGRULA(d, dosya_oku(yol) == eski, "var olan dosyanın içeriği değişmedi");
    // Başka run_id ile aynı dizinde açılır; kapandıktan sonra aynı run_id yine açılamaz.
    const std::string baska = "20261125-101501-ffffff";
    y = Yazici::ac(a, ornek_ust_veri(baska), ek, hata);
    DOGRULA(d, y != nullptr, "başka run_id açılır: " + hata);
    if (y) {
        DOGRULA(d, y->kapat(ornek_kapanis()).dosya_kapandi, "kapandı");
        const std::vector<std::uint8_t> ilk = dosya_oku(dizin.yol() / (baska + ".mcap"));
        auto y2 = Yazici::ac(a, ornek_ust_veri(baska), ek, hata);
        DOGRULA(d, y2 == nullptr, "kapanmış kaydın üstüne açılmaz");
        DOGRULA(d, dosya_oku(dizin.yol() / (baska + ".mcap")) == ilk && !ilk.empty(), "kapanmış kayıt değişmedi");
    }
    // Açılış başarısız olursa (geçersiz üst veri, eksik ek) dosya oluşturulmaz.
    const std::string ucuncu = "20261125-101502-000000";
    AcilisUstVerisi kotu = ornek_ust_veri(ucuncu);
    kotu.kip = "bilinmeyen";
    DOGRULA(d, Yazici::ac(a, kotu, ek, hata) == nullptr, "geçersiz kip reddedilir");
    kotu = ornek_ust_veri("20261125-1015-xyz");
    DOGRULA(d, Yazici::ac(a, kotu, ek, hata) == nullptr, "geçersiz run_id reddedilir");
    std::vector<Ek> eksik_ek(ek.begin(), ek.end() - 1);
    DOGRULA(d, Yazici::ac(a, ornek_ust_veri(ucuncu), eksik_ek, hata) == nullptr && hata.find("zorunlu ek") != std::string::npos,
            "zorunlu ek eksik: " + hata);
    std::vector<Ek> fazla_ek = ek;
    const std::vector<std::uint8_t> bos;
    fazla_ek.push_back(Ek{"bilinmeyen.bin", bos});
    DOGRULA(d, Yazici::ac(a, ornek_ust_veri(ucuncu), fazla_ek, hata) == nullptr, "bilinmeyen ek reddedilir");
    fazla_ek.back().ad = std::string(kKatalogEki);
    DOGRULA(d, Yazici::ac(a, ornek_ust_veri(ucuncu), fazla_ek, hata) == nullptr, "katalog ekini çağıran veremez");
    DOGRULA(d, !std::filesystem::exists(dizin.yol() / (ucuncu + ".mcap")), "başarısız açılış dosya bırakmaz");
    return d.hata;
}

// A6-K10: `log_time`'ı negatif olacak ileti yazılmaz; `kayit/eksik`'te yazma_hatasi ile bildirilir.
int test_negatif_zaman()
{
    Denetci d;
    Duzenek dz("negatif_zaman", Kademe::olaylar, "hil_bariyer");
    DOGRULA(d, dz.y != nullptr, "yazıcı açıldı: " + dz.hata);
    if (!dz.y) {
        return d.hata;
    }
    a3::yuk_calistirma_durum y;
    DOGRULA(d, dz.u().yaz<Kanal::calistirma_durum>(0, 0, y) == YazmaSonucu::yazildi, "sim zamanı 0 yazılır");
    DOGRULA(d, dz.u().yaz<Kanal::calistirma_durum>(-1, 0, y) == YazmaSonucu::dustu_gecersiz, "negatif log_time yazılmaz");
    DOGRULA(d, dz.u().yaz<Kanal::calistirma_durum>(-5'000'000, 0, y) == YazmaSonucu::dustu_gecersiz, "negatif log_time yazılmaz");
    DOGRULA(d, dz.u().yaz<Kanal::calistirma_durum>(7, 1, y) == YazmaSonucu::yazildi, "sonraki ileti yazılır");
    const KapanisSonucu ks = dz.kapat();
    DOGRULA(d, ks.dosya_kapandi, "dosya kapandı");
    const auto o = dz.oku();
    DOGRULA(d, o != nullptr && o->ozet().saglam(), "dosya sağlam");
    if (!o) {
        return d.hata;
    }
    const std::vector<OkunanIleti> v = hepsini_oku(*o);
    std::vector<std::int64_t> zamanlar;
    for (const OkunanIleti& i : v) {
        DOGRULA(d, i.log_time >= 0, "dosyada negatif log_time yok");
        if (i.kanal == "calistirma/durum") {
            zamanlar.push_back(i.log_time);
        }
    }
    DOGRULA(d, zamanlar == std::vector<std::int64_t>({0, 7}), "yalnız negatif olmayanlar yazıldı");
    DOGRULA(d, o->ozet().eksikler.size() == 1, "bir eksik kaydı");
    if (o->ozet().eksikler.size() == 1) {
        const EksikKaydi& e = o->ozet().eksikler[0];
        DOGRULA(d, e.kanal->ad == "calistirma/durum" && e.neden == 1 && e.kayip_sayisi == 2 && e.ilk_sim_t_ns == -1 &&
                       e.son_sim_t_ns == -5'000'000,
                "yazma_hatasi nedeniyle, düşen iletilerin zamanlarıyla bildirilir");
    }
    DOGRULA(d, ks.kapanis_kaydi.at("kayit_eksik") == "true", "kapanış kaydı: kayit_eksik");
    return d.hata;
}

// A6-K12: bir dosya en çok 2^32 − 1 ileti taşır; sınırda `sinira_ulasti` ile kapanır, çalıştırma kayıtsız sürer.
int test_sira_siniri()
{
    Denetci d;
    {
        GeciciDizin dizin("sira_gecersiz");
        SahteSaat saat;
        Ayar a = elle_ayar(dizin, Kademe::telemetri, saat);
        a.ilk_sira = 0;
        const OrnekEkler ekler = ornek_ekler();
        const std::vector<Ek> ek = ekler.ekler();
        std::string hata;
        DOGRULA(d, Yazici::ac(a, ornek_ust_veri(), ek, hata) == nullptr, "sequence 0'dan başlayamaz");
    }
    // Enjekte edilen sayaç: sınıra 400 kala. Yedek (düşme bildirimleri için) 2·1·61·2 + 16 = 260.
    constexpr std::uint32_t kIlk = 0xFFFFFFFFu - 400u;
    Duzenek dz("sira_siniri", Kademe::telemetri, "hil_bariyer", [](Ayar& a) { a.ilk_sira = kIlk; });
    DOGRULA(d, dz.y != nullptr, "yazıcı açıldı: " + dz.hata);
    if (!dz.y) {
        return d.hata;
    }
    KapanisAlanlari taslak = ornek_kapanis();
    taslak.son_adim_no = 123;
    taslak.gecerli = true;
    dz.y->kapanis_taslagi(taslak);
    a3::yuk_calistirma_durum y;
    std::uint64_t kabul = 0;
    std::uint64_t kapali = 0;
    for (int i = 0; i < 1000; ++i) {
        const YazmaSonucu r = dz.u().yaz<Kanal::calistirma_durum>(i * 1000LL, static_cast<std::uint64_t>(i), y);
        kabul += r == YazmaSonucu::yazildi ? 1u : 0u;
        kapali += r == YazmaSonucu::kapali ? 1u : 0u;
        if (i % 20 == 19) {
            dz.y->isle();
        }
    }
    DOGRULA(d, dz.y->durum() == YaziciDurumu::sinira_ulasti, "yazıcı sınıra ulaştı");
    DOGRULA(d, kapali > 0 && kabul + kapali == 1000, "dosya kapandıktan sonra üretici `kapali` alır; çalıştırma sürer");
    const YaziciSayaclari ys = dz.y->sayaclar();
    DOGRULA(d, ys.yazilan_uretici_iletisi == 400u - 260u + 1u, "sınırdan önce yazılan ileti: " + std::to_string(ys.yazilan_uretici_iletisi));
    DOGRULA(d, ys.yazilan_uretici_iletisi + ys.kapali_iken_atilan == kabul, "kuyruktaki iletilerin hesabı");
    const KapanisSonucu ks = dz.kapat();  // daha önce kapandı: o kapanışın sonucu
    DOGRULA(d, ks.dosya_kapandi && ks.bitis_durumu == BitisDurumu::sinira_ulasti && ks.kapanis_kaydi.at("bitis_durumu") == "sinira_ulasti",
            "bitis_durumu = sinira_ulasti");
    DOGRULA(d, ks.kapanis_kaydi.at("son_adim_no") == "123", "kapanış alanları son taslaktan");
    const auto o = dz.oku();
    DOGRULA(d, o != nullptr && o->ozet().saglam(), "dosya düzgün kapandı ve sağlam");
    if (!o) {
        return d.hata;
    }
    const std::vector<OkunanIleti> v = hepsini_oku(*o);
    DOGRULA(d, v.size() == ys.yazilan_uretici_iletisi, "dosyadaki ileti sayısı");
    for (std::size_t i = 0; i < v.size(); ++i) {
        DOGRULA(d, v[i].sequence == kIlk + i && v[i].log_time == static_cast<std::int64_t>(i) * 1000, "sequence sınırın altında ve ardışık");
    }
    // Önek güvencesi: son tam kaydedilen adım (A6-K12 → K33).
    const std::uint64_t son_adim = v.empty() ? 0 : static_cast<std::uint64_t>(v.back().log_time / 1000);
    DOGRULA(d, ks.kapanis_kaydi.at("tekrar_guvencesi") == "fizik" &&
                   ks.kapanis_kaydi.at("guvence_on_eki_son_adim") == std::to_string(son_adim - 1),
            "önek güvencesi son tam adıma kadar");
    DOGRULA(d, o->ozet().guvence == Guvence::fizik && o->ozet().guvence_on_eki_son_adim == std::optional<std::uint64_t>(son_adim - 1),
            "okuyucu: önek");
    return d.hata;
}

// A6-K20: kayıt kademesi ve `kamera_kareleri` çalıştırma boyunca değişmez.
int test_kademe_degismez()
{
    Denetci d;
    GeciciDizin dizin("kademe_degismez");
    SahteSaat saat;
    const Ayar a = elle_ayar(dizin, Kademe::telemetri, saat);
    const OrnekEkler ekler = ornek_ekler();
    const std::vector<Ek> ek = ekler.ekler();
    std::string hata;
    // Üst verideki kademe ayardakiyle çelişemez; kamera kareleri bu dilimde yalnız `kapali` (W5-K37).
    AcilisUstVerisi u = ornek_ust_veri();
    u.kayit_kademesi = "tam";
    DOGRULA(d, Yazici::ac(a, u, ek, hata) == nullptr, "üst veride farklı kademe reddedilir");
    u = ornek_ust_veri();
    u.kamera_kareleri = "surekli";
    DOGRULA(d, Yazici::ac(a, u, ek, hata) == nullptr, "kamera_kareleri != kapali reddedilir");
    u = ornek_ust_veri();
    u.a6_surumu = "2.0";
    DOGRULA(d, Yazici::ac(a, u, ek, hata) == nullptr, "başka a6 sürümü reddedilir");
    u = ornek_ust_veri();
    u.kayit_kademesi = "telemetri";
    u.kamera_kareleri = "kapali";
    auto y = Yazici::ac(a, u, ek, hata);
    DOGRULA(d, y != nullptr, "tutarlı üst veri: " + hata);
    if (!y) {
        return d.hata;
    }
    // Arayüzde kademeyi değiştiren bir işlev yoktur; kademe açılıştaki değerdir ve `tam` kanalı
    // çalıştırmanın her anında süzülür.
    yuk_a1_blok blok;
    a3::yuk_calistirma_durum cd;
    for (int i = 0; i < 100; ++i) {
        DOGRULA(d, y->kademe() == Kademe::telemetri && y->uretici(0).kademe() == Kademe::telemetri, "kademe sabit");
        DOGRULA(d, y->uretici(0).yaz<Kanal::a1_sim_kart>(i, 0, blok) == YazmaSonucu::suzuldu, "tam kademesinin kanalı süzülür");
        DOGRULA(d, y->uretici(0).yaz<Kanal::a1_kart_sim>(i, 0, blok) == YazmaSonucu::yazildi, "telemetri kanalı yazılır");
        DOGRULA(d, y->uretici(0).yaz<Kanal::calistirma_durum>(i, 0, cd) == YazmaSonucu::yazildi, "olaylar kanalı yazılır");
        y->isle();
    }
    // Güncelleme kaydı kademeyi ya da kamera karelerini değiştiremez (kapalı liste; A6-K46).
    DOGRULA(d, !y->guncelle("kayit_kademesi", "tam", hata) && !y->guncelle("kamera_kareleri", "surekli", hata),
            "kademe güncellemeyle değişmez");
    DOGRULA(d, y->kapat(ornek_kapanis()).dosya_kapandi, "kapandı");
    AcmaHatasi ah = AcmaHatasi::yok;
    const auto o = Okuyucu::ac(y->yol(), ah);
    DOGRULA(d, o != nullptr && o->ozet().kademe == Kademe::telemetri && o->ozet().ust_veri.at("kayit_kademesi") == "telemetri" &&
                   o->ozet().ust_veri.at("kamera_kareleri") == "kapali",
            "dosyadaki kademe");
    if (o) {
        for (const KanalOzeti& k : o->ozet().kanallar) {
            DOGRULA(d, k.konu != "a1/sim_kart", "tam kademesinin kanalı dosyada yok");
        }
    }
    return d.hata;
}

// A6-K44…K46: olağan kapanış; kapanış kaydının alanları.
int test_kapanis()
{
    Denetci d;
    {
        Duzenek dz("kapanis", Kademe::tam, "sil");
        DOGRULA(d, dz.y != nullptr, "yazıcı açıldı: " + dz.hata);
        if (!dz.y) {
            return d.hata;
        }
        a3::yuk_sayac_gercek_zaman sayac;
        a3::yuk_calistirma_durum cd;
        for (int i = 0; i < 10; ++i) {
            (void)dz.u().yaz<Kanal::calistirma_durum>(i * 1000LL, static_cast<std::uint64_t>(i), cd);
        }
        // Kapanıştan önce son sayaç iletisi üretilir (A6-K44); kapanış bekleyen her şeyi yazar.
        DOGRULA(d, dz.u().yaz<Kanal::sayac_gercek_zaman>(10'000, 10, sayac) == YazmaSonucu::yazildi, "son sayaç iletisi");
        KapanisAlanlari k;
        k.bitis_durumu = BitisDurumu::durduruldu;
        k.son_adim_no = 4800;
        k.son_sim_t_ns = 6'000'000'000;
        k.gecerli = false;
        k.gercek_zamani_kacirdi = true;
        k.gecerlilik_nedenleri = "gercek_zamani_kacirdi,kacirilan_parti";
        k.sayaclar = "kayit_dusen=0,kacirilan_parti=3";
        const KapanisSonucu ks = dz.y->kapat(k);
        DOGRULA(d, ks.dosya_kapandi && ks.bitis_durumu == BitisDurumu::durduruldu && dz.y->durum() == YaziciDurumu::kapandi, "kapandı");
        const UstVeri beklenen = {
            {"bitis_durumu", "durduruldu"},
            {"son_adim_no", "4800"},
            {"son_sim_t_ns", "6000000000"},
            {"gecerli", "false"},
            {"gercek_zamani_kacirdi", "true"},
            {"gecerlilik_nedenleri", "gercek_zamani_kacirdi,kacirilan_parti"},
            {"sayaclar", "kayit_dusen=0,kacirilan_parti=3"},
            {"kayit_eksik", "false"},
            {"tekrar_guvencesi", "girdi"},
            {"ileti_sayisi", "11"},
        };
        DOGRULA(d, ks.kapanis_kaydi == beklenen, "kapanış kaydı");
        const auto o = dz.oku();
        DOGRULA(d, o != nullptr && o->ozet().saglam() && o->ozet().kapanis.has_value() && *o->ozet().kapanis == beklenen,
                "dosyadaki kapanış kaydı");
        if (o) {
            const std::vector<OkunanIleti> v = hepsini_oku(*o);
            DOGRULA(d, v.size() == 11 && v.back().kanal == "sayac/gercek_zaman", "son ileti sayaç iletisi");
        }
        // İkinci `kapat` aynı sonucu verir; kapandıktan sonra üretici `kapali` alır.
        const KapanisSonucu ks2 = dz.kapat();
        DOGRULA(d, ks2.dosya_kapandi && ks2.kapanis_kaydi == beklenen, "ikinci kapat aynı sonucu döndürür");
        DOGRULA(d, dz.u().yaz<Kanal::calistirma_durum>(1, 1, cd) == YazmaSonucu::kapali, "kapalı dosyaya yazılmaz");
        std::string hata;
        DOGRULA(d, !dz.y->guncelle("kart_fw_version", "1", hata), "kapalı dosya güncellenmez");
    }
    {
        // A6-K53: canlı müdahale; A6-K38: güvencesiz başlatma.
        GeciciDizin dizin("kapanis_guvence");
        SahteSaat saat;
        const Ayar a = elle_ayar(dizin, Kademe::tam, saat);
        const OrnekEkler ekler = ornek_ekler();
        const std::vector<Ek> ek = ekler.ekler();
        std::string hata;
        AcilisUstVerisi u = ornek_ust_veri("20261125-101500-000001", "tekrar_fizik");
        u.kaynak_run_id = "20261125-090000-abcdef";
        auto y = Yazici::ac(a, u, ek, hata);
        DOGRULA(d, y != nullptr, "tekrar koşusu kaydı: " + hata);
        if (y) {
            KapanisAlanlari k = ornek_kapanis();
            k.canli_mudahale_adimi = 777;
            const KapanisSonucu ks = y->kapat(k);
            DOGRULA(d, ks.kapanis_kaydi.at("tekrar_guvencesi") == "yok" && ks.kapanis_kaydi.at("guvence_nedenleri") == "canli_mudahale" &&
                           ks.kapanis_kaydi.at("canli_mudahale_adimi") == "777",
                    "canlı müdahale: güvence yok");
            AcmaHatasi ah = AcmaHatasi::yok;
            const auto o = Okuyucu::ac(y->yol(), ah);
            DOGRULA(d, o != nullptr && o->ozet().guvence == Guvence::yok, "okuyucu: güvence yok");
        }
        u = ornek_ust_veri("20261125-101500-000002", "tekrar_fizik");
        u.kaynak_run_id = "20261125-090000-abcdef";
        u.guvencesiz_nedenler = "sim_surumu,derleyici";
        y = Yazici::ac(a, u, ek, hata);
        DOGRULA(d, y != nullptr, "güvencesiz tekrar koşusu kaydı: " + hata);
        if (y) {
            const KapanisSonucu ks = y->kapat(ornek_kapanis());
            DOGRULA(d, ks.kapanis_kaydi.at("tekrar_guvencesi") == "yok" && ks.kapanis_kaydi.at("guvence_nedenleri") == "guvencesiz_baslatma",
                    "güvencesiz başlatma: güvence yok");
        }
    }
    {
        // `kapat` çağrılmadan yıkılan yazıcı dosyayı son taslakla ve `hata` ile kapatır.
        GeciciDizin dizin("kapanis_yikici");
        SahteSaat saat;
        const Ayar a = elle_ayar(dizin, Kademe::olaylar, saat);
        const OrnekEkler ekler = ornek_ekler();
        const std::vector<Ek> ek = ekler.ekler();
        std::string hata;
        std::filesystem::path yol;
        {
            auto y = Yazici::ac(a, ornek_ust_veri(), ek, hata);
            DOGRULA(d, y != nullptr, "yazıcı açıldı: " + hata);
            if (!y) {
                return d.hata;
            }
            yol = y->yol();
            KapanisAlanlari taslak = ornek_kapanis();
            taslak.son_adim_no = 55;
            y->kapanis_taslagi(taslak);
            a3::yuk_calistirma_durum cd;
            (void)y->uretici(0).yaz<Kanal::calistirma_durum>(1, 1, cd);
        }
        AcmaHatasi ah = AcmaHatasi::yok;
        const auto o = Okuyucu::ac(yol, ah);
        DOGRULA(d, o != nullptr && o->ozet().saglam() && o->ozet().ileti_sayisi == 1, "yıkıcı dosyayı kapattı");
        if (o && o->ozet().kapanis.has_value()) {
            DOGRULA(d, o->ozet().kapanis->at("bitis_durumu") == "hata" && o->ozet().kapanis->at("son_adim_no") == "55",
                    "bitis_durumu = hata, alanlar son taslaktan");
        } else {
            DOGRULA(d, false, "kapanış kaydı yok");
        }
    }
    return d.hata;
}

// A6-K31: disk dolu ya da yazma hatası: dosya özetiyle kapatılmaya çalışılır; `bitis_durumu` buna
// göre yazılır; kaybolan iletiler `kayit/eksik`'te; çalıştırma kayıtsız sürer.
int test_hata_kapanisi()
{
    Denetci d;
    struct Vaka {
        const char* ad;
        Dosya::Sonuc tur;
        bool kuyruk_yazilir;
    };
    const Vaka vakalar[] = {
        {"disk_dolu", Dosya::Sonuc::disk_dolu, true},
        {"yazma_hatasi", Dosya::Sonuc::hata, true},
        {"kalici_hata", Dosya::Sonuc::hata, false},
    };
    int no = 0;
    for (const Vaka& v : vakalar) {
        HedefAyari h;
        h.hata_turu = v.tur;
        h.kesmeden_sonra_yazilir = v.kuyruk_yazilir;
        // Başlık bölgesi (üst veri, ekler, katalog eki ≈ 200 KiB) geçildikten sonra, veri yazılırken.
        Duzenek olcu("hata_olcu_" + std::to_string(no), Kademe::telemetri, "hil_bariyer");
        const std::uint64_t baslik = olcu.y ? olcu.y->sayaclar().dosya_bayt : 0;
        if (olcu.y) {
            (void)olcu.kapat();
        }
        h.hata_esigi = baslik + 20000;
        Duzenek dz("hata_" + std::to_string(no++), Kademe::telemetri, "hil_bariyer", [&](Ayar& a) {
            a.parca_hedefi_bayt = 4096;
            a.dosya_fabrikasi = hedef_fabrikasi(h);
        });
        DOGRULA(d, dz.y != nullptr && baslik > 0, std::string(v.ad) + ": yazıcı açıldı: " + dz.hata);
        if (!dz.y) {
            continue;
        }
        KapanisAlanlari taslak = ornek_kapanis();
        taslak.son_adim_no = 9;
        dz.y->kapanis_taslagi(taslak);
        FizikDurumu fd;
        std::uint64_t kabul = 0;
        std::uint64_t kapali = 0;
        for (std::uint64_t adim = 0; adim < 400; ++adim) {
            fd.guncelle(adim);
            const YazmaSonucu r = dz.u().yaz<Kanal::fizik_durum>(static_cast<std::int64_t>(adim) * 1000, adim, fd.y);
            kabul += r == YazmaSonucu::yazildi ? 1u : 0u;
            kapali += r == YazmaSonucu::kapali ? 1u : 0u;
            if (adim % 4 == 3) {
                dz.y->isle();
            }
        }
        const YaziciDurumu beklenen_durum = v.tur == Dosya::Sonuc::disk_dolu ? YaziciDurumu::disk_dolu : YaziciDurumu::yazma_hatasi;
        DOGRULA(d, h.hata_verdi->load() && dz.y->durum() == beklenen_durum, std::string(v.ad) + ": yazıcı hata durumunda");
        DOGRULA(d, kapali > 0 && kabul + kapali == 400, std::string(v.ad) + ": hatadan sonra üretici `kapali` alır; çalıştırma sürer");
        const YaziciSayaclari ys = dz.y->sayaclar();
        DOGRULA(d, ys.yazilan_uretici_iletisi > 0 && ys.yazimda_kaybolan > 0 &&
                       ys.yazilan_uretici_iletisi + ys.yazimda_kaybolan + ys.kapali_iken_atilan == kabul,
                std::string(v.ad) + ": kuyruklanan = yazılan + yazımda kaybolan + kapalıyken atılan");
        const KapanisSonucu ks = dz.kapat();
        const BitisDurumu beklenen_bitis = v.tur == Dosya::Sonuc::disk_dolu ? BitisDurumu::disk_dolu : BitisDurumu::yazma_hatasi;
        DOGRULA(d, ks.bitis_durumu == beklenen_bitis && ks.dosya_kapandi == v.kuyruk_yazilir && !ks.hata.empty(),
                std::string(v.ad) + ": kapanış sonucu: " + ks.hata);
        const auto o = dz.oku();
        DOGRULA(d, o != nullptr, std::string(v.ad) + ": dosya okunur");
        if (!o) {
            continue;
        }
        const DosyaOzeti& oz = o->ozet();
        // Yarım yazılmış parça dosyada kalmaz: dosya ya düzgün kapanmıştır ya da temiz bir yarım dosyadır.
        DOGRULA(d, !oz.bozuk, std::string(v.ad) + ": dosya bozuk değil" + (oz.bozuk ? ": " + oz.bozulma : std::string()));
        DOGRULA(d, oz.kapanmis == v.kuyruk_yazilir, std::string(v.ad) + ": kapanmış mı");
        std::set<std::uint64_t> adimlar;
        for (const OkunanIleti& i : hepsini_oku(*o)) {
            if (i.kanal == "fizik/durum") {
                adimlar.insert(static_cast<std::uint64_t>(i.log_time / 1000));
            }
        }
        DOGRULA(d, !adimlar.empty(), std::string(v.ad) + ": hatadan önce kalıcı olan ileti var");
        if (adimlar.empty()) {
            continue;
        }
        DOGRULA(d, adimlar.size() == ys.yazilan_uretici_iletisi && *adimlar.begin() == 0 && *adimlar.rbegin() == adimlar.size() - 1,
                std::string(v.ad) + ": dosyadaki iletiler hatadan önce kalıcı olanlardır (" + std::to_string(adimlar.size()) + ")");
        if (v.kuyruk_yazilir) {
            DOGRULA(d, oz.kapanis.has_value() && oz.kapanis->at("bitis_durumu") == bitis_durumu_adi(beklenen_bitis) &&
                           oz.kapanis->at("kayit_eksik") == "true" && oz.kapanis->at("son_adim_no") == "9",
                    std::string(v.ad) + ": kapanış kaydı");
            DOGRULA(d, oz.eksikler.size() == 1, std::string(v.ad) + ": eksik kaydı");
            if (oz.eksikler.size() == 1) {
                const EksikKaydi& e = oz.eksikler[0];
                DOGRULA(d, e.kanal->ad == "fizik/durum" && e.neden == (v.tur == Dosya::Sonuc::disk_dolu ? 2 : 1) &&
                               e.kayip_sayisi == ys.yazimda_kaybolan && e.ilk_adim == adimlar.size() &&
                               e.son_adim == adimlar.size() + ys.yazimda_kaybolan - 1,
                        std::string(v.ad) + ": kaybolan iletiler neden ve aralıkla bildirilir");
            }
            // Önek: ilk kayıp adımdan ve son tam kaydedilen adımdan önce.
            DOGRULA(d, oz.guvence == Guvence::fizik && oz.guvence_on_eki_son_adim.has_value() &&
                           *oz.guvence_on_eki_son_adim == adimlar.size() - 2,
                    std::string(v.ad) + ": önek güvencesi");
            DOGRULA(d, oz.ileti_sayisi == adimlar.size() + 1 && oz.kapanis->at("ileti_sayisi") == std::to_string(oz.ileti_sayisi),
                    std::string(v.ad) + ": ileti_sayisi");
            // Özet bölümü olmadan kapanan dosyayı kitaplığın okuyucusu da yedek taramayla okur; burada
            // yalnız A6 okuyucusu ölçüttür.
        } else {
            DOGRULA(d, !oz.kapanis.has_value() && oz.guvence == Guvence::izleme, std::string(v.ad) + ": kapanmamış dosya yalnız izleme");
        }
    }
    {
        // Açılışta yazma hatası: çalıştırma başlamaz, yarım dosya bırakılmaz.
        HedefAyari h;
        h.hata_esigi = 100;
        GeciciDizin dizin("hata_acilis");
        SahteSaat saat;
        Ayar a = elle_ayar(dizin, Kademe::telemetri, saat);
        a.dosya_fabrikasi = hedef_fabrikasi(h);
        const OrnekEkler ekler = ornek_ekler();
        const std::vector<Ek> ek = ekler.ekler();
        std::string hata;
        DOGRULA(d, Yazici::ac(a, ornek_ust_veri(), ek, hata) == nullptr && !hata.empty(), "açılışta yazma hatası: " + hata);
        DOGRULA(d, !std::filesystem::exists(dizin.yol() / (std::string(kOrnekRunId) + ".mcap")), "yarım dosya bırakılmadı");
    }
    return d.hata;
}

// A6-K46: açılışta boş bırakılan koşullu anahtar güncelleme kaydıyla doldurulur.
int test_guncelleme()
{
    Denetci d;
    GeciciDizin dizin("guncelleme");
    SahteSaat saat;
    const Ayar a = elle_ayar(dizin, Kademe::telemetri, saat);
    const OrnekEkler ekler = ornek_ekler();
    const std::vector<Ek> ek = ekler.ekler();
    std::string hata;
    AcilisUstVerisi u = ornek_ust_veri(kOrnekRunId, "hil_bariyer");
    u.a2_surumu = "1.2";  // açılışta dolu
    auto y = Yazici::ac(a, u, ek, hata);
    DOGRULA(d, y != nullptr, "yazıcı açıldı: " + hata);
    if (!y) {
        return d.hata;
    }
    a3::yuk_calistirma_durum cd;
    (void)y->uretici(0).yaz<Kanal::calistirma_durum>(1, 1, cd);
    DOGRULA(d, y->guncelle("kart_fw_version", "17", hata), "boş anahtar doldurulur: " + hata);
    DOGRULA(d, y->guncelle("kart_fw_version", "17", hata), "aynı değerle yeniden yazım etkisizdir");
    DOGRULA(d, !y->guncelle("kart_fw_version", "18", hata), "dolu anahtar farklı değerle değiştirilemez");
    DOGRULA(d, !y->guncelle("a2_surumu", "1.3", hata), "açılışta dolu anahtar değiştirilemez");
    DOGRULA(d, y->guncelle("a2_surumu", "1.2", hata), "açılıştaki değerle aynı: etkisiz");
    DOGRULA(d, !y->guncelle("tohum", "43", hata), "kapalı liste dışındaki anahtar reddedilir");
    DOGRULA(d, !y->guncelle("kart_fw_git", "", hata), "boş değer reddedilir");
    DOGRULA(d, y->guncelle("kart_core_version", "3", hata), "ikinci anahtar");
    y->isle();
    (void)y->uretici(0).yaz<Kanal::calistirma_durum>(2, 2, cd);
    DOGRULA(d, y->guncelle("a8_surumu_major", "1", hata), "kapanıştan hemen önceki güncelleme");
    DOGRULA(d, y->kapat(ornek_kapanis()).dosya_kapandi, "kapandı");
    AcmaHatasi ah = AcmaHatasi::yok;
    const auto o = Okuyucu::ac(y->yol(), ah);
    DOGRULA(d, o != nullptr && o->ozet().saglam() && o->ozet().ust_veri_gecerli, "dosya sağlam, üst veri geçerli");
    if (!o) {
        return d.hata;
    }
    const UstVeri& uv = o->ozet().ust_veri;
    DOGRULA(d, uv.count("kart_fw_version") == 1 && uv.at("kart_fw_version") == "17" && uv.at("kart_core_version") == "3" &&
                   uv.at("a8_surumu_major") == "1" && uv.at("a2_surumu") == "1.2" && uv.count("kart_fw_git") == 0,
            "güncellemeler açılış kaydıyla birleştirildi");
    DOGRULA(d, o->ozet().ileti_sayisi == 2 && o->ozet().uyarilar.empty(), "iletiler ve uyarılar");
    // Dosyada üç güncelleme kaydı (aynı değerle yinelenenler yazılmaz), her biri tek anahtarlı ve şemaya uygun.
    const KitaplikDokumu kd = kitaplik_ile_oku(yol_metni(y->yol()));
    std::size_t guncelleme = 0;
    for (const auto& [ad, icerik] : kd.ust_veri) {
        if (ad == "ida_kayit_guncelleme") {
            ++guncelleme;
            UstVeri g(icerik.begin(), icerik.end());
            DOGRULA(d, g.size() == 1 && ust_veri_denetle(kUstVeriGuncelleme, g, hata), "güncelleme kaydı şemaya uygun");
        }
    }
    DOGRULA(d, kd.acildi && guncelleme == 3, "güncelleme kaydı sayısı: " + std::to_string(guncelleme));
    return d.hata;
}

// Ölçüm (eşik yok): 60 s'lik `telemetri` kademesinde sahte veriyle dosya boyutu, yazım hızı ve
// üretici çağrısı süresi. Değerler rapora "ölçüm, bu makine" olarak girer.
int test_olcum_telemetri()
{
    Denetci d;
    GeciciDizin dizin("olcum");
    Ayar a;
    a.dizin = dizin.yol();
    a.kademe = Kademe::telemetri;
    a.ureticiler.push_back(UreticiAyari{"dunya", std::size_t{64} << 20});
    const OrnekEkler ekler = ornek_ekler();
    const std::vector<Ek> ek = ekler.ekler();
    std::string hata;
    auto y = Yazici::ac(a, ornek_ust_veri(kOrnekRunId, "hil_bariyer"), ek, hata);
    DOGRULA(d, y != nullptr, "yazıcı açıldı: " + hata);
    if (!y) {
        return d.hata;
    }
    Uretici& u = y->uretici(0);
    FizikDurumu fd;
    a3::yuk_imu imu;
    a3::yuk_basinc basinc;
    yuk_hil_tf_yazim tf;
    yuk_fizik_adim_sonucu adim_sonucu;
    yuk_a1_blok blok;
    a3::yuk_kart_durum kart;
    std::vector<std::uint8_t> a1(900);
    Rastgele r(60);
    // 60 s: kontrol periyodu 5 ms (200 Hz). Periyot başına: fizik/durum, IMU (ölçüm, temel, gerçek),
    // TF yazımı, adım sonucu, iki A1 bloğu (≤ 900 B; içerik yarı rastgele), kart durumu; 50 Hz basınç.
    constexpr std::uint64_t kPeriyot = 12000;
    std::vector<std::uint32_t> sure;
    sure.reserve(kPeriyot * 9u);
    std::uint64_t yuk_bayt = 0;
    std::uint64_t ileti = 0;
    const auto olc = [&](auto&& cagri, std::size_t bayt) {
        const std::int64_t t0 = simdi_ns();
        const YazmaSonucu s = cagri();
        const std::int64_t t1 = simdi_ns();
        sure.push_back(static_cast<std::uint32_t>(std::min<std::int64_t>(t1 - t0, 0xFFFFFFFFll)));
        if (s == YazmaSonucu::yazildi) {
            yuk_bayt += bayt;
            ++ileti;
        }
    };
    const std::int64_t bas = simdi_ns();
    for (std::uint64_t p = 0; p < kPeriyot; ++p) {
        const auto t = static_cast<std::int64_t>(p) * 5'000'000;
        const std::uint64_t adim = p * 4u;
        fd.guncelle(p);
        const double z = static_cast<double>(p) * 0.005;
        imu.seq = static_cast<std::uint32_t>(p);
        imu.t_hazir_ns = t;
        imu.t_teslim_ns = t + 1'000'000;
        imu.ivme_mps2 = {static_cast<float>(0.1 * std::sin(z)), static_cast<float>(0.05 * std::cos(z)), 9.81f};
        imu.acisal_hiz_radps = {0.001f, static_cast<float>(0.01 * std::sin(2 * z)), 0.05f};
        olc([&] { return u.yaz<Kanal::fizik_durum>(t, adim, fd.y); }, boyut(fd.y));
        olc([&] { return u.yaz<Kanal::sensor_imu>(t, adim, imu); }, boyut(imu));
        olc([&] { return u.yaz<Kanal::sensor_temel_imu>(t, adim, imu); }, boyut(imu));
        olc([&] { return u.yaz<Kanal::sensor_gercek_imu>(t, adim, imu); }, boyut(imu));
        tf.batch_seq = static_cast<std::uint32_t>(p);
        tf.plan_duvar_ns = t;
        tf.yazim_baslangic_duvar_ns = t + 20'000;
        tf.yazim_bitis_duvar_ns = t + 120'000;
        olc([&] { return u.yaz<Kanal::hil_tf_yazim>(t, adim, tf); }, boyut(tf));
        adim_sonucu.adim_no = adim;
        adim_sonucu.komut_durumu = 1;
        adim_sonucu.uygulanan_step_seq = static_cast<std::uint32_t>(p);
        for (auto& pwm : adim_sonucu.pwm_us) {
            pwm = static_cast<std::uint16_t>(1500 + static_cast<int>(100.0 * std::sin(z)));
        }
        olc([&] { return u.yaz<Kanal::fizik_adim_sonucu>(t, adim, adim_sonucu); }, boyut(adim_sonucu));
        for (std::size_t i = 0; i < a1.size(); ++i) {
            // MAVLink çerçevelerine benzer: yapısal baytlar yinelenir, yükün yarısı değişkendir.
            a1[i] = i % 2 == 0 ? static_cast<std::uint8_t>(i & 0xFFu) : static_cast<std::uint8_t>(r.ham());
        }
        blok.bayt = a1;
        blok.duvar_ns = t;
        olc([&] { return u.yaz<Kanal::a1_kart_sim>(t, adim, blok); }, boyut(blok));
        kart.kart_t_ns = t;
        kart.acilis_sayaci = 3;
        olc([&] { return u.yaz<Kanal::kart_durum>(t, adim, kart); }, boyut(kart));
        if (p % 4 == 0) {
            basinc.seq = static_cast<std::uint32_t>(p / 4);
            olc([&] { return u.yaz<Kanal::sensor_basinc>(t, adim, basinc); }, boyut(basinc));
        }
    }
    const std::int64_t uretim_bitti = simdi_ns();
    const UreticiSayaclari us = u.sayaclar();
    const KapanisSonucu ks = y->kapat(ornek_kapanis());
    const std::int64_t son = simdi_ns();
    DOGRULA(d, ks.dosya_kapandi, "dosya kapandı: " + ks.hata);
    std::error_code ec;
    const auto dosya_bayt = static_cast<std::uint64_t>(std::filesystem::file_size(y->yol(), ec));
    AcmaHatasi ah = AcmaHatasi::yok;
    const std::int64_t okuma_bas = simdi_ns();
    const auto o = Okuyucu::ac(y->yol(), ah);
    std::uint64_t okunan = 0;
    if (o) {
        Ileti i;
        while (o->sonraki(i)) {
            ++okunan;
        }
    }
    const std::int64_t okuma_son = simdi_ns();
    DOGRULA(d, o != nullptr && o->ozet().saglam() && okunan == ks.sayaclar.yazilan_ileti, "dosya sağlam ve tümü okundu");
    DOGRULA(d, us.uretilen == us.kuyruklanan + us.dusen_halka, "üretilen = kuyruklanan + düşen");
    const Yuzdelik yz = yuzdelik(sure);
    const double toplam_s = static_cast<double>(son - bas) / 1e9;
    std::printf("  ÖLÇÜM (bu makine; %s derleme): 60 s sim zamanı, telemetri kademesi, sahte veri\n",
                ida::test::iyilestirmesiz_derleme() ? "iyileştirmesiz" : "iyileştirilmiş");
    std::printf("    ileti %llu (düşen %llu), yük %llu bayt, dosya %llu bayt (oran %.2f), parça %llu\n",
                static_cast<unsigned long long>(ileti), static_cast<unsigned long long>(us.dusen_halka),
                static_cast<unsigned long long>(yuk_bayt), static_cast<unsigned long long>(dosya_bayt),
                dosya_bayt > 0 ? static_cast<double>(yuk_bayt) / static_cast<double>(dosya_bayt) : 0.0,
                static_cast<unsigned long long>(ks.sayaclar.kapanan_parca));
    std::printf("    üretim %.3f s, kapanışa kadar toplam %.3f s: yazım hızı %.1f MB/s yük (%.0f ileti/s)\n",
                static_cast<double>(uretim_bitti - bas) / 1e9, toplam_s, static_cast<double>(yuk_bayt) / 1e6 / toplam_s,
                static_cast<double>(ileti) / toplam_s);
    std::printf("    üretici çağrısı: p50 %.0f ns, p99 %.0f ns, en kötü %.0f ns (%zu çağrı)\n", yz.p50, yz.p99, yz.en_kotu, sure.size());
    std::printf("    okuma (açılış taraması + sıralı okuma): %.3f s, %.1f MB/s yük\n", static_cast<double>(okuma_son - okuma_bas) / 1e9,
                static_cast<double>(yuk_bayt) / 1e6 / (static_cast<double>(okuma_son - okuma_bas) / 1e9));
    return d.hata;
}

}  // namespace ida::kayit::test
