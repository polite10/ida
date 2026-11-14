// W0-MP1a ölçüm prototipi: PC seri kayıt ve çözümleme aracı (atılabilir; yalnız POSIX/macOS).
//
//   mp1_seri kayit <port> <süre_s> <çıktı.csv>
//       Portu 115200 8N1 ham kipte açar; her satırın '\n' baytının okunduğu anı iki saatle damgalar:
//       steady_clock (macOS'ta düzeltilmemiş donanım zaman tabanı) ve system_clock (NTP ile disiplinli).
//       CSV: mono_ns,real_ns,satır
//   mp1_seri analiz <kayıt.csv> [nominal_hz]
//       "T <sıra> <çevrim>" satırlarından en küçük kareler eğimi (çevrim/s) ve nominalden sapma (ppm);
//       sıra boşlukları, "B" (açılış = sıfırlama) ve "E" (hata) satırları, "K" satırlarındaki saat kaynağı.
//
// Ölçüm belirsizliği: PC saatinin kendi frekans hatası sonuca eklenir; iki saatin eğimi ayrı raporlanır.

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
    double hz;
    double rms_us;
    double maks_us;
    double se_hz;  // eğimin standart hatası (artıkların bağımsız olduğu varsayımıyla)
};

// x: PC zamanı (s), y: çevrim. y = a + b x; b = Hz.
Egim egim(const std::vector<Nokta> &v, bool real, size_t i0, size_t i1)
{
    const size_t n = i1 - i0;
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
    const long double b = sxy / sxx;
    const long double a = my - b * mx;
    long double ss = 0, mk = 0;
    for (size_t i = 0; i < n; ++i) {
        // Artık, zaman biriminde (µs): çevrim artığı / eğim
        const long double r = (y[i] - (a + b * x[i])) / b * 1e6L;
        ss += r * r;
        if (std::fabs(r) > mk) mk = std::fabs(r);
    }
    // ss µs² cinsinden; çevrim cinsine: (r_us * 1e-6 * b)^2
    const long double ss_cyc = ss * 1e-12L * b * b;
    const long double se = std::sqrt(ss_cyc / static_cast<long double>(n - 2) / sxx);
    return {static_cast<double>(b), static_cast<double>(std::sqrt(ss / static_cast<long double>(n))),
            static_cast<double>(mk), static_cast<double>(se)};
}

int analiz(const char *dosya, double nominal)
{
    std::ifstream in(dosya);
    if (!in) {
        std::cerr << "açılamadı: " << dosya << "\n";
        return 2;
    }
    std::string l;
    std::getline(in, l);  // başlık
    std::vector<Nokta> v;
    long n_b = 0, n_e = 0, n_k = 0, n_k_iyi = 0, n_diger = 0, bosluk = 0;
    std::vector<int64_t> t_b, t_e;  // B ve E satırlarının varış anı (mono)
    while (std::getline(in, l)) {
        const auto c1 = l.find(',');
        const auto c2 = l.find(',', c1 + 1);
        if (c1 == std::string::npos || c2 == std::string::npos) continue;
        const int64_t tm = std::stoll(l.substr(0, c1));
        const int64_t tr = std::stoll(l.substr(c1 + 1, c2 - c1 - 1));
        const std::string s = l.substr(c2 + 1);
        if (s.rfind("T ", 0) == 0) {
            std::istringstream is(s.substr(2));
            uint64_t sira = 0, cyc = 0;
            if (!(is >> sira >> cyc)) {
                ++n_diger;
                continue;
            }
            v.push_back({tm, tr, sira, cyc});
        } else if (s.find("B MP1a") != std::string::npos) {  // açılış satırının önünde hat gürültüsü olabilir
            ++n_b;
            t_b.push_back(tm);
        } else if (s.rfind("E ", 0) == 0) {
            ++n_e;
            t_e.push_back(tm);
        } else if (s.rfind("K ", 0) == 0) {
            ++n_k;
            if (s.find("kaynak=PLL1 ") != std::string::npos &&
                s.find("hesap_hz=160000000") != std::string::npos)
                ++n_k_iyi;
        } else if (!s.empty()) {
            ++n_diger;
        }
    }
    // Port açılınca, okunmadan beklemiş satırlar tek parça gelir (aynı damga); ilk 0,3 s'dekiler atılır.
    size_t at = 0;
    while (at < v.size() && v[at].mono - v.front().mono < 300000000) ++at;
    if (at > 0 && at < v.size()) {
        std::printf("açılış yığını: ilk %zu T satırı atıldı (port açılmadan önce tamponda beklemiş)\n", at);
        v.erase(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(at));
    }
    for (size_t i = 1; i < v.size(); ++i)
        if (v[i].sira != v[i - 1].sira + 1) ++bosluk;
    // Ölçüm penceresi: tutulan ilk T satırından sonrası. Penceredeki B satırı koşu sırasında sıfırlama demektir.
    long b_pencere = 0, e_pencere = 0;
    if (!v.empty()) {
        for (int64_t t : t_b) if (t > v.front().mono) ++b_pencere;
        for (int64_t t : t_e) if (t > v.front().mono) ++e_pencere;
    }
    long saat_sicrama = 0;
    for (size_t i = 1; i < v.size(); ++i) {
        const int64_t dm = v[i].mono - v[i - 1].mono, dr = v[i].real - v[i - 1].real;
        if (dr - dm > 5000000 || dm - dr > 5000000) ++saat_sicrama;
    }
    std::printf("T satırı: %zu, sıra boşluğu: %ld, B (açılış): %ld (ölçüm penceresinde %ld), E (hata): %ld "
                "(penceresinde %ld), K: %ld (PLL1 ve 160 MHz: %ld), tanınmayan: %ld\n",
                v.size(), bosluk, n_b, b_pencere, n_e, e_pencere, n_k, n_k_iyi, n_diger);
    std::printf("system_clock'ta steady_clock'a göre > 5 ms sıçrama: %ld\n", saat_sicrama);
    if (b_pencere > 0 || bosluk > 0 || e_pencere > 0)
        std::printf("UYARI: kayıtta açılış (sıfırlama), sıra boşluğu ya da hata satırı var; tur hatalı, eğim "
                    "geçersiz olabilir\n");
    if (v.size() < 10) {
        std::printf("yetersiz veri\n");
        return 1;
    }
    const double sure_s = static_cast<double>(v.back().mono - v.front().mono) * 1e-9;
    std::printf("ilk sıra %llu, son sıra %llu, PC süresi %.3f s\n",
                static_cast<unsigned long long>(v.front().sira), static_cast<unsigned long long>(v.back().sira),
                sure_s);
    for (int real = 0; real < 2; ++real) {
        const Egim e = egim(v, real != 0, 0, v.size());
        const Egim e1 = egim(v, real != 0, 0, v.size() / 2);
        const Egim e2 = egim(v, real != 0, v.size() / 2, v.size());
        auto ppm = [&](double hz) { return (hz / nominal - 1.0) * 1e6; };
        std::printf("%s: eğim %.3f Hz, sapma %+.3f ppm (std. hata %.3f ppm; ilk yarı %+.3f, ikinci yarı %+.3f); "
                    "artık rms %.1f µs, en büyük %.1f µs\n",
                    real ? "system_clock (NTP)" : "steady_clock (ham)", e.hz, ppm(e.hz), e.se_hz / nominal * 1e6,
                    ppm(e1.hz), ppm(e2.hz), e.rms_us, e.maks_us);
    }
    return 0;
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc >= 5 && std::strcmp(argv[1], "kayit") == 0) return kayit(argv[2], std::atof(argv[3]), argv[4]);
    if (argc >= 3 && std::strcmp(argv[1], "analiz") == 0)
        return analiz(argv[2], argc >= 4 ? std::atof(argv[3]) : 160e6);
    std::cerr << "kullanım: mp1_seri kayit <port> <süre_s> <çıktı.csv> | mp1_seri analiz <kayıt.csv> [nominal_hz]\n";
    return 64;
}
