// tasima.hpp — SIL taşıması: yalnız 127.0.0.1 üzerinde TCP (A1-K97). macOS ve Windows
// (Winsock) dalları tasima.cpp'dedir; bu başlık işletim sistemi başlığı içermez.
//
// Soket engelleyicidir; okuma, zaman aşımlı bekleme (poll / WSAPoll) ile yapılır. Zaman aşımı
// yalnız durdurma isteğini görmek içindir: koşucunun davranışı ve çıktısı ona bağlı değildir.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

namespace ida::sil::tasima {

#if defined(_WIN32)
using Tutamac = std::uintptr_t;   // SOCKET (UINT_PTR)
#else
using Tutamac = int;
#endif

// Süreç başına bir kez ağ kitaplığını başlatır (Windows'ta WSAStartup); başarısızsa false.
bool ag_baslat();

class Soket {
public:
    Soket() = default;
    ~Soket();
    Soket(const Soket&) = delete;
    Soket& operator=(const Soket&) = delete;
    Soket(Soket&& o) noexcept;
    Soket& operator=(Soket&& o) noexcept;

    // 127.0.0.1:port'a bağlanır, TCP_NODELAY açar (A1-K97). Başarısızsa geçersiz soket.
    static Soket baglan(std::uint16_t port, std::string* hata = nullptr);

    bool gecerli() const;

    // > 0: okunan bayt; 0: zaman aşımı (veri yok); < 0: bağlantı kapandı ya da hata.
    long long oku(std::uint8_t* tampon, std::size_t kapasite, int zaman_asimi_ms);

    // Bütün baytları yazar; bağlantı koptuysa false. Soket engellemesizdir: karşı taraf
    // okumazken yazım POLLOUT ile bekler ve *dur true olunca (en geç ~50 ms içinde) yazılan
    // kısmı bırakıp false döner; dur NULL ise yazım bitene ya da bağlantı kopana kadar sürer.
    bool yaz(const std::uint8_t* veri, std::size_t n, const std::atomic<bool>* dur = nullptr);

    // Gönderme yönünü kapatır (karşı taraf akışın sonunu görür); okuma sürer.
    void yazmayi_kapat();

    // İşletim sisteminin alım tamponu (SO_RCVBUF), bayt; sınamalar için.
    void alim_tamponu(int bayt);

    void kapat();

private:
    friend class Dinleyici;
    explicit Soket(Tutamac t) : t_(t) {}
    Tutamac t_ = gecersiz();
    static Tutamac gecersiz();
};

// Sınamalar ve sahte sim için: 127.0.0.1'de dinleyen soket.
class Dinleyici {
public:
    Dinleyici() = default;
    ~Dinleyici();
    Dinleyici(const Dinleyici&) = delete;
    Dinleyici& operator=(const Dinleyici&) = delete;
    Dinleyici(Dinleyici&& o) noexcept;
    Dinleyici& operator=(Dinleyici&& o) noexcept;

    // port = 0: işletim sisteminin seçtiği boş port.
    static Dinleyici ac(std::uint16_t port, std::string* hata = nullptr);

    bool gecerli() const;
    std::uint16_t port() const { return port_; }

    // Bağlantı bekler; zaman aşımında geçersiz soket.
    Soket kabul(int zaman_asimi_ms);

    void kapat();

private:
    Tutamac t_ = Soket::gecersiz();
    std::uint16_t port_ = 0;
};

}  // namespace ida::sil::tasima
