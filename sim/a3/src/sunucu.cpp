// sunucu.cpp — A3 sunucusunun soket döngüsü ve G/Ç iş parçacığı (bkz. ida/a3/sunucu.hpp).
#include "ida/a3/sunucu.hpp"

#include "cekirdek.hpp"
#include "ida/a3/zaman.hpp"
#include "rastgele.hpp"
#include "soket.hpp"

#include <atomic>
#include <exception>
#include <map>
#include <mutex>
#include <thread>

namespace ida::a3 {

struct Sunucu::Ic {
    SunucuAyari ayar;
    std::string jeton;
    std::uint16_t port = 0;
    ag::Tutamac dinleyici = ag::gecersiz();
    std::unique_ptr<Yayinci> yayinci;
    std::unique_ptr<Cekirdek> cekirdek;
    std::map<BagId, ag::Tutamac> soketler;
    std::thread is;
    std::thread tik_is;  // A3-K33: 50 Hz yayın tiki (yüksek çözünürlüklü bekleme; zaman.hpp)
    ag::Tutamac uyan_oku = ag::gecersiz();
    ag::Tutamac uyan_yaz = ag::gecersiz();
    std::atomic<bool> dur{false};
    mutable std::mutex sayac_kilidi;
    SunucuSayaclari son_sayaclar;

    void kapat_bag(BagId id)
    {
        const auto it = soketler.find(id);
        if (it != soketler.end()) {
            ag::kapat(it->second);
            soketler.erase(it);
        }
        cekirdek->kapandi(id);
    }

    void yaz_hepsi()
    {
        for (auto& [id, t] : soketler) {
            for (int tur = 0; tur < 64; ++tur) {
                const std::string_view c = cekirdek->cikis(id);
                if (c.empty()) {
                    break;
                }
                const long long n = ag::yaz(t, reinterpret_cast<const std::uint8_t*>(c.data()), c.size());
                if (n > 0) {
                    cekirdek->yazildi(id, static_cast<std::size_t>(n));
                    continue;
                }
                if (n == -2) {
                    cekirdek->karsi_kapatti(id, std::chrono::steady_clock::now());
                    cekirdek->yazildi(id, c.size());  // karşı taraf yok: bekleyen çıkış atılır
                }
                break;
            }
        }
    }

    // Tik iş parçacığı: her 20 ms'de uyandırma soketine bir bayt yazar. Windows'ta yoklama zaman
    // aşımı sistem tikine (≈ 15,6 ms) yuvarlandığı için tik, zaman aşımına değil bu bayta bağlıdır.
    void tik_dongusu()
    {
        auto sonraki = std::chrono::steady_clock::now();
        while (!dur.load(std::memory_order_acquire)) {
            sonraki += sure::yayin_tiki;
            hassas_bekle(sonraki);
            const auto simdi = std::chrono::steady_clock::now();
            if (simdi - sonraki > std::chrono::milliseconds(100)) {
                sonraki = simdi;  // uzun takılmadan sonra birikmiş tikleri koşma
            }
            const std::uint8_t b = 1;
            (void)ag::yaz(uyan_yaz, &b, 1);  // tampon doluysa tik zaten bekliyor demektir
        }
    }

    // Uyandırma soketindeki baytları boşaltır; bekleyen tik sayısını döndürür.
    int tikleri_al()
    {
        std::uint8_t b[64];
        int n = 0;
        for (;;) {
            const long long r = ag::oku(uyan_oku, b, sizeof b);
            if (r <= 0) {
                return n;
            }
            n += static_cast<int>(r);
        }
    }

    void dongu()
    {
        std::vector<std::uint8_t> tampon(64 * 1024);
        std::vector<ag::Yoklama> liste;
        std::vector<BagId> sira;
        bool kapanis_basladi = false;
        Zaman kapanis_sonu{};
        for (;;) {
            Zaman simdi = std::chrono::steady_clock::now();
            if (dur.load(std::memory_order_acquire) && !kapanis_basladi) {
                kapanis_basladi = true;
                kapanis_sonu = simdi + std::chrono::milliseconds(500);
                cekirdek->kapaniyor(simdi);
            }
            if (kapanis_basladi && (soketler.empty() || simdi >= kapanis_sonu)) {
                break;
            }
            liste.clear();
            sira.clear();
            {
                ag::Yoklama y;
                y.t = uyan_oku;
                liste.push_back(y);
            }
            if (!kapanis_basladi) {
                ag::Yoklama y;
                y.t = dinleyici;
                liste.push_back(y);
            }
            for (const auto& [id, t] : soketler) {
                ag::Yoklama y;
                y.t = t;
                y.oku_iste = cekirdek->okumali(id);
                y.yaz_iste = !cekirdek->cikis(id).empty();
                liste.push_back(y);
                sira.push_back(id);
            }
            const auto bekle = std::min<long long>(cekirdek->tike_kalan(simdi).count(), 20);
            ag::yokla(liste, static_cast<int>(bekle < 0 ? 0 : bekle));
            simdi = std::chrono::steady_clock::now();
            std::size_t ilk = 1;
            const int tik_sayisi = liste[0].okunabilir ? tikleri_al() : 0;
            if (!kapanis_basladi) {
                ilk = 2;
                if (liste[1].okunabilir) {
                    for (int n = 0; n < 64; ++n) {
                        const ag::Tutamac t = ag::kabul(dinleyici);
                        if (t == ag::gecersiz()) {
                            break;
                        }
                        std::vector<BagId> kapatilacak;
                        const BagId id = cekirdek->ac(simdi, kapatilacak);
                        for (const BagId k : kapatilacak) {
                            const auto it = soketler.find(k);
                            if (it != soketler.end()) {
                                ag::kapat(it->second);
                                soketler.erase(it);
                            }
                        }
                        if (id == 0) {
                            ag::kapat(t);
                            continue;
                        }
                        soketler[id] = t;
                    }
                }
            }
            for (std::size_t i = 0; i < sira.size(); ++i) {
                const BagId id = sira[i];
                const ag::Yoklama& y = liste[ilk + i];
                const auto it = soketler.find(id);
                if (it == soketler.end()) {
                    continue;
                }
                if (y.hata && !y.okunabilir) {
                    kapat_bag(id);
                    continue;
                }
                if (!y.okunabilir || !cekirdek->okumali(id)) {
                    continue;
                }
                for (int tur = 0; tur < 16 && cekirdek->okumali(id); ++tur) {
                    const long long n = ag::oku(it->second, tampon.data(), tampon.size());
                    if (n > 0) {
                        cekirdek->gelen(id, tampon.data(), static_cast<std::size_t>(n), simdi);
                        continue;
                    }
                    if (n == 0 || n == -2) {
                        cekirdek->karsi_kapatti(id, simdi);
                    }
                    break;
                }
            }
            cekirdek->dongu(simdi);
            // Birikmiş tikler: en çok üçü koşulur (uzun takılmadan sonra hepsi koşulmaz).
            for (int t = 0; t < tik_sayisi && t < 3; ++t) {
                cekirdek->tik();
            }
            yaz_hepsi();
            for (const BagId id : cekirdek->baglantilar()) {
                if (cekirdek->kapatilmali(id)) {
                    kapat_bag(id);
                }
            }
            {
                std::lock_guard<std::mutex> k(sayac_kilidi);
                son_sayaclar = cekirdek->sayaclar();
            }
        }
        for (const auto& [id, t] : soketler) {
            (void)id;
            ag::kapat(t);
        }
        soketler.clear();
        ag::kapat(dinleyici);
        dinleyici = ag::gecersiz();
    }
};

Sunucu::Sunucu(std::unique_ptr<Ic> ic) : ic_(std::move(ic)) {}

std::unique_ptr<Sunucu> Sunucu::baslat(const SunucuAyari& ayar, KomutAlicisi& alici, std::string& hata)
{
    auto ic = std::make_unique<Ic>();
    ic->ayar = ayar;
    // A3-K02: döngü arabirimi dışında dinlenmez (ag::dinle adresi denetler).
    if (!ag::dinle(ayar.adres, ayar.port, ic->dinleyici, ic->port, hata)) {
        return nullptr;
    }
    // A3-K04: 32 bayt, işletim sisteminin kriptografik kaynağı; diske yazılmaz.
    ic->jeton = rastgele_onaltilik(32);
    if (ic->jeton.size() != 64) {
        hata = "kriptografik rastgele kaynak kullanılamadı";
        ag::kapat(ic->dinleyici);
        return nullptr;
    }
    ic->yayinci = std::make_unique<Yayinci>(ayar.telemetri_halkasi, ayar.olay_halkasi);
    ic->cekirdek = std::make_unique<Cekirdek>(ayar, alici, ic->yayinci->ic(), ic->jeton, ic->port);
    if (!ic->cekirdek->hazirla(hata)) {
        ag::kapat(ic->dinleyici);
        return nullptr;
    }
    if (!ag::uyandirma_cifti(ic->uyan_oku, ic->uyan_yaz)) {
        hata = "uyandırma soketleri açılamadı";
        ag::kapat(ic->dinleyici);
        return nullptr;
    }
    ic->cekirdek->dis_tik(true);
    Ic* p = ic.get();
    ic->tik_is = std::thread([p] { p->tik_dongusu(); });
    ic->is = std::thread([p] {
        // G/Ç iş parçacığındaki bir istisna sim sürecini sonlandırmaz: sunucu durur, soketler kapanır
        // ve günlüğe yazılır (A3-K43: panel bağlantıları çalıştırmayı etkilemez).
        try {
            p->dongu();
        } catch (const std::exception& h) {
            if (p->ayar.gunluk) {
                p->ayar.gunluk(std::string("A3 sunucusu beklenmeyen hatayla durdu: ") + h.what());
            }
            for (const auto& [id, t] : p->soketler) {
                (void)id;
                ag::kapat(t);
            }
            p->soketler.clear();
            ag::kapat(p->dinleyici);
            p->dinleyici = ag::gecersiz();
        }
    });
    return std::unique_ptr<Sunucu>(new Sunucu(std::move(ic)));
}

Sunucu::~Sunucu() { durdur(); }

void Sunucu::durdur()
{
    if (ic_ && ic_->is.joinable()) {
        ic_->dur.store(true, std::memory_order_release);
        ic_->is.join();
    }
    if (ic_ && ic_->tik_is.joinable()) {
        ic_->dur.store(true, std::memory_order_release);
        ic_->tik_is.join();
        ag::kapat(ic_->uyan_oku);
        ag::kapat(ic_->uyan_yaz);
        ic_->uyan_oku = ag::gecersiz();
        ic_->uyan_yaz = ag::gecersiz();
    }
}

std::uint16_t Sunucu::port() const noexcept { return ic_->port; }

std::string Sunucu::acilis_adresi() const
{
    const std::string host = ic_->ayar.adres == "::1" ? "[::1]" : "127.0.0.1";
    return "http://" + host + ":" + std::to_string(ic_->port) + "/#erisim=" + ic_->jeton;
}

const std::string& Sunucu::jeton() const noexcept { return ic_->jeton; }

Yayinci& Sunucu::yayinci() noexcept { return *ic_->yayinci; }

SunucuSayaclari Sunucu::sayaclar() const
{
    std::lock_guard<std::mutex> k(ic_->sayac_kilidi);
    return ic_->son_sayaclar;
}

}  // namespace ida::a3
