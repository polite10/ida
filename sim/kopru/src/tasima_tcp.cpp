// SIL taşıması: 127.0.0.1 üzerinde TCP (A1-K97, K99). POSIX ve Winsock dalları.
//
// Winsock: <winsock2.h> <windows.h>'den önce gelir; NOMINMAX min/max makrolarını,
// WIN32_LEAN_AND_MEAN eski winsock.h'yi dışarıda tutar; bekleme WSAPoll'dur (ms çözünürlüğü,
// yukarı yuvarlanır). POSIX'te bekleme select'tir (µs). Windows dalı bu işte derlenmedi (CI).
#include "ida/kopru/tasima.hpp"

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
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cstddef>
#include <string>

namespace ida::kopru {
namespace {

#if defined(_WIN32)
using Soket = SOCKET;
using Uzunluk = int;
const Soket kGecersiz = INVALID_SOCKET;

bool ag_baslat(std::string& hata)
{
    static const int sonuc = [] {
        WSADATA w{};
        return WSAStartup(MAKEWORD(2, 2), &w);
    }();
    if (sonuc != 0) {
        hata = "WSAStartup: " + std::to_string(sonuc);
        return false;
    }
    return true;
}
void soket_kapat(Soket s) { (void)closesocket(s); }
int son_hata() { return WSAGetLastError(); }
bool engelsiz(Soket s)
{
    u_long bir = 1;
    return ioctlsocket(s, FIONBIO, &bir) == 0;
}
bool bekliyor_hatasi(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS || e == WSAEINTR; }
void miras_kapat(Soket s) { (void)SetHandleInformation(reinterpret_cast<HANDLE>(s), HANDLE_FLAG_INHERIT, 0); }

// 1: hazır, 0: zaman aşımı, -1: hata.
int bekle(Soket s, bool yazma, std::int64_t bekle_us)
{
    WSAPOLLFD p{};
    p.fd = s;
    p.events = yazma ? POLLWRNORM : POLLRDNORM;
    const std::int64_t ms = std::max<std::int64_t>((bekle_us + 999) / 1000, 0);
    const int r = WSAPoll(&p, 1u, static_cast<INT>(std::min<std::int64_t>(ms, 60'000)));
    if (r < 0) {
        return -1;
    }
    return r > 0 ? 1 : 0;
}
#else
using Soket = int;
using Uzunluk = socklen_t;
constexpr Soket kGecersiz = -1;
bool ag_baslat(std::string&) { return true; }
void soket_kapat(Soket s) { ::close(s); }
int son_hata() { return errno; }
bool engelsiz(Soket s)
{
    const int fl = fcntl(s, F_GETFL);
    return fl >= 0 && fcntl(s, F_SETFL, fl | O_NONBLOCK) == 0;
}
bool bekliyor_hatasi(int e) { return e == EAGAIN || e == EWOULDBLOCK || e == EINPROGRESS || e == EINTR; }
// Soket alt süreçlere geçmez (sim'in başlattığı koşucu dinleyiciyi devralmasın).
void miras_kapat(Soket s) { (void)fcntl(s, F_SETFD, FD_CLOEXEC); }

int bekle(Soket s, bool yazma, std::int64_t bekle_us)
{
    fd_set k;
    FD_ZERO(&k);
    FD_SET(s, &k);
    timeval tv{};
    const std::int64_t us = std::max<std::int64_t>(bekle_us, 0);
    tv.tv_sec = static_cast<decltype(tv.tv_sec)>(us / 1'000'000);
    tv.tv_usec = static_cast<decltype(tv.tv_usec)>(us % 1'000'000);
    const int r = select(s + 1, yazma ? nullptr : &k, yazma ? &k : nullptr, nullptr, &tv);
    if (r < 0) {
        return errno == EINTR ? 0 : -1;
    }
    return r > 0 ? 1 : 0;
}
#endif

std::string hata_metni(const char* islem)
{
    return std::string(islem) + ": hata " + std::to_string(son_hata());
}

void gecikmesiz(Soket s)
{
    int bir = 1;
    (void)setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&bir),
                     static_cast<Uzunluk>(sizeof bir));
#if defined(__APPLE__)
    (void)setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &bir, static_cast<Uzunluk>(sizeof bir));
#endif
}

class TcpUcu final : public Tasima {
public:
    TcpUcu(Soket s, std::string ad) : s_(s), ad_(std::move(ad)) {}
    ~TcpUcu() override { kapat(); }

    long long yaz_biraz(const std::uint8_t* p, std::size_t n) override
    {
        if (s_ == kGecersiz) {
            return -1;
        }
        const std::size_t kalan = std::min<std::size_t>(n, std::size_t{1} << 20U);
#if defined(_WIN32)
        const int k = ::send(s_, reinterpret_cast<const char*>(p), static_cast<int>(kalan), 0);
#elif defined(MSG_NOSIGNAL)
        const ssize_t k = ::send(s_, p, kalan, MSG_NOSIGNAL);
#else
        const ssize_t k = ::send(s_, p, kalan, 0);
#endif
        if (k >= 0) {
            return static_cast<long long>(k);
        }
        if (bekliyor_hatasi(son_hata())) {
            return 0;
        }
        kapat();
        return -1;
    }

    OkumaSonucu oku(std::uint8_t* p, std::size_t n, std::int64_t bekle_us, std::size_t& okunan) override
    {
        okunan = 0;
        if (s_ == kGecersiz) {
            return OkumaSonucu::kapandi;
        }
        const std::size_t istek = std::min<std::size_t>(n, std::size_t{1} << 20U);
        for (int deneme = 0; deneme < 2; ++deneme) {
#if defined(_WIN32)
            const int k = ::recv(s_, reinterpret_cast<char*>(p), static_cast<int>(istek), 0);
#else
            const ssize_t k = ::recv(s_, p, istek, 0);
#endif
            if (k > 0) {
                okunan = static_cast<std::size_t>(k);
                return OkumaSonucu::tamam;
            }
            if (k == 0 || !bekliyor_hatasi(son_hata())) {
                kapat();
                return OkumaSonucu::kapandi;
            }
            if (deneme == 1 || bekle_us <= 0) {
                break;
            }
            const int r = bekle(s_, false, bekle_us);
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

    void giris_bosalt() override {}
    bool acik() const override { return s_ != kGecersiz; }
    void kapat() override
    {
        if (s_ != kGecersiz) {
            soket_kapat(s_);
            s_ = kGecersiz;
        }
    }
    std::string ad() const override { return ad_; }

private:
    Soket s_;
    std::string ad_;
};

class TcpDinleyiciGercek final : public TcpDinleyici {
public:
    TcpDinleyiciGercek(Soket s, std::uint16_t port) : s_(s), port_(port) {}
    ~TcpDinleyiciGercek() override { soket_kapat(s_); }
    std::uint16_t port() const override { return port_; }

    std::unique_ptr<Tasima> kabul(std::int64_t bekle_us, std::string& hata) override
    {
        const int r = bekle(s_, false, bekle_us);
        if (r <= 0) {
            hata = r < 0 ? hata_metni("bekleme") : "zaman aşımı";
            return nullptr;
        }
        const Soket c = ::accept(s_, nullptr, nullptr);
        if (c == kGecersiz) {
            hata = hata_metni("accept");
            return nullptr;
        }
        miras_kapat(c);
        if (!engelsiz(c)) {
            hata = hata_metni("engelsiz kip");
            soket_kapat(c);
            return nullptr;
        }
        gecikmesiz(c);
        return std::make_unique<TcpUcu>(c, "tcp:127.0.0.1:" + std::to_string(port_));
    }

private:
    Soket s_;
    std::uint16_t port_;
};

sockaddr_in yerel_adres(std::uint16_t port)
{
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return a;
}

}  // namespace

std::unique_ptr<TcpDinleyici> tcp_dinle(std::uint16_t port, std::string& hata)
{
    if (!ag_baslat(hata)) {
        return nullptr;
    }
    const Soket s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == kGecersiz) {
        hata = hata_metni("socket");
        return nullptr;
    }
    miras_kapat(s);
    int bir = 1;
    (void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&bir),
                     static_cast<Uzunluk>(sizeof bir));
    const sockaddr_in a = yerel_adres(port);
    if (::bind(s, reinterpret_cast<const sockaddr*>(&a), static_cast<Uzunluk>(sizeof a)) != 0 ||
        ::listen(s, 1) != 0) {
        hata = hata_metni("bind/listen");
        soket_kapat(s);
        return nullptr;
    }
    sockaddr_in g{};
    Uzunluk boy = static_cast<Uzunluk>(sizeof g);
    if (::getsockname(s, reinterpret_cast<sockaddr*>(&g), &boy) != 0) {
        hata = hata_metni("getsockname");
        soket_kapat(s);
        return nullptr;
    }
    return std::make_unique<TcpDinleyiciGercek>(s, ntohs(g.sin_port));
}

std::unique_ptr<Tasima> tcp_baglan(std::uint16_t port, std::string& hata)
{
    if (!ag_baslat(hata)) {
        return nullptr;
    }
    const Soket s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == kGecersiz) {
        hata = hata_metni("socket");
        return nullptr;
    }
    miras_kapat(s);
    const sockaddr_in a = yerel_adres(port);
    if (::connect(s, reinterpret_cast<const sockaddr*>(&a), static_cast<Uzunluk>(sizeof a)) != 0) {
        hata = hata_metni("connect");
        soket_kapat(s);
        return nullptr;
    }
    if (!engelsiz(s)) {
        hata = hata_metni("engelsiz kip");
        soket_kapat(s);
        return nullptr;
    }
    gecikmesiz(s);
    return std::make_unique<TcpUcu>(s, "tcp:127.0.0.1:" + std::to_string(port));
}

}  // namespace ida::kopru
