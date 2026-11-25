// soket.hpp — engellemesiz TCP soketleri ve yoklama (poll / WSAPoll); yalnız 127.0.0.1 (A2-K01).
// macOS ve Windows (Winsock) dalları soket.cpp'dedir; bu başlık işletim sistemi başlığı içermez.
// (`sim/a3` ve `viz/a2/soket` ile ortaklaştırma ayrı iştir; W5-a2-sunucu "Dosya sahipliği".)
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ida::a2::ag {

#if defined(_WIN32)
using Tutamac = std::uintptr_t;  // SOCKET
#else
using Tutamac = int;
#endif

Tutamac gecersiz() noexcept;

// Süreç başına bir kez (Windows'ta WSAStartup); başarısızsa false.
bool baslat();

// 127.0.0.1:port'ta dinler (başka arabirime bağlanılmaz: A2-K01). port = 0: işletim sistemi atar.
// Port kullanımdaysa false (A2-K05).
bool dinle(std::uint16_t port, Tutamac& t, std::uint16_t& atanan, std::string& hata);

// Engellemesiz kabul; bekleyen bağlantı yoksa gecersiz(). TCP_NODELAY açılır (A2-K04).
Tutamac kabul(Tutamac dinleyici);

// > 0 okunan; 0 karşı taraf kapattı; -1 şimdilik veri yok; -2 hata.
long long oku(Tutamac t, std::uint8_t* hedef, std::size_t n);
// >= 0 yazılan; -1 tampon dolu (sonra dene); -2 hata.
long long yaz(Tutamac t, const std::uint8_t* veri, std::size_t n);
// Yazma yönünü kapatır (FIN): son ileti karşıya ulaşsın diye kapanıştan önce çağrılır.
void yazmayi_kapat(Tutamac t);
void kapat(Tutamac t);

struct Yoklama {
    Tutamac t;
    bool oku_iste = true;
    bool yaz_iste = false;
    bool okunabilir = false;
    bool yazilabilir = false;
    bool hata = false;
};
// En çok `zaman_asimi_ms` bekler; dönüş < 0 ise hata. Windows'ta bekleme yüksek çözünürlüklü
// zamanlayıcıyla yapılır (WSAPoll zaman aşımı ≈ 15,6 ms'ye yuvarlanır): önce beklemeden yoklanır,
// hazır soket yoksa beklenir ve yeniden yoklanır.
int yokla(std::vector<Yoklama>& liste, int zaman_asimi_ms);

// Sınamalar için: 127.0.0.1:port'a bağlanır (engelleyici bağlanma, sonra engellemesiz).
Tutamac baglan(std::uint16_t port);
// SO_RCVBUF / SO_SNDBUF (bayt).
void alim_tamponu(Tutamac t, int bayt);
void gonderim_tamponu(Tutamac t, int bayt);

}  // namespace ida::a2::ag
