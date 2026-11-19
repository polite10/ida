// W0-MP3a ölçüm prototipi: PC tarafı koşu akışı (bkz. oturum.hpp; atılabilir).
#include "oturum.hpp"

#include "mp3_cerceve.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#if defined(__APPLE__)
#include <pthread/qos.h>
#endif

namespace mp3 {

namespace {

using Saat = std::chrono::steady_clock;

int64_t simdi_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Saat::now().time_since_epoch()).count();
}

// Takvim: hedefin 3 ms öncesine kadar uyu, kalanını meşgul bekle (macOS zamanlayıcı birleştirmesi sleep_until'i
// ms mertebesinde geciktirebiliyor; ilk kısa koşularda gözlendi). Kalan gecikme ölçülür ve kaydedilir.
void uyu_kadar(int64_t hedef_ns)
{
    constexpr int64_t PAY = 3000000;
    if (hedef_ns - simdi_ns() > PAY) {
        const Saat::time_point t{std::chrono::duration_cast<Saat::duration>(std::chrono::nanoseconds(hedef_ns - PAY))};
        std::this_thread::sleep_until(t);
    }
    while (simdi_ns() < hedef_ns) {}
}

void etkilesimli_qos()
{
#if defined(__APPLE__)
    (void)pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
}

void uyu_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

Io gonder(Port &p, uint8_t tur, uint32_t kosu, uint32_t sira, const uint8_t *yuk, uint16_t n, int ms)
{
    uint8_t f[MP3_AZAMI_CERCEVE];
    const size_t k = mp3_kodla(f, sizeof f, tur, kosu, sira, yuk, n);
    size_t w = 0;
    return p.yaz(f, k, ms, w);
}

struct EsZamanli {
    std::function<bool(const mp3_cerceve_t &)> kosul;
    bool bulundu = false;
};

void es_cb(void *ctx, const mp3_cerceve_t *c)
{
    auto *e = static_cast<EsZamanli *>(ctx);
    if (!e->bulundu && e->kosul(*c)) e->bulundu = true;
}

// Okuyucu iş parçacığı olmadan: koşul sağlanana ya da süre dolana dek okur.
bool bekle(Port &p, mp3_ayr_t &a, int ms, const std::function<bool(const mp3_cerceve_t &)> &kosul, bool &kopma)
{
    EsZamanli e{kosul, false};
    const int64_t bitis = simdi_ns() + static_cast<int64_t>(ms) * 1000000;
    uint8_t b[2048];
    while (!e.bulundu) {
        const int64_t kalan = (bitis - simdi_ns()) / 1000000;
        if (kalan <= 0) return false;
        size_t k = 0;
        const Io r = p.oku(b, sizeof b, static_cast<int>(kalan), k);
        if (r == Io::Kopma) { kopma = true; return false; }
        if (r == Io::Tamam) mp3_ayr_besle(&a, b, k, es_cb, &e);
    }
    return true;
}

std::map<std::string, std::string> kv_ayir(const std::string &s)
{
    std::map<std::string, std::string> m;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = s.find(' ', i);
        if (j == std::string::npos) j = s.size();
        const std::string t = s.substr(i, j - i);
        const size_t e = t.find('=');
        if (e != std::string::npos) m[t.substr(0, e)] = t.substr(e + 1);
        i = j + 1;
    }
    return m;
}

// ---------------- okuyucu iş parçacığı ----------------
struct Okuyucu {
    Port &p;
    bool echo;
    uint16_t yuk;
    uint32_t kosu;
    const std::atomic<int64_t> *yb = nullptr;   // echo: yazma başı (-1: gönderilmedi)
    std::atomic<int64_t> *yanit = nullptr;      // echo: yanıt alınış (mutlak ns, -1 yok)
    size_t adet = 0;

    mp3_ayr_t ayr{};
    mp3_sira_t sira{};
    uint64_t okunan = 0;
    uint32_t kosu_hata = 0, tur_hata = 0, icerik_hata = 0, fazla_yanit = 0, kontrol = 0, kontrol_bayt = 0;
    int64_t t_son_rx = 0;
    int64_t ts = 0;
    // pencere
    bool pencere = false, donduruldu = false;
    uint64_t esle_oncesi = 0;    // ilk ESLE_YANIT dahil tüketilen
    uint32_t bayt_duzelt = 0;
    struct Dondurulmus {
        mp3_ayr_t ayr;
        mp3_sira_t sira;
        uint64_t bayt;
        uint32_t kosu_hata, tur_hata, icerik_hata, fazla_yanit, kontrol, kontrol_bayt;
        int64_t t_son_rx;
    } dz{};

    std::atomic<uint32_t> alinan{0};
    std::atomic<bool> dur{false};
    std::atomic<bool> kopma{false};

    std::mutex m;
    std::condition_variable cv;
    bool esle_yanit = false;
    bool ozet_var = false;
    uint32_t ozet_bozuk = 0;
    mp3_ozet_t ozet{};
    std::thread t;

    Okuyucu(Port &p_, bool echo_, uint16_t yuk_, uint32_t kosu_) : p(p_), echo(echo_), yuk(yuk_), kosu(kosu_)
    {
        mp3_ayr_sifirla(&ayr);
        mp3_sira_sifirla(&sira);
    }

    static void cb(void *ctx, const mp3_cerceve_t *c) { static_cast<Okuyucu *>(ctx)->cerceve(*c); }

    void dondur(uint32_t son_boy)
    {
        dz.ayr = ayr;
        dz.sira = sira;
        dz.bayt = static_cast<uint64_t>(ayr.tuketilen) - bayt_duzelt + son_boy;
        dz.kosu_hata = kosu_hata;
        dz.tur_hata = tur_hata + ozet_bozuk;
        dz.icerik_hata = icerik_hata;
        dz.fazla_yanit = fazla_yanit;
        dz.kontrol = kontrol;
        dz.kontrol_bayt = kontrol_bayt;
        dz.t_son_rx = t_son_rx;
        donduruldu = true;
    }

    void cerceve(const mp3_cerceve_t &c)
    {
        const uint32_t boy = MP3_EK + static_cast<uint32_t>(c.uzunluk);
        if (c.kosu != kosu) { kosu_hata++; return; }
        switch (c.tur) {
        case MP3_T_ESLE_YANIT: {
            std::lock_guard<std::mutex> l(m);
            if (!pencere) {
                // Pencere bu çerçeveden SONRA başlar: öncesindeki her şey (hız geçişi çöpü dahil) sayılmaz.
                esle_oncesi = static_cast<uint64_t>(ayr.tuketilen) + boy;
                mp3_ayr_sayac_sifirla(&ayr);
                bayt_duzelt = boy;
                kosu_hata = tur_hata = icerik_hata = fazla_yanit = 0;
                pencere = true;
            } else if (!donduruldu) {
                kontrol++;
                kontrol_bayt += boy;
            }
            esle_yanit = true;
            cv.notify_all();
            return;
        }
        case MP3_T_OZET: {
            mp3_ozet_t o{};
            std::lock_guard<std::mutex> l(m);
            if (mp3_ozet_oku(c.yuk, c.uzunluk, &o) == 0) {
                if (!ozet_var && pencere && !donduruldu) {
                    kontrol++;
                    kontrol_bayt += boy;
                    ozet = o;
                    ozet_var = true;
                    dondur(boy);
                }
            } else {
                ozet_bozuk++;
            }
            cv.notify_all();
            return;
        }
        case MP3_T_VERI_KP:
        case MP3_T_ECHO_YANIT: {
            const uint8_t bek = echo ? static_cast<uint8_t>(MP3_T_ECHO_YANIT) : static_cast<uint8_t>(MP3_T_VERI_KP);
            if (c.tur != bek || c.uzunluk != yuk) { tur_hata++; return; }
            const uint8_t ureten = echo ? static_cast<uint8_t>(MP3_T_ECHO_ISTEK) : static_cast<uint8_t>(MP3_T_VERI_KP);
            if (!mp3_veri_dogru(c.yuk, c.uzunluk, ureten, c.kosu, c.sira)) { icerik_hata++; return; }
            if (echo && (c.sira >= adet || yb[c.sira].load(std::memory_order_acquire) < 0)) {
                fazla_yanit++;   // gönderilmemiş isteğe yanıt
                return;
            }
            const int r = mp3_sira_isle(&sira, c.sira);
            if (r == MP3_SIRA_YENI || r == MP3_SIRA_BOSLUK || r == MP3_SIRA_GEC) {
                if (echo) yanit[c.sira].store(ts, std::memory_order_release);
                alinan.store(sira.alinan, std::memory_order_release);
                t_son_rx = std::max(t_son_rx, ts);
            }
            return;
        }
        default:
            tur_hata++;
            return;
        }
    }

    void dongu()
    {
        uint8_t b[8192];
        while (!dur.load(std::memory_order_acquire)) {
            size_t k = 0;
            const Io r = p.oku(b, sizeof b, 20, k);
            if (r == Io::Kopma) {
                kopma.store(true);
                std::lock_guard<std::mutex> l(m);
                cv.notify_all();
                return;
            }
            if (r != Io::Tamam) continue;
            ts = simdi_ns();
            okunan += k;
            mp3_ayr_besle(&ayr, b, k, cb, this);
        }
    }

    void basla()
    {
        t = std::thread([this] {
            etkilesimli_qos();
            dongu();
        });
    }
    void bitir()
    {
        dur.store(true, std::memory_order_release);
        if (t.joinable()) t.join();
    }

    bool bekle_kosul(int ms, const std::function<bool()> &k)
    {
        std::unique_lock<std::mutex> l(m);
        return cv.wait_for(l, std::chrono::milliseconds(ms), [&] { return k() || kopma.load(); }) && k();
    }
};

std::string u(uint64_t v) { return std::to_string(v); }

std::string utc_simdi()
{
    char b[32];
    const std::time_t t = std::time(nullptr);
    std::tm g{};
    gmtime_r(&t, &g);
    std::strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ", &g);
    return b;
}

}  // namespace

void BekleyenKuyruk::cozumle(int64_t simdi, int64_t zaman_asimi_ns, const std::function<bool(uint32_t)> &yanit_var)
{
    std::deque<Oge> kalan;
    for (const Oge &o : q_) {
        if (yanit_var(o.sira)) continue;                  // yanıt geldi
        if (simdi - o.ye > zaman_asimi_ns) continue;      // zaman aşımı (geç gelecek yanıt yine tek kez sayılır)
        kalan.push_back(o);
    }
    q_.swap(kalan);
}

bool kimlik_sor(Port &p, int sure_ms, std::map<std::string, std::string> &kimlik, std::string &ham)
{
    mp3_ayr_t a;
    mp3_ayr_sifirla(&a);
    const int64_t bitis = simdi_ns() + static_cast<int64_t>(sure_ms) * 1000000;
    uint32_t sira = 0;
    while (simdi_ns() < bitis) {
        if (gonder(p, MP3_T_SORGU, 0, sira++, nullptr, 0, 500) == Io::Kopma) return false;
        bool kopma = false;
        std::string metin;
        const bool ok = bekle(p, a, 300, [&](const mp3_cerceve_t &c) {
            if (c.tur != MP3_T_KIMLIK) return false;
            metin.assign(reinterpret_cast<const char *>(c.yuk), c.uzunluk);
            return true;
        }, kopma);
        if (kopma) return false;
        if (ok) {
            ham = metin;
            kimlik = kv_ayir(metin);
            return true;
        }
    }
    return false;
}

AnalizSonuc kos(Port &p, const Ayar &a, uint32_t kosu, const AnalizBeklenti &bek, std::string &hata)
{
    mp3_hazirla();
    etkilesimli_qos();
    AnalizSonuc hs;
    if (!(a.akis_oran > 0 && a.akis_oran <= AKIS_ORAN_SINIR) || !(a.echo_hiz > 0) || !(a.sure_s > 0) ||
        a.yuk > MP3_AZAMI_YUK || a.baud == 0) {
        hata = "ayar geçersiz (akış oranı %80'i aşamaz)";
        hs.nedenler.push_back(hata);
        return hs;
    }
    KayitYazici kw;
    if (!kw.ac(a.kayit, hata)) {
        hs.nedenler.push_back(hata);
        return hs;
    }
    const char *mod = a.echo ? "echo" : "akis";
    const uint64_t oran_ppm = a.echo ? 0 : static_cast<uint64_t>(std::llround(a.akis_oran * 1e6));
    const uint64_t hiz_milli = a.echo ? static_cast<uint64_t>(std::llround(a.echo_hiz * 1000.0)) : 0;
    const std::string profil = profil_adi(a.echo, a.yuk, oran_ppm, hiz_milli);
    std::string asama = "tamam";
    std::map<std::string, std::string> kim;
    std::string kim_ham;
    HizRaporu hr{};
    mp3_kur_yanit_t ky{};
    mp3_baud_t pcb{};
    uint64_t aralik = 0, adet = 0;
    uint32_t esle_deneme = 0, ozet_deneme = 0, donus_ok = 0;
    const std::string utc = utc_simdi();

    uint64_t tx_cerceve = 0, tx_bayt = 0, tx_atlanan = 0, yazma_hata = 0, kopma = 0, takvim_kesildi = 0;
    int64_t tx_gec_azami = 0, yazma_azami = 0, t_son_plan = 0, t_son_yaz = 0;
    uint32_t azami_bek_gozlenen = 0;
    std::vector<int64_t> ye_v;
    std::unique_ptr<std::atomic<int64_t>[]> yb_v, yanit_v;

    Okuyucu od(p, a.echo, a.yuk, kosu);
    int64_t t0 = 0;
    bool okuyucu_basladi = false;
    bool ozet_var = false;
    mp3_ozet_t ozet{};

    auto kontrol_hizina_don = [&](int64_t kart_bitis_ns) {
        const int64_t bekle_ns = kart_bitis_ns - simdi_ns();
        if (bekle_ns > 0) uyu_kadar(simdi_ns() + bekle_ns);
        HizRaporu h0{};
        if (!p.hiz(MP3_KONTROL_BAUD, h0)) return;
        p.giris_at();
        std::map<std::string, std::string> k2;
        std::string ham2;
        donus_ok = kimlik_sor(p, a.donus_ms, k2, ham2) ? 1u : 0u;
    };

    do {
        HizRaporu h0{};
        if (!p.hiz(MP3_KONTROL_BAUD, h0)) { asama = "kontrol_hizi"; break; }
        p.giris_at();
        if (!kimlik_sor(p, 3000, kim, kim_ham)) { asama = "kimlik_yok"; break; }
        uint32_t usart_hz = 0;
        if (kim.count("usart_hz")) usart_hz = static_cast<uint32_t>(std::strtoul(kim["usart_hz"].c_str(), nullptr, 10));
        const bool hesap_ok = mp3_baud_hesapla(usart_hz, a.baud, &pcb) == 0;
        const uint32_t etkin = hesap_ok ? std::min(a.baud, pcb.hesap_baud) : a.baud;
        aralik = a.echo ? echo_araligi(hiz_milli) : akis_araligi(etkin, a.yuk, static_cast<double>(oran_ppm) / 1e6);
        if (aralik == 0 || aralik > 0xFFFFFFFFull) { asama = "ayar"; break; }
        adet = static_cast<uint64_t>(std::ceil(a.sure_s * 1e9 / static_cast<double>(aralik)));
        if (adet == 0 || adet > 0xFFFFFFFFull) { asama = "ayar"; break; }
        {
            // Kapasiteyi aşan profil koşturulmaz (akış %80 sabit, echo %100): hesapla GECMEDI.
            const double yon = (static_cast<double>(a.yuk) + MP3_EK) * 1e9 / static_cast<double>(aralik);
            const double sinir = (a.echo ? ECHO_KAPASITE_SINIR : AKIS_ORAN_SINIR) * static_cast<double>(etkin) / 10.0;
            if (yon > sinir * (1.0 + 1e-12)) { asama = "kapasite"; break; }
        }
        if (a.echo) {
            ye_v.assign(adet, -1);
            yb_v.reset(new std::atomic<int64_t>[adet]);
            yanit_v.reset(new std::atomic<int64_t>[adet]);
            for (uint64_t k = 0; k < adet; ++k) {
                yb_v[k].store(-1);
                yanit_v[k].store(-1);
            }
        }

        // KUR
        mp3_kur_t kur{};
        kur.baud = a.baud;
        kur.mod = static_cast<uint8_t>(a.echo ? MP3_MOD_ECHO : MP3_MOD_AKIS);
        kur.yuk = a.yuk;
        kur.kart_adet = a.echo ? 0u : static_cast<uint32_t>(adet);
        kur.aralik_ns = static_cast<uint32_t>(aralik);
        kur.esle_ms = a.esle_kart_ms;
        kur.bos_ms = a.bos_kart_ms;
        uint8_t kb[MP3_KUR_BOY];
        mp3_kur_yaz(kb, &kur);
        mp3_ayr_t ay;
        mp3_ayr_sifirla(&ay);
        bool ky_var = false;
        int64_t t_kur = 0;
        for (int d = 0; d < 3 && !ky_var; ++d) {
            if (gonder(p, MP3_T_KUR, kosu, static_cast<uint32_t>(d), kb, MP3_KUR_BOY, 500) == Io::Kopma) { kopma = 1; break; }
            bool kp = false;
            ky_var = bekle(p, ay, 500, [&](const mp3_cerceve_t &c) {
                if (c.tur != MP3_T_KUR_YANIT || c.kosu != kosu || c.uzunluk != MP3_KUR_YANIT_BOY) return false;
                mp3_kur_yanit_oku(c.yuk, &ky);
                return true;
            }, kp);
            if (kp) { kopma = 1; break; }
            t_kur = simdi_ns();
        }
        if (kopma) { asama = "kopma"; break; }
        if (!ky_var) { asama = "kur_yanit_yok"; break; }
        if (!ky.kabul) { asama = "kur_ret"; break; }
        const int64_t kart_esle_bitis = t_kur + static_cast<int64_t>(a.esle_kart_ms) * 1000000 + 300000000;
        if (!hesap_ok || ky.brr != pcb.brr || ky.over8 != pcb.over8 || ky.hesap_baud != pcb.hesap_baud ||
            ky.usart_hz != pcb.usart_hz) {
            asama = "kur_uyusmaz";
            kontrol_hizina_don(kart_esle_bitis);
            break;
        }
        if (!p.hiz(a.baud, hr)) {
            asama = "os_hiz_ret";
            kontrol_hizina_don(kart_esle_bitis);
            break;
        }
        uyu_ms(50);
        p.giris_at();

        // ESLE
        od.yb = yb_v.get();
        od.yanit = yanit_v.get();
        od.adet = a.echo ? adet : 0;
        od.basla();
        okuyucu_basladi = true;
        const int64_t esle_bitis = simdi_ns() + static_cast<int64_t>(a.esle_pc_ms) * 1000000;
        bool esle_ok = false;
        while (!esle_ok && simdi_ns() < esle_bitis && !od.kopma.load()) {
            esle_deneme++;
            if (gonder(p, MP3_T_ESLE, kosu, esle_deneme, nullptr, 0, a.yazma_ms) == Io::Kopma) { kopma = 1; break; }
            esle_ok = od.bekle_kosul(200, [&] { return od.esle_yanit; });
        }
        if (kopma || od.kopma.load()) { asama = "kopma"; break; }
        if (!esle_ok) {
            asama = "esle_yok";
            od.bitir();
            okuyucu_basladi = false;
            kontrol_hizina_don(kart_esle_bitis);
            break;
        }

        // trafik
        t0 = simdi_ns();
        std::vector<uint8_t> f(MP3_AZAMI_CERCEVE), y(a.yuk == 0 ? 1 : a.yuk);
        const uint8_t tur = a.echo ? static_cast<uint8_t>(MP3_T_ECHO_ISTEK) : static_cast<uint8_t>(MP3_T_VERI_PK);
        BekleyenKuyruk q(a.azami_bekleyen);
        const int64_t za_ns = static_cast<int64_t>(a.zaman_asimi_ms) * 1000000;
        for (uint64_t k = 0; k < adet; ++k) {
            if (od.kopma.load()) { kopma = 1; break; }
            const int64_t plan = t0 + static_cast<int64_t>(k * aralik);
            uyu_kadar(plan);
            const uint32_t sr = static_cast<uint32_t>(k);
            if (a.echo) {
                // Karar anı t: yanıtı t'de görünür olan (yanıt ≤ t) ya da t − yazma_sonu > zaman_aşımı olan istek çıkar.
                q.cozumle(simdi_ns(), za_ns, [&](uint32_t j) { return yanit_v[j].load(std::memory_order_acquire) >= 0; });
                if (q.dolu()) { tx_atlanan++; continue; }
            }
            mp3_veri_uret(y.data(), a.yuk, tur, kosu, sr);
            const size_t n = mp3_kodla(f.data(), f.size(), tur, kosu, sr, y.data(), a.yuk);
            const int64_t yb = simdi_ns();
            const int64_t gec = yb - plan;
            tx_gec_azami = std::max(tx_gec_azami, gec);
            if (a.echo) yb_v[k].store(yb, std::memory_order_release);
            size_t w = 0;
            const Io r = p.yaz(f.data(), n, a.yazma_ms, w);
            const int64_t ye = simdi_ns();
            if (a.echo) ye_v[k] = ye;
            yazma_azami = std::max(yazma_azami, ye - yb);
            if (r != Io::Tamam) {
                yazma_hata++;
                if (r == Io::Kopma) kopma = 1;
                break;
            }
            if (a.echo) {
                q.gonderildi(sr, ye);   // ekleme SONRASI doluluk (k dahil) ölçülür; sınır 64 ise en çok 64 olur
                azami_bek_gozlenen = std::max<uint32_t>(azami_bek_gozlenen, static_cast<uint32_t>(q.boyut()));
            }
            tx_cerceve++;
            tx_bayt += n;
            t_son_plan = plan - t0;
            t_son_yaz = ye - t0;
            if (gec > a.takvim_kes_ns) { takvim_kesildi = 1; break; }
        }
        if (kopma) { asama = "kopma"; break; }
        if (yazma_hata) { asama = "yazma_hata"; break; }

        // boşaltma (sınırlı)
        if (a.echo) {
            const int64_t son = t0 + t_son_yaz + za_ns;
            while (od.alinan.load() < tx_cerceve && simdi_ns() < son && !od.kopma.load()) uyu_ms(5);
        } else {
            const int64_t son = t0 + static_cast<int64_t>((adet - 1) * aralik) + takvim_payi(static_cast<int64_t>(adet * aralik)) +
                                static_cast<int64_t>(a.bosaltma_ms) * 1000000;
            while (od.alinan.load() < adet && simdi_ns() < son && !od.kopma.load()) uyu_ms(5);
        }
        if (od.kopma.load()) { asama = "kopma"; break; }

        // BITIR -> OZET
        for (int d = 0; d < 3 && !ozet_var; ++d) {
            ozet_deneme++;
            if (gonder(p, MP3_T_BITIR, kosu, static_cast<uint32_t>(d), nullptr, 0, a.yazma_ms) == Io::Kopma) { kopma = 1; break; }
            ozet_var = od.bekle_kosul(1000, [&] { return od.ozet_var; });
        }
        if (kopma || od.kopma.load()) { asama = "kopma"; break; }
        if (!ozet_var) { asama = "ozet_yok"; }
        {
            std::lock_guard<std::mutex> l(od.m);
            ozet = od.ozet;
        }
        (void)gonder(p, MP3_T_DONUS, kosu, 0, nullptr, 0, a.yazma_ms);
        uyu_ms(100);
        od.bitir();
        okuyucu_basladi = false;
        kontrol_hizina_don(0);
    } while (false);
    if (okuyucu_basladi) od.bitir();
    if (od.kopma.load()) kopma = 1;

    // ---- kayıt ----
    const auto g = [](int64_t v) { return v < 0 ? std::string("-") : std::to_string(v); };
    Alanlar B = {
        {"surum", u(MP3_SURUM)}, {"kosu", u(kosu)}, {"mod", mod}, {"profil", profil}, {"baud", u(a.baud)},
        {"yuk", u(a.yuk)}, {"pc_adet", u(adet)}, {"kart_adet", u(a.echo ? 0 : adet)}, {"aralik_ns", u(aralik)},
        {"oran_ppm", u(oran_ppm)}, {"hiz_milli", u(hiz_milli)},
        {"zaman_asimi_ms", u(static_cast<uint64_t>(a.zaman_asimi_ms))}, {"azami_bekleyen", u(a.azami_bekleyen)},
        {"port", p.ad()}, {"os_istenen", u(hr.istenen)}, {"os_tcsetattr", u(hr.tcsetattr_ok ? 1 : 0)},
        {"os_ioctl", u(hr.ioctl_ok ? 1 : 0)}, {"os_errno", u(static_cast<uint64_t>(hr.hata_errno))},
        {"os_geri_i", u(hr.geri_ispeed)}, {"os_geri_o", u(hr.geri_ospeed)}, {"kart_kabul", u(ky.kabul)},
        {"kart_neden", u(ky.neden)}, {"kart_usart_hz", u(ky.usart_hz)}, {"kart_brr", u(ky.brr)},
        {"kart_over8", u(ky.over8)}, {"kart_hesap_baud", u(ky.hesap_baud)},
        {"kart_hata_ppm", std::to_string(ky.hata_ppm)}, {"pc_hesap_baud", u(pcb.hesap_baud)},
        {"kart_kimlik", kim.count("kimlik") ? kim["kimlik"] : std::string("-")}, {"saat", "steady_clock"},
        {"utc", utc}};
    kw.alanli("B", B);
    if (a.echo && yb_v) {
        // Tamamlanan koşuda planın tamamı; kesilen koşuda yalnız denenen istekler (kayıt zaten GECERSIZ/BASLAMADI olur).
        const uint64_t r_say = asama == "tamam" ? adet : std::min<uint64_t>(adet, tx_cerceve + tx_atlanan);
        for (uint64_t k = 0; k < r_say; ++k) {
            const int64_t ybk = yb_v[k].load();
            const int64_t yb = ybk < 0 ? -1 : ybk - t0;
            const int64_t ye = ye_v[k] < 0 || ybk < 0 ? -1 : ye_v[k] - t0;
            const int64_t ynk = yanit_v[k].load();
            const int64_t yn = ynk < 0 ? -1 : ynk - t0;
            kw.konumsal("R", u(k) + " " + std::to_string(static_cast<int64_t>(k * aralik)) + " " + g(yb) + " " + g(ye) + " " + g(yn));
        }
    }
    // pencere sayaçları: OZET ile dondurulmuş değerler (OZET yoksa son değerler; kayıt zaten GECERSIZ olur)
    if (!od.donduruldu) od.dondur(0);
    const auto &z = od.dz;
    const uint64_t pencere_sonu = od.esle_oncesi + z.bayt;
    const uint64_t sonrasi = od.okunan >= pencere_sonu ? od.okunan - pencere_sonu : 0;
    const int64_t t_son_rx = z.t_son_rx > 0 && t0 > 0 ? z.t_son_rx - t0 : 0;
    Alanlar P = {
        {"kosu", u(kosu)}, {"asama", asama}, {"tx_cerceve", u(tx_cerceve)}, {"tx_bayt", u(tx_bayt)},
        {"tx_atlanan", u(tx_atlanan)}, {"tx_gec_azami_ns", u(static_cast<uint64_t>(std::max<int64_t>(0, tx_gec_azami)))},
        {"yazma_hata", u(yazma_hata)}, {"yazma_azami_ns", u(static_cast<uint64_t>(yazma_azami))},
        {"takvim_kesildi", u(takvim_kesildi)}, {"rx_okunan_bayt", u(od.okunan)},
        {"esle_oncesi_bayt", u(od.esle_oncesi)}, {"ozet_sonrasi_bayt", u(sonrasi)}, {"rx_bayt", u(z.bayt)},
        {"rx_iyi", u(z.ayr.iyi)}, {"rx_atilan_bayt", u(z.ayr.atilan_bayt)}, {"rx_baslik_hata", u(z.ayr.baslik_hata)},
        {"rx_surum_hata", u(z.ayr.surum_hata)}, {"rx_uzunluk_hata", u(z.ayr.uzunluk_hata)},
        {"rx_crc_hata", u(z.ayr.crc_hata)}, {"rx_kosu_hata", u(z.kosu_hata)}, {"rx_tur_hata", u(z.tur_hata)},
        {"rx_icerik_hata", u(z.icerik_hata)}, {"rx_alinan", u(z.sira.alinan)}, {"rx_beklenen", u(z.sira.beklenen)},
        {"rx_eksik", u(z.sira.eksik)}, {"rx_tekrar", u(z.sira.tekrar)}, {"rx_sirasiz", u(z.sira.sirasiz)},
        {"rx_eski", u(z.sira.eski)}, {"rx_kontrol", u(z.kontrol)}, {"rx_kontrol_bayt", u(z.kontrol_bayt)},
        {"fazla_yanit", u(z.fazla_yanit)}, {"kopma", u(kopma)}, {"esle_deneme", u(esle_deneme)},
        {"ozet_deneme", u(ozet_deneme)}, {"donus_ok", u(donus_ok)},
        {"t_son_plan_ns", u(static_cast<uint64_t>(t_son_plan))}, {"t_son_yaz_ns", u(static_cast<uint64_t>(t_son_yaz))},
        {"t_son_rx_ns", u(static_cast<uint64_t>(std::max<int64_t>(0, t_son_rx)))},
        {"azami_bekleyen_gozlenen", u(azami_bek_gozlenen)}};
    kw.alanli("P", P);
    if (ozet_var) {
        Alanlar K;
        for (unsigned i = 0; i < static_cast<unsigned>(MP3_OZET_N); ++i) K.emplace_back(mp3_ozet_ad(i), u(mp3_ozet_deger(&ozet, i)));
        kw.alanli("K", K);
    }
    if (!kw.kapat()) {
        hata = "kayıt yazılamadı: " + a.kayit;
        hs.nedenler.push_back(hata);
        return hs;
    }
    return analiz_dosya(a.kayit, bek);
}

int tara(Port &p, const std::vector<uint32_t> &baudlar, double akis_sn, double echo_sn, const std::string &dizin,
         const std::function<uint32_t()> &kosu_uret, std::vector<TaramaSatiri> &tablo, const Ayar &taban)
{
    int sonuc = 0;
    for (uint32_t baud : baudlar) {
        TaramaSatiri s{baud, Hukum::Gecersiz, Hukum::Gecersiz};
        for (int kip = 0; kip < 2; ++kip) {
            Ayar a = taban;
            a.baud = baud;
            a.echo = kip == 1;
            a.yuk = a.echo ? 64 : 256;
            a.sure_s = a.echo ? echo_sn : akis_sn;
            a.akis_oran = AKIS_ORAN_SINIR;
            a.echo_hiz = 200.0;
            a.kayit = dizin + "/tara-" + std::to_string(baud) + (a.echo ? "-echo64-200.kayit" : "-akis256.kayit");
            std::string hata;
            AnalizBeklenti b;
            b.kampanya = true;
            const uint32_t kosu = kosu_uret();
            std::cout << "=== " << baud << " " << (a.echo ? "echo 64 B 200/s" : "akış 256 B %80") << " koşu " << std::hex
                      << kosu << std::dec << std::endl;
            const AnalizSonuc r = kos(p, a, kosu, b, hata);
            if (!hata.empty()) std::cout << "  hata: " << hata << "\n";
            std::cout << r.rapor;
            for (const auto &nd : r.nedenler) std::cout << "  neden: " << nd << "\n";
            std::cout << "  SONUC: " << hukum_ad(r.hukum) << " (profil " << r.profil << ")" << std::endl;
            (a.echo ? s.echo : s.akis) = r.hukum;
            if (r.hukum == Hukum::Gecersiz) sonuc = 2;
            else if (r.hukum != Hukum::Gecti && sonuc == 0) sonuc = 1;
        }
        tablo.push_back(s);
    }
    return sonuc;
}

}  // namespace mp3
