// tasima.cpp — 127.0.0.1 TCP soketi (tasima.hpp). POSIX (macOS) ve Winsock dalları.
#include "tasima.hpp"

#if defined(_WIN32)
// Winsock: <winsock2.h> <windows.h>'den önce gelmelidir; NOMINMAX min/max makrolarını,
// WIN32_LEAN_AND_MEAN eski winsock.h'yi dışarıda tutar. WSAPoll Vista ve sonrasında vardır.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>   // SetHandleInformation; winsock2.h'den sonra
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <climits>
#include <cstring>
#include <utility>

namespace ida::sil::tasima {

namespace {

// Engellenen gönderimde durdurma isteğinin en geç fark edilme süresi (duvar saati; çıktıyı
// etkilemez).
constexpr int kYazmaBeklemesiMs = 50;

#if defined(_WIN32)
using Uzunluk = int;
constexpr int kGonderBayrak = 0;

SOCKET yerli(Tutamac t)
{
    return static_cast<SOCKET>(t);
}

void tutamac_kapat(Tutamac t)
{
    (void)::closesocket(yerli(t));
}

// Soket alt süreçlere geçmez (ör. sınamanın başlattığı program dinleyiciyi devralmasın).
void miras_kapat(SOCKET s)
{
    (void)::SetHandleInformation(reinterpret_cast<HANDLE>(s), HANDLE_FLAG_INHERIT, 0);
}

// Engellemesiz kip: gönderim durdurma isteğiyle uyandırılabilsin (bekleme WSAPoll'da).
void engellemesiz(SOCKET s)
{
    u_long bir = 1;
    (void)::ioctlsocket(s, FIONBIO, &bir);
}

bool bekletir()
{
    return ::WSAGetLastError() == WSAEWOULDBLOCK;
}

int bekle(Tutamac t, short olay, int zaman_asimi_ms)
{
    WSAPOLLFD p{};
    p.fd = yerli(t);
    p.events = olay;
    return ::WSAPoll(&p, 1u, zaman_asimi_ms);
}
#else
using Uzunluk = socklen_t;
#if defined(MSG_NOSIGNAL)
constexpr int kGonderBayrak = MSG_NOSIGNAL;
#else
constexpr int kGonderBayrak = 0;
#endif

int yerli(Tutamac t)
{
    return t;
}

void tutamac_kapat(Tutamac t)
{
    (void)::close(t);
}

int bekle(Tutamac t, short olay, int zaman_asimi_ms)
{
    pollfd p{};
    p.fd = t;
    p.events = olay;
    return ::poll(&p, 1u, zaman_asimi_ms);
}

// Engellemesiz kip: gönderim durdurma isteğiyle uyandırılabilsin (bekleme poll'da).
void engellemesiz(int fd)
{
    const int bayrak = ::fcntl(fd, F_GETFL, 0);
    if (bayrak >= 0) {
        (void)::fcntl(fd, F_SETFL, bayrak | O_NONBLOCK);
    }
}

bool bekletir()
{
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

// Soket exec ile alt süreçlere geçmez (macOS'ta SOCK_CLOEXEC yok).
void miras_kapat(int fd)
{
    (void)::fcntl(fd, F_SETFD, FD_CLOEXEC);
}

// SIGPIPE yerine hata dönüşü (macOS: SO_NOSIGPIPE; Linux: MSG_NOSIGNAL).
void sigpipe_kapat(int fd)
{
#if defined(SO_NOSIGPIPE)
    int bir = 1;
    (void)::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &bir, static_cast<socklen_t>(sizeof bir));
#else
    (void)fd;
#endif
}
#endif

void nodelay_ac(Tutamac t)
{
    int bir = 1;
    (void)::setsockopt(yerli(t), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&bir),
                       static_cast<Uzunluk>(sizeof bir));
}

sockaddr_in geri_dongu(std::uint16_t port)
{
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return a;
}

}  // namespace

bool ag_baslat()
{
#if defined(_WIN32)
    struct Baslatici {
        bool tamam = false;
        Baslatici()
        {
            WSADATA veri{};
            tamam = ::WSAStartup(MAKEWORD(2, 2), &veri) == 0;
        }
        ~Baslatici()
        {
            if (tamam) {
                (void)::WSACleanup();
            }
        }
        Baslatici(const Baslatici&) = delete;
        Baslatici& operator=(const Baslatici&) = delete;
    };
    static const Baslatici kBaslatici;
    return kBaslatici.tamam;
#else
    return true;
#endif
}

// ---- Soket ---------------------------------------------------------------------------------

Tutamac Soket::gecersiz()
{
#if defined(_WIN32)
    return static_cast<Tutamac>(INVALID_SOCKET);
#else
    return -1;
#endif
}

Soket::~Soket()
{
    kapat();
}

Soket::Soket(Soket&& o) noexcept : t_(std::exchange(o.t_, gecersiz())) {}

Soket& Soket::operator=(Soket&& o) noexcept
{
    if (this != &o) {
        kapat();
        t_ = std::exchange(o.t_, gecersiz());
    }
    return *this;
}

bool Soket::gecerli() const
{
    return t_ != gecersiz();
}

Soket Soket::baglan(std::uint16_t port, std::string* hata)
{
    if (!ag_baslat()) {
        if (hata != nullptr) {
            *hata = "ağ kitaplığı başlatılamadı";
        }
        return Soket();
    }
#if defined(_WIN32)
    const SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        if (hata != nullptr) {
            *hata = "socket: " + std::to_string(::WSAGetLastError());
        }
        return Soket();
    }
    Soket sonuc(static_cast<Tutamac>(s));
    miras_kapat(s);
#else
    const int s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        if (hata != nullptr) {
            *hata = std::string("socket: ") + std::strerror(errno);
        }
        return Soket();
    }
    Soket sonuc(s);
    miras_kapat(s);
    sigpipe_kapat(s);
#endif
    const sockaddr_in a = geri_dongu(port);
    if (::connect(yerli(sonuc.t_), reinterpret_cast<const sockaddr*>(&a),
                  static_cast<Uzunluk>(sizeof a)) != 0) {
        if (hata != nullptr) {
#if defined(_WIN32)
            *hata = "connect: " + std::to_string(::WSAGetLastError());
#else
            *hata = std::string("connect: ") + std::strerror(errno);
#endif
        }
        return Soket();
    }
    nodelay_ac(sonuc.t_);
    engellemesiz(yerli(sonuc.t_));   // bağlantı engelleyici kipte kuruldu
    return sonuc;
}

long long Soket::oku(std::uint8_t* tampon, std::size_t kapasite, int zaman_asimi_ms)
{
    if (!gecerli()) {
        return -1;
    }
    const std::size_t istek = kapasite > static_cast<std::size_t>(INT_MAX)
                                  ? static_cast<std::size_t>(INT_MAX)
                                  : kapasite;
    const int r = bekle(t_, POLLIN, zaman_asimi_ms);
    if (r == 0) {
        return 0;
    }
#if defined(_WIN32)
    if (r < 0) {
        return -1;
    }
    const int n = ::recv(yerli(t_), reinterpret_cast<char*>(tampon), static_cast<int>(istek), 0);
    if (n > 0) {
        return static_cast<long long>(n);
    }
    if (n < 0 && bekletir()) {
        return 0;
    }
    return -1;   // 0: karşı taraf kapattı; < 0: hata
#else
    if (r < 0) {
        return errno == EINTR ? 0 : -1;
    }
    for (;;) {
        const ssize_t n = ::recv(yerli(t_), tampon, istek, 0);
        if (n > 0) {
            return static_cast<long long>(n);
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n < 0 && bekletir()) {
            return 0;
        }
        return -1;   // 0: karşı taraf kapattı; < 0: hata
    }
#endif
}

bool Soket::yaz(const std::uint8_t* veri, std::size_t n, const std::atomic<bool>* dur)
{
    std::size_t yazilan = 0;
    while (gecerli() && yazilan < n) {
        if (dur != nullptr && dur->load()) {
            return false;   // durdurma: kısmi yazım kalır, bağlantı bırakılır
        }
        const std::size_t kalan = n - yazilan;
        const std::size_t istek =
            kalan > static_cast<std::size_t>(INT_MAX) ? static_cast<std::size_t>(INT_MAX) : kalan;
#if defined(_WIN32)
        const int r = ::send(yerli(t_), reinterpret_cast<const char*>(veri + yazilan),
                             static_cast<int>(istek), kGonderBayrak);
        if (r < 0 && bekletir()) {
            (void)bekle(t_, POLLOUT, kYazmaBeklemesiMs);   // karşı taraf okuyunca ya da süre
            continue;
        }
        if (r <= 0) {
            return false;
        }
#else
        const ssize_t r = ::send(yerli(t_), veri + yazilan, istek, kGonderBayrak);
        if (r < 0 && errno == EINTR) {
            continue;
        }
        if (r < 0 && bekletir()) {
            (void)bekle(t_, POLLOUT, kYazmaBeklemesiMs);
            continue;
        }
        if (r <= 0) {
            return false;
        }
#endif
        yazilan += static_cast<std::size_t>(r);
    }
    return yazilan == n;
}

void Soket::alim_tamponu(int bayt)
{
    if (gecerli()) {
        (void)::setsockopt(yerli(t_), SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bayt),
                           static_cast<Uzunluk>(sizeof bayt));
    }
}

void Soket::yazmayi_kapat()
{
    if (gecerli()) {
#if defined(_WIN32)
        (void)::shutdown(yerli(t_), SD_SEND);
#else
        (void)::shutdown(yerli(t_), SHUT_WR);
#endif
    }
}

void Soket::kapat()
{
    if (gecerli()) {
        tutamac_kapat(t_);
        t_ = gecersiz();
    }
}

// ---- Dinleyici -----------------------------------------------------------------------------

Dinleyici::~Dinleyici()
{
    kapat();
}

Dinleyici::Dinleyici(Dinleyici&& o) noexcept
    : t_(std::exchange(o.t_, Soket::gecersiz())), port_(std::exchange(o.port_, 0))
{
}

Dinleyici& Dinleyici::operator=(Dinleyici&& o) noexcept
{
    if (this != &o) {
        kapat();
        t_ = std::exchange(o.t_, Soket::gecersiz());
        port_ = std::exchange(o.port_, 0);
    }
    return *this;
}

bool Dinleyici::gecerli() const
{
    return t_ != Soket::gecersiz();
}

Dinleyici Dinleyici::ac(std::uint16_t port, std::string* hata)
{
    Dinleyici d;
    if (!ag_baslat()) {
        if (hata != nullptr) {
            *hata = "ağ kitaplığı başlatılamadı";
        }
        return d;
    }
#if defined(_WIN32)
    const SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        return d;
    }
    d.t_ = static_cast<Tutamac>(s);
    miras_kapat(s);
#else
    const int s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        return d;
    }
    d.t_ = s;
    miras_kapat(s);
#endif
    const sockaddr_in a = geri_dongu(port);
    if (::bind(yerli(d.t_), reinterpret_cast<const sockaddr*>(&a), static_cast<Uzunluk>(sizeof a)) !=
            0 ||
        ::listen(yerli(d.t_), 4) != 0) {
        if (hata != nullptr) {
            *hata = "bind/listen başarısız";
        }
        d.kapat();
        return d;
    }
    sockaddr_in b{};
    Uzunluk boy = static_cast<Uzunluk>(sizeof b);
    if (::getsockname(yerli(d.t_), reinterpret_cast<sockaddr*>(&b), &boy) != 0) {
        d.kapat();
        return d;
    }
    d.port_ = ntohs(b.sin_port);
    return d;
}

Soket Dinleyici::kabul(int zaman_asimi_ms)
{
    if (!gecerli() || bekle(t_, POLLIN, zaman_asimi_ms) <= 0) {
        return Soket();
    }
#if defined(_WIN32)
    const SOCKET s = ::accept(yerli(t_), nullptr, nullptr);
    if (s == INVALID_SOCKET) {
        return Soket();
    }
    Soket sonuc(static_cast<Tutamac>(s));
    miras_kapat(s);
#else
    const int s = ::accept(yerli(t_), nullptr, nullptr);
    if (s < 0) {
        return Soket();
    }
    Soket sonuc(s);
    miras_kapat(s);
    sigpipe_kapat(s);
#endif
    nodelay_ac(sonuc.t_);
    engellemesiz(yerli(sonuc.t_));
    return sonuc;
}

void Dinleyici::kapat()
{
    if (gecerli()) {
        tutamac_kapat(t_);
        t_ = Soket::gecersiz();
    }
    port_ = 0;
}

}  // namespace ida::sil::tasima
