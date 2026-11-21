// HIL taşıması: ST-LINK sanal seri portu (A1-K01). POSIX (macOS) ve Windows dalları.
//
// POSIX ayarları W0-MP3'ün PC tarafından alındı (`olcum/mp3-vcp` dalı,
// tools/olcum/mp3/pc/seri_posix.cpp; 3 000 000 baud'da 600 s kayıpsız): O_NONBLOCK + TIOCEXCL,
// cfmakeraw, CLOCAL | CREAD | CS8, CSTOPB/PARENB/CRTSCTS ve IXON/IXOFF/IXANY kapalı,
// VMIN = VTIME = 0; macOS'ta önce termios ile bilinen bir hız, sonra IOSSIOSPEED ile istenen
// hız. Windows dalı bu işte derlenmedi ve sınanmadı (CI derler).
#include "ida/kopru/tasima.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <IOKit/serial/ioss.h>
#endif
#endif

#include <algorithm>
#include <string>

namespace ida::kopru {
namespace {

#if defined(_WIN32)

class SeriPort final : public Tasima {
public:
    SeriPort(HANDLE h, std::string ad) : h_(h), ad_(std::move(ad)) {}
    ~SeriPort() override { kapat(); }

    bool yaz(const std::uint8_t* p, std::size_t n) override
    {
        std::size_t yazilan = 0;
        while (yazilan < n && h_ != INVALID_HANDLE_VALUE) {
            DWORD k = 0;
            const DWORD istek = static_cast<DWORD>(std::min<std::size_t>(n - yazilan, 1u << 16));
            if (WriteFile(h_, p + yazilan, istek, &k, nullptr) == 0) {
                kapat();
                return false;
            }
            yazilan += static_cast<std::size_t>(k);
        }
        return yazilan == n;
    }

    OkumaSonucu oku(std::uint8_t* p, std::size_t n, std::int64_t bekle_us, std::size_t& okunan) override
    {
        okunan = 0;
        if (h_ == INVALID_HANDLE_VALUE) {
            return OkumaSonucu::kapandi;
        }
        // Bir bayt gelince hemen dön; gelmezse en çok bekle_us (ms'ye yukarı yuvarlanır).
        const DWORD ms = static_cast<DWORD>(std::max<std::int64_t>((bekle_us + 999) / 1000, 0));
        if (ms != son_ms_) {
            COMMTIMEOUTS z{};
            z.ReadIntervalTimeout = MAXDWORD;
            z.ReadTotalTimeoutMultiplier = ms == 0 ? 0 : MAXDWORD;
            z.ReadTotalTimeoutConstant = ms;
            z.WriteTotalTimeoutConstant = 1000;
            if (SetCommTimeouts(h_, &z) == 0) {
                kapat();
                return OkumaSonucu::kapandi;
            }
            son_ms_ = ms;
        }
        DWORD k = 0;
        const DWORD istek = static_cast<DWORD>(std::min<std::size_t>(n, 1u << 16));
        if (ReadFile(h_, p, istek, &k, nullptr) == 0) {
            kapat();
            return OkumaSonucu::kapandi;
        }
        okunan = static_cast<std::size_t>(k);
        return okunan > 0 ? OkumaSonucu::tamam : OkumaSonucu::zaman_asimi;
    }

    void giris_bosalt() override
    {
        if (h_ != INVALID_HANDLE_VALUE) {
            (void)PurgeComm(h_, PURGE_RXCLEAR | PURGE_RXABORT);
        }
    }
    bool acik() const override { return h_ != INVALID_HANDLE_VALUE; }
    void kapat() override
    {
        if (h_ != INVALID_HANDLE_VALUE) {
            CloseHandle(h_);
            h_ = INVALID_HANDLE_VALUE;
        }
    }
    std::string ad() const override { return ad_; }

private:
    HANDLE h_;
    std::string ad_;
    DWORD son_ms_ = 0xFFFFFFFFu;
};

#else

class SeriPort final : public Tasima {
public:
    SeriPort(int fd, std::string ad) : fd_(fd), ad_(std::move(ad)) {}
    ~SeriPort() override { kapat(); }

    bool yaz(const std::uint8_t* p, std::size_t n) override
    {
        std::size_t yazilan = 0;
        while (yazilan < n && fd_ >= 0) {
            const ssize_t k = ::write(fd_, p + yazilan, n - yazilan);
            if (k > 0) {
                yazilan += static_cast<std::size_t>(k);
                continue;
            }
            if (k < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                if (sec(true, 1'000'000) < 0) {
                    kapat();
                    return false;
                }
                continue;
            }
            kapat();
            return false;
        }
        return yazilan == n;
    }

    OkumaSonucu oku(std::uint8_t* p, std::size_t n, std::int64_t bekle_us, std::size_t& okunan) override
    {
        okunan = 0;
        if (fd_ < 0) {
            return OkumaSonucu::kapandi;
        }
        for (int deneme = 0; deneme < 2; ++deneme) {
            const ssize_t k = ::read(fd_, p, n);
            if (k > 0) {
                okunan = static_cast<std::size_t>(k);
                return OkumaSonucu::tamam;
            }
            if (k < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                kapat();   // aygıt kalktı (A1-K23)
                return OkumaSonucu::kapandi;
            }
            if (deneme == 1 || bekle_us <= 0) {
                break;
            }
            const int r = sec(false, bekle_us);
            if (r < 0) {
                kapat();
                return OkumaSonucu::kapandi;
            }
            if (r == 0) {
                break;
            }
        }
        return OkumaSonucu::zaman_asimi;
    }

    void giris_bosalt() override
    {
        if (fd_ >= 0) {
            (void)tcflush(fd_, TCIFLUSH);
        }
    }
    bool acik() const override { return fd_ >= 0; }
    void kapat() override
    {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }
    std::string ad() const override { return ad_; }

private:
    int sec(bool yazma, std::int64_t bekle_us)
    {
        fd_set k;
        FD_ZERO(&k);
        FD_SET(fd_, &k);
        timeval tv{};
        tv.tv_sec = static_cast<decltype(tv.tv_sec)>(bekle_us / 1'000'000);
        tv.tv_usec = static_cast<decltype(tv.tv_usec)>(bekle_us % 1'000'000);
        const int r = ::select(fd_ + 1, yazma ? nullptr : &k, yazma ? &k : nullptr, nullptr, &tv);
        if (r < 0 && errno == EINTR) {
            return 0;
        }
        return r;
    }

    int fd_;
    std::string ad_;
};

#endif

}  // namespace

SozdeUcbirim sozde_ucbirim_ac(std::string& hata)
{
    SozdeUcbirim s;
#if defined(_WIN32)
    hata = "sözde uçbirim Windows'ta yok";
#else
    const int ana = posix_openpt(O_RDWR | O_NOCTTY);
    if (ana < 0 || grantpt(ana) != 0 || unlockpt(ana) != 0) {
        hata = std::string("posix_openpt: ") + std::strerror(errno);
        if (ana >= 0) {
            ::close(ana);
        }
        return s;
    }
    const char* ad = ptsname(ana);
    if (ad == nullptr) {
        hata = "ptsname";
        ::close(ana);
        return s;
    }
    s.yol = ad;
    // Ana ucun kendi satır disiplini yoktur; yalnız engelsiz kip.
    const int fl = fcntl(ana, F_GETFL);
    (void)fcntl(ana, F_SETFL, fl | O_NONBLOCK);
    s.ana = std::make_unique<SeriPort>(ana, "pty-ana");
#endif
    return s;
}

std::unique_ptr<Tasima> seri_ac(const std::string& yol, const SeriAyar& ayar, std::string& hata)
{
#if defined(_WIN32)
    const std::string ad = yol.rfind("\\\\.\\", 0) == 0 ? yol : "\\\\.\\" + yol;
    HANDLE h = CreateFileA(ad.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        hata = "CreateFile: hata " + std::to_string(GetLastError());
        return nullptr;
    }
    DCB d{};
    d.DCBlength = sizeof d;
    if (GetCommState(h, &d) == 0) {
        hata = "GetCommState: hata " + std::to_string(GetLastError());
        CloseHandle(h);
        return nullptr;
    }
    d.BaudRate = ayar.baud;
    d.ByteSize = 8;
    d.Parity = NOPARITY;
    d.StopBits = ONESTOPBIT;
    d.fBinary = TRUE;
    d.fParity = FALSE;
    d.fOutxCtsFlow = FALSE;
    d.fOutxDsrFlow = FALSE;
    d.fDsrSensitivity = FALSE;
    d.fOutX = FALSE;
    d.fInX = FALSE;
    d.fNull = FALSE;
    d.fAbortOnError = FALSE;
    d.fRtsControl = RTS_CONTROL_ENABLE;
    d.fDtrControl = DTR_CONTROL_ENABLE;
    if (SetCommState(h, &d) == 0) {
        if (ayar.hiz_zorunlu) {
            hata = "SetCommState: hata " + std::to_string(GetLastError());
            CloseHandle(h);
            return nullptr;
        }
        hata = "uyarı: hız kurulamadı";
    }
    return std::make_unique<SeriPort>(h, yol);
#else
    const int fd = ::open(yol.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        hata = std::string("open: ") + std::strerror(errno);
        return nullptr;
    }
    if (ioctl(fd, TIOCEXCL) != 0) {
        hata = std::string("TIOCEXCL: ") + std::strerror(errno);
        ::close(fd);
        return nullptr;
    }
    termios t{};
    if (tcgetattr(fd, &t) != 0) {
        hata = std::string("tcgetattr: ") + std::strerror(errno);
        ::close(fd);
        return nullptr;
    }
    cfmakeraw(&t);
    t.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD | CS8);
    t.c_cflag &= ~static_cast<tcflag_t>(CSTOPB | PARENB | CRTSCTS);
    t.c_iflag &= ~static_cast<tcflag_t>(IXON | IXOFF | IXANY);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    bool hiz_tamam = false;
    std::string hiz_hatasi;
#if defined(__APPLE__)
    cfsetispeed(&t, B9600);
    cfsetospeed(&t, B9600);
    if (tcsetattr(fd, TCSANOW, &t) != 0) {
        hata = std::string("tcsetattr: ") + std::strerror(errno);
        ::close(fd);
        return nullptr;
    }
    speed_t s = static_cast<speed_t>(ayar.baud);
    if (ioctl(fd, IOSSIOSPEED, &s) == 0) {
        hiz_tamam = true;
    } else {
        hiz_hatasi = std::string("IOSSIOSPEED: ") + std::strerror(errno);
    }
#else
#if defined(B3000000)
    if (ayar.baud == 3'000'000u && cfsetspeed(&t, B3000000) == 0) {
        hiz_tamam = true;
    } else {
        hiz_hatasi = "hız desteklenmiyor";
    }
#else
    hiz_hatasi = "hız desteklenmiyor";
#endif
    if (tcsetattr(fd, TCSANOW, &t) != 0) {
        hata = std::string("tcsetattr: ") + std::strerror(errno);
        ::close(fd);
        return nullptr;
    }
#endif
    if (!hiz_tamam) {
        if (ayar.hiz_zorunlu) {
            hata = hiz_hatasi;
            ::close(fd);
            return nullptr;
        }
        hata = "uyarı: " + hiz_hatasi;
    }
    (void)tcflush(fd, TCIOFLUSH);
    return std::make_unique<SeriPort>(fd, yol);
#endif
}

}  // namespace ida::kopru
