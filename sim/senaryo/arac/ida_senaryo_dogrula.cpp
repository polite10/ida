// ida_senaryo_dogrula: A5 senaryosunu doğrular ve çözümler.
//
//   ida_senaryo_dogrula <senaryo.toml> [--cozumlenmis <çıktı.json>] [--rapor <çıktı.json>]
//                       [--kok <depo kökü>]
//
// Doğrulama raporu (A5 §3.14, kanonik JSON) `--rapor` verilmişse dosyaya, verilmemişse
// standart çıktıya yazılır. `--cozumlenmis` verilmişse ve senaryo geçerliyse çözümlenmiş
// senaryonun kanonik baytları (A5-K17) yazılır. Özet ve bulgular standart hataya yazılır.
//
// Depo kökü (bulgulardaki `dosya` alanlarının ve A7 kümesinin, `assets/<kume>/`, tabanı):
// `--kok` ile verilir; verilmezse senaryonun dizininden yukarı doğru `assets/` dizini olan
// ilk dizin; bulunmazsa senaryonun dizini.
//
// Çıkış kodları:
//   0  geçerli (hata bulgusu yok; uyarı olabilir)
//   1  geçersiz (en az bir hata bulgusu)
//   2  kullanım hatası (bilinmeyen ya da eksik argüman)
//   3  G/Ç hatası (senaryo dosyası okunamadı ya da çıktı yazılamadı)
#include "ida/senaryo/senaryo.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

constexpr int kGecerli = 0;
constexpr int kGecersiz = 1;
constexpr int kKullanim = 2;
constexpr int kGc = 3;

int kullanim()
{
    std::fprintf(stderr,
                 "kullanım: ida_senaryo_dogrula <senaryo.toml> [--cozumlenmis <çıktı.json>] "
                 "[--rapor <çıktı.json>] [--kok <depo kökü>]\n");
    return kKullanim;
}

bool yaz(const fs::path& yol, const std::string& icerik)
{
    std::ofstream f(yol, std::ios::binary | std::ios::trunc);
    if (!f) {
        return false;
    }
    f.write(icerik.data(), static_cast<std::streamsize>(icerik.size()));
    return static_cast<bool>(f);
}

fs::path kok_bul(const fs::path& senaryo_dizini)
{
    std::error_code ec;
    for (fs::path d = senaryo_dizini; !d.empty(); d = d.parent_path()) {
        if (fs::is_directory(d / "assets", ec)) {
            return d;
        }
        if (d == d.parent_path()) {
            break;
        }
    }
    return senaryo_dizini;
}

}  // namespace

int main(int argc, char** argv)
{
    std::string senaryo;
    std::string cozumlenmis_cikti;
    std::string rapor_cikti;
    std::string kok_arg;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto deger = [&](std::string& hedef) {
            if (i + 1 >= argc) {
                return false;
            }
            hedef = argv[++i];
            return true;
        };
        if (a == "--cozumlenmis") {
            if (!deger(cozumlenmis_cikti)) return kullanim();
        } else if (a == "--rapor") {
            if (!deger(rapor_cikti)) return kullanim();
        } else if (a == "--kok") {
            if (!deger(kok_arg)) return kullanim();
        } else if (!a.empty() && a[0] == '-') {
            return kullanim();
        } else if (senaryo.empty()) {
            senaryo = a;
        } else {
            return kullanim();
        }
    }
    if (senaryo.empty()) {
        return kullanim();
    }

    std::error_code ec;
    const fs::path senaryo_mutlak = fs::absolute(fs::path(senaryo), ec).lexically_normal();
    if (ec || !fs::is_regular_file(senaryo_mutlak, ec)) {
        std::fprintf(stderr, "ida_senaryo_dogrula: senaryo dosyası okunamadı: %s\n", senaryo.c_str());
        return kGc;
    }
    fs::path kok = kok_arg.empty() ? kok_bul(senaryo_mutlak.parent_path())
                                   : fs::absolute(fs::path(kok_arg), ec).lexically_normal();
    fs::path goreli = senaryo_mutlak.lexically_relative(kok);
    if (goreli.empty() || *goreli.begin() == "..") {
        kok = senaryo_mutlak.parent_path();
        goreli = senaryo_mutlak.filename();
    }

    const ida::senaryo::DiskKaynagi kaynak(kok);
    const std::u8string goreli_u8 = goreli.generic_u8string();
    const ida::senaryo::CozumSonucu s =
        ida::senaryo::senaryo_coz(kaynak, std::string(goreli_u8.begin(), goreli_u8.end()));
    const auto& r = s.rapor;

    for (const auto& b : r.bulgular) {
        std::fprintf(stderr, "%s %s (aşama %d) %s%s%s: %s\n", b.onem == ida::senaryo::Onem::hata ? "HATA" : "uyarı",
                     b.kural.c_str(), b.asama, b.dosya.c_str(),
                     b.satir ? (":" + std::to_string(*b.satir)).c_str() : "",
                     b.yol.empty() ? "" : (" " + b.yol).c_str(), b.ileti.c_str());
    }
    std::fprintf(stderr, "%s: %zu hata, %zu uyarı%s%s\n", r.gecerli ? "GEÇERLİ" : "GEÇERSİZ", r.hata_sayisi(),
                 r.uyari_sayisi(), r.davranis_ozeti ? "; davranış özeti sha256:" : "",
                 r.davranis_ozeti ? r.davranis_ozeti->c_str() : "");

    const std::string rapor = r.json();
    if (rapor_cikti.empty()) {
        std::fwrite(rapor.data(), 1, rapor.size(), stdout);
        std::fputc('\n', stdout);
    } else if (!yaz(rapor_cikti, rapor)) {
        std::fprintf(stderr, "ida_senaryo_dogrula: rapor yazılamadı: %s\n", rapor_cikti.c_str());
        return kGc;
    }
    if (!cozumlenmis_cikti.empty() && r.gecerli && !yaz(cozumlenmis_cikti, s.kanonik)) {
        std::fprintf(stderr, "ida_senaryo_dogrula: çözümlenmiş senaryo yazılamadı: %s\n",
                     cozumlenmis_cikti.c_str());
        return kGc;
    }
    return r.gecerli ? kGecerli : kGecersiz;
}
