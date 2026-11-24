// statik.cpp — bkz. statik.hpp.
#include "statik.hpp"

#include <vector>

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
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ida::a3 {

namespace {

std::vector<std::string> bilesenler(const std::string& goreli)
{
    std::vector<std::string> r;
    std::size_t bas = 0;
    for (;;) {
        const std::size_t k = goreli.find('/', bas);
        r.push_back(goreli.substr(bas, k == std::string::npos ? std::string::npos : k - bas));
        if (k == std::string::npos) {
            return r;
        }
        bas = k + 1;
    }
}

#if defined(_WIN32)

std::wstring genis(const std::string& s)
{
    if (s.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) {
        return {};
    }
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

struct Tanitici {
    HANDLE h = INVALID_HANDLE_VALUE;
    Tanitici() = default;
    explicit Tanitici(HANDLE x) : h(x) {}
    Tanitici(const Tanitici&) = delete;
    Tanitici& operator=(const Tanitici&) = delete;
    ~Tanitici()
    {
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
        }
    }
};

bool gercek_yol(HANDLE h, std::wstring& yol)
{
    const DWORD n = GetFinalPathNameByHandleW(h, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (n == 0) {
        return false;
    }
    yol.assign(n, L'\0');
    const DWORD m = GetFinalPathNameByHandleW(h, yol.data(), n, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (m == 0 || m >= n) {
        return false;
    }
    yol.resize(m);
    return true;
}

#endif

}  // namespace

bool statik_oku(const std::string& kok, const std::string& goreli, std::size_t azami, std::string& icerik)
{
    icerik.clear();
    if (kok.empty() || goreli.empty()) {
        return false;
    }
    const std::vector<std::string> parca = bilesenler(goreli);
    for (const auto& b : parca) {
        if (b.empty() || b.front() == '.') {
            return false;
        }
    }
#if defined(_WIN32)
    // Kökün gerçek yolu (kökün kendisi yapılandırmadır; bağ olabilir).
    Tanitici k(CreateFileW(genis(kok).c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (k.h == INVALID_HANDLE_VALUE) {
        return false;
    }
    std::wstring kok_gercek;
    if (!gercek_yol(k.h, kok_gercek)) {
        return false;
    }
    std::wstring beklenen = kok_gercek;
    std::wstring tam = genis(kok);
    for (const auto& b : parca) {
        beklenen += L'\\';
        beklenen += genis(b);
        tam += L'\\';
        tam += genis(b);
    }
    Tanitici f(CreateFileW(tam.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (f.h == INVALID_HANDLE_VALUE) {
        return false;
    }
    BY_HANDLE_FILE_INFORMATION bilgi{};
    if (GetFileInformationByHandle(f.h, &bilgi) == 0 || (bilgi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        (bilgi.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return false;
    }
    std::wstring dosya_gercek;
    if (!gercek_yol(f.h, dosya_gercek) || dosya_gercek.size() != beklenen.size() ||
        CompareStringOrdinal(dosya_gercek.c_str(), static_cast<int>(dosya_gercek.size()), beklenen.c_str(),
                             static_cast<int>(beklenen.size()), TRUE) != CSTR_EQUAL) {
        return false;  // yolda reparse noktası (bağ, junction) var ya da kök dışına çıkıyor
    }
    const unsigned long long boy =
        (static_cast<unsigned long long>(bilgi.nFileSizeHigh) << 32u) | static_cast<unsigned long long>(bilgi.nFileSizeLow);
    if (boy > azami) {
        return false;
    }
    icerik.resize(static_cast<std::size_t>(boy));
    std::size_t okunan = 0;
    while (okunan < icerik.size()) {
        DWORD n = 0;
        const DWORD istek = static_cast<DWORD>(icerik.size() - okunan > 0x100000u ? 0x100000u : icerik.size() - okunan);
        if (ReadFile(f.h, icerik.data() + okunan, istek, &n, nullptr) == 0 || n == 0) {
            icerik.clear();
            return false;
        }
        okunan += n;
    }
    return true;
#else
    int dizin = open(kok.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dizin < 0) {
        return false;
    }
    for (std::size_t i = 0; i + 1 < parca.size(); ++i) {
        // O_NOFOLLOW: bileşen simgesel bağsa açılmaz (ELOOP) — kök içini gösterse de.
        const int alt = openat(dizin, parca[i].c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        close(dizin);
        if (alt < 0) {
            return false;
        }
        dizin = alt;
    }
    const int fd = openat(dizin, parca.back().c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    close(dizin);
    if (fd < 0) {
        return false;
    }
    struct stat st{};
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 || static_cast<unsigned long long>(st.st_size) > azami) {
        close(fd);
        return false;
    }
    icerik.resize(static_cast<std::size_t>(st.st_size));
    std::size_t okunan = 0;
    while (okunan < icerik.size()) {
        const ssize_t n = read(fd, icerik.data() + okunan, icerik.size() - okunan);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            close(fd);
            icerik.clear();
            return false;
        }
        okunan += static_cast<std::size_t>(n);
    }
    close(fd);
    return true;
#endif
}

}  // namespace ida::a3
