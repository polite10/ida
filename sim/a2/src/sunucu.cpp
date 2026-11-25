// sunucu.cpp — A2 sunucusunun soket döngüsü ve G/Ç iş parçacığı (bkz. ida/a2/sunucu.hpp).
//
// Dünya iş parçacığı bu dosyadaki hiçbir şeye dokunmaz: veriyi Yayinci'nin halkasına ve kesitine
// yazar; soketler, kuyruk ve oturum yalnız G/Ç iş parçacığındadır. Dünyadan G/Ç iş parçacığına
// uyandırma yoktur (uyandırma sistem çağrısı gerektirir): döngü oturum varken 2 ms, yokken 20 ms
// aralıkla halkayı yoklar (ölçülmemiş seçim; `durum` gecikmesine en çok bir tik ekler).
#include "ida/a2/sunucu.hpp"

#include "cekirdek.hpp"
#include "kilit.hpp"
#include "saat.hpp"
#include "soket.hpp"

#include <atomic>
#include <chrono>
#include <random>
#include <thread>
#include <vector>

namespace ida::a2 {

namespace {

constexpr int kOturumTikMs = 2;      // oturum varken yoklama aralığı
constexpr int kBostaTikMs = 20;      // oturum yokken
constexpr std::size_t kOkumaParcasi = 4096;
constexpr std::size_t kTikOkumaButcesi = 64u * 1024u;   // bir turda oturum soketinden okunan en çok bayt
constexpr std::size_t kYazmaParcasi = 256u * 1024u;
constexpr std::size_t kAzamiKapanan = 8;               // kapanışı süren soket sayısı
constexpr auto kKapanisSuresi = std::chrono::seconds(2);       // son iletinin karşıya ulaşması için
constexpr auto kDurdurmaSuresi = std::chrono::milliseconds(500);

using Saat = std::chrono::steady_clock;

}  // namespace

const char* a2_durumu_adi(A2Durumu d) noexcept
{
    switch (d) {
        case A2Durumu::dinliyor: return "dinliyor";
        case A2Durumu::kilit_baskasinda: return "kilit_baskasinda";
        case A2Durumu::kilit_acilamadi: return "kilit_acilamadi";
        case A2Durumu::port_acilamadi: return "port_acilamadi";
        case A2Durumu::ic_hata: return "ic_hata";
        case A2Durumu::durduruldu: return "durduruldu";
    }
    return "?";
}

const char* kopma_nedeni_adi(KopmaNedeni n) noexcept
{
    switch (n) {
        case KopmaNedeni::yok: return "yok";
        case KopmaNedeni::karsi_kapatti: return "karsi_kapatti";
        case KopmaNedeni::karsi_veda: return "karsi_veda";
        case KopmaNedeni::protokol_hatasi: return "protokol_hatasi";
        case KopmaNedeni::protokol_reddi: return "protokol_reddi";
        case KopmaNedeni::surum_reddi: return "surum_reddi";
        case KopmaNedeni::manifest_reddi: return "manifest_reddi";
        case KopmaNedeni::halka_reddi: return "halka_reddi";
        case KopmaNedeni::merhaba_zaman_asimi: return "merhaba_zaman_asimi";
        case KopmaNedeni::hazir_zaman_asimi: return "hazir_zaman_asimi";
        case KopmaNedeni::nabiz_zaman_asimi: return "nabiz_zaman_asimi";
        case KopmaNedeni::tampon_tasti: return "tampon_tasti";
        case KopmaNedeni::yayin_tasmasi: return "yayin_tasmasi";
        case KopmaNedeni::yeni_calistirma: return "yeni_calistirma";
        case KopmaNedeni::sim_kapaniyor: return "sim_kapaniyor";
    }
    return "?";
}

struct Sunucu::Ic {
    SunucuAyari ayar;
    std::unique_ptr<Yayinci> yayinci;
    std::unique_ptr<ic::Cekirdek> cekirdek;
    ic::SahiplikKilidi kilit;
    std::string aciklama;                 // baslat'ta yazılır, sonra değişmez
    std::atomic<std::uint8_t> durum{static_cast<std::uint8_t>(A2Durumu::dinliyor)};
    std::uint16_t port = 0;
    std::thread is;
    std::atomic<bool> dur{false};

    // ---- yalnız G/Ç iş parçacığı ----
    struct Kapanan {
        ag::Tutamac t;
        ic::Baytlar kalan;
        std::size_t gonderilen = 0;
        bool yazma_kapali = false;
        Saat::time_point son;
    };
    ag::Tutamac dinleyici = ag::gecersiz();
    ag::Tutamac oturum = ag::gecersiz();
    std::vector<Kapanan> kapananlar;
    std::vector<std::uint8_t> oku_tamponu = std::vector<std::uint8_t>(kOkumaParcasi);
    std::uint64_t kimlik_sayaci = 0;

    void gunluk(std::string_view s) const
    {
        if (ayar.gunluk) {
            ayar.gunluk(s);
        }
    }
    std::int64_t simdi() const { return ayar.saat ? ayar.saat() : ic::duvar_ns(); }

    // Her kabul edilen bağlantıya yeni, rastgele 64 bitlik kimlik (§2); 0 olmaz.
    std::uint64_t yeni_kimlik()
    {
        std::random_device rd;
        std::uint64_t k = (static_cast<std::uint64_t>(rd()) << 32u) ^ static_cast<std::uint64_t>(rd());
        k ^= (++kimlik_sayaci) * 0x9E3779B97F4A7C15ull;
        return k != 0 ? k : 1;
    }

    void kapanana_ekle(ag::Tutamac t, ic::Baytlar&& kalan)
    {
        if (kapananlar.size() >= kAzamiKapanan) {
            ag::kapat(kapananlar.front().t);
            kapananlar.erase(kapananlar.begin());
        }
        Kapanan k;
        k.t = t;
        k.kalan = std::move(kalan);
        k.son = Saat::now() + kKapanisSuresi;
        kapananlar.push_back(std::move(k));
    }

    // Çekirdek oturumu bitirdiyse soketi kapanış sırasına aktarır: kalan baytlar (son ret/veda
    // dahil) yazılır, yazma yönü kapatılır, karşı taraf kapatana ya da süre dolana kadar okunur.
    void devret()
    {
        if (!cekirdek->kapanis_var()) {
            return;
        }
        ic::Baytlar kalan = cekirdek->kapanis_al();
        if (oturum != ag::gecersiz()) {
            kapanana_ekle(oturum, std::move(kalan));
            oturum = ag::gecersiz();
        }
    }

    void oturum_koptu()
    {
        ag::kapat(oturum);
        oturum = ag::gecersiz();
        cekirdek->baglanti_koptu();
    }

    void kabul_et(std::int64_t simdi_ns)
    {
        for (int i = 0; i < 8; ++i) {
            const ag::Tutamac t = ag::kabul(dinleyici);
            if (t == ag::gecersiz()) {
                return;
            }
            if (oturum == ag::gecersiz() && cekirdek->baglanti_acildi(simdi_ns, yeni_kimlik())) {
                oturum = t;
                gunluk("A2: görselleştirici bağlandı");
            } else {
                // A2-K03: ikinci bağlantıya ret(mesgul); mevcut oturum etkilenmez.
                ic::Baytlar b;
                cekirdek->mesgul_reddi(b);
                kapanana_ekle(t, std::move(b));
            }
        }
    }

    void oturum_oku(std::int64_t simdi_ns)
    {
        std::size_t toplam = 0;
        while (oturum != ag::gecersiz() && cekirdek->oturum_var() && toplam < kTikOkumaButcesi) {
            const long long r = ag::oku(oturum, oku_tamponu.data(), oku_tamponu.size());
            if (r > 0) {
                toplam += static_cast<std::size_t>(r);
                cekirdek->veri_geldi(oku_tamponu.data(), static_cast<std::size_t>(r), simdi_ns);
            } else if (r == -1) {
                return;
            } else {
                oturum_koptu();  // 0: karşı taraf kapattı; -2: hata (A2-K64)
                return;
            }
        }
    }

    void oturum_yaz()
    {
        ic::GondermeKuyrugu& q = cekirdek->kuyruk();
        while (oturum != ag::gecersiz() && !q.bos()) {
            const std::span<const std::uint8_t> on = q.on();
            const long long r = ag::yaz(oturum, on.data(), on.size() < kYazmaParcasi ? on.size() : kYazmaParcasi);
            if (r > 0) {
                q.tuket(static_cast<std::size_t>(r));
            } else if (r == -2) {
                oturum_koptu();
                return;
            } else {
                return;  // soket tamponu dolu: sonraki turda (A2-K26: sim beklemez)
            }
        }
    }

    void kapananlari_ilerlet()
    {
        const Saat::time_point simdi_s = Saat::now();
        for (std::size_t i = 0; i < kapananlar.size();) {
            Kapanan& k = kapananlar[i];
            bool bitti = simdi_s >= k.son;
            while (!bitti && k.gonderilen < k.kalan.size()) {
                const long long r = ag::yaz(k.t, k.kalan.data() + k.gonderilen, k.kalan.size() - k.gonderilen);
                if (r > 0) {
                    k.gonderilen += static_cast<std::size_t>(r);
                } else if (r == -2) {
                    bitti = true;
                } else {
                    break;
                }
            }
            if (!bitti && k.gonderilen == k.kalan.size()) {
                if (!k.yazma_kapali) {
                    ag::yazmayi_kapat(k.t);  // FIN: son ileti karşıya ulaşır
                    k.yazma_kapali = true;
                }
                // Karşı taraf kapatana kadar gelenler okunup atılır (okunmamış veriyle kapatmak
                // sıfırlamaya yol açar ve son iletiyi düşürebilir).
                for (int tur = 0; tur < 16 && !bitti; ++tur) {
                    const long long r = ag::oku(k.t, oku_tamponu.data(), oku_tamponu.size());
                    if (r == 0 || r == -2) {
                        bitti = true;
                    } else if (r == -1) {
                        break;
                    }
                }
            }
            if (bitti) {
                ag::kapat(k.t);
                kapananlar.erase(kapananlar.begin() + static_cast<std::ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
    }

    void yoklama_listesi(std::vector<ag::Yoklama>& liste, bool dinleyici_dahil) const
    {
        liste.clear();
        if (dinleyici_dahil && dinleyici != ag::gecersiz()) {
            ag::Yoklama y;
            y.t = dinleyici;
            liste.push_back(y);
        }
        if (oturum != ag::gecersiz()) {
            ag::Yoklama y;
            y.t = oturum;
            y.yaz_iste = !cekirdek->kuyruk().bos();
            liste.push_back(y);
        }
        for (const Kapanan& k : kapananlar) {
            ag::Yoklama y;
            y.t = k.t;
            y.yaz_iste = k.gonderilen < k.kalan.size();
            liste.push_back(y);
        }
    }

    void dinleyiciyi_kapat()
    {
        if (dinleyici != ag::gecersiz()) {
            ag::kapat(dinleyici);
            dinleyici = ag::gecersiz();
        }
        durum.store(static_cast<std::uint8_t>(A2Durumu::durduruldu), std::memory_order_relaxed);
    }

    void dongu()
    {
        std::vector<ag::Yoklama> liste;
        while (!dur.load(std::memory_order_acquire)) {
            if (dinleyici != ag::gecersiz() && cekirdek->kapandi()) {
                dinleyiciyi_kapat();  // Yayinci::kapaniyor(): yeni bağlantı kabul edilmez
                gunluk("A2: sim kapanıyor, dinleyici kapatıldı");
            }
            yoklama_listesi(liste, true);
            const bool mesgul = oturum != ag::gecersiz() || !kapananlar.empty();
            (void)ag::yokla(liste, mesgul ? kOturumTikMs : kBostaTikMs);
            const std::int64_t simdi_ns = simdi();
            if (dinleyici != ag::gecersiz()) {
                kabul_et(simdi_ns);
            }
            oturum_oku(simdi_ns);
            devret();
            cekirdek->tik(simdi_ns);
            devret();
            oturum_yaz();
            kapananlari_ilerlet();
        }
        // Olağan kapanış (A2-K67): veda(sim_kapaniyor), kısa süre boşaltma, sonra her şey kapanır.
        cekirdek->sim_kapaniyor();
        devret();
        const Saat::time_point son = Saat::now() + kDurdurmaSuresi;
        for (Kapanan& k : kapananlar) {
            if (k.son > son) {
                k.son = son;
            }
        }
        while (!kapananlar.empty() && Saat::now() < son) {
            yoklama_listesi(liste, false);
            (void)ag::yokla(liste, 2);
            kapananlari_ilerlet();
        }
        for (const Kapanan& k : kapananlar) {
            ag::kapat(k.t);
        }
        kapananlar.clear();
        if (oturum != ag::gecersiz()) {
            ag::kapat(oturum);
            oturum = ag::gecersiz();
        }
        dinleyiciyi_kapat();
    }
};

Sunucu::Sunucu(std::unique_ptr<Ic> ic) : ic_(std::move(ic)) {}

std::unique_ptr<Sunucu> Sunucu::baslat(const SunucuAyari& ayar, std::string& hata)
{
    hata.clear();
    // A2-K93: port 1024 … 65 535; geçersiz değerle sunucu başlamaz.
    if (!ayar.sinama_bos_port && ayar.port < 1024) {
        hata = "A2 portu geçersiz (1024 … 65535 olmalı; A2-K93): " + std::to_string(ayar.port);
        return nullptr;
    }
    auto ic = std::make_unique<Ic>();
    ic->ayar = ayar;
    const auto a2siz = [&ic](A2Durumu d, std::string aciklama) {
        ic->durum.store(static_cast<std::uint8_t>(d), std::memory_order_relaxed);
        ic->aciklama = std::move(aciklama);
        ic->yayinci = std::make_unique<Yayinci>(4096, false);  // yayın işlevleri hemen döner
        ic->gunluk(ic->aciklama);
    };

    // A2-K77: dinleyiciden önce sahiplik kilidi (beklemeden).
    std::string kilit_hatasi;
    const ic::KilitSonucu ks = ic->kilit.al(ayar.kilit_dosyasi, kilit_hatasi);
    if (ks == ic::KilitSonucu::baskasinda) {
        a2siz(A2Durumu::kilit_baskasinda, "A2 sahiplik kilidi başka bir sim sürecinde (A2-K79); A2'siz çalışılıyor");
        return std::unique_ptr<Sunucu>(new Sunucu(std::move(ic)));
    }
    if (ks == ic::KilitSonucu::acilamadi) {
        a2siz(A2Durumu::kilit_acilamadi, kilit_hatasi + "; A2'siz çalışılıyor");
        return std::unique_ptr<Sunucu>(new Sunucu(std::move(ic)));
    }
    // A2-K01: yalnız 127.0.0.1. A2-K05: açılamazsa A2'siz (kilit tutulmaya devam eder: A2-K77).
    std::string ag_hatasi;
    const std::uint16_t istenen = ayar.sinama_bos_port ? std::uint16_t{0} : ayar.port;
    if (!ag::dinle(istenen, ic->dinleyici, ic->port, ag_hatasi)) {
        ic->port = 0;
        a2siz(A2Durumu::port_acilamadi, ag_hatasi + " (A2-K05); A2'siz çalışılıyor");
        return std::unique_ptr<Sunucu>(new Sunucu(std::move(ic)));
    }
    ic->yayinci = std::make_unique<Yayinci>(ayar.yayin_halkasi_bayt, true);
    ic->cekirdek = std::make_unique<ic::Cekirdek>(*ic->yayinci, ayar.gonderme_kuyrugu_bayt);
    ic->aciklama = "A2 127.0.0.1:" + std::to_string(ic->port) + " adresinde dinliyor";
    Ic* p = ic.get();
    try {
        ic->is = std::thread([p] { p->dongu(); });
    } catch (const std::exception&) {
        ag::kapat(ic->dinleyici);
        ic->dinleyici = ag::gecersiz();
        ic->port = 0;
        ic->cekirdek.reset();
        a2siz(A2Durumu::ic_hata, "A2 G/Ç iş parçacığı başlatılamadı; A2'siz çalışılıyor");
    }
    return std::unique_ptr<Sunucu>(new Sunucu(std::move(ic)));
}

Sunucu::~Sunucu() { durdur(); }

void Sunucu::durdur()
{
    if (ic_ && ic_->is.joinable()) {
        ic_->dur.store(true, std::memory_order_release);
        ic_->is.join();
    }
}

A2Durumu Sunucu::durum() const noexcept
{
    return static_cast<A2Durumu>(ic_->durum.load(std::memory_order_relaxed));
}

std::string Sunucu::durum_aciklamasi() const
{
    if (durum() == A2Durumu::durduruldu) {
        return "A2 durduruldu";
    }
    return ic_->aciklama;
}

std::uint16_t Sunucu::port() const noexcept { return ic_->port; }

Yayinci& Sunucu::yayinci() noexcept { return *ic_->yayinci; }

SunucuSayaclari Sunucu::sayaclar() const
{
    if (ic_->cekirdek) {
        return ic_->cekirdek->sayaclar();
    }
    SunucuSayaclari s;
    s.yayin = ic_->yayinci->sayaclar();
    return s;
}

}  // namespace ida::a2
