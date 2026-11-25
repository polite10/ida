// kilit.hpp — A2 sahiplik kilidi (A2-K77, K79).
//
// Sim süreci, A2 dinleyicisini açmadan önce kullanıcının geçici dizinindeki `ida/a2.kilit`
// dosyasında işletim sisteminin özel dosya kilidini BEKLEMEDEN almayı dener: macOS'ta
// flock(LOCK_EX | LOCK_NB), Windows'ta LockFileEx(LOCKFILE_EXCLUSIVE_LOCK |
// LOCKFILE_FAIL_IMMEDIATELY). Kilit nesne yaşadıkça tutulur; süreç sonlanınca işletim sistemi
// bırakır. Dosyanın içeriği kullanılmaz.
#pragma once

#include <filesystem>
#include <string>

namespace ida::a2::ic {

enum class KilitSonucu { alindi, baskasinda, acilamadi };

class SahiplikKilidi {
public:
    SahiplikKilidi() = default;
    ~SahiplikKilidi();
    SahiplikKilidi(const SahiplikKilidi&) = delete;
    SahiplikKilidi& operator=(const SahiplikKilidi&) = delete;

    // `istenen` boşsa sözleşmedeki sabit yer kullanılır. Dizin yoksa oluşturulur.
    KilitSonucu al(const std::string& istenen, std::string& hata);
    void birak() noexcept;
    bool tutuluyor() const noexcept;
    const std::string& yol() const noexcept { return yol_; }

    // Sözleşmedeki sabit yer: <kullanıcının geçici dizini>/ida/a2.kilit. Bulunamazsa boş.
    static std::filesystem::path varsayilan_yol();

private:
    std::string yol_;
#if defined(_WIN32)
    void* tutamac_ = nullptr;  // HANDLE
#else
    int fd_ = -1;
#endif
};

}  // namespace ida::a2::ic
