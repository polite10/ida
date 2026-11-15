// W0-MP1a ölçüm prototipi: PC seri kayıt ve çözümleme aracı (atılabilir; yalnız POSIX/macOS).
//
//   mp1_seri kayit <port> <süre_s> <çıktı.csv>
//       Portu 115200 8N1 ham kipte açar; her satırın '\n' baytının okunduğu anı iki saatle damgalar:
//       steady_clock (macOS'ta düzeltilmemiş donanım zaman tabanı) ve system_clock (NTP ile disiplinli).
//       CSV: mono_ns,real_ns,satır
//   mp1_seri analiz <kayıt.csv> <nominal_hz> <SMPS|LDO> [en_az_pencere_s=600]
//       Ölçüm penceresi istenen regülatörle açılan imajın ilk "B" satırından başlar. Pencerede yeni açılış
//       (sıfırlama), "E" satırı, sıra boşluğu, geri giden sayaç ya da PC zamanı, istenenden farklı REGS,
//       sıfırdan farklı UART hata sayacı, PLL1/160 MHz olmayan K satırı ya da kısa pencere varsa eğim
//       hesaplanmaz ve çıkış kodu 1 olur. Geçerli koşuda "T <sıra> <çevrim>" satırlarından en küçük kareler
//       eğimi (çevrim/s) ve nominale göre oran sapması (ppm) iki PC saatine göre ayrı verilir.
//
// Oran, kart saatinin Mac saatine oranıdır; Mac saatinin mutlak hatası bilinmez (kalibre referans yok).

#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int64_t ns_mono()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
int64_t ns_real()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

int kayit(const char *port, double sure_s, const char *cikti)
{
    int fd = ::open(port, O_RDONLY | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        std::perror("open");
        return 2;
    }
    termios t{};
    if (tcgetattr(fd, &t) != 0) {
        std::perror("tcgetattr");
        return 2;
    }
    cfmakeraw(&t);
    cfsetispeed(&t, B115200);
    cfsetospeed(&t, B115200);
    t.c_cflag |= (CLOCAL | CREAD);
    t.c_cflag &= ~static_cast<tcflag_t>(CSTOPB | PARENB | CRTSCTS);
    if (tcsetattr(fd, TCSANOW, &t) != 0) {
        std::perror("tcsetattr");
        return 2;
    }
    tcflush(fd, TCIFLUSH);

    std::ofstream out(cikti);
    if (!out) {
        std::cerr << "çıktı açılamadı: " << cikti << "\n";
        return 2;
    }
    out << "mono_ns,real_ns,satir\n";

    const int64_t bas = ns_mono();
    const int64_t bitis = bas + static_cast<int64_t>(sure_s * 1e9);
    std::string satir;
    char buf[512];
    long n_satir = 0;
    while (ns_mono() < bitis) {
        fd_set rs;
        FD_ZERO(&rs);
        FD_SET(fd, &rs);
        timeval tv{0, 200000};
        int r = ::select(fd + 1, &rs, nullptr, nullptr, &tv);
        if (r < 0) {
            std::perror("select");
            return 3;
        }
        if (r == 0) continue;
        ssize_t n = ::read(fd, buf, sizeof buf);
        const int64_t tm = ns_mono();
        const int64_t tr = ns_real();
        if (n < 0) {
            std::perror("read");
            return 3;
        }
        if (n == 0) {
            std::cerr << "port kapandı\n";
            return 3;
        }
        for (ssize_t i = 0; i < n; ++i) {
            char c = buf[i];
            if (c == '\r') continue;
            if (c == '\n') {
                out << tm << ',' << tr << ',' << satir << '\n';
                if (satir.empty() || satir[0] != 'T') std::cout << satir << std::endl;
                ++n_satir;
                satir.clear();
            } else if (satir.size() < 1024) {
                satir.push_back(c);
            }
        }
        out.flush();
    }
    ::close(fd);
    std::cout << "kayıt bitti: " << n_satir << " satır, " << cikti << std::endl;
    return 0;
}

struct Nokta {
    int64_t mono, real;
    uint64_t sira, cyc;
};

struct Egim {
    bool gecerli;
    double hz;
    double rms_us;
    double maks_us;
    double se_hz;  // eğimin standart hatası (artıkların bağımsız olduğu varsayımıyla)
};

// x: PC zamanı (s), y: çevrim. y = a + b x; b = Hz. Çağıran, cyc'nin kesin artan olduğunu denetlemiş olmalı.
Egim egim(const std::vector<Nokta> &v, bool real, size_t i0, size_t i1)
{
    const size_t n = i1 - i0;
    if (n < 3) return {false, 0, 0, 0, 0};
    long double mx = 0, my = 0;
    const int64_t t0 = real ? v[i0].real : v[i0].mono;
    const uint64_t c0 = v[i0].cyc;
    std::vector<long double> x(n), y(n);
    for (size_t i = 0; i < n; ++i) {
        const Nokta &p = v[i0 + i];
        x[i] = static_cast<long double>((real ? p.real : p.mono) - t0) * 1e-9L;
        y[i] = static_cast<long double>(p.cyc - c0);
        mx += x[i];
        my += y[i];
    }
    mx /= static_cast<long double>(n);
    my /= static_cast<long double>(n);
    long double sxy = 0, sxx = 0;
    for (size_t i = 0; i < n; ++i) {
        sxy += (x[i] - mx) * (y[i] - my);
        sxx += (x[i] - mx) * (x[i] - mx);
    }
    if (!(sxx > 0)) return {false, 0, 0, 0, 0};
    const long double b = sxy / sxx;
    if (!std::isfinite(static_cast<double>(b)) || !(b > 0)) return {false, 0, 0, 0, 0};
    const long double a = my - b * mx;
    long double ss = 0, mk = 0;
    for (size_t i = 0; i < n; ++i) {
        const long double r = (y[i] - (a + b * x[i])) / b * 1e6L;  // artık, µs
        ss += r * r;
        if (std::fabs(r) > mk) mk = std::fabs(r);
    }
    const long double ss_cyc = ss * 1e-12L * b * b;
    const long double se = std::sqrt(ss_cyc / static_cast<long double>(n - 2) / sxx);
    const Egim e{true, static_cast<double>(b), static_cast<double>(std::sqrt(ss / static_cast<long double>(n))),
                 static_cast<double>(mk), static_cast<double>(se)};
    if (!std::isfinite(e.hz) || !std::isfinite(e.se_hz) || !std::isfinite(e.rms_us)) return {false, 0, 0, 0, 0};
    return e;
}

// "anahtar=değer" alanını kesin okur: değer yalnız ondalık rakamlardan oluşmalı ve boşlukla ya da satır
// sonuyla bitmeli (en çok 18 rakam). Alan yoksa -1, değer bozuksa -2 döner; bozuk değer 0 sayılmaz.
long long alan(const std::string &s, const char *anahtar)
{
    const std::string k = std::string(" ") + anahtar + "=";
    const auto p = s.find(k);
    if (p == std::string::npos) return -1;
    size_t i = p + k.size(), n = 0;
    long long deger = 0;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        if (++n > 18) return -2;
        deger = deger * 10 + (s[i] - '0');
        ++i;
    }
    if (n == 0 || (i < s.size() && s[i] != ' ')) return -2;
    return deger;
}

// "T <sıra> <çevrim>" satırını kesin çözer: tam iki rakam dizisi, başka karakter yok.
bool t_coz(const std::string &s, uint64_t &sira, uint64_t &cyc)
{
    if (s.rfind("T ", 0) != 0) return false;
    uint64_t d[2] = {0, 0};
    size_t i = 2;
    for (int k = 0; k < 2; ++k) {
        size_t n = 0;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            if (++n > 19) return false;
            d[k] = d[k] * 10 + static_cast<uint64_t>(s[i] - '0');
            ++i;
        }
        if (n == 0) return false;
        if (k == 0) {
            if (i >= s.size() || s[i] != ' ') return false;
            ++i;
        }
    }
    if (i != s.size()) return false;
    sira = d[0];
    cyc = d[1];
    return true;
}

struct Satir {
    int64_t mono, real;
    std::string s;
};

// Ölçüm penceresi: istenen regülatörle açılan imajın ilk açılış satırından ("B MP1a reg_istenen=<REG>")
// sonrası. Penceredeki her şey o açılışın aynı çalışmasına ait olmalıdır; aksi halde koşu geçersizdir.
int analiz(const char *dosya, double nominal, const std::string &reg, double min_s)
{
    std::ifstream in(dosya);
    if (!in) {
        std::cerr << "açılamadı: " << dosya << "\n";
        return 2;
    }
    std::string l;
    std::getline(in, l);  // başlık
    std::vector<Satir> sat;
    while (std::getline(in, l)) {
        const auto c1 = l.find(',');
        const auto c2 = (c1 == std::string::npos) ? std::string::npos : l.find(',', c1 + 1);
        if (c1 == std::string::npos || c2 == std::string::npos) continue;
        sat.push_back({std::stoll(l.substr(0, c1)), std::stoll(l.substr(c1 + 1, c2 - c1 - 1)), l.substr(c2 + 1)});
    }
    const std::string acilis = "B MP1a reg_istenen=" + reg;
    size_t bas = sat.size();
    for (size_t i = 0; i < sat.size(); ++i)
        if (sat[i].s.find(acilis) != std::string::npos) {
            bas = i;
            break;
        }
    std::vector<std::string> red;
    if (bas == sat.size()) {
        std::printf("RED: '%s' açılış satırı yok; ölçüm penceresi belirlenemedi\n", acilis.c_str());
        return 1;
    }
    std::printf("açılış: %s\n", sat[bas].s.substr(sat[bas].s.find("B MP1a")).c_str());
    if (alan(sat[bas].s, "saat_adim") != 0) red.push_back("açılışta saat_adim ≠ 0 ya da okunamadı");

    std::vector<Nokta> v;
    long n_b = 0, n_e = 0, n_k = 0, n_k_iyi = 0, n_p = 0, n_p_reg = 0, n_diger = 0, n_t_bozuk = 0;
    long n_p_uart_iyi = 0;
    long long uart_hata_maks = -1;
    std::vector<long long> k_sira, p_sira;  // her K/P satırının sıra alanı (bozuksa < 0)
    for (size_t i = bas + 1; i < sat.size(); ++i) {
        const std::string &s = sat[i].s;
        if (s.rfind("T ", 0) == 0) {
            uint64_t sira = 0, cyc = 0;
            if (!t_coz(s, sira, cyc)) {
                ++n_t_bozuk;
                continue;
            }
            v.push_back({sat[i].mono, sat[i].real, sira, cyc});
        } else if (s.find("B MP1a") != std::string::npos) {
            ++n_b;
        } else if (s.rfind("E ", 0) == 0) {
            ++n_e;
        } else if (s.rfind("K ", 0) == 0) {
            ++n_k;
            k_sira.push_back(alan(s, "sira"));
            if (s.find(" kaynak=PLL1 ") != std::string::npos && alan(s, "hesap_hz") == 160000000) ++n_k_iyi;
        } else if (s.rfind("P ", 0) == 0) {
            ++n_p;
            p_sira.push_back(alan(s, "sira"));
            if (s.find(" regs=" + reg + " ") != std::string::npos) ++n_p_reg;
            const long long u = alan(s, "uart_hata");  // -1 eksik, -2 bozuk
            if (u == 0) ++n_p_uart_iyi;
            if (u > uart_hata_maks) uart_hata_maks = u;
        } else if (!s.empty()) {
            ++n_diger;
        }
    }
    long bosluk = 0, mono_ters = 0, cyc_ters = 0, saat_sicrama = 0;
    for (size_t i = 1; i < v.size(); ++i) {
        if (v[i].sira != v[i - 1].sira + 1) ++bosluk;
        if (v[i].mono < v[i - 1].mono) ++mono_ters;
        if (v[i].cyc <= v[i - 1].cyc) ++cyc_ters;
        const int64_t dm = v[i].mono - v[i - 1].mono, dr = v[i].real - v[i - 1].real;
        if (dr - dm > 5000000 || dm - dr > 5000000) ++saat_sicrama;
    }
    const double sure_s = v.size() >= 2 ? static_cast<double>(v.back().mono - v.front().mono) * 1e-9 : 0.0;
    std::printf("pencere: T %zu (sıra %llu → %llu), %.3f s; sıra boşluğu %ld; sayaç geri gidişi %ld; PC zamanı geri "
                "gidişi %ld\n",
                v.size(), v.empty() ? 0ULL : static_cast<unsigned long long>(v.front().sira),
                v.empty() ? 0ULL : static_cast<unsigned long long>(v.back().sira), sure_s, bosluk, cyc_ters, mono_ters);
    std::printf("pencerede: B (yeni açılış = sıfırlama) %ld, E (hata) %ld, K %ld (PLL1 ve 160 MHz: %ld), P %ld "
                "(regs=%s: %ld), uart_hata en büyük %lld, tanınmayan %ld, bozuk T %ld\n",
                n_b, n_e, n_k, n_k_iyi, n_p, reg.c_str(), n_p_reg, uart_hata_maks, n_diger, n_t_bozuk);
    std::printf("system_clock'ta steady_clock'a göre > 5 ms sıçrama: %ld\n", saat_sicrama);

    if (n_b > 0) red.push_back("pencerede yeni açılış (sıfırlama)");
    if (n_e > 0) red.push_back("pencerede hata satırı");
    if (bosluk > 0) red.push_back("sıra boşluğu");
    if (cyc_ters > 0) red.push_back("çevrim sayacı kesin artan değil");
    if (mono_ters > 0) red.push_back("PC zamanı geri gitti");
    if (v.size() < 10) red.push_back("10'dan az T satırı");
    else if (v.front().sira != 1) red.push_back("pencerenin ilk T sırası 1 değil (açılıştan sonra T satırı eksik)");
    if (n_t_bozuk > 0) red.push_back("bozuk T satırı");
    // Beklenen durum satırları: açılışta sıra 0, sonra son T'den önceki her 60'ın katı; her biri tam bir K ve bir P.
    {
        const long long son = v.empty() ? 0 : static_cast<long long>(v.back().sira);
        std::vector<long long> beklenen;
        for (long long q = 0; q < son || q == 0; q += 60) beklenen.push_back(q);
        auto denetle = [&](std::vector<long long> got, const char *ad) {
            std::sort(got.begin(), got.end());
            // Kayıt son T satırından hemen sonra bitmişse o sıranın K/P'si gelmiş de olabilir, gelmemiş de.
            if (son > 0 && son % 60 == 0 && !got.empty() && got.back() == son) got.pop_back();
            if (got != beklenen) {
                std::printf("%s sıraları beklenenle tutmuyor: %zu beklenen, %zu bulunan\n", ad, beklenen.size(),
                            got.size());
                red.push_back(std::string(ad) + " satırları eksik, fazla ya da sıra alanı bozuk");
            }
        };
        denetle(k_sira, "K");
        denetle(p_sira, "P");
    }
    if (!(sure_s >= min_s)) red.push_back("pencere süresi gerekenin altında");
    if (n_k == 0 || n_k_iyi != n_k) red.push_back("K satırı yok ya da PLL1/160 MHz olmayan K satırı var");
    if (n_p == 0 || n_p_reg != n_p) red.push_back("P satırı yok ya da istenenden farklı REGS");
    if (n_p == 0 || n_p_uart_iyi != n_p) red.push_back("bir P satırında uart_hata eksik, bozuk ya da 0 değil");
    if (!red.empty()) {
        for (const auto &r : red) std::printf("RED: %s\n", r.c_str());
        std::printf("SONUÇ: GEÇERSİZ KOŞU; eğim hesaplanmadı\n");
        return 1;
    }
    auto ppm = [&](double hz) { return (hz / nominal - 1.0) * 1e6; };
    for (int real = 0; real < 2; ++real) {
        const char *ad = real ? "system_clock (NTP disiplinli)" : "steady_clock (ham)";
        if (real && saat_sicrama > 0) {
            std::printf("%s: GEÇERSİZ (pencerede saat sıçraması var)\n", ad);
            continue;
        }
        const Egim e = egim(v, real != 0, 0, v.size());
        const Egim e1 = egim(v, real != 0, 0, v.size() / 2);
        const Egim e2 = egim(v, real != 0, v.size() / 2, v.size());
        if (!e.gecerli || !e1.gecerli || !e2.gecerli) {
            std::printf("%s: eğim hesaplanamadı (sonlu değil ya da süre sıfır)\n", ad);
            if (!real) return 1;
            continue;
        }
        std::printf("%s: eğim %.3f Hz, oran sapması %+.3f ppm (std. hata %.3f ppm; ilk yarı %+.3f, ikinci yarı "
                    "%+.3f); artık rms %.1f µs, en büyük %.1f µs\n",
                    ad, e.hz, ppm(e.hz), e.se_hz / nominal * 1e6, ppm(e1.hz), ppm(e2.hz), e.rms_us, e.maks_us);
    }
    std::printf("SONUÇ: GEÇERLİ KOŞU\n");
    return 0;
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc >= 5 && std::strcmp(argv[1], "kayit") == 0) return kayit(argv[2], std::atof(argv[3]), argv[4]);
    if (argc >= 5 && std::strcmp(argv[1], "analiz") == 0)
        return analiz(argv[2], std::atof(argv[3]), argv[4], argc >= 6 ? std::atof(argv[5]) : 600.0);
    std::cerr << "kullanım: mp1_seri kayit <port> <süre_s> <çıktı.csv>\n"
                 "          mp1_seri analiz <kayıt.csv> <nominal_hz> <SMPS|LDO> [en_az_pencere_s=600]\n";
    return 64;
}
