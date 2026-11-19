// W0-MP3a ölçüm prototipi: POSIX seri port (macOS; atılabilir).
#include "seri.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <IOKit/serial/ioss.h>
#endif

namespace mp3 {

const char *io_ad(Io s)
{
    switch (s) {
    case Io::Tamam: return "tamam";
    case Io::ZamanAsimi: return "zaman_asimi";
    case Io::Kopma: return "kopma";
    }
    return "?";
}

namespace {

int64_t ms_simdi()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

class PosixPort final : public Port {
public:
    PosixPort(int fd, std::string yol, std::function<bool(uint32_t)> kanca = nullptr, YazmaKanca yazma = nullptr)
        : fd_(fd), yol_(std::move(yol)), kanca_(std::move(kanca)), yazma_(std::move(yazma)) {}
    ~PosixPort() override { ::close(fd_); }

    bool hiz(uint32_t baud, HizRaporu &r) override
    {
        r = HizRaporu{};
        r.istenen = baud;
        if (kanca_) {
            r.tcsetattr_ok = true;
            r.ioctl_ok = kanca_(baud);
            r.geri_ispeed = r.geri_ospeed = r.ioctl_ok ? baud : 0;
            r.aciklama = r.ioctl_ok ? "test kancasi" : "test kancasi reddetti";
            return r.ioctl_ok;
        }
        termios t{};
        if (tcgetattr(fd_, &t) != 0) { r.hata_errno = errno; r.aciklama = "tcgetattr"; return false; }
        cfmakeraw(&t);
        t.c_cflag |= (CLOCAL | CREAD | CS8);
        t.c_cflag &= ~static_cast<tcflag_t>(CSTOPB | PARENB | CRTSCTS);
        t.c_iflag &= ~static_cast<tcflag_t>(IXON | IXOFF | IXANY);
        t.c_cc[VMIN] = 0;
        t.c_cc[VTIME] = 0;
#if defined(__APPLE__)
        // termios ile bilinen bir hız kur, ardından IOSSIOSPEED ile istenen hızı (standart dışı dahil) ver.
        cfsetispeed(&t, B9600);
        cfsetospeed(&t, B9600);
        if (tcsetattr(fd_, TCSANOW, &t) != 0) { r.hata_errno = errno; r.aciklama = "tcsetattr"; return false; }
        r.tcsetattr_ok = true;
        speed_t s = static_cast<speed_t>(baud);
        if (ioctl(fd_, IOSSIOSPEED, &s) != 0) { r.hata_errno = errno; r.aciklama = "IOSSIOSPEED"; return false; }
        r.ioctl_ok = true;
#else
        (void)t;
        r.aciklama = "yalniz macOS gerceklendi";
        return false;
#endif
        termios g{};
        if (tcgetattr(fd_, &g) == 0) {
            r.geri_ispeed = static_cast<uint64_t>(cfgetispeed(&g));
            r.geri_ospeed = static_cast<uint64_t>(cfgetospeed(&g));
        }
        r.aciklama = "tamam";
        return true;
    }

    // Süre sınırı HER yazma denemesinden önce denetlenir (ilk deneme hariç): sürekli kısmi ilerleme olsa da toplam
    // süre aşılırsa ZamanAsimi döner; `yazilan` gerçekten yazılan bayttır.
    Io yaz(const uint8_t *p, size_t n, int zaman_asimi_ms, size_t &yazilan) override
    {
        yazilan = 0;
        const int64_t bitis = ms_simdi() + zaman_asimi_ms;
        bool ilk = true;
        while (yazilan < n) {
            if (!ilk && ms_simdi() >= bitis) { return Io::ZamanAsimi; }
            ilk = false;
            const ssize_t k = yazma_ ? static_cast<ssize_t>(yazma_(fd_, p + yazilan, n - yazilan))
                                     : ::write(fd_, p + yazilan, n - yazilan);
            if (k > 0) { yazilan += static_cast<size_t>(k); continue; }
            if (k < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) { return Io::Kopma; }
            const int64_t kalan = bitis - ms_simdi();
            if (kalan <= 0) { return Io::ZamanAsimi; }
            pollfd pf{fd_, POLLOUT, 0};
            const int r = ::poll(&pf, 1, static_cast<int>(kalan));
            if (r < 0 && errno != EINTR) { return Io::Kopma; }
            if (r > 0 && (pf.revents & (POLLHUP | POLLERR | POLLNVAL)) != 0) { return Io::Kopma; }
        }
        return Io::Tamam;
    }

    Io oku(uint8_t *p, size_t n, int zaman_asimi_ms, size_t &okunan) override
    {
        okunan = 0;
        const int64_t bitis = ms_simdi() + zaman_asimi_ms;
        bool okunur_dendi = false;   // poll POLLIN dedikten sonra read 0 dönerse dosya sonu/askı: kopma
        for (;;) {
            const ssize_t k = ::read(fd_, p, n);
            if (k > 0) { okunan = static_cast<size_t>(k); return Io::Tamam; }
            if (k == 0) {
                if (okunur_dendi) { return Io::Kopma; }
            } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                return Io::Kopma;
            }
            const int64_t kalan = bitis - ms_simdi();
            if (kalan <= 0) { return Io::ZamanAsimi; }
            pollfd pf{fd_, POLLIN, 0};
            const int r = ::poll(&pf, 1, static_cast<int>(kalan));
            if (r < 0 && errno != EINTR) { return Io::Kopma; }
            if (r > 0 && (pf.revents & (POLLHUP | POLLERR | POLLNVAL)) != 0) { return Io::Kopma; }
            if (r == 0) { return Io::ZamanAsimi; }
            okunur_dendi = r > 0 && (pf.revents & POLLIN) != 0;
        }
    }

    void giris_at() override
    {
        if (kanca_) {   // test: soket; bekleyen baytları oku ve at
            uint8_t b[4096];
            while (::read(fd_, b, sizeof b) > 0) {}
            return;
        }
        tcflush(fd_, TCIFLUSH);
    }
    std::string ad() const override { return yol_; }

private:
    int fd_;
    std::string yol_;
    std::function<bool(uint32_t)> kanca_;
    YazmaKanca yazma_;
};

}  // namespace

Port *posix_ac(const std::string &yol, std::string &hata)
{
    const int fd = ::open(yol.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) { hata = std::string("open: ") + std::strerror(errno); return nullptr; }
    if (ioctl(fd, TIOCEXCL) != 0) { hata = std::string("TIOCEXCL: ") + std::strerror(errno); ::close(fd); return nullptr; }
    return new PosixPort(fd, yol);
}

Port *posix_fd_ac(int fd, const std::string &ad, std::function<bool(uint32_t)> hiz_kanca, YazmaKanca yazma_kanca)
{
    const int fl = fcntl(fd, F_GETFL);
    if (fl < 0 || fcntl(fd, F_SETFL, fl | O_NONBLOCK) != 0) return nullptr;
    return new PosixPort(fd, ad, std::move(hiz_kanca), std::move(yazma_kanca));
}

}  // namespace mp3
