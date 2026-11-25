// dosya.cpp — yerel dosya yazım hedefi: dışlayıcı oluşturma (A6-K01), işletim sistemine teslim,
// kesme. Yalnız kayıt yazıcısının iş parçacığı kullanır.
//
// Tamponlama burada yapılır (stdio değil): yazma hatasından sonra bekleyen baytların ne olduğu
// kesin bilinir ve `kes` onları atar; kesilen dosyaya sonradan bayat veri yazılmaz.
#include "ida/kayit/yazici.hpp"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ida::kayit {

namespace {

constexpr std::size_t kTamponBayt = std::size_t{1} << 18;

std::string yol_metni(const std::filesystem::path& p)
{
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

class YerelDosya final : public Dosya {
public:
    explicit YerelDosya(int fd) : fd_(fd) { tampon_.reserve(kTamponBayt); }
    ~YerelDosya() override { kapat(); }

    Sonuc yaz(const std::uint8_t* veri, std::size_t n) override
    {
        if (fd_ < 0) {
            return Sonuc::hata;
        }
        if (tampon_.size() + n > kTamponBayt) {
            const Sonuc s = tamponu_yaz_();
            if (s != Sonuc::tamam) {
                return s;
            }
        }
        if (n >= kTamponBayt) {
            return ham_yaz_(veri, n);
        }
        tampon_.insert(tampon_.end(), veri, veri + n);
        return Sonuc::tamam;
    }

    Sonuc bosalt() override
    {
        if (fd_ < 0) {
            return Sonuc::hata;
        }
        return tamponu_yaz_();
    }

    Sonuc kes(std::uint64_t boy) override
    {
        if (fd_ < 0) {
            return Sonuc::hata;
        }
        if (boy >= diskte_) {
            // Kesim noktası henüz yazılmamış tamponun içinde: tamponun fazlası atılır, dosyaya dokunulmaz.
            const std::uint64_t kalan = boy - diskte_;
            if (kalan > tampon_.size()) {
                return Sonuc::hata;  // dosya hiç bu boya ulaşmadı
            }
            tampon_.resize(static_cast<std::size_t>(kalan));
            return Sonuc::tamam;
        }
        tampon_.clear();  // bekleyen baytlar kesilen bölgeye aitti
        diskte_ = boy;
#if defined(_WIN32)
        if (_chsize_s(fd_, static_cast<long long>(boy)) != 0) {
            return Sonuc::hata;
        }
        if (_lseeki64(fd_, static_cast<long long>(boy), SEEK_SET) < 0) {
            return Sonuc::hata;
        }
#else
        if (::ftruncate(fd_, static_cast<off_t>(boy)) != 0) {
            return Sonuc::hata;
        }
        if (::lseek(fd_, static_cast<off_t>(boy), SEEK_SET) < 0) {
            return Sonuc::hata;
        }
#endif
        return Sonuc::tamam;
    }

    void kapat() override
    {
        if (fd_ >= 0) {
            (void)tamponu_yaz_();
#if defined(_WIN32)
            (void)_close(fd_);
#else
            (void)::close(fd_);
#endif
            fd_ = -1;
        }
    }

private:
    Sonuc tamponu_yaz_()
    {
        if (tampon_.empty()) {
            return Sonuc::tamam;
        }
        const Sonuc s = ham_yaz_(tampon_.data(), tampon_.size());
        // Hata durumunda tampon atılır: çağıran dosyayı son sağlam boyuna keser.
        tampon_.clear();
        return s;
    }

    Sonuc ham_yaz_(const std::uint8_t* veri, std::size_t n)
    {
        std::size_t yazilan = 0;
        while (yazilan < n) {
            errno = 0;
#if defined(_WIN32)
            const unsigned int parca = static_cast<unsigned int>(std::min<std::size_t>(n - yazilan, std::size_t{1} << 30));
            const int r = _write(fd_, veri + yazilan, parca);
#else
            const ssize_t r = ::write(fd_, veri + yazilan, n - yazilan);
#endif
            if (r < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return errno == ENOSPC ? Sonuc::disk_dolu : Sonuc::hata;
            }
            if (r == 0) {
                return Sonuc::disk_dolu;  // ilerleme yok: yer kalmadı
            }
            yazilan += static_cast<std::size_t>(r);
            diskte_ += static_cast<std::uint64_t>(r);
        }
        return Sonuc::tamam;
    }

    int fd_;
    std::uint64_t diskte_ = 0;  // işletim sistemine verilen bayt (dosyadaki yazma konumu)
    std::vector<std::uint8_t> tampon_;
};

}  // namespace

std::unique_ptr<Dosya> yerel_dosya_olustur(const std::filesystem::path& yol, std::string& hata)
{
    // Dışlayıcı oluşturma: dosya varsa açılmaz (denetim ile oluşturma arasında yarış yok).
#if defined(_WIN32)
    int fd = -1;
    const errno_t e = _wsopen_s(&fd, yol.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY | _O_NOINHERIT, _SH_DENYWR,
                                _S_IREAD | _S_IWRITE);
    if (e != 0 || fd < 0) {
        hata = (e == EEXIST ? "dosya var: " : "dosya oluşturulamadı: ") + yol_metni(yol);
        return nullptr;
    }
#else
    const int fd = ::open(yol.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (fd < 0) {
        hata = (errno == EEXIST ? "dosya var: " : "dosya oluşturulamadı: ") + yol_metni(yol);
        return nullptr;
    }
#endif
    return std::make_unique<YerelDosya>(fd);
}

}  // namespace ida::kayit
