#include "soket.hpp"

#include <cerrno>
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
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ida::viz {

namespace {

#if defined(_WIN32)
constexpr SoketTutamagi kGecersiz = static_cast<SoketTutamagi>(INVALID_SOCKET);
using SoketUzunlugu = int;  // Winsock: optlen ve namelen int
SOCKET yerel(SoketTutamagi t) { return static_cast<SOCKET>(t); }
int son_hata() { return WSAGetLastError(); }
bool bekle_hatasi(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }
void kapat_tutamak(SoketTutamagi t) { closesocket(yerel(t)); }
bool bloklamayan_yap(SoketTutamagi t) {
  u_long bir = 1;
  return ioctlsocket(yerel(t), FIONBIO, &bir) == 0;
}
#else
constexpr SoketTutamagi kGecersiz = -1;
using SoketUzunlugu = socklen_t;
int yerel(SoketTutamagi t) { return t; }
int son_hata() { return errno; }
bool bekle_hatasi(int e) { return e == EWOULDBLOCK || e == EAGAIN || e == EINPROGRESS; }
void kapat_tutamak(SoketTutamagi t) { ::close(t); }
bool bloklamayan_yap(SoketTutamagi t) {
  const int f = fcntl(t, F_GETFL, 0);
  return f >= 0 && fcntl(t, F_SETFL, f | O_NONBLOCK) == 0;
}
#endif

std::string hata_metni(const char* islem, int e) { return std::string(islem) + " hatası (" + std::to_string(e) + ")"; }

void nodelay(SoketTutamagi t) {
  int bir = 1;
  setsockopt(yerel(t), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&bir),
             static_cast<SoketUzunlugu>(sizeof bir));
#if defined(__APPLE__)
  // macOS'ta karşı taraf kapanınca yazma SIGPIPE üretmesin.
  setsockopt(yerel(t), SOL_SOCKET, SO_NOSIGPIPE, &bir, sizeof bir);
#endif
}

sockaddr_in geri_donus(std::uint16_t port) {
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // A2-K01: yalnız 127.0.0.1
  return a;
}

}  // namespace

bool soketleri_hazirla() {
#if defined(_WIN32)
  WSADATA w;
  return WSAStartup(MAKEWORD(2, 2), &w) == 0;
#else
  return true;
#endif
}

SoketTutamagi TcpSoket::gecersiz() { return kGecersiz; }

TcpSoket::~TcpSoket() { kapat(); }

TcpSoket::TcpSoket(TcpSoket&& o) noexcept : t_(o.t_), durum_(o.durum_) {
  o.t_ = kGecersiz;
  o.durum_ = Durum::kapali;
}

TcpSoket& TcpSoket::operator=(TcpSoket&& o) noexcept {
  if (this != &o) {
    kapat();
    t_ = o.t_;
    durum_ = o.durum_;
    o.t_ = kGecersiz;
    o.durum_ = Durum::kapali;
  }
  return *this;
}

TcpSoket TcpSoket::kabul_edilen(SoketTutamagi t) {
  TcpSoket s;
  s.t_ = t;
  s.durum_ = Durum::bagli;
  bloklamayan_yap(t);
  nodelay(t);
  return s;
}

void TcpSoket::kapat() {
  if (t_ != kGecersiz) {
    kapat_tutamak(t_);
    t_ = kGecersiz;
  }
  durum_ = Durum::kapali;
}

bool TcpSoket::baglan_baslat(std::uint16_t port, std::string& hata) {
  kapat();
#if defined(_WIN32)
  const SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) {
    hata = hata_metni("socket", son_hata());
    return false;
  }
  t_ = static_cast<SoketTutamagi>(s);
#else
  const int s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s < 0) {
    hata = hata_metni("socket", son_hata());
    return false;
  }
  t_ = s;
#endif
  if (!bloklamayan_yap(t_)) {
    hata = hata_metni("bloklamayan kip", son_hata());
    kapat();
    return false;
  }
  nodelay(t_);
  const sockaddr_in a = geri_donus(port);
  const int r = ::connect(yerel(t_), reinterpret_cast<const sockaddr*>(&a), static_cast<SoketUzunlugu>(sizeof a));
  if (r == 0) {
    durum_ = Durum::bagli;
    return true;
  }
  const int e = son_hata();
  if (!bekle_hatasi(e)) {
    hata = hata_metni("connect", e);
    kapat();
    return false;
  }
  durum_ = Durum::baglaniyor;
  return true;
}

TcpSoket::Durum TcpSoket::durum_yokla(std::string& hata) {
  if (durum_ != Durum::baglaniyor) return durum_;
#if defined(_WIN32)
  WSAPOLLFD p{};
  p.fd = yerel(t_);
  p.events = POLLWRNORM;
  const int r = ::WSAPoll(&p, 1, 0);
#else
  pollfd p{};
  p.fd = t_;
  p.events = POLLOUT;
  const int r = ::poll(&p, 1, 0);
#endif
  if (r < 0) {
    hata = hata_metni("poll", son_hata());
    kapat();
    return durum_;
  }
  if (r == 0) return durum_;
  int so_hata = 0;
  SoketUzunlugu uz = static_cast<SoketUzunlugu>(sizeof so_hata);
  getsockopt(yerel(t_), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_hata), &uz);
  if (so_hata != 0 || (p.revents & (POLLERR | POLLHUP)) != 0) {
    hata = hata_metni("bağlantı", so_hata);
    kapat();
    return durum_;
  }
  durum_ = Durum::bagli;
  return durum_;
}

bool TcpSoket::oku(Baytlar& out, std::size_t azami, std::string& hata) {
  if (durum_ != Durum::bagli) return false;
  // Tek çağrıda en çok `azami` bayt (sol #1: sınırsız biriktirme yok). Tampon çağıranın
  // vektörüdür; yığında büyük dizi yok (MSVC C6262).
  const std::size_t bas = out.size();
  out.resize(bas + azami);
  std::size_t alinan = 0;
  bool acik = true;
  while (alinan < azami) {
    Bayt* hedef = out.data() + bas + alinan;
    const std::size_t kalan = azami - alinan;
#if defined(_WIN32)
    const int istek = kalan > 0x7FFFFFFFu ? 0x7FFFFFFF : static_cast<int>(kalan);
    const int n = ::recv(yerel(t_), reinterpret_cast<char*>(hedef), istek, 0);
#else
    const ssize_t n = ::recv(t_, hedef, kalan, 0);
#endif
    if (n > 0) {
      alinan += static_cast<std::size_t>(n);
      continue;
    }
    if (n == 0) {
      hata = "karşı taraf bağlantıyı kapattı";
      acik = false;
      break;
    }
    const int e = son_hata();
    if (bekle_hatasi(e)) break;
#if !defined(_WIN32)
    if (e == EINTR) continue;
#endif
    hata = hata_metni("recv", e);
    acik = false;
    break;
  }
  out.resize(bas + alinan);
  if (!acik) kapat();
  return acik;
}

bool TcpSoket::yaz(Baytlar& tampon, std::string& hata) {
  if (durum_ != Durum::bagli) return false;
  std::size_t yazilan = 0;
  while (yazilan < tampon.size()) {
#if defined(_WIN32)
    const int n = ::send(yerel(t_), reinterpret_cast<const char*>(tampon.data() + yazilan),
                         static_cast<int>(tampon.size() - yazilan), 0);
#else
    const ssize_t n = ::send(t_, tampon.data() + yazilan, tampon.size() - yazilan, 0);
#endif
    if (n > 0) {
      yazilan += static_cast<std::size_t>(n);
      continue;
    }
    const int e = son_hata();
    if (bekle_hatasi(e)) break;
#if !defined(_WIN32)
    if (e == EINTR) continue;
#endif
    hata = hata_metni("send", e);
    kapat();
    return false;
  }
  tampon.erase(tampon.begin(), tampon.begin() + static_cast<std::ptrdiff_t>(yazilan));
  return true;
}

// ---------------------------------------------------------------------------

TcpDinleyici::~TcpDinleyici() { kapat(); }

void TcpDinleyici::kapat() {
  if (acik_) {
    kapat_tutamak(t_);
    acik_ = false;
  }
}

bool TcpDinleyici::ac(std::uint16_t port, std::string& hata) {
  kapat();
#if defined(_WIN32)
  const SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) {
    hata = hata_metni("socket", son_hata());
    return false;
  }
  t_ = static_cast<SoketTutamagi>(s);
#else
  const int s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s < 0) {
    hata = hata_metni("socket", son_hata());
    return false;
  }
  t_ = s;
  int bir = 1;
  setsockopt(t_, SOL_SOCKET, SO_REUSEADDR, &bir, sizeof bir);
#endif
  acik_ = true;
  const sockaddr_in a = geri_donus(port);
  if (::bind(yerel(t_), reinterpret_cast<const sockaddr*>(&a), static_cast<SoketUzunlugu>(sizeof a)) != 0) {
    hata = hata_metni("bind", son_hata());
    kapat();
    return false;
  }
  if (::listen(yerel(t_), 4) != 0) {
    hata = hata_metni("listen", son_hata());
    kapat();
    return false;
  }
  if (!bloklamayan_yap(t_)) {
    hata = hata_metni("bloklamayan kip", son_hata());
    kapat();
    return false;
  }
  return true;
}

bool TcpDinleyici::kabul(TcpSoket& out) {
  if (!acik_) return false;
#if defined(_WIN32)
  const SOCKET s = ::accept(yerel(t_), nullptr, nullptr);
  if (s == INVALID_SOCKET) return false;
  out = TcpSoket::kabul_edilen(static_cast<SoketTutamagi>(s));
#else
  const int s = ::accept(t_, nullptr, nullptr);
  if (s < 0) return false;
  out = TcpSoket::kabul_edilen(s);
#endif
  return true;
}

}  // namespace ida::viz
