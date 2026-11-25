// kilit.cpp — bkz. kilit.hpp.
#include "kilit.hpp"

#include <filesystem>
#include <system_error>

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
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace ida::a2::ic {

namespace {

// Yolun UTF-8 yazımı (yalnız ileti ve tanılama için; dosya işlemleri `path` ile yapılır).
std::string yol_metni(const std::filesystem::path& p)
{
    const std::u8string u = p.u8string();
    return std::string(u.begin(), u.end());
}

}  // namespace

std::filesystem::path SahiplikKilidi::varsayilan_yol()
{
    std::error_code ec;
    std::filesystem::path kok;
#if defined(__APPLE__)
    // Kullanıcıya özel geçici dizin (ortam değişkeninden bağımsız; süreçler arasında aynıdır).
    char tampon[1024];
    const std::size_t n = confstr(_CS_DARWIN_USER_TEMP_DIR, tampon, sizeof tampon);
    if (n > 0 && n <= sizeof tampon) {
        kok = std::filesystem::path(tampon);
    }
#endif
    if (kok.empty()) {
        kok = std::filesystem::temp_directory_path(ec);  // Windows: GetTempPathW
        if (ec) {
            return {};
        }
    }
    return kok / "ida" / "a2.kilit";
}

SahiplikKilidi::~SahiplikKilidi() { birak(); }

bool SahiplikKilidi::tutuluyor() const noexcept
{
#if defined(_WIN32)
    return tutamac_ != nullptr;
#else
    return fd_ >= 0;
#endif
}

KilitSonucu SahiplikKilidi::al(const std::string& istenen, std::string& hata)
{
    birak();
    const std::filesystem::path p = istenen.empty() ? varsayilan_yol() : std::filesystem::path(istenen);
    if (p.empty()) {
        yol_.clear();
        hata = "A2 sahiplik kilidi: geçici dizin bulunamadı";
        return KilitSonucu::acilamadi;
    }
    yol_ = yol_metni(p);
    std::error_code ec;
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);  // varsa hata değildir
    }
#if defined(_WIN32)
    const std::wstring genis = p.wstring();
    const HANDLE h = CreateFileW(genis.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        hata = "A2 sahiplik kilidi: dosya açılamadı: " + yol_;
        return KilitSonucu::acilamadi;
    }
    OVERLAPPED ov{};
    if (LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &ov) == 0) {
        const DWORD kod = GetLastError();
        CloseHandle(h);
        if (kod == ERROR_LOCK_VIOLATION || kod == ERROR_IO_PENDING) {
            hata = "A2 sahiplik kilidi başka bir sim sürecinde";
            return KilitSonucu::baskasinda;
        }
        hata = "A2 sahiplik kilidi: kilit alınamadı: " + yol_;
        return KilitSonucu::acilamadi;
    }
    tutamac_ = h;
    return KilitSonucu::alindi;
#else
    // O_CLOEXEC: alt süreçler tanımlayıcıyı devralıp kilidi sahibinden uzun yaşatmasın.
    const int fd = open(p.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) {
        hata = "A2 sahiplik kilidi: dosya açılamadı: " + yol_;
        return KilitSonucu::acilamadi;
    }
    int r = -1;
    do {
        r = flock(fd, LOCK_EX | LOCK_NB);
    } while (r != 0 && errno == EINTR);
    if (r != 0) {
        const int kod = errno;
        close(fd);
        if (kod == EWOULDBLOCK) {
            hata = "A2 sahiplik kilidi başka bir sim sürecinde";
            return KilitSonucu::baskasinda;
        }
        hata = "A2 sahiplik kilidi: kilit alınamadı: " + yol_;
        return KilitSonucu::acilamadi;
    }
    fd_ = fd;
    return KilitSonucu::alindi;
#endif
}

void SahiplikKilidi::birak() noexcept
{
#if defined(_WIN32)
    if (tutamac_ != nullptr) {
        OVERLAPPED ov{};
        (void)UnlockFileEx(static_cast<HANDLE>(tutamac_), 0, 1, 0, &ov);
        CloseHandle(static_cast<HANDLE>(tutamac_));
        tutamac_ = nullptr;
    }
#else
    if (fd_ >= 0) {
        (void)flock(fd_, LOCK_UN);
        close(fd_);
        fd_ = -1;
    }
#endif
}

}  // namespace ida::a2::ic
