// soket.hpp — engellemesiz TCP soketleri ve yoklama (poll / WSAPoll). macOS ve Windows
// (Winsock) dalları soket.cpp'dedir; bu başlık işletim sistemi başlığı içermez.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ida::a3::ag {

#if defined(_WIN32)
using Tutamac = std::uintptr_t;  // SOCKET
#else
using Tutamac = int;
#endif

Tutamac gecersiz() noexcept;

// Süreç başına bir kez (Windows'ta WSAStartup); başarısızsa false.
bool baslat();

// A3-K02: yalnız "127.0.0.1" ya da "::1". Başka adreste dinlemez ve false döner.
bool dinle(const std::string& adres, std::uint16_t port, Tutamac& t, std::uint16_t& atanan, std::string& hata);

// Engellemesiz kabul; bekleyen bağlantı yoksa gecersiz(). TCP_NODELAY açılır.
Tutamac kabul(Tutamac dinleyici);

// > 0 okunan; 0 karşı taraf kapattı; -1 şimdilik veri yok; -2 hata.
long long oku(Tutamac t, std::uint8_t* hedef, std::size_t n);
// >= 0 yazılan; -1 tampon dolu (sonra dene); -2 hata.
long long yaz(Tutamac t, const std::uint8_t* veri, std::size_t n);
void kapat(Tutamac t);

struct Yoklama {
    Tutamac t;
    bool oku_iste = true;
    bool yaz_iste = false;
    bool okunabilir = false;
    bool yazilabilir = false;
    bool hata = false;
};
// Yoklar; dönüş < 0 ise hata.
int yokla(std::vector<Yoklama>& liste, int zaman_asimi_ms);

// Sınamalar için: adrese bağlanır (engelleyici bağlanma, sonra engellemesiz). Başarısızsa gecersiz().
Tutamac baglan(const std::string& adres, std::uint16_t port);
// SO_RCVBUF / SO_SNDBUF (bayt).
void alim_tamponu(Tutamac t, int bayt);
void gonderim_tamponu(Tutamac t, int bayt);

}  // namespace ida::a3::ag
