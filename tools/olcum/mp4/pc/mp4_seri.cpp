// W0-MP4a ölçüm prototipi: PC seri kayıt ve çözümleme aracı (atılabilir; yalnız POSIX/macOS).
//
//   mp4_seri kayit <port> <azami_s> <çıktı.csv>
//       Portu 115200 8N1 ham kipte açar; her satır varış anıyla CSV'ye yazılır (mono_ns,satır). Kartın
//       kampanya sonu satırı ("T ...") gelince biter ve 0 döner. azami_s içinde gelmezse 4 döner (eksik koşu).
//       "D" dışındaki satırlar ekrana da basılır.
//   mp4_seri analiz <kayıt.csv> --kimlik K --beklenen-hz HZ --beklenen-fazlar MASKE --beklenen-faz-adim N
//                   --yigin-azami B [--hist önek] [--en-az-adim N] [--en-az-sn S]
//       --yigin-azami: ölçülen imajın boyanabilen yığın bölgesi, _estack − (_ebss + 256) bayt; koşu betiği ELF'in
//       sembollerinden hesaplar (kayıttan türetilmez). yigin_tepe bunu aşamaz; yigin_tepe > yigin_kayit olmalı.
//       Histogram CSV'leri (--hist) yalnız BÜTÜN denetimler (T dahil) geçerse yazılır; var olan dosya ezilmez
//       (o durumda hiçbir dosya yazılmaz, çıkış 3).
//       Kampanya, koşunun gerçek ayarları açıkça verilerek doğrulanır (beklenen hız, faz maskesi, faz başına adım;
//       açılış satırındaki değerler bunlarla aynı olmalı, maske boş olamaz). Herhangi bir denetim tutmazsa çıkış
//       kodu 1 ve sayısal özet BASILMAZ. Denetimler: kimliği K olan ilk açılıştan itibaren her satırın CRC-16'sı,
//       sıra sayacının kesintisizliği, ikinci açılış / X satırı yokluğu; açılış başlığının değer ve tutarlılığı;
//       tek ve eksiksiz B2 (saat 160 MHz, saat kurulumu ve UART/FIFO başarılı, FPU, ICACHE, flash gecikmesi); K
//       satırları (TIM2 periyodu DWT ile); her satır türü için kesin şema (zorunlu alanlar, yalnız rakamdan oluşan
//       taşmasız sayılar, tanımsız alan yok); her beklenen fazın F, G, Q, S, E satırlarının tam bir kez ve
//       histogram satırlarının E'deki sayıyla bulunması; n = faz_adim; adım ve gecikme histogramlarının toplamı,
//       en küçük/en büyük kutusu ve toplam süreyle tutarlılığı; aşım/atlanan/temiz gecikme/kesme girişi/parça
//       süresi sayaçlarının sınırları; T satırı (toplam adım, faz maskesi). --en-az-adim/--en-az-sn ile her fazın
//       kayıtlı adım sayısı ve kayıt süresinin MUHAFAZAKÂR ALT SINIRI (duvar_cyc: ilk adımın başından son adımın
//       başına; son adımın süresi dahil değil) denetlenir.
//       Yüzdelikler ham histogramdan tam sayı aritmetiğiyle hesaplanır: sıralı ⌈q·n⌉'inci örneğin kutusunun
//       kapsayıcı üst kenarı, en büyük değerle kırpılır. Değerler çevrim/160 olarak beş ondalıkla KESİN basılır
//       (yuvarlama yok). KAPSAM: adım süresi ya da gecikme histogramında taşma (F.tasan > 0, G.lat_tasan > 0)
//       içeren kampanya desteklenmez ve geçersiz sayılır (mimar kararı, docs/gorevler/W0-MP4a-kapsam-daraltma.md).
//   mp4_seri imza <metin>
//       Metnin sonuna kartın satır CRC'sini ekleyip basar (sınama verisi üretmek için).
//
// Çevrim -> µs dönüşümü 160 MHz varsayar; kartın saati açılış satırında (hesap_hz) ayrıca bildirilir.

#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr double CYC_US = 160.0;  // 160 MHz

int64_t ns_mono()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

int kayit(const char *port, double azami_s, const char *cikti)
{
    int fd = ::open(port, O_RDONLY | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) { std::perror("open"); return 2; }
    termios t{};
    if (tcgetattr(fd, &t) != 0) { std::perror("tcgetattr"); return 2; }
    cfmakeraw(&t);
    cfsetispeed(&t, B115200);
    cfsetospeed(&t, B115200);
    t.c_cflag |= (CLOCAL | CREAD);
    t.c_cflag &= ~static_cast<tcflag_t>(CSTOPB | PARENB | CRTSCTS);
    if (tcsetattr(fd, TCSANOW, &t) != 0) { std::perror("tcsetattr"); return 2; }
    tcflush(fd, TCIFLUSH);
    std::ofstream out(cikti);
    if (!out) { std::cerr << "çıktı açılamadı: " << cikti << "\n"; return 2; }
    out << "mono_ns,satir\n";
    const int64_t bitis = ns_mono() + static_cast<int64_t>(azami_s * 1e9);
    std::string satir;
    char buf[512];
    long n_satir = 0;
    bool son_geldi = false;
    while (!son_geldi && ns_mono() < bitis) {
        fd_set rs;
        FD_ZERO(&rs);
        FD_SET(fd, &rs);
        timeval tv{0, 200000};
        int r = ::select(fd + 1, &rs, nullptr, nullptr, &tv);
        if (r < 0) { std::perror("select"); return 3; }
        if (r == 0) continue;
        ssize_t n = ::read(fd, buf, sizeof buf);
        const int64_t tm = ns_mono();
        if (n < 0) { std::perror("read"); return 3; }
        if (n == 0) { std::cerr << "port kapandı\n"; return 3; }
        for (ssize_t i = 0; i < n; ++i) {
            char c = buf[i];
            if (c == '\r') continue;
            if (c == '\n') {
                out << tm << ',' << satir << '\n';
                if (satir.empty() || satir[0] != 'D') std::cout << satir << std::endl;
                ++n_satir;
                if (satir.rfind("T ", 0) == 0) son_geldi = true;
                satir.clear();
            } else if (satir.size() < 2048) {
                satir.push_back(c);
            }
        }
        out.flush();
    }
    ::close(fd);
    if (!son_geldi) {
        std::cout << "kayıt EKSİK: " << azami_s << " s içinde kampanya sonu (T) satırı gelmedi; " << n_satir
                  << " satır, " << cikti << std::endl;
        return 4;
    }
    std::cout << "kayıt bitti (T satırı geldi): " << n_satir << " satır, " << cikti << std::endl;
    return 0;
}

uint16_t crc16(const std::string &s)
{
    uint16_t crc = 0xFFFF;
    for (char ch : s) {
        const auto b = static_cast<unsigned char>(ch);
        uint8_t t = static_cast<uint8_t>(b ^ static_cast<uint8_t>(crc & 0xFF));
        t = static_cast<uint8_t>(t ^ static_cast<uint8_t>(t << 4));
        crc = static_cast<uint16_t>((crc >> 8) ^ (static_cast<uint16_t>(t) << 8) ^ (static_cast<uint16_t>(t) << 3) ^
                                    static_cast<uint16_t>(t >> 4));
    }
    return crc;
}

struct Satir {
    std::string tip;
    uint64_t sira = 0;
    std::map<std::string, std::string> kv;
    std::vector<std::string> ciplak;  // "=" içermeyen sözcükler (histogram satırlarındaki kutu:sayı)
    std::string govde;
};

// Kesin ondalık sayı: yalnız rakam (işaret, boşluk, ondalık, çöp yok), uint64 taşması yok.
std::optional<uint64_t> kesin_sayi(const std::string &t)
{
    if (t.empty() || t.size() > 20) return std::nullopt;
    uint64_t v = 0;
    for (char c : t) {
        if (c < '0' || c > '9') return std::nullopt;
        const auto d = static_cast<uint64_t>(c - '0');
        if (v > (UINT64_MAX - d) / 10) return std::nullopt;
        v = v * 10 + d;
    }
    return v;
}

// Kesin "0x" + 1..8 büyük harfli onaltılık hane (kart 8 hane basar).
std::optional<uint32_t> kesin_hex32(const std::string &t)
{
    if (t.size() < 3 || t.size() > 10 || t[0] != '0' || t[1] != 'x') return std::nullopt;
    uint32_t v = 0;
    for (size_t i = 2; i < t.size(); ++i) {
        const char c = t[i];
        uint32_t d;
        if (c >= '0' && c <= '9') d = static_cast<uint32_t>(c - '0');
        else if (c >= 'A' && c <= 'F') d = static_cast<uint32_t>(c - 'A' + 10);
        else return std::nullopt;
        v = (v << 4) | d;
    }
    return v;
}

// 0: tamam, 1: biçim hatası, 2: CRC hatası
int coz(const std::string &ham, Satir &s)
{
    const auto yildiz = ham.rfind(" *");
    if (yildiz == std::string::npos || ham.size() != yildiz + 6) return 1;
    const std::string govde = ham.substr(0, yildiz);
    uint32_t c = 0;
    for (size_t i = yildiz + 2; i < ham.size(); ++i) {
        const char h = ham[i];
        uint32_t d;
        if (h >= '0' && h <= '9') d = static_cast<uint32_t>(h - '0');
        else if (h >= 'A' && h <= 'F') d = static_cast<uint32_t>(h - 'A' + 10);
        else return 1;
        c = (c << 4) | d;
    }
    if (crc16(govde) != static_cast<uint16_t>(c)) return 2;
    std::istringstream is(govde);
    std::string sira;
    if (!(is >> s.tip >> sira) || s.tip.empty() || s.tip.size() > 2) return 1;
    const auto sv = kesin_sayi(sira);
    if (!sv) return 1;
    s.sira = *sv;
    s.govde = govde;
    std::string w;
    while (is >> w) {
        const auto e = w.find('=');
        if (e == std::string::npos) s.ciplak.push_back(w);
        else if (!s.kv.emplace(w.substr(0, e), w.substr(e + 1)).second) return 1;  // yinelenen alan
    }
    return 0;
}

struct Hata {
    int n = 0;
    void operator()(const std::string &m)
    {
        ++n;
        std::cout << "HATA: " << m << "\n";
    }
};

// Bir satırın şemalı okuyucusu: zorunlu alanlar kesin ayrıştırılır, izin verilmeyen alan ya da çıplak sözcük hata.
struct Okur {
    const Satir &s;
    std::string bag;
    Hata &h;
    void hata(const std::string &m) { h(bag + ": " + s.tip + " satırı: " + m); }
    const std::string *al(const char *k)
    {
        auto it = s.kv.find(k);
        if (it == s.kv.end()) { hata(std::string("alan yok: ") + k); return nullptr; }
        return &it->second;
    }
    uint64_t u(const char *k, uint64_t ust = UINT32_MAX)
    {
        const std::string *t = al(k);
        if (!t) return 0;
        auto v = kesin_sayi(*t);
        if (!v) { hata(std::string("sayı değil ya da taşıyor: ") + k + "=" + *t); return 0; }
        if (*v > ust) { hata(std::string("aralık dışı: ") + k + "=" + *t); return 0; }
        return *v;
    }
    uint32_t x(const char *k)
    {
        const std::string *t = al(k);
        if (!t) return 0;
        auto v = kesin_hex32(*t);
        if (!v) { hata(std::string("0x + 1..8 onaltılık hane değil: ") + k + "=" + *t); return 0; }
        return *v;
    }
    std::string str(const char *k)
    {
        const std::string *t = al(k);
        return t ? *t : std::string();
    }
    void sema(std::initializer_list<const char *> izinli, bool ciplak_izinli = false)
    {
        for (const auto &[k, v] : s.kv) {
            bool ok = false;
            for (const char *i : izinli) ok |= (k == i);
            if (!ok) hata("tanımsız alan: " + k);
        }
        if (!ciplak_izinli && !s.ciplak.empty()) hata("tanımsız sözcük: " + s.ciplak.front());
    }
};

// Çevrim -> µs, kesin (160 = 2^5·5 olduğundan c/160 beş ondalıkla tam gösterilir; yuvarlama yok).
std::string us(uint64_t cyc)
{
    char b[48];
    std::snprintf(b, sizeof b, "%llu.%05llu", static_cast<unsigned long long>(cyc / 160),
                  static_cast<unsigned long long>((cyc % 160) * 625));
    return b;
}
// Çevrim -> saniye, mikrosaniye basamağında aşağı (alt sınır) ya da yukarı (üst sınır) yuvarlanmış.
std::string sn(uint64_t cyc, bool yukari)
{
    const uint64_t u = yukari ? (cyc + 159) / 160 : cyc / 160;  // µs
    char b[48];
    std::snprintf(b, sizeof b, "%llu.%06llu", static_cast<unsigned long long>(u / 1000000),
                  static_cast<unsigned long long>(u % 1000000));
    return b;
}
std::string ort_us(uint64_t top, uint64_t n)
{
    char b[48];
    std::snprintf(b, sizeof b, "%.3f", n ? static_cast<double>(top) / static_cast<double>(n) / CYC_US : 0.0);
    return b;
}

// Ham histogram: kutu -> sayı.
using Hist = std::map<uint64_t, uint64_t>;
using u128 = unsigned __int128;

// Sıralı ⌈q·n⌉'inci örneğin kutusu; aralık dışıysa nullopt. q = pay/payda (tam sayı, kayan nokta yok).
std::optional<uint64_t> yuzdelik_kutu(const Hist &h, uint64_t n, uint64_t pay, uint64_t payda)
{
    if (n == 0) return std::nullopt;
    const u128 hedef = (static_cast<u128>(n) * pay + payda - 1) / payda;
    u128 top = 0;
    for (const auto &[k, c] : h) {
        top += c;
        if (top >= hedef) return k;
    }
    return std::nullopt;
}

bool hist_oku(const Satir &s, Hist &h, uint64_t kutu_n, Okur &o)
{
    for (const auto &w : s.ciplak) {
        const auto c = w.find(':');
        if (c == std::string::npos) { o.hata("histogram sözcüğü bozuk: " + w); return false; }
        const auto k = kesin_sayi(w.substr(0, c)), v = kesin_sayi(w.substr(c + 1));
        if (!k || !v || *v == 0 || *v > UINT32_MAX) { o.hata("histogram sözcüğü bozuk: " + w); return false; }
        if (*k >= kutu_n) { o.hata("histogram kutusu aralık dışı: " + w); return false; }
        if (!h.emplace(*k, *v).second) { o.hata("histogram kutusu yinelenmiş: " + w); return false; }
    }
    if (s.ciplak.empty()) { o.hata("boş histogram satırı"); return false; }
    return true;
}

u128 toplam(const Hist &h)
{
    u128 t = 0;
    for (const auto &kv : h) t += kv.second;
    return t;
}

struct Faz {
    std::vector<const Satir *> F, G, Q, S, E;
    std::vector<const Satir *> H, L, M;
};

int analiz(int argc, char **argv)
{
    const char *yol = argv[2];
    std::string kimlik, hist_onek;
    uint64_t en_az_adim = 0, en_az_sn = 0;
    std::optional<uint64_t> b_hz, b_fazlar, b_faz_adim, yigin_azami;
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        auto deger = [&]() -> std::optional<uint64_t> {
            if (i + 1 >= argc) return std::nullopt;
            return kesin_sayi(argv[++i]);
        };
        std::optional<uint64_t> v;
        if (a == "--kimlik" && i + 1 < argc) { kimlik = argv[++i]; continue; }
        if (a == "--hist" && i + 1 < argc) { hist_onek = argv[++i]; continue; }
        if (a == "--en-az-adim" && (v = deger())) { en_az_adim = *v; continue; }
        if (a == "--en-az-sn" && (v = deger())) { en_az_sn = *v; continue; }
        if (a == "--beklenen-hz" && (v = deger())) { b_hz = v; continue; }
        if (a == "--beklenen-fazlar" && (v = deger())) { b_fazlar = v; continue; }
        if (a == "--beklenen-faz-adim" && (v = deger())) { b_faz_adim = v; continue; }
        if (a == "--yigin-azami" && (v = deger())) { yigin_azami = v; continue; }
        std::cerr << "analiz: bilinmeyen ya da bozuk argüman: " << a << "\n";
        return 2;
    }
    if (kimlik.empty() || !b_hz || !b_fazlar || !b_faz_adim || !yigin_azami || *yigin_azami == 0) {
        std::cerr << "analiz: --kimlik, --beklenen-hz, --beklenen-fazlar, --beklenen-faz-adim ve --yigin-azami zorunludur "
                     "(kampanya, koşunun gerçek ayarlarıyla ve ölçülen imajın bellek düzeniyle açıkça doğrulanır)\n";
        return 2;
    }
    std::ifstream in(yol);
    if (!in) { std::cerr << "açılamadı: " << yol << "\n"; return 2; }
    std::string l;
    std::getline(in, l);
    std::vector<std::string> satirlar;
    while (std::getline(in, l)) {
        const auto v = l.find(',');
        if (v != std::string::npos) satirlar.push_back(l.substr(v + 1));
    }
    Hata hata;
    auto gecersiz = [&]() {
        std::cout << "sonuç: GEÇERSİZ (" << hata.n << " hata; sayısal özet basılmadı)\n";
        return 1;
    };

    // Açılış: kimliği tutan ilk "B" satırı. Öncesi (önceki imajın çıktısı, port açılış kırıntısı) yok sayılır.
    long bas = -1;
    long n_b_toplam = 0;
    for (size_t i = 0; i < satirlar.size(); ++i) {
        if (satirlar[i].rfind("B ", 0) != 0) continue;
        ++n_b_toplam;
        Satir s;
        if (bas < 0 && coz(satirlar[i], s) == 0 && s.tip == "B" && s.kv.count("kimlik") && s.kv.at("kimlik") == kimlik)
            bas = static_cast<long>(i);
    }
    if (bas < 0) { hata("kimliği " + kimlik + " olan açılış satırı (B) yok"); return gecersiz(); }
    std::cout << "açılış öncesi yok sayılan satır: " << bas << "; kayıttaki toplam B satırı: " << n_b_toplam << "\n";

    std::vector<Satir> v;
    uint64_t beklenen = 0;
    long n_crc = 0, n_bicim = 0, n_bosluk = 0, n_x = 0, n_b = 0, n_b2 = 0, n_t = 0, n_sonra = 0;
    for (size_t i = static_cast<size_t>(bas); i < satirlar.size(); ++i) {
        if (n_t) { ++n_sonra; continue; }
        Satir s;
        const int r = coz(satirlar[i], s);
        if (r == 1) { ++n_bicim; hata("biçim: " + satirlar[i].substr(0, 80)); continue; }
        if (r == 2) { ++n_crc; hata("CRC: " + satirlar[i].substr(0, 80)); continue; }
        if (s.sira != beklenen) {
            ++n_bosluk;
            hata("sıra boşluğu: beklenen " + std::to_string(beklenen) + ", gelen " + std::to_string(s.sira));
        }
        beklenen = s.sira + 1;
        if (s.tip == "X") { ++n_x; hata("kart hata satırı: " + s.govde); }
        if (s.tip == "B") ++n_b;
        if (s.tip == "B2") ++n_b2;
        if (s.tip == "T") ++n_t;
        v.push_back(std::move(s));
    }
    if (n_b > 1) hata("koşu sırasında yeniden açılış: B satırı " + std::to_string(n_b));
    if (n_sonra) std::cout << "not: T satırından sonra " << n_sonra << " satır yok sayıldı\n";
    std::cout << "bütünlük: geçerli satır " << v.size() << ", son sıra " << (beklenen ? beklenen - 1 : 0)
              << ", CRC hatası " << n_crc << ", biçim hatası " << n_bicim << ", sıra boşluğu " << n_bosluk
              << ", X satırı " << n_x << ", açılış " << n_b << "\n";

    // ---- açılış başlığı (B) ve beklenen kampanya ----
    const Satir &B = v.front();
    Okur ob{B, "açılış", hata};
    ob.sema({"derleme", "kimlik", "hz", "periyot_cyc", "fazlar", "faz_adim", "gecis", "ince_kutu_cyc", "ince_kutu_n",
             "lat_ince_n", "lat_kaba_cyc", "lat_kaba_n"});
    const std::string derleme = ob.str("derleme");
    const uint64_t hz = ob.u("hz"), periyot = ob.u("periyot_cyc"), fazlar = ob.u("fazlar"), faz_adim = ob.u("faz_adim");
    const uint64_t gecis = ob.u("gecis"), ince_cyc = ob.u("ince_kutu_cyc"), ince_n = ob.u("ince_kutu_n");
    const uint64_t lat_ince_n = ob.u("lat_ince_n"), lat_kaba_cyc = ob.u("lat_kaba_cyc"), lat_kaba_n = ob.u("lat_kaba_n");
    if (hz == 0 || 160000000 % hz != 0) ob.hata("hz 160 MHz'i tam bölmüyor: " + std::to_string(hz));
    else if (periyot != 160000000 / hz) ob.hata("periyot_cyc ≠ 160 MHz / hz");
    if (fazlar < 1 || fazlar > 7) ob.hata("faz maskesi 1..7 dışında: " + std::to_string(fazlar));
    if (faz_adim == 0) ob.hata("faz_adim 0");
    if (gecis != hz) ob.hata("geçiş adım sayısı 1 s değil");
    if (ince_cyc == 0 || ince_n == 0 || lat_ince_n == 0 || lat_kaba_cyc == 0) ob.hata("histogram parametresi 0");
    else if (lat_kaba_n != (periyot + lat_kaba_cyc - 1) / lat_kaba_cyc) ob.hata("lat_kaba_n bir periyodu kapsamıyor");
    if (hz != *b_hz) ob.hata("hız " + std::to_string(hz) + " ≠ beklenen " + std::to_string(*b_hz));
    if (fazlar != *b_fazlar) ob.hata("faz maskesi " + std::to_string(fazlar) + " ≠ beklenen " + std::to_string(*b_fazlar));
    if (faz_adim != *b_faz_adim) ob.hata("faz_adim " + std::to_string(faz_adim) + " ≠ beklenen " + std::to_string(*b_faz_adim));
    if (en_az_adim && faz_adim < en_az_adim) ob.hata("faz_adim " + std::to_string(faz_adim) + " < " + std::to_string(en_az_adim));

    // ---- B2: tek, açılışın hemen ardından; saat, FPU, UART koşulları ----
    if (n_b2 != 1 || v.size() < 2 || v[1].tip != "B2") {
        hata("açılış ayrıntı satırı (B2) tam bir kez ve B'nin hemen ardından olmalı (bulunan " + std::to_string(n_b2) + ")");
    } else {
        Okur o{v[1], "açılış", hata};
        o.sema({"rcc_csr", "idcode", "saat_adim", "hesap_hz", "hal_hz", "cfgr2", "cfgr3", "flash_acr", "icache_cr",
                "pwr_svmsr", "fpscr", "cpacr", "fpccr", "usart_cr1", "uart_hata"});
        const uint32_t idcode = o.x("idcode"), flash_acr = o.x("flash_acr"), icache = o.x("icache_cr"),
                       fpscr = o.x("fpscr"), cpacr = o.x("cpacr"), cr1 = o.x("usart_cr1");
        (void)o.x("rcc_csr"); (void)o.x("cfgr2"); (void)o.x("cfgr3"); (void)o.x("pwr_svmsr"); (void)o.x("fpccr");
        if (o.u("saat_adim") != 0) o.hata("saat kurulumu başarısız (saat_adim ≠ 0)");
        if (o.u("hesap_hz") != 160000000) o.hata("PLL yazmaçlarından hesaplanan saat 160 MHz değil");
        if (o.u("hal_hz") != 160000000) o.hata("HAL saati 160 MHz değil");
        if (o.u("uart_hata") != 0) o.hata("UART/FIFO kurulumu başarısız");
        if ((idcode & 0xFFFu) != 0x481u) o.hata("DEV_ID 0x481 değil");
        if ((flash_acr & 0xFu) != 4u) o.hata("flash gecikmesi 4 değil");
        if ((icache & 1u) == 0u) o.hata("ICACHE kapalı");
        if ((cpacr & 0x00F00000u) != 0x00F00000u) o.hata("FPU açık değil");
        if ((fpscr & ((1u << 24) | (3u << 22))) != 0u) o.hata("FPSCR FZ/yuvarlama kipi beklenen değil");
        if ((cr1 & ((1u << 29) | (1u << 3) | 1u)) != ((1u << 29) | (1u << 3) | 1u)) o.hata("USART1 UE/TE/FIFOEN açık değil");
    }
    // ---- K: damga maliyeti ve TIM2 periyodu ----
    bool k_tim = false, k_damga = false;
    for (const auto &s : v) {
        if (s.tip != "K") continue;
        Okur o{s, "açılış", hata};
        if (s.kv.count("tim2_periyot_cyc")) {
            o.sema({"tim2_periyot_cyc", "beklenen", "sizeof_yt", "yigin_tepe_acilis"});
            const uint64_t t = o.u("tim2_periyot_cyc");
            if (o.u("beklenen") != periyot) o.hata("beklenen periyot açılıştakiyle farklı");
            if (t + 16 < periyot || t > periyot + 16) o.hata("TIM2 periyodu DWT ile uyuşmuyor: " + std::to_string(t));
            (void)o.u("sizeof_yt");
            const uint64_t ya = o.u("yigin_tepe_acilis");
            if (ya == 0 || ya > *yigin_azami) o.hata("açılış yığın tepesi imajın boyanabilen bölgesi dışında");
            k_tim = true;
        } else if (s.kv.count("damga_100_cyc")) {
            o.sema({"damga_100_cyc"});
            (void)o.u("damga_100_cyc");
            k_damga = true;
        } else {
            o.hata("tanımsız K satırı");
        }
    }
    if (!k_tim || !k_damga) hata("açılış ölçüm satırları (K: tim2_periyot_cyc, damga_100_cyc) eksik");
    if (hata.n) return gecersiz();

    // ---- faz satırlarını topla ----
    std::map<std::string, Faz> fz;
    const Satir *T = nullptr;
    for (const auto &s : v) {
        if (s.tip == "T") { T = &s; continue; }
        if (s.tip.size() != 1 || std::string("FGQSELHM").find(s.tip[0]) == std::string::npos) continue;
        if (!s.kv.count("faz")) { hata(s.tip + " satırında faz yok"); continue; }
        Faz &f = fz[s.kv.at("faz")];
        switch (s.tip[0]) {
        case 'F': f.F.push_back(&s); break;
        case 'G': f.G.push_back(&s); break;
        case 'Q': f.Q.push_back(&s); break;
        case 'S': f.S.push_back(&s); break;
        case 'E': f.E.push_back(&s); break;
        case 'H': f.H.push_back(&s); break;
        case 'L': f.L.push_back(&s); break;
        case 'M': f.M.push_back(&s); break;
        }
    }
    const char *AD[3] = {"A", "B", "C"};
    uint64_t n_faz = 0;
    for (int i = 0; i < 3; ++i) n_faz += (fazlar >> i) & 1u;
    for (const auto &[k, f] : fz) {
        bool bekleniyor = false;
        for (int i = 0; i < 3; ++i) bekleniyor |= ((fazlar >> i) & 1u) && k == AD[i];
        if (!bekleniyor) hata("beklenmeyen faz satırları: faz=" + k);
    }

    static const char *parca_ad[] = {"mesaj_giris", "nominal", "kovaryans", "fuzyon", "sifirlama",
                                     "kip", "gudum", "kontrol", "tahsis", "mesaj_cikis"};
    std::ostringstream rapor;
    std::vector<std::pair<std::string, std::string>> disa;  // (dosya adı, içerik): yalnız bütün denetimler geçerse yazılır
    uint64_t yigin_tepe_en = 0, yigin_kayit_en = 0, d_atlanan_son = 0;
    uint64_t kov_onceki = 0, skaler_onceki = 0;
    for (int i = 0; i < 3; ++i) {
        if (!((fazlar >> i) & 1u)) continue;
        const std::string ad = AD[i];
        const std::string bag = "faz " + ad;
        auto it = fz.find(ad);
        if (it == fz.end()) { hata(bag + ": hiç özet satırı yok (faz tamamlanmadı)"); continue; }
        const Faz &f = it->second;
        auto tek = [&](const std::vector<const Satir *> &x, const char *tip) -> const Satir * {
            if (x.size() != 1) {
                hata(bag + ": " + tip + " satırı " + std::to_string(x.size()) + " adet (1 beklenir)");
                return nullptr;
            }
            return x[0];
        };
        const Satir *F = tek(f.F, "F"), *G = tek(f.G, "G"), *Q = tek(f.Q, "Q"), *S = tek(f.S, "S"), *E = tek(f.E, "E");
        if (!F || !G || !Q || !S || !E) continue;
        const int h0 = hata.n;
        // sıralama: F < G,Q,S,H,L,M < E
        for (const auto *x : {G, Q, S})
            if (!(F->sira < x->sira && x->sira < E->sira)) hata(bag + ": özet satırı F..E aralığı dışında");
        for (const auto &grup : {f.H, f.L, f.M})
            for (const auto *x : grup)
                if (!(F->sira < x->sira && x->sira < E->sira)) hata(bag + ": histogram satırı F..E aralığı dışında");

        // E
        Okur oe{*E, bag, hata};
        oe.sema({"faz", "h", "l", "m"});
        if (oe.u("h") != f.H.size() || oe.u("l") != f.L.size() || oe.u("m") != f.M.size())
            oe.hata("histogram satır sayısı E satırıyla uyuşmuyor");

        // F
        Okur of{*F, bag, hata};
        of.sema({"faz", "derleme", "n", "min", "max", "max_adim", "top", "tasan", "asim", "asim_dwt", "atlanan",
                 "duvar_cyc", "tik_aralik", "saglama"});
        if (of.str("derleme") != derleme) of.hata("derleme etiketi açılıştakiyle farklı");
        const uint64_t n = of.u("n"), mn = of.u("min"), mx = of.u("max"), max_adim = of.u("max_adim");
        const uint64_t top = of.u("top", UINT64_MAX), tasan = of.u("tasan"), asim = of.u("asim"),
                       asim_dwt = of.u("asim_dwt"), atlanan = of.u("atlanan");
        const uint64_t duvar = of.u("duvar_cyc", UINT64_MAX), tik_aralik = of.u("tik_aralik", UINT64_MAX);
        const uint32_t saglama = of.x("saglama");
        if (n != faz_adim) of.hata("kayıtlı adım " + std::to_string(n) + " ≠ faz_adim " + std::to_string(faz_adim));
        if (n == 0) { of.hata("kayıtlı adım yok"); continue; }
        if (mn > mx) of.hata("min > max");
        if (max_adim >= n) of.hata("max_adim kayıt aralığı dışında");
        if (static_cast<u128>(top) < static_cast<u128>(n) * mn || static_cast<u128>(top) > static_cast<u128>(n) * mx)
            of.hata("süre toplamı n·min..n·max dışında");
        if (asim > n || asim_dwt > n) of.hata("aşım sayısı n'den büyük");
        if (tasan > n) of.hata("taşan n'den büyük");
        if (tik_aralik < n) of.hata("tetik aralığı adım sayısından küçük");
        else if (atlanan < tik_aralik - n) of.hata("atlanan tetik, tetik aralığıyla çelişiyor");
        // duvar_cyc = ilk kayıtlı adımın başından son kayıtlı adımın BAŞINA (son adımın süresi dahil değil)
        const u128 duvar_bek = static_cast<u128>(tik_aralik - 1) * periyot;
        const u128 fark = duvar > duvar_bek ? duvar - duvar_bek : duvar_bek - duvar;
        if (fark >= periyot) of.hata("duvar_cyc tetik aralığıyla uyuşmuyor");
        if (static_cast<u128>(duvar) < static_cast<u128>(n - 1) * mn) of.hata("duvar_cyc (n−1)·min'den küçük");
        if (mn >= periyot && (asim != n || asim_dwt != n)) of.hata("her adım periyottan uzun ama aşım n değil");

        // G
        Okur og{*G, bag, hata};
        og.sema({"faz", "lat_min", "lat_max", "lat_top", "lat_tasan", "lat_temiz_n", "lat_temiz_max", "isr_min", "isr_max"});
        const uint64_t lat_min = og.u("lat_min"), lat_max = og.u("lat_max"), lat_top = og.u("lat_top", UINT64_MAX),
                       lat_tasan = og.u("lat_tasan"), lt_n = og.u("lat_temiz_n"), lt_max = og.u("lat_temiz_max"),
                       isr_min = og.u("isr_min"), isr_max = og.u("isr_max");
        if (lat_min > lat_max) og.hata("lat_min > lat_max");
        if (lat_max >= periyot) og.hata("gecikme periyottan büyük (tutarlı okuma bozuk)");
        if (lt_n > n) og.hata("temiz adım sayısı n'den büyük");
        if (lt_max > lat_max) og.hata("temiz gecikme en büyüğü genel en büyükten büyük");
        if (lt_n == 0 && lt_max != 0) og.hata("temiz adım yokken temiz gecikme en büyüğü 0 değil");
        if (lt_n > 0 && lt_max < lat_min) og.hata("temiz gecikme en büyüğü genel en küçükten küçük");
        if (lt_n == n && lt_max != lat_max) og.hata("bütün adımlar temizken temiz gecikme en büyüğü genel en büyüğe eşit değil");
        if (isr_min > isr_max || isr_max >= periyot) og.hata("kesme girişi değerleri tutarsız");
        if (mx + lat_max + 64 < periyot && (asim != 0 || asim_dwt != 0)) of.hata("hiçbir adım periyoda yaklaşmadığı halde aşım sayılmış");

        // Q
        Okur oq{*Q, bag, hata};
        oq.sema({"faz", "p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8", "p9"});
        uint64_t p_top[10] = {}, p_max[10] = {};
        u128 p_toplam = 0;
        for (int p = 0; p < 10; ++p) {
            const std::string k = "p" + std::to_string(p);
            const std::string *t = oq.al(k.c_str());
            if (!t) continue;
            const auto sl = t->find('/');
            const auto a = sl == std::string::npos ? std::nullopt : kesin_sayi(t->substr(0, sl));
            const auto b = sl == std::string::npos ? std::nullopt : kesin_sayi(t->substr(sl + 1));
            if (!a || !b || *b > UINT32_MAX) { oq.hata("parça alanı bozuk: " + k + "=" + *t); continue; }
            p_top[p] = *a; p_max[p] = *b;
            if (*b > mx) oq.hata("parça en büyüğü adım en büyüğünden büyük: " + k);
            if (*b > *a) oq.hata("parça en büyüğü parça toplamından büyük: " + k);
            if (static_cast<u128>(*a) > static_cast<u128>(n) * *b) oq.hata("parça toplamı n·en büyük'ten büyük: " + k);
            p_toplam += *a;
        }
        if (p_toplam > top) oq.hata("parça süreleri toplamı adım süresi toplamından büyük");

        // S
        Okur os{*S, bag, hata};
        os.sema({"faz", "kov", "skaler", "sifirlama", "kapi_gecti", "kapi_red", "gec", "ldl_hata", "yeniden_hiza", "crc_hata",
                 "doyma", "tasma", "yigin_kayit", "yigin_kayit_tasti", "yigin_tepe", "d_atlanan"});
        const uint64_t kov = os.u("kov"), skaler = os.u("skaler");
        for (const char *k : {"sifirlama", "kapi_gecti", "kapi_red", "gec", "ldl_hata", "yeniden_hiza", "doyma", "tasma"})
            (void)os.u(k);
        if (os.u("crc_hata") != 0) os.hata("adımın gelen mesaj CRC hatası 0 değil (girdi bozuk)");
        const uint64_t yk = os.u("yigin_kayit"), yt = os.u("yigin_tepe"), d_atl = os.u("d_atlanan");
        if (os.u("yigin_kayit_tasti") != 0) os.hata("yığın ölçüm bölgesi taştı (yigin_kayit alt sınırdır, geçersiz)");
        // Y2: üst sınırlar ölçülen imajın bellek düzeninden (kayıttan değil) ve firmware sabitlerinden gelir:
        //   yigin_tepe ≤ yigin_azami = _estack − (_ebss + 256) (boyanabilen bölge; koşu betiği ELF'ten hesaplar)
        //   yigin_kayit ≤ 32768 (YIGIN_KAYIT_PAY) + 256 (boyama üst payı) + 256 (main ile boyama işlevi arası çerçeve payı)
        // ve kapsama ilişkisi: yigin_tepe, yigin_kayit'ın ölçüldüğü noktanın üstündeki main çerçevesini de içerir ve daha
        // sonra okunur; bu yüzden yigin_tepe > yigin_kayit olmalıdır.
        if (yk == 0 || yk > 32768 + 512) os.hata("yigin_kayit aralık dışı: " + std::to_string(yk));
        if (yt == 0 || yt > *yigin_azami)
            os.hata("yigin_tepe imajın boyanabilen bölgesinden büyük: " + std::to_string(yt) + " > " + std::to_string(*yigin_azami));
        if (yt <= yk) os.hata("yigin_tepe (" + std::to_string(yt) + ") ≤ yigin_kayit (" + std::to_string(yk) + "): kapsama aykırı");
        if (kov < kov_onceki || skaler < skaler_onceki) os.hata("birikimli sayaç azaldı");
        if (d_atl < d_atlanan_son) os.hata("birikimli d_atlanan azaldı");
        kov_onceki = kov; skaler_onceki = skaler; d_atlanan_son = d_atl;

        // Histogramlar
        Hist h, lh, lk;
        for (const auto *x : f.H) { Okur o{*x, bag, hata}; o.sema({"faz"}, true); hist_oku(*x, h, ince_n, o); }
        for (const auto *x : f.L) { Okur o{*x, bag, hata}; o.sema({"faz"}, true); hist_oku(*x, lh, lat_ince_n, o); }
        for (const auto *x : f.M) { Okur o{*x, bag, hata}; o.sema({"faz"}, true); hist_oku(*x, lk, lat_kaba_n, o); }
        // KAPSAM (mimar kararı 2026-11-18, docs/gorevler/W0-MP4a-kapsam-daraltma.md): bu çözümleyici histogram
        // taşması içeren kampanyaları desteklemez. Adım süresi ya da gecikme histogramında taşma varsa kampanya
        // geçersizdir (rc 1, sayısal özet ve dışa aktarım yok). Taşmalı kayıtların tutarlılığı denetlenmez.
        const u128 aralik_ust = static_cast<u128>(ince_n) * ince_cyc;
        if (tasan != 0) hata(bag + ": KAPSAM DIŞI: adım süresi histogram taşması var (tasan=" + std::to_string(tasan) +
                             "); bu çözümleyici taşmalı kampanyaları desteklemez");
        if (lat_tasan != 0) hata(bag + ": KAPSAM DIŞI: gecikme histogramı taşması var (lat_tasan=" + std::to_string(lat_tasan) + ")");
        if (mx >= aralik_ust) hata(bag + ": KAPSAM DIŞI: en büyük adım süresi histogram aralığının dışında");
        if (toplam(h) != n) hata(bag + ": adım süresi histogramı toplamı ≠ n");
        if (toplam(lh) + toplam(lk) != n) hata(bag + ": gecikme histogramı toplamı ≠ n");
        if (h.empty()) {
            hata(bag + ": adım süresi histogramı boş");
        } else {
            if (mn >= aralik_ust || h.begin()->first != mn / ince_cyc) hata(bag + ": en küçük değer histogramın ilk kutusunda değil");
            if (mx >= aralik_ust || h.rbegin()->first != mx / ince_cyc) hata(bag + ": en büyük değer histogramın son kutusunda değil");
            // her örnek kendi kutusunun sınırları içinde
            u128 alt = 0, ust = 0;
            for (const auto &[k, c] : h) { alt += static_cast<u128>(k) * ince_cyc * c; ust += static_cast<u128>(k * ince_cyc + ince_cyc - 1) * c; }
            if (top < alt || top > ust) hata(bag + ": süre toplamı histogramla tutarsız");
        }
        // gecikme: birleşik histogram (anahtar = kutunun kapsayıcı alt ve üst değeri)
        for (const auto &[k, c] : lk)
            if ((k + 1) * lat_kaba_cyc <= lat_ince_n) hata(bag + ": kaba gecikme kutusu ince aralıkta: " + std::to_string(k));
        std::map<uint64_t, std::pair<uint64_t, uint64_t>> lb;  // üst -> (alt, sayı)
        for (const auto &[k, c] : lh) lb[k] = {k, c};
        for (const auto &[k, c] : lk) lb[k * lat_kaba_cyc + lat_kaba_cyc - 1] = {std::max<uint64_t>(k * lat_kaba_cyc, lat_ince_n), c};
        if (!lb.empty()) {
            const auto &ilk = *lb.begin();
            const auto &son = *lb.rbegin();
            if (lat_min < ilk.second.first || lat_min > ilk.first) hata(bag + ": lat_min histogramın ilk kutusunda değil");
            if (lat_max < son.second.first || lat_max > son.first) hata(bag + ": lat_max histogramın son kutusunda değil");
            u128 alt = 0, ust = 0;
            for (const auto &[u, ac] : lb) { alt += static_cast<u128>(ac.first) * ac.second; ust += static_cast<u128>(u) * ac.second; }
            if (lat_top < alt || lat_top > ust) hata(bag + ": lat_top histogramla tutarsız");
        }
        if (static_cast<u128>(lat_top) < static_cast<u128>(n) * lat_min || static_cast<u128>(lat_top) > static_cast<u128>(n) * lat_max)
            og.hata("lat_top n·lat_min..n·lat_max dışında");

        // Süre alt sınırı ve ölçüt
        const u128 sure_alt = duvar;  // muhafazakâr: son adımın süresi dahil değil
        if (en_az_sn && sure_alt < static_cast<u128>(en_az_sn) * 160000000u)
            of.hata("kayıt süresi alt sınırı " + sn(duvar, false) + " s < " + std::to_string(en_az_sn) + " s");
        if (en_az_adim && n < en_az_adim) of.hata("kayıtlı adım " + std::to_string(n) + " < " + std::to_string(en_az_adim));
        if (hata.n != h0) continue;

        yigin_tepe_en = std::max(yigin_tepe_en, yt);
        yigin_kayit_en = std::max(yigin_kayit_en, yk);
        // ---- rapor (yalnız geçerli faz) ----
        auto adim_q = [&](uint64_t pay, uint64_t payda) -> std::string {
            auto k = yuzdelik_kutu(h, n, pay, payda);
            if (!k) return "belirlenemedi";
            return us(std::min<uint64_t>(*k * ince_cyc + ince_cyc - 1, mx));
        };
        auto lat_q = [&](uint64_t pay, uint64_t payda) -> std::string {
            Hist hh;
            for (const auto &[u, ac] : lb) hh[u] = ac.second;
            auto k = yuzdelik_kutu(hh, n, pay, payda);
            if (!k) return "belirlenemedi";
            return us(std::min<uint64_t>(*k, lat_max));
        };
        char b[512];
        std::snprintf(b, sizeof b,
                      "=== faz %s (%s, %llu Hz) — kayıt: %llu adım; süre alt sınırı (ilk adım başı → son adım başı) %s s; "
                      "son adım dahil süre [%s, %s] s; %llu tetik aralığı\n",
                      ad.c_str(), derleme.c_str(), (unsigned long long)hz, (unsigned long long)n,
                      sn(duvar, false).c_str(), sn(duvar + mn, false).c_str(), sn(duvar + mx, true).c_str(),
                      (unsigned long long)tik_aralik);
        rapor << b;
        rapor << "adım süresi µs (kesin; yüzdelikler kutunun kapsayıcı üst kenarı, ≤): min " << us(mn) << " | p50 ≤"
              << adim_q(1, 2) << " | p99 ≤" << adim_q(99, 100) << " | p99,9 ≤" << adim_q(999, 1000) << " | p99,99 ≤"
              << adim_q(9999, 10000) << " | maks " << us(mx) << " | ort ≈" << ort_us(top, n) << " | 20ms_ustu " << tasan
              << " | maks_adim " << max_adim << "\n";
        rapor << "kaçırılan adım (asim) " << asim << " | DWT çapraz denetimi " << asim_dwt << " | atlanan tetik " << atlanan
              << " | periyot " << us(periyot) << " µs\n";
        rapor << "tetik gecikmesi (güncelleme olayı → ana döngünün tetiği gördüğü an) µs: min " << us(lat_min) << " | p50 ≤"
              << lat_q(1, 2) << " | p99 ≤" << lat_q(99, 100) << " | p99,9 ≤" << lat_q(999, 1000) << " | maks " << us(lat_max)
              << " | ort ≈" << ort_us(lat_top, n) << "\n";
        rapor << "  önceki adım aşmamış ve tetik atlanmamış adımlar: " << lt_n << ", bunlarda en büyük gecikme "
              << (lt_n ? us(lt_max) : std::string("-")) << " µs | kesme girişi (güncelleme → TIM2 işleyicisinin CNT okuması) µs: min "
              << us(isr_min) << " maks " << us(isr_max) << "\n";
        rapor << "parça ort/maks µs:";
        for (int p = 0; p < 10; ++p) rapor << " " << parca_ad[p] << " " << ort_us(p_top[p], n) << "/" << us(p_max[p]);
        rapor << "\n" << S->govde << "\n";
        std::snprintf(b, sizeof b, "sağlama 0x%08X\n", saglama);
        rapor << b;

        if (!hist_onek.empty()) {
            const std::string a1 = hist_onek + "-" + ad + "-10us.csv";
            std::ostringstream o;
            o << "kutu_alt_us,kutu_ust_us,sayi\n";
            std::map<uint64_t, uint64_t> kaba;
            for (const auto &[k, c] : h) kaba[k * ince_cyc / 1600] += c;
            for (const auto &[bk, c] : kaba) o << bk * 10 << ',' << (bk + 1) * 10 << ',' << c << '\n';
            const std::string a2 = hist_onek + "-" + ad + "-ince.csv";
            std::ostringstream o2;
            o2 << "kutu_alt_cyc,kutu_ust_cyc_dahil,sayi\n";
            for (const auto &[k, c] : h) o2 << k * ince_cyc << ',' << k * ince_cyc + ince_cyc - 1 << ',' << c << '\n';
            const std::string a3 = hist_onek + "-" + ad + "-gecikme.csv";
            std::ostringstream o3;
            o3 << "kutu_alt_cyc,kutu_ust_cyc_dahil,sayi\n";
            for (const auto &[u, ac] : lb) o3 << ac.first << ',' << u << ',' << ac.second << '\n';
            disa.emplace_back(a1, o.str());
            disa.emplace_back(a2, o2.str());
            disa.emplace_back(a3, o3.str());
            rapor << "histogram: " << a1 << ", " << a2 << ", " << a3 << "\n";
        }
    }
    // ---- T: kampanya sonu ----
    if (!T) {
        hata("kampanya sonu (T) satırı yok: koşu tamamlanmadı");
    } else {
        Okur ot{*T, "kampanya sonu", hata};
        ot.sema({"adim", "fazlar", "d_atlanan"});
        if (ot.u("fazlar") != fazlar) ot.hata("faz maskesi açılıştakiyle farklı");
        const uint64_t beklenen_adim = 2 * hz + n_faz * (gecis + faz_adim);
        if (ot.u("adim") != beklenen_adim) ot.hata("toplam adım " + std::to_string(beklenen_adim) + " beklenirdi");
        if (ot.u("d_atlanan") != d_atlanan_son) ot.hata("d_atlanan son fazın S satırıyla uyuşmuyor");
    }
    if (hata.n) return gecersiz();
    // Y4: dışa aktarım yalnız bütün kampanya denetimleri (T dahil) geçtikten sonra. Var olan dosya ezilmez; önce
    // hiçbir hedefin bulunmadığı denetlenir, dosyalar geçici adla yazılıp yeniden adlandırılır; bir hata olursa
    // bu çağrının yazdığı geçici/son dosyalar silinir. Dışa aktarım başarısızsa çıkış 3 (veri geçerli, çıktı yok).
    for (const auto &[ad, icerik] : disa) {
        if (::access(ad.c_str(), F_OK) == 0) {
            std::cout << "HATA: histogram dosyası zaten var, ezilmedi: " << ad << "\n";
            std::cout << "sonuç: VERİ GEÇERLİ ama dışa aktarım yapılmadı (hiçbir dosya yazılmadı); başka önek seçin\n";
            return 3;
        }
    }
    std::vector<std::string> yazilan;
    auto geri_al = [&]() { for (const auto &y : yazilan) ::unlink(y.c_str()); };
    for (const auto &[ad, icerik] : disa) {
        const std::string gecici = ad + ".yaziliyor-" + std::to_string(::getpid());
        std::ofstream o(gecici, std::ios::binary | std::ios::trunc);
        o << icerik;
        o.close();
        if (!o || ::link(gecici.c_str(), ad.c_str()) != 0) {  // link: hedef varsa başarısız olur (ezmez)
            ::unlink(gecici.c_str());
            geri_al();
            std::cout << "HATA: histogram dosyası yazılamadı: " << ad << "\n";
            std::cout << "sonuç: VERİ GEÇERLİ ama dışa aktarım yapılmadı (yazılanlar geri alındı)\n";
            return 3;
        }
        ::unlink(gecici.c_str());
        yazilan.push_back(ad);
    }
    std::cout << rapor.str();
    std::cout << "yığın (fazlarda gözlenen tepelerin en büyüğü): yigin_tepe " << yigin_tepe_en << " B, yigin_kayit "
              << yigin_kayit_en << " B\n";
    std::cout << "sonuç: GEÇERLİ (beklenen kampanya: " << *b_hz << " Hz, faz maskesi " << *b_fazlar << ", faz başına "
              << *b_faz_adim << " adım; bütün fazlar eksiksiz, bütünlük ve şema denetimleri geçti)\n";
    return 0;
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc >= 5 && std::strcmp(argv[1], "kayit") == 0) return kayit(argv[2], std::atof(argv[3]), argv[4]);
    if (argc >= 3 && std::strcmp(argv[1], "analiz") == 0) return analiz(argc, argv);
    if (argc == 3 && std::strcmp(argv[1], "imza") == 0) {
        char b[8];
        std::snprintf(b, sizeof b, "%04X", crc16(argv[2]));
        std::cout << argv[2] << " *" << b << "\n";
        return 0;
    }
    std::cerr << "kullanım: mp4_seri kayit <port> <azami_s> <çıktı.csv>\n"
                 "          mp4_seri analiz <kayıt.csv> --kimlik K --beklenen-hz HZ --beklenen-fazlar MASKE\n"
                 "                  --beklenen-faz-adim N --yigin-azami B [--hist önek] [--en-az-adim N] [--en-az-sn S]\n"
                 "          mp4_seri imza <metin>\n";
    return 2;
}
