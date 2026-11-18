// W0-MP4a: bağlanmış ELF'in ayrıştırılmış çıktısından (arm-none-eabi-objdump -d) çağrı grafiği denetimi.
//
//   mp4_cagri <objdump.txt> <kök_sembol> [kök_sembol...]
//
// Kökten doğrudan (bl, blx <sembol>) ve dallanma/kuyruk çağrısıyla (b, b.w, bXX, cbz/cbnz <başka sembol>)
// erişilen işlevleri geçişli olarak listeler. Çıkış kodu:
//   0  erişilen işlevlerde çift duyarlıklı yazılım kayan nokta yordamı, çift duyarlıklı libm işlevi ve
//      çift duyarlıklı (.f64) komut yok; dolaylı çağrı yok
//   1  çift duyarlıklı yordam/işlev/komut bulundu
//   3  bulunmadı ama erişilen işlevlerde dolaylı çağrı/dallanma var (blx rN, bx rN [lr dışı], ldr pc / mov pc):
//      çağrı grafiği tam izlenemediği için yokluk kanıtlanamadı
//   2  kullanım/girdi hatası (kök sembol bulunamadı dahil)
// Çift duyarlıklı yordamlar: __aeabi_d*, __aeabi_*2d*, __*df*, __extendsfdf2, __truncdfsf2; libm: "f" soneksiz
// adlar (sin, cos, sqrt, atan2, ...), __ieee754_*/__kernel_* içinde "f" ile bitmeyenler. M33'ün FPU'su (FPv5-SP)
// çift duyarlıklı komut çalıştıramaz; çift duyarlıklı her işlem bu yordamlara çağrı olarak görünür.

#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    if (argc < 3) {
        std::cerr << "kullanım: mp4_cagri <objdump.txt> <kök> [kök...]\n";
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) { std::cerr << "açılamadı: " << argv[1] << "\n"; return 2; }
    const std::regex baslik(R"(^[0-9a-f]+ <([^>]+)>:$)");
    // komut satırı: "  8000abc:\tf000 f8a0 \tbl\t8000c00 <yt_m3m3>"
    const std::regex komut(R"(^\s*[0-9a-f]+:\t[0-9a-f ]+\t([a-z][a-z0-9.]*)\s*(.*)$)");
    const std::regex hedef(R"(<([^>+]+)(\+0x[0-9a-f]+)?>)");
    const std::regex dallanma(R"(^(b|bl|blx|cbz|cbnz)(eq|ne|cs|cc|hs|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le|al)?(\.w|\.n)?$)");
    const std::regex dolayli_bx(R"(^(blx|bx)(eq|ne|cs|cc|hs|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le|al)?(\.n|\.w)?$)");
    std::map<std::string, std::set<std::string>> kenar;
    std::map<std::string, int> dolayli_say;
    std::map<std::string, int> f64_say;
    std::string isl;
    std::string l;
    std::smatch m, mh;
    while (std::getline(in, l)) {
        if (std::regex_match(l, m, baslik)) { isl = m[1]; kenar[isl]; continue; }
        if (isl.empty()) continue;
        if (!std::regex_match(l, m, komut)) continue;
        const std::string op = m[1], arg = m[2];
        if (op.find(".f64") != std::string::npos || arg.find(".f64") != std::string::npos) f64_say[isl]++;
        if (std::regex_match(op, dallanma) && std::regex_search(arg, mh, hedef)) {
            const std::string h = mh[1];
            if (h != isl) kenar[isl].insert(h);
            continue;
        }
        if (std::regex_match(op, dolayli_bx)) {
            if (arg.rfind("lr", 0) != 0) dolayli_say[isl]++;   // "bx lr" dönüştür
            continue;
        }
        if ((op.rfind("ldr", 0) == 0 || op.rfind("mov", 0) == 0) && arg.rfind("pc,", 0) == 0) dolayli_say[isl]++;
    }
    std::set<std::string> ziyaret;
    std::vector<std::string> yigin;
    for (int i = 2; i < argc; ++i) {
        if (!kenar.count(argv[i])) { std::cerr << "kök sembol bulunamadı: " << argv[i] << "\n"; return 2; }
        yigin.push_back(argv[i]);
    }
    while (!yigin.empty()) {
        const std::string f = yigin.back();
        yigin.pop_back();
        if (!ziyaret.insert(f).second) continue;
        for (const auto &h : kenar[f]) yigin.push_back(h);
    }
    const std::regex cift(R"(^(__aeabi_d.*|__aeabi_.*2d.*|__.*df.*|__extendsfdf2|__truncdfsf2|sin|cos|tan|sqrt|atan|atan2|asin|acos|sinh|cosh|tanh|exp|exp2|expm1|log|log2|log10|log1p|pow|hypot|cbrt|fabs|floor|ceil|trunc|fmin|fmax|fmod|round|lround|rint|lrint|nearbyint|copysign|scalbn|ldexp|frexp|modf|__ieee754_.*[^f]|__kernel_.*[^f])$)");
    int bulunan = 0, dolayli = 0;
    std::cout << "erişilen işlev sayısı: " << ziyaret.size() << "\n";
    for (const auto &f : ziyaret) {
        const bool kotu = std::regex_match(f, cift) || f64_say.count(f);
        std::cout << (kotu ? "  ÇİFT  " : "        ") << f;
        if (dolayli_say.count(f)) { std::cout << "  (dolaylı çağrı/dallanma: " << dolayli_say[f] << ")"; dolayli += dolayli_say[f]; }
        if (f64_say.count(f)) std::cout << "  (.f64 komutu: " << f64_say[f] << ")";
        std::cout << "\n";
        if (kotu) ++bulunan;
    }
    std::cout << "dolaylı çağrı/dallanma: " << dolayli << "\n";
    std::cout << "çift duyarlıklı yazılım kayan nokta / libm işlevi / komut: " << bulunan << "\n";
    if (bulunan) return 1;
    return dolayli ? 3 : 0;
}
