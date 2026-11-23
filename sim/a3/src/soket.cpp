// soket.cpp — bkz. soket.hpp. POSIX (macOS, Linux) ve Winsock dalları.
#include "soket.hpp"

#include <cstring>

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

namespace ida::a3::ag {

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

bool dinle(const std::string& adres, std::uint16_t port, Tutamac& t, std::uint16_t& atanan, std::string& hata)
{
    t = gecersiz();
    atanan = 0;
    // A3-K02, K03: yalnız döngü arabirimi; ad çözümlemesi yapılmaz.
    const bool v4 = adres == "127.0.0.1";
    const bool v6 = adres == "::1";
    if (!v4 && !v6) {
        hata = "A3-K02: yalnız 127.0.0.1 ya da ::1 adresinde dinlenir; verilen adres reddedildi";
        return false;
    }
    if (!baslat()) {
        hata = "ağ kitaplığı başlatılamadı";
        return false;
    }
    const Soket s = socket(v4 ? AF_INET : AF_INET6, SOCK_STREAM, IPPROTO_TCP);
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
    int sonuc = -1;
    if (v4) {
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
        sonuc = bind(s, reinterpret_cast<const sockaddr*>(&a), static_cast<UzTip>(sizeof a));
    } else {
        sockaddr_in6 a{};
        a.sin6_family = AF_INET6;
        a.sin6_port = htons(port);
        a.sin6_addr.s6_addr[15] = 1;  // ::1 (sıfırla ilklendirildi)
        int yalniz6 = 1;
        setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&yalniz6), static_cast<UzTip>(sizeof yalniz6));
        sonuc = bind(s, reinterpret_cast<const sockaddr*>(&a), static_cast<UzTip>(sizeof a));
    }
    if (sonuc != 0 || listen(s, 64) != 0 || !engellemesiz(s)) {
        hata = "dinleme başlatılamadı (port kullanımda olabilir)";
        kapat(sar(s));
        return false;
    }
    sockaddr_storage ss{};
    UzTip uz = static_cast<UzTip>(sizeof ss);
    if (getsockname(s, reinterpret_cast<sockaddr*>(&ss), &uz) != 0) {
        hata = "atanan port okunamadı";
        kapat(sar(s));
        return false;
    }
    atanan = ss.ss_family == AF_INET ? ntohs(reinterpret_cast<const sockaddr_in*>(&ss)->sin_port)
                                     : ntohs(reinterpret_cast<const sockaddr_in6*>(&ss)->sin6_port);
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
    if (p.empty()) {
#if defined(_WIN32)
        Sleep(static_cast<DWORD>(zaman_asimi_ms < 0 ? 0 : zaman_asimi_ms));
#else
        poll(nullptr, 0, zaman_asimi_ms);
#endif
        return 0;
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

Tutamac baglan(const std::string& adres, std::uint16_t port)
{
    if (!baslat()) {
        return gecersiz();
    }
    const bool v6 = adres == "::1";
    const Soket s = socket(v6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sar(s) == gecersiz()) {
        return gecersiz();
    }
    int sonuc = -1;
    if (v6) {
        sockaddr_in6 a{};
        a.sin6_family = AF_INET6;
        a.sin6_port = htons(port);
        a.sin6_addr.s6_addr[15] = 1;  // ::1
        sonuc = connect(s, reinterpret_cast<const sockaddr*>(&a), static_cast<UzTip>(sizeof a));
    } else {
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(port);
        if (inet_pton(AF_INET, adres.c_str(), &a.sin_addr) != 1) {
            kapat(sar(s));
            return gecersiz();
        }
        sonuc = connect(s, reinterpret_cast<const sockaddr*>(&a), static_cast<UzTip>(sizeof a));
    }
    if (sonuc != 0 || !engellemesiz(s)) {
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

}  // namespace ida::a3::ag
