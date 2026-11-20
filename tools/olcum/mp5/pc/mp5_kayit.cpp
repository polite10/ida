// W0-MP5a ölçüm prototipi (atılabilir; yalnız POSIX/macOS): seri kayıt.
//
//   mp5_kayit <port> <azami_s> <çıktı.txt>
//
// MP4a'nın mp4_seri kayit işlevinin uyarlanmış kopyasıdır (tools/olcum/mp4/pc/mp4_seri.cpp, main'de).
// Portu 115200 8N1 ham kipte açar ve gelen her satırı olduğu gibi (\r atılarak) çıktı dosyasına yazar.
// Kartın kampanya sonu satırı ("T ...") gelince biter ve 0 döner; azami_s içinde gelmezse 4 döner (eksik
// koşu). "X" (hata) satırı gelirse 5 döner. "S" ve "T" satırları ekrana da basılır.
// Satırların doğrulaması (CRC, sıra) ve özet ozet.py'dedir; bu araç yalnız kaydeder.

#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

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
    const int64_t bitis = ns_mono() + static_cast<int64_t>(azami_s * 1e9);
    std::string satir;
    char buf[512];
    long n_satir = 0;
    int sonuc = -1;
    while (sonuc < 0 && ns_mono() < bitis) {
        fd_set rs;
        FD_ZERO(&rs);
        FD_SET(fd, &rs);
        timeval tv{0, 200000};
        const int r = ::select(fd + 1, &rs, nullptr, nullptr, &tv);
        if (r < 0) { std::perror("select"); return 3; }
        if (r == 0) continue;
        const ssize_t n = ::read(fd, buf, sizeof buf);
        if (n < 0) { std::perror("read"); return 3; }
        if (n == 0) { std::cerr << "port kapandı\n"; return 3; }
        for (ssize_t i = 0; i < n; ++i) {
            const char c = buf[i];
            if (c == '\r') continue;
            if (c == '\n') {
                out << satir << '\n';
                ++n_satir;
                if (!satir.empty() && (satir[0] == 'S' || satir[0] == 'T' || satir[0] == 'X' || satir[0] == 'B')) {
                    std::cout << satir << std::endl;
                }
                if (satir.rfind("T ", 0) == 0) sonuc = 0;
                if (satir.rfind("X ", 0) == 0) sonuc = 5;
                satir.clear();
            } else if (satir.size() < 4096) {
                satir.push_back(c);
            }
        }
        out.flush();
    }
    ::close(fd);
    if (sonuc < 0) {
        std::cout << "kayıt EKSİK: " << azami_s << " s içinde T satırı gelmedi; " << n_satir << " satır, " << cikti
                  << std::endl;
        return 4;
    }
    std::cout << "kayıt bitti (" << (sonuc == 0 ? "T" : "X") << " satırı): " << n_satir << " satır, " << cikti
              << std::endl;
    return sonuc;
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc == 4) return kayit(argv[1], std::atof(argv[2]), argv[3]);
    std::cerr << "kullanım: mp5_kayit <port> <azami_s> <çıktı.txt>\n";
    return 2;
}
