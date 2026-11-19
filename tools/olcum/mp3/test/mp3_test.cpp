// W0-MP3a masaüstü testleri (atılabilir). Kart gerekmez. Çıkış kodu 0 = bütün denetimler geçti.
//   - çerçeve/CRC/ayrıştırıcı/sıra izleyici/baud hesabı birim denetimleri
//   - uçtan uca: PC oturumu (gerçek PosixPort okuma/yazma kodu) <-> socketpair <-> kart mantığı öykünücüsü,
//     temiz ve hata enjeksiyonlu koşular (kısmi IO, bozuk/düşen bayt, donanım sayacı, kopma, susma, tıkanma,
//     hız reddi, eşleme yok, kapasite, süre)
//   - kayıt negatif denetimleri (V4): kesik, bozuk CRC, eksik özet, yanlış koşu kimliği, sayaç/sıra hatası,
//     zaman aşımı, yinelenen satır, S'den sonra veri
#include "kayit.hpp"
#include "mp3_halka.h"
#include "secenek.hpp"
#include "mp3_cerceve.h"
#include "oturum.hpp"
#include "sahte_hat.hpp"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <set>
#include <memory>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <deque>
#include <tuple>

using namespace mp3;
using namespace mp3test;

namespace {

int g_denetim = 0, g_hata = 0;

#define DENETLE(k)                                                                                 \
    do {                                                                                           \
        ++g_denetim;                                                                               \
        if (!(k)) {                                                                                \
            ++g_hata;                                                                              \
            std::cout << "  BAŞARISIZ " << __FILE__ << ":" << __LINE__ << ": " #k << std::endl;    \
        }                                                                                          \
    } while (0)

std::string g_dizin;
int g_dosya = 0;

std::string yeni_dosya(const std::string &ad) { return g_dizin + "/" + std::to_string(g_dosya++) + "-" + ad + ".kayit"; }

std::string oku(const std::string &yol)
{
    std::ifstream f(yol, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ---------------- birim ----------------
struct Toplayici {
    std::vector<mp3_cerceve_t> c;
    std::vector<std::vector<uint8_t>> yukler;
};
void topla(void *ctx, const mp3_cerceve_t *c)
{
    auto *t = static_cast<Toplayici *>(ctx);
    t->yukler.emplace_back(c->yuk, c->yuk + c->uzunluk);
    t->c.push_back(*c);
}

std::vector<uint8_t> cerceve(uint8_t tur, uint32_t kosu, uint32_t sira, uint16_t n)
{
    std::vector<uint8_t> y(n), f(MP3_AZAMI_CERCEVE);
    mp3_veri_uret(y.data(), n, tur, kosu, sira);
    f.resize(mp3_kodla(f.data(), f.size(), tur, kosu, sira, y.data(), n));
    return f;
}

// DMA alım konumu (düzeltme turu 2, sol Y1). Önce belirlenimci sınır dizileri, sonra rastgele olay modeli.
void halka_sinir()
{
    const uint32_t N = 16384;
    mp3_halka_t h;
    mp3_halka_baslat(&h, N, 255);
    const uint32_t B = 1000000;   // bayt/s (yoklama aralığı koruması için)
    // 1 MB/s'de yoklamalar arası ilerleme yarım halkadan az olmalı: 7000 µs'lik adımlarla ilerlenir
    DENETLE(mp3_halka_guncelle(&h, N, 0, N, 0, B) == MP3_HALKA_TAMAM && h.toplam == 0);
    DENETLE(mp3_halka_guncelle(&h, N - 7000, 0, N - 7000, 7000, B) == MP3_HALKA_TAMAM && h.toplam == 7000);
    DENETLE(mp3_halka_guncelle(&h, N - 14000, 0, N - 14000, 14000, B) == MP3_HALKA_TAMAM && h.toplam == 14000);
    DENETLE(mp3_halka_guncelle(&h, 10, 0, 10, 16374, B) == MP3_HALKA_TAMAM && h.toplam == N - 10);
    // sol'un karşı örneği: TC hizmet görmüş, düğüm henüz yeniden yüklenmemiş (BNDT = 0, FIFOL = 0): konum N
    // (eski işlev yarim = 2 ile 2N = 32768 veriyordu; yeni işlev kesme sayımını kullanmaz)
    DENETLE(mp3_halka_guncelle(&h, 0, 0, 0, 16384, B) == MP3_HALKA_TAMAM && h.toplam == N);
    // yeniden yükleme (BNDT = N): konum değişmez, iki kez sayılmaz
    DENETLE(mp3_halka_guncelle(&h, N, 0, N, 16385, B) == MP3_HALKA_TAMAM && h.toplam == N);
    DENETLE(mp3_halka_guncelle(&h, N - 4, 0, N - 4, 16390, B) == MP3_HALKA_TAMAM && h.toplam == N + 4);
    // FIFO'da bayt varken blok sonu ve yeniden yükleme: (0, 3) ve (N, 3) aynı RAM konumu
    mp3_halka_t g;
    mp3_halka_baslat(&g, N, 255);
    DENETLE(mp3_halka_guncelle(&g, N, 0, N, 0, B) == MP3_HALKA_TAMAM);
    DENETLE(mp3_halka_guncelle(&g, N - 7000, 0, N - 7000, 7000, B) == MP3_HALKA_TAMAM && g.toplam == 7000);
    DENETLE(mp3_halka_guncelle(&g, N - 14000, 0, N - 14000, 13900, B) == MP3_HALKA_TAMAM && g.toplam == 14000);
    DENETLE(mp3_halka_guncelle(&g, 5, 2, 5, 14000, B) == MP3_HALKA_TAMAM && g.toplam == N - 7);
    DENETLE(mp3_halka_guncelle(&g, 0, 3, 0, 14005, B) == MP3_HALKA_TAMAM && g.toplam == N - 3);
    DENETLE(mp3_halka_guncelle(&g, N, 3, N, 14006, B) == MP3_HALKA_TAMAM && g.toplam == N - 3);
    DENETLE(mp3_halka_guncelle(&g, N, 0, N, 14007, B) == MP3_HALKA_TAMAM && g.toplam == N);
    // kararsız görüntü (BNDT okumalar arasında değişti): hiçbir şey değişmez
    DENETLE(mp3_halka_guncelle(&g, N - 1, 0, N - 2, 14008, B) == MP3_HALKA_KARARSIZ && g.toplam == N);
    // tek yoklamada yarım halka ya da fazlası ilerleme: tur bilinemez
    DENETLE(mp3_halka_guncelle(&g, N / 2, 0, N / 2, 14010, B) == MP3_HALKA_BELIRSIZ);
    // tur belirsizliği: uzun yoklama boşluğu (1 MB/s'de 9 ms > yarım halka) ya da geçersiz yazmaç
    DENETLE(mp3_halka_guncelle(&g, N / 2 - 1, 0, N / 2 - 1, 14010 + 9000, B) == MP3_HALKA_BELIRSIZ);
    DENETLE(mp3_halka_guncelle(&g, N + 1, 0, N + 1, 23011, B) == MP3_HALKA_BELIRSIZ);
    DENETLE(mp3_halka_guncelle(&g, 5, 256, 5, 23012, B) == MP3_HALKA_BELIRSIZ);
}

// Olay modeli: kaynak okuması (BNDT−1 ve FIFO+1 birlikte; varsayım V2), hedef yazımı (FIFO−1, RAM), blok sonu,
// TC bayrağı, kesme hizmeti ve LLI yeniden yüklemesi AYRI olaylardır ve sıraları rastgeledir (yeniden yükleme FIFO
// boşalmadan önce de olabilir). PRIMASK DMA'yı durdurmaz: okuyucunun BNDT, FIFOL, BNDT okumaları arasına DMA
// olayları girer. Denetimler: kabul edilen her görüntüde toplam = o anki RAM'e yazılmış bayt (tam eşitlik, tekdüzelik
// zorlaması yok); tüketici yalnız yazılmış baytları ve doğru değerleri alır; geride kalan tüketici ve uzun yoklama
// boşlukları hata olarak yakalanır, yanlış bayt kabul edilmez.
void halka_modeli(uint32_t tohum)
{
    const uint32_t N = 64, FIFO = 8;
    std::mt19937 rng(tohum);
    uint64_t S = 0, W = 0;          // kaynak ve hedef (toplam)
    uint32_t bndt = N;              // kanal yazmacı
    bool blok_sonu = false;         // kaynak bloğu bitirdi, yeniden yükleme bekleniyor
    bool tc_bayrak = false;
    uint32_t tc_hizmet = 0;         // kesme hizmeti (kullanılmaz; yalnız olay sırası için)
    int64_t zaman = 0;              // her olay 1 µs; kaynak en çok 1 B/µs
    std::vector<int> ram(N, -1);
    mp3_halka_t h;
    mp3_halka_baslat(&h, N, FIFO);
    uint64_t R = 0, kabul = 0, kararsiz = 0, belirsiz = 0, tasma = 0;
    uint64_t hata_toplam = 0, hata_veri = 0;
    auto dma_olay = [&]() {
        zaman++;
        switch (rng() % 5) {
        case 0: case 1:
            if (!blok_sonu && S - W < FIFO) {
                S++;
                bndt--;
                if (bndt == 0) blok_sonu = true;
            }
            break;
        case 2:
            if (W < S) { ram[W % N] = static_cast<int>(W & 0xFF); W++; }
            break;
        case 3:   // TC bayrağı: blok sonu ve FIFO boş
            if (blok_sonu && W == S) tc_bayrak = true;
            break;
        case 4:   // LLI yeniden yükleme (TC'den önce ya da sonra; FIFO boşalmadan da olabilir)
            if (blok_sonu && rng() % 2 == 0) { bndt = N; blok_sonu = false; }
            if (tc_bayrak && rng() % 2 == 0) { tc_bayrak = false; tc_hizmet++; }
            break;
        }
    };
    for (int adim = 0; adim < 300000; ++adim) {
        dma_olay();
        if (rng() % 4 != 0) continue;
        const bool uzun = adim % 50000 == 25000;   // arada bir uzun yoklama boşluğu
        if (uzun) for (int k = 0; k < 40; ++k) dma_olay();
        const uint32_t b1 = bndt;
        if (rng() % 2) dma_olay();
        const uint32_t f = static_cast<uint32_t>(S - W);
        const uint64_t W_f = W;
        if (rng() % 2) dma_olay();
        const uint32_t b2 = bndt;
        const int r = mp3_halka_guncelle(&h, b1, f, b2, static_cast<uint32_t>(zaman), 1000000);
        if (r == MP3_HALKA_KARARSIZ) { kararsiz++; continue; }
        if (r == MP3_HALKA_BELIRSIZ) { belirsiz++; R = h.toplam; continue; }
        kabul++;
        if (h.toplam != W_f) hata_toplam++;
        if (adim % 20000 < 300) continue;   // tüketici geride kalır
        if (h.toplam - R > N) { tasma++; R = h.toplam; continue; }
        while (R < h.toplam) {
            if (ram[R % N] != static_cast<int>(R & 0xFF)) hata_veri++;
            R++;
        }
    }
    std::cout << "  halka modeli (tohum " << tohum << "): kabul " << kabul << ", kararsız " << kararsiz << ", belirsiz "
              << belirsiz << ", taşma " << tasma << ", TC hizmeti " << tc_hizmet << std::endl;
    DENETLE(kabul > 30000 && hata_toplam == 0 && hata_veri == 0 && kararsiz > 0 && tasma > 0 && belirsiz > 0);
}

// Echo bekleyen kuyruğu: sol'un karşı örneği (sınır 2; 0,1 gönderilir, 2 atlanır, 0 yanıtlanır, 3 gönderilir, ...)
void kuyruk()
{
    BekleyenKuyruk q(2);
    std::set<uint32_t> yanit;
    auto var = [&](uint32_t j) { return yanit.count(j) != 0; };
    const int64_t za = 1000;
    q.gonderildi(0, 0);
    q.gonderildi(1, 1);
    q.cozumle(2, za, var);
    DENETLE(q.dolu());                       // 2 atlanır
    yanit.insert(0);
    q.cozumle(3, za, var);
    DENETLE(q.boyut() == 1 && !q.dolu());    // eski kod 3 gösteriyordu
    q.gonderildi(3, 3);
    q.cozumle(4, za, var);
    DENETLE(q.dolu());                       // 4 atlanır (1 ve 3 bekliyor)
    yanit.insert(1);
    yanit.insert(3);
    q.cozumle(5, za, var);
    DENETLE(q.boyut() == 0);                 // eski kod 6 − 4 = 2 gösteriyordu
    q.gonderildi(5, 5);
    yanit.insert(5);
    q.cozumle(6, za, var);
    DENETLE(q.boyut() == 0);
    q.gonderildi(6, 10);
    q.cozumle(10 + za + 1, za, var);         // zaman aşımı: bir kez çıkar
    DENETLE(q.boyut() == 0);
    yanit.insert(6);                         // geç yanıt kuyruğu bir daha etkilemez
    q.cozumle(3000, za, var);
    DENETLE(q.boyut() == 0);
}

void secenek()
{
    DENETLE(!tam_sayi("65536", 0, 256) && tam_sayi("256", 0, 256) == 256u && !tam_sayi("-1", 0, 256));
    DENETLE(!tam_sayi("12a", 0, 1000) && !tam_sayi(" 1", 0, 10) && !tam_sayi("", 0, 10));
    DENETLE(!tam_sayi("18446744073709551616", 0, UINT64_MAX) && tam_sayi("ff", 1, 0xFFFFFFFF, 16) == 255u);
    DENETLE(!ondalik("0.9", 1e-6, 0.80) && ondalik("0.8", 1e-6, 0.80).has_value() && !ondalik("nan", 0, 1));
    DENETLE(!ondalik("inf", 0, 1e9) && !ondalik("1e3", 0, 1e9) && !ondalik("-1", -5, 5) && !ondalik("1.2.3", 0, 9));
}

// Yazma süre sınırı: her çağrı 1 bayt yazıp 5 ms sürer; 276 bayt 50 ms'de bitmez -> ZamanAsimi, gerçek sayı korunur.
void yazma_suresi()
{
    int sv[2];
    DENETLE(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    std::unique_ptr<Port> p(posix_fd_ac(sv[0], "test", [](uint32_t) { return true; },
                                        [](int fd, const uint8_t *b, size_t) -> long {
                                            std::this_thread::sleep_for(std::chrono::milliseconds(5));
                                            return static_cast<long>(::write(fd, b, 1));
                                        }));
    std::vector<uint8_t> v(276, 0x55);
    size_t w = 0;
    const auto t0 = std::chrono::steady_clock::now();
    const Io r = p->yaz(v.data(), v.size(), 50, w);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::cout << "  yazma süre sınırı: " << io_ad(r) << ", " << w << " bayt, " << ms << " ms" << std::endl;
    DENETLE(r == Io::ZamanAsimi && w >= 5 && w < 276 && ms < 100.0);
    ::close(sv[1]);
}

void birim()
{
    std::cout << "[birim]" << std::endl;
    const uint8_t s[] = "123456789";
    DENETLE(mp3_crc16(s, 9) == 0x29B1);
    DENETLE(mp3_crc32(s, 9) == 0xCBF43926u);

    // gidiş-dönüş, rastgele parçalarla
    std::mt19937 rng(7);
    std::vector<uint8_t> akis;
    std::vector<uint16_t> boylar;
    for (uint32_t i = 0; i < 300; ++i) {
        const uint16_t n = static_cast<uint16_t>(rng() % (MP3_AZAMI_YUK + 1));
        boylar.push_back(n);
        auto f = cerceve(MP3_T_VERI_PK, 0xABCD1234u, i, n);
        akis.insert(akis.end(), f.begin(), f.end());
    }
    {
        mp3_ayr_t a;
        mp3_ayr_sifirla(&a);
        Toplayici t;
        size_t i = 0;
        while (i < akis.size()) {
            const size_t k = std::min<size_t>(1 + rng() % 50, akis.size() - i);
            mp3_ayr_besle(&a, akis.data() + i, k, topla, &t);
            i += k;
        }
        DENETLE(t.c.size() == 300 && a.iyi == 300 && a.atilan_bayt == 0 && a.crc_hata == 0 && a.baslik_hata == 0);
        bool hepsi = t.c.size() == 300;
        for (size_t k = 0; hepsi && k < t.c.size(); ++k) {
            hepsi = t.c[k].sira == k && t.c[k].uzunluk == boylar[k] &&
                    mp3_veri_dogru(t.yukler[k].data(), t.c[k].uzunluk, MP3_T_VERI_PK, 0xABCD1234u, t.c[k].sira) == 1;
        }
        DENETLE(hepsi);
    }
    // eşleme: çöp + çerçeveler; bozuk uzunluk alanı; bozuk yük; kesik çerçeve
    {
        std::vector<uint8_t> v = {0x00, 0xA5, 0x13, 0xA5, 0x5A, 0xFF, 0x01};
        auto f0 = cerceve(MP3_T_VERI_PK, 1, 0, 32);
        auto f1 = cerceve(MP3_T_VERI_PK, 1, 1, 32);
        f1[12] ^= 0x40;   // uzunluk alanı bozuk -> başlık CRC'si tutmaz
        auto f2 = cerceve(MP3_T_VERI_PK, 1, 2, 32);
        auto f3 = cerceve(MP3_T_VERI_PK, 1, 3, 32);
        f3[MP3_BASLIK + 5] ^= 0x01;   // yük bozuk -> CRC-32
        auto f4 = cerceve(MP3_T_VERI_PK, 1, 4, 200);
        f4.resize(100);   // kesik
        auto f5 = cerceve(MP3_T_VERI_PK, 1, 5, 32);
        auto f6 = cerceve(MP3_T_VERI_PK, 1, 6, 32);
        auto f7 = cerceve(MP3_T_VERI_PK, 1, 7, 32);
        for (auto *f : {&f0, &f1, &f2, &f3, &f4, &f5, &f6, &f7}) v.insert(v.end(), f->begin(), f->end());
        mp3_ayr_t a;
        mp3_ayr_sifirla(&a);
        Toplayici t;
        mp3_ayr_besle(&a, v.data(), v.size(), topla, &t);
        std::vector<uint32_t> s2;
        for (auto &c : t.c) s2.push_back(c.sira);
        DENETLE(a.baslik_hata >= 1 && a.crc_hata >= 2);
        // bozuk ya da kesik çerçeve asla iyi sayılmaz
        bool yanlis = false;
        for (auto x : s2) yanlis |= (x == 1 || x == 3 || x == 4);
        DENETLE(!yanlis);
        DENETLE((s2 == std::vector<uint32_t>{0, 2, 5, 6, 7}));   // kesik f4'ün iddia ettiği uzunluğun içindeki f5..f7 kurtarılır
        DENETLE(a.atilan_bayt > 0);
    }
    // sürüm ve uzunluk: başlık CRC'si geçerli ama alan sınır dışı
    {
        auto f = cerceve(MP3_T_VERI_PK, 1, 0, 8);
        f[2] = 2;
        mp3_le16_yaz(f.data() + 14, mp3_crc16(f.data(), 14));
        auto g = cerceve(MP3_T_VERI_PK, 1, 1, 8);
        mp3_le16_yaz(g.data() + 12, 300);
        mp3_le16_yaz(g.data() + 14, mp3_crc16(g.data(), 14));
        auto h = cerceve(MP3_T_VERI_PK, 1, 2, 8);
        std::vector<uint8_t> v(f);
        v.insert(v.end(), g.begin(), g.end());
        v.insert(v.end(), h.begin(), h.end());
        mp3_ayr_t a;
        mp3_ayr_sifirla(&a);
        Toplayici t;
        mp3_ayr_besle(&a, v.data(), v.size(), topla, &t);
        DENETLE(a.surum_hata == 1 && a.uzunluk_hata == 1 && t.c.size() == 1 && t.c[0].sira == 2);
    }
    // sıra izleyici
    {
        mp3_sira_t q;
        mp3_sira_sifirla(&q);
        DENETLE(mp3_sira_isle(&q, 0) == MP3_SIRA_YENI);
        DENETLE(mp3_sira_isle(&q, 1) == MP3_SIRA_YENI);
        DENETLE(mp3_sira_isle(&q, 4) == MP3_SIRA_BOSLUK && q.eksik == 2);
        DENETLE(mp3_sira_isle(&q, 2) == MP3_SIRA_GEC && q.eksik == 1 && q.sirasiz == 1);
        DENETLE(mp3_sira_isle(&q, 2) == MP3_SIRA_TEKRAR && q.tekrar == 1);
        DENETLE(mp3_sira_isle(&q, 4) == MP3_SIRA_TEKRAR && q.tekrar == 2);
        DENETLE(mp3_sira_isle(&q, 5000) == MP3_SIRA_BOSLUK && q.beklenen == 5001);
        DENETLE(mp3_sira_isle(&q, 10) == MP3_SIRA_ESKI && q.eski == 1);
        DENETLE(q.alinan == 5 && q.eksik == 1 + (5000 - 5));
    }
    // baud hesabı (160 MHz)
    {
        mp3_baud_t b;
        // Beklenen değerler el hesabıdır (aynı işlevden üretilmez): OVER16 baud = f/BRR; OVER8 D = (BRR&0xFFF0) |
        // ((BRR&7)<<1), baud = 2f/D (ST LL_USART_GetBaudRate). 160 MHz'te OVER8 çift bölücüyle OVER16 ile aynı
        // hızları verir; OVER8 yalnız OVER16 bölücüsü 16'nın altına düşünce (12 Mbaud) seçilir.
        struct { uint32_t baud, over8, brr, hesap; int32_t ppm; } bek[] = {
            {115200, 0, 1389, 115191, -78},          // 160e6/1389 = 115190,78
            {921600, 0, 174, 919540, -2235},         // 160e6/174 = 919540,23; 173 -> 924855 daha uzak
            {2000000, 0, 80, 2000000, 0},
            {3000000, 0, 53, 3018868, 6289},         // 160e6/53 = 3018867,92; 54 -> 2962963 daha uzak
            {6000000, 0, 27, 5925926, -12345},       // 160e6/27 = 5925925,93; 26 -> 6153846 daha uzak
            {12000000, 1, 0x15, 12307692, 25641},    // OVER16 13 < 16; OVER8 D=26: BRR 0x15, 320e6/26 = 12307692,3
        };
        for (const auto &x : bek) {
            const int r = mp3_baud_hesapla(160000000u, x.baud, &b);
            DENETLE(r == 0 && b.over8 == x.over8 && b.brr == x.brr && b.hesap_baud == x.hesap && b.hata_ppm == x.ppm);
            DENETLE(mp3_brr_baud(160000000u, b.brr, b.over8) == x.hesap);
        }
        // sol karşı örneği: eski kodun 12 Mbaud için yazdığı BRR 0x15 (USARTDIV 27 sanılıyordu) gerçekte 26'dır
        DENETLE(mp3_brr_baud(160000000u, 0x15, 1) == 12307692);
        DENETLE(mp3_brr_baud(160000000u, 0x18, 1) == 0);   // OVER8'de BRR[3] = 1 geçersiz
        DENETLE(mp3_brr_baud(160000000u, 15, 0) == 0);
        DENETLE(mp3_baud_hesapla(160000000u, 30000000, &b) != 0);
        DENETLE(mp3_baud_hesapla(0u, 115200, &b) != 0);
    }
    // KUR/özet kodlama
    {
        mp3_kur_t k{3000000, MP3_MOD_AKIS, 256, 12345, 987654, 5000, 6000}, k2{};
        uint8_t p[MP3_KUR_BOY];
        mp3_kur_yaz(p, &k);
        mp3_kur_oku(p, &k2);
        DENETLE(std::memcmp(&k, &k2, sizeof k) == 0);
        mp3_ozet_t o{}, o2{};
        o.rx_crc_hata = 7;
        o.echo_dusen = 9;
        uint8_t q[MP3_OZET_BOY];
        mp3_ozet_yaz(q, &o);
        DENETLE(mp3_ozet_oku(q, MP3_OZET_BOY, &o2) == 0 && o2.rx_crc_hata == 7 && o2.echo_dusen == 9);
        DENETLE(mp3_ozet_oku(q, MP3_OZET_BOY - 4, &o2) != 0);
    }
    DENETLE(yuzdelik({1, 2, 3, 4}, 1, 2) == 2 && yuzdelik({1, 2, 3, 4}, 99, 100) == 4 && yuzdelik({5}, 999, 1000) == 5);
    halka_sinir();
    halka_modeli(11);
    halka_modeli(13);
    kuyruk();
    secenek();
    yazma_suresi();
}

// ---------------- uçtan uca ----------------
Ayar ayar(bool echo, uint32_t baud, double sure, const std::string &ad)
{
    Ayar a;
    a.echo = echo;
    a.baud = baud;
    a.yuk = echo ? 64 : 256;
    a.sure_s = sure;
    a.echo_hiz = 200;
    a.esle_pc_ms = 800;
    a.esle_kart_ms = 1200;
    a.bos_kart_ms = 2000;
    a.donus_ms = 4000;
    a.bosaltma_ms = 500;
    a.zaman_asimi_ms = 300;
    a.kayit = yeni_dosya(ad);
    return a;
}

struct Kosu {
    AnalizSonuc s;
    std::string kayit;
    double sure_s;
};

Kosu kos_sahte(const HataAyar &h, const Ayar &a, const AnalizBeklenti &b = AnalizBeklenti{})
{
    SahteHat hat(h);
    std::string hata;
    const auto t0 = std::chrono::steady_clock::now();
    AnalizSonuc s = kos(hat.port(), a, 0x1000u + static_cast<uint32_t>(g_dosya), b, hata);
    const double sn = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::cout << "  " << a.kayit.substr(g_dizin.size() + 1) << ": " << hukum_ad(s.hukum) << " (" << sn << " s)";
    for (auto &n : s.nedenler) std::cout << " | " << n;
    std::cout << std::endl;
    return {s, a.kayit, sn};
}

bool neden_var(const AnalizSonuc &s, const std::string &parca)
{
    for (auto &n : s.nedenler)
        if (n.find(parca) != std::string::npos) return true;
    return false;
}

std::string g_temiz_akis, g_temiz_echo;
// ---------------- kayıt düzenleme yardımcıları (negatif testler) ----------------
struct Satir {
    std::string tur;
    std::vector<std::string> alan;   // satır numarasından sonraki gövde parçaları
};

std::vector<Satir> coz(const std::string &icerik)
{
    std::vector<Satir> r;
    std::stringstream ss(icerik);
    std::string l;
    while (std::getline(ss, l)) {
        const std::string g = l.substr(0, l.rfind(" *"));
        std::stringstream t(g);
        Satir s;
        std::string no, x;
        t >> s.tur >> no;
        while (t >> x) s.alan.push_back(x);
        r.push_back(s);
    }
    return r;
}

std::string imzala(const std::vector<Satir> &v, bool s_yenile = true)
{
    std::string o;
    for (size_t i = 0; i < v.size(); ++i) {
        std::string g = v[i].tur + " " + std::to_string(i);
        if (v[i].tur == "S" && s_yenile) {
            g += " satir=" + std::to_string(i);
        } else {
            for (auto &a : v[i].alan) g += " " + a;
        }
        o += satir_imzala(g) + "\n";
    }
    return o;
}

void alan_degistir(std::vector<Satir> &v, const std::string &tur, const std::string &anahtar,
                   const std::function<std::string(const std::string &)> &f)
{
    for (auto &s : v) {
        if (s.tur != tur) continue;
        for (auto &a : s.alan) {
            if (a.rfind(anahtar + "=", 0) == 0) a = anahtar + "=" + f(a.substr(anahtar.size() + 1));
        }
    }
}

Hukum hk(const std::string &icerik) { return analiz_metin(icerik, AnalizBeklenti{}).hukum; }

// Bir kayıttaki alanın sayısal değeri (yoksa 0)
uint64_t alan_u(const std::vector<Satir> &v, const std::string &tur, const std::string &ad)
{
    for (const auto &s2 : v)
        if (s2.tur == tur)
            for (const auto &a : s2.alan)
                if (a.rfind(ad + "=", 0) == 0) return std::stoull(a.substr(ad.size() + 1));
    return 0;
}



void uctan_uca()
{
    std::cout << "[uçtan uca]" << std::endl;
    {
        auto k = kos_sahte(HataAyar{}, ayar(false, 921600, 1.0, "temiz-akis"));
        DENETLE(k.s.hukum == Hukum::Gecti);
        g_temiz_akis = k.kayit;
    }
    {
        auto k = kos_sahte(HataAyar{}, ayar(true, 2000000, 1.0, "temiz-echo"));
        DENETLE(k.s.hukum == Hukum::Gecti);
        DENETLE(k.s.rapor.find("RTT planlanan gönderimden (µs): n=200 ") != std::string::npos);
        g_temiz_echo = k.kayit;
    }
    {
        HataAyar h;
        h.parca_azami = 7;
        auto k = kos_sahte(h, ayar(false, 921600, 0.5, "kismi-io-akis"));
        DENETLE(k.s.hukum == Hukum::Gecti);
        auto e = kos_sahte(h, ayar(true, 3000000, 0.5, "kismi-io-echo"));
        DENETLE(e.s.hukum == Hukum::Gecti);
    }
    {
        HataAyar h;
        h.pk_cevir = 5000;
        auto k = kos_sahte(h, ayar(false, 921600, 0.5, "pk-cevir"));
        DENETLE(k.s.hukum == Hukum::Gecmedi && (neden_var(k.s, "kart rx_crc_hata") || neden_var(k.s, "kart rx_baslik_hata")));
    }
    {
        HataAyar h;
        h.kp_dusur = 3000;
        auto k = kos_sahte(h, ayar(false, 921600, 0.5, "kp-dusur"));
        DENETLE(k.s.hukum == Hukum::Gecmedi && neden_var(k.s, "PC rx_"));
    }
    {
        HataAyar h;
        h.kp_cevir = 1000;
        auto k = kos_sahte(h, ayar(true, 2000000, 0.5, "kp-cevir-echo"));
        DENETLE(k.s.hukum == Hukum::Gecmedi && neden_var(k.s, "zaman aşımı"));
    }
    {
        HataAyar h;
        h.enjekte_ore = 1;
        auto k = kos_sahte(h, ayar(false, 921600, 0.5, "ore"));
        DENETLE(k.s.hukum == Hukum::Gecmedi && neden_var(k.s, "kart uart_ore=1"));
        HataAyar h2;
        h2.enjekte_tasma = 1;
        auto k2 = kos_sahte(h2, ayar(false, 921600, 0.5, "tasma"));
        DENETLE(k2.s.hukum == Hukum::Gecmedi && neden_var(k2.s, "kart rx_tasma=1"));
    }
    {
        HataAyar h;
        h.kopma_kp = 2000;
        auto k = kos_sahte(h, ayar(false, 921600, 1.0, "kopma"));
        DENETLE(k.s.hukum == Hukum::Gecersiz && neden_var(k.s, "asama=kopma"));
        DENETLE(k.sure_s < 10.0);
    }
    {
        HataAyar h;
        h.kosuda_sus = true;
        auto k = kos_sahte(h, ayar(true, 921600, 0.5, "sus"));
        DENETLE(k.s.hukum == Hukum::Gecersiz && neden_var(k.s, "asama=ozet_yok"));
        DENETLE(k.sure_s < 15.0);
    }
    {
        HataAyar h;
        h.kosuda_okuma_dur = true;
        h.parca_azami = 64;   // küçük soket tamponu: PC yazması tıkanır
        auto a = ayar(false, 921600, 2.0, "tikanma");
        a.yazma_ms = 300;
        auto k = kos_sahte(h, a);
        DENETLE(k.s.hukum == Hukum::Gecersiz && neden_var(k.s, "asama=yazma_hata"));
        DENETLE(k.sure_s < 15.0);
    }
    {
        HataAyar h;
        h.ret_baud = {12000000};
        auto k = kos_sahte(h, ayar(false, 12000000, 0.5, "kart-ret"));
        DENETLE(k.s.hukum == Hukum::Baslamadi && neden_var(k.s, "kur_ret"));
    }
    {
        HataAyar h;
        h.os_ret_baud = {6000000};
        auto k = kos_sahte(h, ayar(false, 6000000, 0.5, "os-ret"));
        DENETLE(k.s.hukum == Hukum::Baslamadi && neden_var(k.s, "os_hiz_ret"));
        DENETLE(oku(k.kayit).find(" donus_ok=1 ") != std::string::npos);
    }
    {
        HataAyar h;
        h.kart_hiz_degistirmez = true;
        auto k = kos_sahte(h, ayar(false, 3000000, 0.5, "esle-yok"));
        DENETLE(k.s.hukum == Hukum::Baslamadi && neden_var(k.s, "esle_yok"));
        DENETLE(oku(k.kayit).find(" donus_ok=1 ") != std::string::npos);
        // ardından aynı hatta (kart denetim hızına döndü) temiz koşu: yeniden eşleme gerçekten çalışıyor mu
    }
    {
        auto a = ayar(true, 921600, 0.5, "kapasite-echo");   // 256 B × 400/s = 110 400 B/s > 92 160 B/s
        a.yuk = 256;
        a.echo_hiz = 400;
        auto k = kos_sahte(HataAyar{}, a);
        DENETLE(k.s.hukum == Hukum::Gecmedi && neden_var(k.s, "koşturulmadı") && k.sure_s < 1.0);
        auto v = coz(oku(k.kayit));
        alan_degistir(v, "B", "aralik_ns", [](const std::string &) { return "100000000"; });
        DENETLE(hk(imzala(v)) == Hukum::Gecersiz);   // kapasite dendi ama ayar aşmıyor: tutarsız kayıt
    }
    {
        // %80 sabit: daha yüksek oran koşturulmaz, kayıt da oluşmaz
        auto a = ayar(false, 921600, 0.5, "oran-090");
        a.akis_oran = 0.9;
        auto k = kos_sahte(HataAyar{}, a);
        DENETLE(k.s.hukum == Hukum::Gecersiz && ::access(a.kayit.c_str(), F_OK) != 0);
    }
    {
        // deneysel düşük oran: geçebilir ama profil deneysel; kampanya profili istenirse GECMEDI
        auto a = ayar(false, 921600, 0.5, "deneysel-akis");
        a.akis_oran = 0.5;
        auto k = kos_sahte(HataAyar{}, a);
        DENETLE(k.s.hukum == Hukum::Gecti && k.s.profil == "deneysel");
        AnalizBeklenti b;
        b.kampanya = true;
        const auto s2 = analiz_dosya(k.kayit, b);
        DENETLE(s2.hukum == Hukum::Gecmedi && neden_var(s2, "kampanya profili"));
        DENETLE(analiz_dosya(g_temiz_akis, b).hukum == Hukum::Gecti);   // mp3-akis-v1
        auto v = coz(oku(k.kayit));
        alan_degistir(v, "B", "profil", [](const std::string &) { return "mp3-akis-v1"; });
        DENETLE(hk(imzala(v)) == Hukum::Gecersiz);                      // profil adı ayarla tutmuyor
    }
    {
        // sol bulgu 4: her yazma aralığın iki katından uzun sürer; sayaçlar eşleşse de takvim aşımı GECMEDI
        HataAyar h;
        h.pc_yazma_gecikme_us = 8000;   // 921600'de akış aralığı ≈ 3,74 ms
        h.pc_yazma_gecikme_sonra = 6;   // denetim yazmaları (SORGU/KUR/ESLE) etkilenmesin
        auto a = ayar(false, 921600, 0.5, "yavas-gonderici");
        auto k = kos_sahte(h, a);
        DENETLE(k.s.hukum == Hukum::Gecmedi && neden_var(k.s, "PC takvim gecikmesi") && neden_var(k.s, "PC son yazma"));
        DENETLE(!neden_var(k.s, "rx_crc") && !neden_var(k.s, "alınan"));   // sayaçlar eşleşiyor; tek neden takvim
    }
    {
        // sol Y4: gerçek bekleyen kuyruğu (kart çıkışı 50 ms gecikir; 200 istek/s -> ~10 bekleyen): geçerli koşu reddedilmez
        HataAyar h;
        h.kp_gecikme_us = 50000;
        auto a = ayar(true, 2000000, 1.0, "echo-gecikme-50ms");
        a.zaman_asimi_ms = 1000;
        auto k = kos_sahte(h, a);
        const uint64_t az = alan_u(coz(oku(k.kayit)), "P", "azami_bekleyen_gozlenen");
        std::cout << "    gözlenen azami bekleyen " << az << std::endl;
        DENETLE(k.s.hukum == Hukum::Gecti && az >= 10 && az <= 14);
        // 400 ms gecikme: bekleyen 64'e ulaşır, istekler atlanır -> GECMEDI (kuyruk sınırı), kayıt tutarlı
        HataAyar h2;
        h2.kp_gecikme_us = 400000;
        auto a2 = ayar(true, 2000000, 1.0, "echo-gecikme-400ms");
        a2.zaman_asimi_ms = 1000;
        auto k2 = kos_sahte(h2, a2);
        const uint64_t az2 = alan_u(coz(oku(k2.kayit)), "P", "azami_bekleyen_gozlenen");
        std::cout << "    gözlenen azami bekleyen " << az2 << std::endl;
        DENETLE(k2.s.hukum == Hukum::Gecmedi && neden_var(k2.s, "kuyruk sınırı") && az2 == 64);
    }
    {
        // ön tarama: hepsi geçerse 0; bir aday kartça reddedilirse 1 (tarama tamamlandı)
        std::vector<TaramaSatiri> t1, t2;
        uint32_t no = 0x7000;
        Ayar taban = ayar(false, 0, 0, "tara");
        taban.kayit.clear();
        {
            SahteHat hat(HataAyar{});
            const std::string d = g_dizin + "/tara1";
            ::mkdir(d.c_str(), 0755);
            DENETLE(tara(hat.port(), {921600, 2000000}, 0.3, 0.3, d, [&] { return ++no; }, t1, taban) == 0);
            DENETLE(t1.size() == 2 && t1[1].akis == Hukum::Gecti && t1[1].echo == Hukum::Gecti);
        }
        {
            HataAyar h;
            h.ret_baud = {2000000};
            SahteHat hat(h);
            const std::string d = g_dizin + "/tara2";
            ::mkdir(d.c_str(), 0755);
            DENETLE(tara(hat.port(), {921600, 2000000}, 0.3, 0.3, d, [&] { return ++no; }, t2, taban) == 1);
            DENETLE(t2.size() == 2 && t2[0].akis == Hukum::Gecti && t2[1].akis == Hukum::Baslamadi);
        }
    }
    {
        AnalizBeklenti b;
        b.en_az_sn = 5.0;
        DENETLE(analiz_dosya(g_temiz_akis, b).hukum == Hukum::Gecmedi);
        b.en_az_sn = 0.9;
        DENETLE(analiz_dosya(g_temiz_akis, b).hukum == Hukum::Gecti);
        DENETLE(analiz_dosya(g_dizin + "/yok.kayit", b).hukum == Hukum::Gecersiz);
    }
    {
        // var olan kayıt ezilmez
        auto a = ayar(false, 921600, 0.5, "ezme");
        a.kayit = g_temiz_akis;
        const std::string once = oku(g_temiz_akis);
        auto k = kos_sahte(HataAyar{}, a);
        DENETLE(k.s.hukum == Hukum::Gecersiz && oku(g_temiz_akis) == once);
    }
}

// sol Y2 (temsil aralığı, taşma) ve Y3 (akış kronolojisi); sol'un çalıştırdığı biçimde kampanya + süre beklentisiyle
void y2_y3_negatif(const std::string &A)
{
    AnalizBeklenti kb;
    kb.kampanya = true;
    kb.en_az_sn = 0.9;
    DENETLE(analiz_metin(A, kb).hukum == Hukum::Gecti);
    const std::string U64 = "18446744073709551615", U64_1 = "18446744073709551614", I64 = "9223372036854775807",
                      I64P = "9223372036854775808", U32 = "4294967295", U32P = "4294967296";
    struct D { std::string tur, ad, deger; bool tam_gecersiz; };
    const std::vector<D> denemeler = {
        {"P", "tx_gec_azami_ns", U64, true}, {"P", "tx_gec_azami_ns", U64_1, true}, {"P", "tx_gec_azami_ns", I64P, true},
        {"P", "tx_gec_azami_ns", I64, false}, {"P", "t_son_yaz_ns", U64, true}, {"P", "t_son_yaz_ns", I64P, true},
        {"P", "t_son_yaz_ns", I64, true}, {"K", "tx_gec_azami_us", U64, true}, {"K", "tx_gec_azami_us", U32P, true},
        {"K", "tx_gec_azami_us", U32, false}, {"K", "tx_son_us", U64, true}, {"K", "tx_son_us", U32P, true},
        {"K", "tx_son_us", U32, true}, {"P", "rx_alinan", I64, true}, {"P", "rx_okunan_bayt", U64, true},
        {"B", "pc_adet", U32P, true}, {"B", "aralik_ns", U64, true}, {"K", "sure_us", U32P, true},
        {"P", "yazma_azami_ns", I64, false}, {"P", "t_son_plan_ns", I64, true}};
    for (const auto &d : denemeler) {
        auto v = coz(A);
        alan_degistir(v, d.tur, d.ad, [x = d.deger](const std::string &) { return x; });
        const auto s2 = analiz_metin(imzala(v), kb);
        const bool ok = d.tam_gecersiz ? s2.hukum == Hukum::Gecersiz : s2.hukum != Hukum::Gecti;
        if (!ok) std::cout << "  beklenmeyen: " << d.tur << "." << d.ad << "=" << d.deger << " -> " << hukum_ad(s2.hukum) << std::endl;
        DENETLE(ok);
    }
    // Y3: imkânsız kronoloji
    const auto v0 = coz(A);
    const uint64_t aralik = alan_u(v0, "B", "aralik_ns"), ktx = alan_u(v0, "K", "tx_cerceve"),
                   kgec = alan_u(v0, "K", "tx_gec_azami_us");
    const uint64_t plan_us = (ktx - 1) * aralik / 1000;
    for (const auto &deger : {std::string("0"), std::to_string(plan_us - 1), std::to_string(plan_us + kgec + 1)}) {
        auto v = v0;
        alan_degistir(v, "K", "tx_son_us", [deger](const std::string &) { return deger; });
        DENETLE(analiz_metin(imzala(v), kb).hukum == Hukum::Gecersiz);
    }
    {
        auto v = v0;   // sınırlar dahil: plan ve plan + azami gecikme kabul
        alan_degistir(v, "K", "tx_son_us", [&](const std::string &) { return std::to_string(plan_us); });
        DENETLE(analiz_metin(imzala(v), kb).hukum == Hukum::Gecti);
        alan_degistir(v, "K", "tx_son_us", [&](const std::string &) { return std::to_string(plan_us + kgec); });
        DENETLE(analiz_metin(imzala(v), kb).hukum == Hukum::Gecti);
    }
    {
        auto v = v0;   // P son yazma = 0 ya da son plandan önce
        alan_degistir(v, "P", "t_son_yaz_ns", [](const std::string &) { return "0"; });
        DENETLE(analiz_metin(imzala(v), AnalizBeklenti{}).hukum == Hukum::Gecersiz);
        auto w = v0;   // P ve K son zamanlarını birlikte sıfırlamak (süre beklentisi yok)
        alan_degistir(w, "P", "t_son_yaz_ns", [](const std::string &) { return "0"; });
        alan_degistir(w, "K", "tx_son_us", [](const std::string &) { return "0"; });
        DENETLE(analiz_metin(imzala(w), AnalizBeklenti{}).hukum == Hukum::Gecersiz);
        auto z = v0;   // kart son gönderimi koşu süresinden sonra
        alan_degistir(z, "K", "sure_us", [&](const std::string &) { return std::to_string(plan_us); });
        alan_degistir(z, "K", "tx_son_us", [&](const std::string &) { return std::to_string(plan_us + 1); });
        DENETLE(analiz_metin(imzala(z), AnalizBeklenti{}).hukum == Hukum::Gecersiz);
    }
}

// sol Y4: bekleyen kuyruğu R'den yeniden üretilir
void y4_negatif(const std::string &E)
{
    const auto v0 = coz(E);
    DENETLE(alan_u(v0, "P", "azami_bekleyen_gozlenen") >= 1);
    {
        auto v = v0;   // sol'un karşı örneği: yanıt = max(eski, 0,5 s + sıra × 1 ms), zaman aşımı 1 s: 100 bekleyen
        alan_degistir(v, "B", "zaman_asimi_ms", [](const std::string &) { return "1000"; });
        DENETLE(analiz_metin(imzala(v), AnalizBeklenti{}).hukum == Hukum::Gecti);   // yalnız zaman aşımı değişimi geçerli
        int64_t son = 0;
        for (auto &r : v)
            if (r.tur == "R") {
                const int64_t y = std::max<int64_t>(std::stoll(r.alan[4]), 500000000 + std::stoll(r.alan[0]) * 1000000);
                r.alan[4] = std::to_string(y);
                son = std::max(son, y);
            }
        alan_degistir(v, "P", "t_son_rx_ns", [son](const std::string &) { return std::to_string(son); });
        const auto s2 = analiz_metin(imzala(v), AnalizBeklenti{});
        DENETLE(s2.hukum == Hukum::Gecersiz && (neden_var(s2, "yeniden üretilen azami bekleyen") || neden_var(s2, "R'den yeniden üretilen")));
    }
    for (const auto &deger : {std::string("0"), std::string("65")}) {
        auto v = v0;
        alan_degistir(v, "P", "azami_bekleyen_gozlenen", [deger](const std::string &) { return deger; });
        DENETLE(hk(imzala(v)) == Hukum::Gecersiz);
    }
}

void kayit_negatif()
{
    std::cout << "[kayıt negatif]" << std::endl;
    const std::string A = oku(g_temiz_akis), E = oku(g_temiz_echo);
    DENETLE(hk(A) == Hukum::Gecti && hk(E) == Hukum::Gecti);
    DENETLE(hk(imzala(coz(A))) == Hukum::Gecti);   // yeniden imzalama aracı doğru
    DENETLE(hk(A.substr(0, A.size() - 1)) == Hukum::Gecersiz);                       // son satır sonu yok
    DENETLE(hk(A.substr(0, A.rfind("S ", A.size() - 2))) == Hukum::Gecersiz);       // S yok (kesik)
    DENETLE(hk(A.substr(0, A.size() / 2)) == Hukum::Gecersiz);                       // ortadan kesik
    DENETLE(hk("") == Hukum::Gecersiz);
    {
        std::string b = A;
        b[b.size() / 3] = static_cast<char>(b[b.size() / 3] == '1' ? '2' : '1');   // CRC tutmaz
        DENETLE(hk(b) == Hukum::Gecersiz);
    }
    DENETLE(hk(A + satir_imzala("S 99 satir=99") + "\n") == Hukum::Gecersiz);       // S'den sonra veri
    {
        auto v = coz(A);
        std::vector<Satir> w;
        for (auto &s : v)
            if (s.tur != "K") w.push_back(s);
        DENETLE(hk(imzala(w)) == Hukum::Gecersiz);                                   // özet yok
        alan_degistir(w, "P", "asama", [](const std::string &) { return "ozet_yok"; });
        DENETLE(hk(imzala(w)) == Hukum::Gecersiz);
    }
    {
        auto v = coz(A);
        alan_degistir(v, "K", "kosu", [](const std::string &x) { return std::to_string(std::stoull(x) + 1); });
        DENETLE(hk(imzala(v)) == Hukum::Gecersiz);                                   // yanlış koşu kimliği (kart)
        auto w = coz(A);
        alan_degistir(w, "P", "kosu", [](const std::string &x) { return std::to_string(std::stoull(x) + 1); });
        DENETLE(hk(imzala(w)) == Hukum::Gecersiz);                                   // yanlış koşu kimliği (PC)
    }
    const auto artir = [](uint64_t d) { return [d](const std::string &x) { return std::to_string(std::stoull(x) + d); }; };
    const auto azalt = [](uint64_t d) { return [d](const std::string &x) { return std::to_string(std::stoull(x) - d); }; };
    for (const char *k : {"rx_crc_hata", "rx_eksik", "uart_fe", "uart_ore", "dma_rx_hata", "rx_tasma_bayt", "tx_eksik"}) {
        auto v = coz(A);
        alan_degistir(v, "K", k, [](const std::string &) { return "1"; });
        DENETLE(hk(imzala(v)) == Hukum::Gecmedi);
    }
    {
        auto v = coz(A);   // tekrar: tutarlı sahte (rx_iyi de artar) GECMEDI, tutarsız GECERSIZ
        alan_degistir(v, "K", "rx_tekrar", [](const std::string &) { return "1"; });
        DENETLE(hk(imzala(v)) == Hukum::Gecersiz);
        alan_degistir(v, "K", "rx_iyi", artir(1));
        DENETLE(hk(imzala(v)) == Hukum::Gecmedi);
    }
    for (const char *k : {"rx_crc_hata", "rx_sirasiz", "kopma"}) {
        auto v = coz(A);
        alan_degistir(v, "P", k, [](const std::string &) { return "1"; });
        DENETLE(hk(imzala(v)) == Hukum::Gecmedi);
    }
    {
        auto v = coz(A);
        alan_degistir(v, "P", "rx_icerik_hata", [](const std::string &) { return "1"; });
        DENETLE(hk(imzala(v)) == Hukum::Gecersiz);
        alan_degistir(v, "P", "rx_iyi", artir(1));
        DENETLE(hk(imzala(v)) == Hukum::Gecmedi);
    }
    {
        auto v = coz(A);
        alan_degistir(v, "P", "rx_alinan", azalt(1));
        DENETLE(hk(imzala(v)) == Hukum::Gecersiz);                                   // iç muhasebe tutmuyor
        alan_degistir(v, "P", "rx_iyi", azalt(1));
        alan_degistir(v, "P", "rx_bayt", azalt(276));
        alan_degistir(v, "P", "ozet_sonrasi_bayt", artir(276));
        DENETLE(hk(imzala(v)) == Hukum::Gecmedi);                                    // tutarlı sahte: iki uç eşleşmiyor
        auto w = coz(A);
        alan_degistir(w, "K", "rx_beklenen", [](const std::string &x) { return std::to_string(std::stoull(x) + 1); });
        DENETLE(hk(imzala(w)) == Hukum::Gecmedi);
        auto z = coz(A);
        alan_degistir(z, "P", "donus_ok", [](const std::string &) { return "0"; });
        DENETLE(hk(imzala(z)) == Hukum::Gecmedi);
    }
    {
        auto v = coz(A);
        alan_degistir(v, "K", "rx_crc_hata", [](const std::string &) { return "x1"; });
        DENETLE(hk(imzala(v)) == Hukum::Gecersiz);                                   // sayı değil
        auto w = coz(A);
        for (auto &s : w)
            if (s.tur == "B") s.alan.push_back("baud=1");
        DENETLE(hk(imzala(w)) == Hukum::Gecersiz);                                   // yinelenen alan
        auto z = coz(A);
        for (auto &s : z)
            if (s.tur == "K") s.alan.pop_back();
        DENETLE(hk(imzala(z)) == Hukum::Gecersiz);                                   // eksik alan
    }
    // sol bulgu 6: hız kabulü, OS sonucu, BRR ve bayt/çerçeve muhasebesi
    for (const auto &[tur, ad, deger] : std::vector<std::tuple<std::string, std::string, std::string>>{
             {"B", "kart_kabul", "0"}, {"B", "os_ioctl", "0"}, {"B", "os_tcsetattr", "0"}, {"B", "kart_brr", "1"},
             {"B", "kart_over8", "1"}, {"B", "kart_usart_hz", "80000000"}, {"B", "os_istenen", "115200"}, {"B", "os_geri_o", "9600"},
             {"B", "aralik_ns", "3743491"}, {"B", "oran_ppm", "700000"}, {"P", "rx_iyi", "0"}, {"K", "rx_iyi", "0"},
             {"P", "rx_bayt", "0"}, {"K", "rx_bayt", "0"}, {"K", "rx_kontrol", "0"}, {"P", "rx_okunan_bayt", "1"}}) {
        auto v = coz(A);
        alan_degistir(v, tur, ad, [d = deger](const std::string &) { return d; });
        const auto s2 = analiz_metin(imzala(v), AnalizBeklenti{});
        if (s2.hukum != Hukum::Gecersiz) std::cout << "  beklenmeyen: " << tur << "." << ad << "=" << deger << " -> " << hukum_ad(s2.hukum) << std::endl;
        DENETLE(s2.hukum == Hukum::Gecersiz);
    }
    {
        auto v = coz(A);   // kart_hesap_baud ve pc_hesap_baud birlikte değişse de BRR geri hesabı yakalar
        alan_degistir(v, "B", "kart_hesap_baud", artir(1000));
        alan_degistir(v, "B", "pc_hesap_baud", artir(1000));
        DENETLE(hk(imzala(v)) == Hukum::Gecersiz);
        auto w = coz(A);   // rx_iyi = 0, rx_bayt = 0 iki uçta birlikte
        alan_degistir(w, "P", "rx_iyi", [](const std::string &) { return "0"; });
        alan_degistir(w, "P", "rx_bayt", [](const std::string &) { return "0"; });
        alan_degistir(w, "K", "rx_iyi", [](const std::string &) { return "0"; });
        alan_degistir(w, "K", "rx_bayt", [](const std::string &) { return "0"; });
        DENETLE(hk(imzala(w)) == Hukum::Gecersiz);
    }
    // sol bulgu 4: takvim alanları (akış); kronolojiyle tutarlı aşım GECMEDI
    for (const auto &[tur, ad, deger] : std::vector<std::tuple<std::string, std::string, std::string>>{
             {"P", "tx_gec_azami_ns", "10000001"}, {"K", "tx_gec_azami_us", "10001"}, {"P", "takvim_kesildi", "1"}}) {
        auto v = coz(A);
        alan_degistir(v, tur, ad, [d = deger](const std::string &) { return d; });
        const auto s2 = analiz_metin(imzala(v), AnalizBeklenti{});
        DENETLE(s2.hukum == Hukum::Gecmedi);
    }
    {
        auto v = coz(A);   // geç bitiş, gecikme alanıyla tutarlı: GECMEDI
        alan_degistir(v, "P", "t_son_yaz_ns", [](const std::string &) { return "2000000000"; });
        alan_degistir(v, "P", "tx_gec_azami_ns", [](const std::string &) { return "2000000000"; });
        DENETLE(hk(imzala(v)) == Hukum::Gecmedi);
        auto w = coz(A);   // yalnız son yazma büyütülürse kronoloji imkânsız: GECERSIZ
        alan_degistir(w, "P", "t_son_yaz_ns", [](const std::string &) { return "2000000000"; });
        DENETLE(hk(imzala(w)) == Hukum::Gecersiz);
    }
    y2_y3_negatif(A);
    {
        auto v = coz(A);   // kartın gönderdiği veri baytı PC'nin aldığından fazla (tutarlı sahte; kronoloji de uyarlanır)
        alan_degistir(v, "K", "tx_cerceve", artir(1));
        alan_degistir(v, "K", "tx_bayt", artir(276));
        DENETLE(hk(imzala(v)) == Hukum::Gecersiz);   // ek çerçevenin planı kartın son gönderim zamanından sonra
        const uint64_t plan_us = alan_u(v, "K", "tx_cerceve") > 0
                                     ? (alan_u(v, "K", "tx_cerceve") - 1) * alan_u(v, "B", "aralik_ns") / 1000 : 0;
        alan_degistir(v, "K", "tx_son_us", [plan_us](const std::string &) { return std::to_string(plan_us); });
        alan_degistir(v, "K", "sure_us", [plan_us](const std::string &x) { return std::to_string(std::max<uint64_t>(std::stoull(x), plan_us)); });
        DENETLE(hk(imzala(v)) == Hukum::Gecmedi);
    }
    y4_negatif(E);
    // echo
    {
        auto v = coz(E);
        std::vector<Satir> w;
        for (auto &s : v) {
            w.push_back(s);
            if (s.tur == "R" && s.alan[0] == "5") w.push_back(s);                    // yinelenen R
        }
        DENETLE(hk(imzala(w)) == Hukum::Gecersiz);
    }
    {
        auto v = coz(E);
        std::vector<Satir> w;
        for (auto &s : v)
            if (!(s.tur == "R" && s.alan[0] == "7")) w.push_back(s);                 // eksik R
        DENETLE(hk(imzala(w)) == Hukum::Gecersiz);
    }
    {
        auto v = coz(E);   // sol bulgu 5: bütün R zamanları 0
        for (auto &s : v)
            if (s.tur == "R") s.alan[1] = s.alan[2] = s.alan[3] = s.alan[4] = "0";
        DENETLE(hk(imzala(v)) == Hukum::Gecersiz);
        auto w = coz(E);   // R yazma zamanları P özetiyle tutmuyor (son yazma)
        for (auto &s : w)
            if (s.tur == "R" && s.alan[0] == "199") s.alan[3] = std::to_string(std::stoll(s.alan[3]) + 1);
        DENETLE(hk(imzala(w)) == Hukum::Gecersiz);
        auto z = coz(E);   // P takvim gecikmesi R ile tutmuyor
        alan_degistir(z, "P", "tx_gec_azami_ns", artir(1));
        DENETLE(hk(imzala(z)) == Hukum::Gecersiz);
    }
    {
        auto v = coz(E);
        for (auto &s : v)
            if (s.tur == "R" && s.alan[0] == "9") s.alan[4] = "-";                   // yanıt yok
        DENETLE(hk(imzala(v)) == Hukum::Gecersiz);                                   // P ile tutmuyor
        alan_degistir(v, "P", "rx_alinan", azalt(1));
        alan_degistir(v, "P", "rx_iyi", azalt(1));
        alan_degistir(v, "P", "rx_bayt", azalt(84));
        alan_degistir(v, "P", "ozet_sonrasi_bayt", artir(84));
        DENETLE(hk(imzala(v)) == Hukum::Gecersiz);   // Y4: yanıtı silinen istek 300 ms bekler; P azami bekleyen düşük kalır
        alan_degistir(v, "P", "azami_bekleyen_gozlenen", [](const std::string &) { return "64"; });
        const auto s2 = analiz_metin(imzala(v), AnalizBeklenti{});
        DENETLE(s2.hukum == Hukum::Gecmedi && neden_var(s2, "zaman aşımı"));        // tutarlı sahte: kayıp yanıt
    }
    {
        auto v = coz(E);
        for (auto &s : v)
            if (s.tur == "R" && s.alan[0] == "11") s.alan[4] = std::to_string(std::stoll(s.alan[3]) + 400000000);   // geç yanıt
        DENETLE(hk(imzala(v)) == Hukum::Gecersiz);   // Y4: P azami bekleyen zaman çizelgesiyle tutmuyor
        alan_degistir(v, "P", "azami_bekleyen_gozlenen", [](const std::string &) { return "64"; });
        const auto s = analiz_metin(imzala(v), AnalizBeklenti{});
        DENETLE(s.hukum == Hukum::Gecmedi && neden_var(s, "zaman aşımı"));
    }
    {
        auto v = coz(E);
        for (auto &s : v)
            if (s.tur == "R" && s.alan[0] == "12") s.alan[4] = std::to_string(std::stoll(s.alan[2]) - 1);   // yanıt yazmadan önce
        DENETLE(hk(imzala(v)) == Hukum::Gecersiz);
    }
    {
        auto v = coz(E);
        for (auto &s : v)
            if (s.tur == "R" && s.alan[0] == "13") { s.alan[2] = "-"; s.alan[3] = "-"; s.alan[4] = "-"; }   // atlandı
        alan_degistir(v, "P", "rx_alinan", azalt(1));
        alan_degistir(v, "P", "tx_atlanan", [](const std::string &) { return "1"; });
        DENETLE(hk(imzala(v)) != Hukum::Gecti);
    }
    {
        auto v = coz(E);
        std::vector<Satir> w;
        for (auto &s : v) {
            if (s.tur == "P") {   // R satırı P'den sonra
                w.push_back(s);
                for (auto &r : v)
                    if (r.tur == "R" && r.alan[0] == "0") w.push_back(r);
                continue;
            }
            if (!(s.tur == "R" && s.alan[0] == "0")) w.push_back(s);
        }
        DENETLE(hk(imzala(w)) == Hukum::Gecersiz);
    }
}

}  // namespace

int main()
{
    std::signal(SIGPIPE, SIG_IGN);   // kopan hatta yazma EPIPE dönsün, süreç ölmesin
    mp3_hazirla();
    char sablon[] = "/tmp/mp3-test-XXXXXX";
    if (!mkdtemp(sablon)) { std::perror("mkdtemp"); return 2; }
    g_dizin = sablon;
    birim();
    uctan_uca();
    kayit_negatif();
    std::cout << "denetim: " << g_denetim << ", başarısız: " << g_hata << " (kayıtlar: " << g_dizin << ")" << std::endl;
    return g_hata == 0 ? 0 : 1;
}
