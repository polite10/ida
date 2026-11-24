// statik.cpp — bkz. statik.hpp.
//
// Windows dayanakları (Microsoft belgeleri):
//   CreateFileW (FILE_FLAG_OPEN_REPARSE_POINT, FILE_FLAG_BACKUP_SEMANTICS, dwShareMode):
//     https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew
//   GetFinalPathNameByHandleW (`\\?\` sözdizimi):
//     https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfinalpathnamebyhandlew
//   BY_HANDLE_FILE_INFORMATION (dwVolumeSerialNumber, nFileIndexHigh/Low, nNumberOfLinks):
//     https://learn.microsoft.com/en-us/windows/win32/api/fileapi/ns-fileapi-by_handle_file_information
//   Sabit bağlar ve junction'lar:
//     https://learn.microsoft.com/en-us/windows/win32/fileio/hard-links-and-junctions
//   Dosya adlandırma (`\\?\` öneki ad normalleştirmesini kapatır):
//     https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file
#include "statik.hpp"

#include <memory>
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

// Bileşen açılışı: reparse noktası izlenmez (FILE_FLAG_OPEN_REPARSE_POINT); dizin açmak için
// FILE_FLAG_BACKUP_SEMANTICS (yalnız dizinlerde; son bileşen dizinse açılış başarısız olur);
// silme paylaşımı verilmez (tutulduğu sürece yeniden adlandırma/silme yok).
HANDLE bilesen_ac(const std::wstring& yol, bool dizin)
{
    const DWORD bayrak = FILE_FLAG_OPEN_REPARSE_POINT | (dizin ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL);
    return CreateFileW(yol.c_str(), dizin ? FILE_READ_ATTRIBUTES : GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                       OPEN_EXISTING, bayrak, nullptr);
}

#endif

}  // namespace

#if defined(_WIN32)

StatikKok::~StatikKok()
{
    if (h_ != nullptr) {
        CloseHandle(static_cast<HANDLE>(h_));
    }
}

bool StatikKok::acik() const noexcept { return h_ != nullptr; }

bool StatikKok::ac(const std::string& kok, std::string& hata)
{
    const std::wstring w = genis(kok);
    if (w.empty()) {
        hata = "kök yolu geçersiz";
        return false;
    }
    // Kökün kendisi yapılandırmadır: bağsa açılışta izlenir. Silme paylaşımı verilmez: tutulan kök
    // dizini yeniden adlandırılamaz ya da silinemez.
    HANDLE h = CreateFileW(w.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        hata = "kök dizini açılamadı (" + std::to_string(GetLastError()) + ")";
        return false;
    }
    BY_HANDLE_FILE_INFORMATION bilgi{};
    std::wstring yol;
    if (GetFileInformationByHandle(h, &bilgi) == 0 || (bilgi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        !gercek_yol(h, yol) || yol.rfind(L"\\\\?\\", 0) != 0) {
        CloseHandle(h);
        hata = "kök bir dizin değil ya da gerçek yolu alınamadı";
        return false;
    }
    h_ = h;
    yol_ = std::move(yol);
    seri_ = bilgi.dwVolumeSerialNumber;
    kimlik_yuksek_ = bilgi.nFileIndexHigh;
    kimlik_dusuk_ = bilgi.nFileIndexLow;
    return true;
}

bool StatikKok::oku(const std::string& goreli, std::size_t azami, std::string& icerik) const
{
    icerik.clear();
    if (h_ == nullptr || goreli.empty()) {
        return false;
    }
    const std::vector<std::string> parca = bilesenler(goreli);
    for (const auto& b : parca) {
        if (b.empty() || b.front() == '.') {
            return false;
        }
    }
    // Kökün gerçek yolu yeniden açılır ve tutulan kökle aynı nesne olduğu (birim seri numarası +
    // dosya kimliği) doğrulanır; reparse noktası izlenmez.
    std::wstring tam = yol_;
    std::vector<std::unique_ptr<Tanitici>> tutulan;
    tutulan.push_back(std::make_unique<Tanitici>(bilesen_ac(tam, true)));
    {
        BY_HANDLE_FILE_INFORMATION k{};
        if (tutulan.back()->h == INVALID_HANDLE_VALUE || GetFileInformationByHandle(tutulan.back()->h, &k) == 0 ||
            k.dwVolumeSerialNumber != seri_ || k.nFileIndexHigh != kimlik_yuksek_ || k.nFileIndexLow != kimlik_dusuk_ ||
            (k.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            return false;  // kök yolu artık tutulan kökü göstermiyor
        }
    }
    for (std::size_t i = 0; i < parca.size(); ++i) {
        if (i > 0) {
            BY_HANDLE_FILE_INFORMATION ust{};
            if (tutulan.back()->h == INVALID_HANDLE_VALUE || GetFileInformationByHandle(tutulan.back()->h, &ust) == 0 ||
                (ust.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
                (ust.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
                return false;  // ara bileşen dizin değil ya da reparse noktası (bağ, junction)
            }
        }
        const std::wstring ad = genis(parca[i]);
        if (ad.empty()) {
            return false;
        }
        tam += L'\\';
        tam += ad;
        const bool son = i + 1 == parca.size();
        tutulan.push_back(std::make_unique<Tanitici>(bilesen_ac(tam, !son)));
    }
    Tanitici& f = *tutulan.back();
    if (f.h == INVALID_HANDLE_VALUE || GetFileType(f.h) != FILE_TYPE_DISK) {
        return false;
    }
    BY_HANDLE_FILE_INFORMATION bilgi{};
    if (GetFileInformationByHandle(f.h, &bilgi) == 0 || (bilgi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        (bilgi.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 || bilgi.nNumberOfLinks != 1) {
        return false;  // dizin, reparse noktası (simgesel bağ dahil) ya da sabit bağlı dosya: 404
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
}

#else

StatikKok::~StatikKok()
{
    if (fd_ >= 0) {
        close(fd_);
    }
}

bool StatikKok::acik() const noexcept { return fd_ >= 0; }

bool StatikKok::ac(const std::string& kok, std::string& hata)
{
    // Kökün kendisi yapılandırmadır: bağsa açılışta izlenir.
    const int fd = open(kok.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) {
        hata = "kök dizini açılamadı (errno " + std::to_string(errno) + ")";
        return false;
    }
    fd_ = fd;
    return true;
}

bool StatikKok::oku(const std::string& goreli, std::size_t azami, std::string& icerik) const
{
    icerik.clear();
    if (fd_ < 0 || goreli.empty()) {
        return false;
    }
    const std::vector<std::string> parca = bilesenler(goreli);
    for (const auto& b : parca) {
        if (b.empty() || b.front() == '.') {
            return false;
        }
    }
    int dizin = fd_;  // tutulan kök; kapatılmaz
    for (std::size_t i = 0; i + 1 < parca.size(); ++i) {
        // O_NOFOLLOW: bileşen simgesel bağsa açılmaz (ELOOP) — kök içini gösterse de.
        const int alt = openat(dizin, parca[i].c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (dizin != fd_) {
            close(dizin);
        }
        if (alt < 0) {
            return false;
        }
        dizin = alt;
    }
    const int fd = openat(dizin, parca.back().c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (dizin != fd_) {
        close(dizin);
    }
    if (fd < 0) {
        return false;
    }
    struct stat st{};
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1 || st.st_size < 0 ||
        static_cast<unsigned long long>(st.st_size) > azami) {
        close(fd);  // düzenli dosya değil, sabit bağlı ya da büyük: 404
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
}

#endif

}  // namespace ida::a3
