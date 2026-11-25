// soket.cpp — bkz. soket.hpp. POSIX (macOS, Linux) ve Winsock dalları.
#include "soket.hpp"

#include "saat.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
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

namespace ida::a2::ag {

namespace {

#if defined(_WIN32)
using Soket = SOCKET;
using UzTip = int;
inline Soket ham(Tutamac t) { return static_cast<Soket>(t); }
inline Tutamac sar(Soket s) { return static_cast<Tutamac>(s); }
bool bekle_hatasi() { return WSAGetLastError() == WSAEWOULDBLOCK; }
bool engellemesiz(Soket s)
{
    u_long bir = 1;
    return ioctlsocket(s, FIONBIO, &bir) == 0;
}
#else
using Soket = int;
using UzTip = socklen_t;
inline Soket ham(Tutamac t) { return t; }
inline Tutamac sar(Soket s) { return s; }
bool bekle_hatasi() { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
bool engellemesiz(Soket s)
{
    const int f = fcntl(s, F_GETFL, 0);
    return f >= 0 && fcntl(s, F_SETFL, f | O_NONBLOCK) == 0;
}
#endif

void ayar_ortak(Soket s)
{
    int bir = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&bir), static_cast<UzTip>(sizeof bir));
#if defined(__APPLE__)
    setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &bir, static_cast<UzTip>(sizeof bir));
#endif
}

sockaddr_in dongu_adresi(std::uint16_t port)
{
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // 127.0.0.1
    return a;
}

int yokla_ham(std::vector<Yoklama>& liste, int zaman_asimi_ms)
{
#if defined(_WIN32)
    std::vector<WSAPOLLFD> p(liste.size());
#else
    std::vector<pollfd> p(liste.size());
#endif
    for (std::size_t i = 0; i < liste.size(); ++i) {
        p[i].fd = ham(liste[i].t);
        p[i].events = static_cast<short>((liste[i].oku_iste ? POLLIN : 0) | (liste[i].yaz_iste ? POLLOUT : 0));
        p[i].revents = 0;
    }
#if defined(_WIN32)
    const int r = WSAPoll(p.data(), static_cast<ULONG>(p.size()), zaman_asimi_ms);
#else
    const int r = poll(p.data(), static_cast<nfds_t>(p.size()), zaman_asimi_ms);
#endif
    if (r < 0) {
#if !defined(_WIN32)
        if (errno == EINTR) {
            return 0;
        }
#endif
        return -1;
    }
    for (std::size_t i = 0; i < liste.size(); ++i) {
        const short re = p[i].revents;
        liste[i].okunabilir = (re & (POLLIN | POLLHUP)) != 0;
        liste[i].yazilabilir = (re & POLLOUT) != 0;
        liste[i].hata = (re & (POLLERR | POLLNVAL)) != 0;
    }
    return r;
}

}  // namespace

Tutamac gecersiz() noexcept
{
#if defined(_WIN32)
    return static_cast<Tutamac>(INVALID_SOCKET);
#else
    return -1;
#endif
}

bool baslat()
{
#if defined(_WIN32)
    static const bool tamam = [] {
        WSADATA w;
        return WSAStartup(MAKEWORD(2, 2), &w) == 0;
    }();
    return tamam;
#else
    return true;
#endif
}

bool dinle(std::uint16_t port, Tutamac& t, std::uint16_t& atanan, std::string& hata)
{
    t = gecersiz();
    atanan = 0;
    if (!baslat()) {
        hata = "ağ kitaplığı başlatılamadı";
        return false;
    }
    const Soket s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sar(s) == gecersiz()) {
        hata = "soket açılamadı";
        return false;
    }
#if defined(_WIN32)
    // Başka bir sürecin aynı portu ele geçirmesini önler.
    BOOL ozel = TRUE;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&ozel), static_cast<UzTip>(sizeof ozel));
#else
    int bir = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &bir, static_cast<UzTip>(sizeof bir));
#endif
    const sockaddr_in a = dongu_adresi(port);
    if (bind(s, reinterpret_cast<const sockaddr*>(&a), static_cast<UzTip>(sizeof a)) != 0 || listen(s, 8) != 0 ||
        !engellemesiz(s)) {
        hata = "A2 dinleyicisi açılamadı (port kullanımda olabilir)";
        kapat(sar(s));
        return false;
    }
    sockaddr_in ss{};
    UzTip uz = static_cast<UzTip>(sizeof ss);
    if (getsockname(s, reinterpret_cast<sockaddr*>(&ss), &uz) != 0) {
        hata = "atanan port okunamadı";
        kapat(sar(s));
        return false;
    }
    atanan = ntohs(ss.sin_port);
    t = sar(s);
    return true;
}

Tutamac kabul(Tutamac dinleyici)
{
    const Soket s = accept(ham(dinleyici), nullptr, nullptr);
    if (sar(s) == gecersiz()) {
        return gecersiz();
    }
    if (!engellemesiz(s)) {
        kapat(sar(s));
        return gecersiz();
    }
    ayar_ortak(s);
    return sar(s);
}

long long oku(Tutamac t, std::uint8_t* hedef, std::size_t n)
{
#if defined(_WIN32)
    const int istek = n > 0x7FFFFFFFu ? 0x7FFFFFFF : static_cast<int>(n);
    const int r = recv(ham(t), reinterpret_cast<char*>(hedef), istek, 0);
    if (r == SOCKET_ERROR) {
        return bekle_hatasi() ? -1 : -2;
    }
    return r;
#else
    const ssize_t r = recv(ham(t), hedef, n, 0);
    if (r < 0) {
        return bekle_hatasi() ? -1 : -2;
    }
    return static_cast<long long>(r);
#endif
}

long long yaz(Tutamac t, const std::uint8_t* veri, std::size_t n)
{
#if defined(_WIN32)
    const int istek = n > 0x7FFFFFFFu ? 0x7FFFFFFF : static_cast<int>(n);
    const int r = send(ham(t), reinterpret_cast<const char*>(veri), istek, 0);
    if (r == SOCKET_ERROR) {
        return bekle_hatasi() ? -1 : -2;
    }
    return r;
#else
#if defined(MSG_NOSIGNAL)
    const ssize_t r = send(ham(t), veri, n, MSG_NOSIGNAL);
#else
    const ssize_t r = send(ham(t), veri, n, 0);
#endif
    if (r < 0) {
        return bekle_hatasi() ? -1 : -2;
    }
    return static_cast<long long>(r);
#endif
}

void yazmayi_kapat(Tutamac t)
{
    if (t == gecersiz()) {
        return;
    }
#if defined(_WIN32)
    shutdown(ham(t), SD_SEND);
#else
    shutdown(ham(t), SHUT_WR);
#endif
}

void kapat(Tutamac t)
{
    if (t == gecersiz()) {
        return;
    }
#if defined(_WIN32)
    closesocket(ham(t));
#else
    close(ham(t));
#endif
}

int yokla(std::vector<Yoklama>& liste, int zaman_asimi_ms)
{
    if (liste.empty()) {
        ic::kisa_bekle_ms(zaman_asimi_ms);
        return 0;
    }
#if defined(_WIN32)
    const int r = yokla_ham(liste, 0);
    if (r != 0 || zaman_asimi_ms <= 0) {
        return r;
    }
    ic::kisa_bekle_ms(zaman_asimi_ms);
    return yokla_ham(liste, 0);
#else
    return yokla_ham(liste, zaman_asimi_ms);
#endif
}

Tutamac baglan(std::uint16_t port)
{
    if (!baslat()) {
        return gecersiz();
    }
    const Soket s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sar(s) == gecersiz()) {
        return gecersiz();
    }
    const sockaddr_in a = dongu_adresi(port);
    if (connect(s, reinterpret_cast<const sockaddr*>(&a), static_cast<UzTip>(sizeof a)) != 0 || !engellemesiz(s)) {
        kapat(sar(s));
        return gecersiz();
    }
    ayar_ortak(s);
    return sar(s);
}

void alim_tamponu(Tutamac t, int bayt)
{
    setsockopt(ham(t), SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bayt), static_cast<UzTip>(sizeof bayt));
}

void gonderim_tamponu(Tutamac t, int bayt)
{
    setsockopt(ham(t), SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bayt), static_cast<UzTip>(sizeof bayt));
}

}  // namespace ida::a2::ag
