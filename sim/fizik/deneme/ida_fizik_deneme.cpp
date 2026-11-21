// ida_fizik_deneme: açık çevrim bir darbe genişliği profilini koşturur ve durumu CSV yazar.
//
// Kullanım:
//   ida_fizik_deneme [--profil <dosya.csv>] [--sure <s>] [--aralik <adım>] [--cikti <dosya.csv>]
//                    [--baslangic-z <m>] [--yuk <iki|f1|f2|bos>]
//
// Profil dosyası: her satır `t_s, pwm0, …, pwm7` (kanal sırası, µs); `#` ile başlayan satır
// yorumdur. Komut, t_s anını içeren fizik adımından itibaren bir sonraki satıra kadar
// yürürlüktedir (parça parça sabit). Profil verilmezse yerleşik profil koşar: 0–2 s ESC
// başlatma (nötr), 2–22 s dikey iticilerle dalış, 22–52 s ileri gidiş (ön dikeyler aşağı,
// arka dikeyler yukarı: baş-kıç momenti dengesi), 52–82 s yerinde dönüş, 82 s'den sonra nötr
// (yüzeye çıkış).
// Çıktı varsayılan olarak standart çıktıya; her `aralik` adımda bir satır (varsayılan 8 = 100 Hz).
#include "ida/fizik/dunya.hpp"

#include <array>
#include <cmath>
#include <cstdarg>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace ida::fizik;

struct ProfilSatiri {
    double t;
    std::array<double, kIticiSayisi> pwm;
};

std::vector<ProfilSatiri> yerlesik_profil()
{
    auto satir = [](double t, std::array<double, kIticiSayisi> p) { return ProfilSatiri{t, p}; };
    return {
        satir(0.0, {1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500}),
        satir(2.0, {1500, 1500, 1500, 1500, 1620, 1620, 1620, 1620}),
        // İleri gidiş: yatay iticilerin O_b altındaki ofseti burnu kaldırır (04 §2.6: M = 0,085 X);
        // ön dikeyler aşağı, arka dikeyler yukarı iterek kabaca dengeler (açık çevrim).
        satir(22.0, {1580, 1580, 1580, 1580, 1560, 1560, 1440, 1440}),
        satir(52.0, {1400, 1600, 1400, 1600, 1530, 1530, 1530, 1530}),
        satir(82.0, {1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500}),
    };
}

bool profil_oku(const std::string& yol, std::vector<ProfilSatiri>& p)
{
    std::ifstream f(yol);
    if (!f) return false;
    std::string s;
    while (std::getline(f, s)) {
        const auto b = s.find_first_not_of(" \t\r");
        if (b == std::string::npos || s[b] == '#') continue;
        for (char& c : s) {
            if (c == ',') c = ' ';
        }
        std::istringstream is(s);
        ProfilSatiri r{};
        is >> r.t;
        for (auto& x : r.pwm) is >> x;
        if (!is) {
            std::fprintf(stderr, "profil satırı okunamadı: %s\n", s.c_str());
            return false;
        }
        if (!p.empty() && !(r.t > p.back().t)) {
            std::fprintf(stderr, "profil zamanları kesin artan olmalı\n");
            return false;
        }
        p.push_back(r);
    }
    return !p.empty();
}

// printf biçimli satır parçası (fopen/fprintf yerine akış; MSVC C4996'dan kaçınır).
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
void yazf(std::ostream& o, const char* bicim, ...)
{
    char tampon[2048];
    va_list ap;
    va_start(ap, bicim);
    std::vsnprintf(tampon, sizeof tampon, bicim, ap);
    va_end(ap);
    o << tampon;
}

}  // namespace

int main(int argc, char** argv)
{
    std::string profil_yolu, cikti_yolu, yuk = "iki";
    double sure = 120.0, z0 = -1.0;
    long aralik = 8;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const bool deger_var = i + 1 < argc;
        if (a == "--profil" && deger_var) profil_yolu = argv[++i];
        else if (a == "--cikti" && deger_var) cikti_yolu = argv[++i];
        else if (a == "--sure" && deger_var) sure = std::atof(argv[++i]);
        else if (a == "--aralik" && deger_var) aralik = std::atol(argv[++i]);
        else if (a == "--baslangic-z" && deger_var) z0 = std::atof(argv[++i]);
        else if (a == "--yuk" && deger_var) yuk = argv[++i];
        else {
            std::fprintf(stderr, "kullanım: %s [--profil f.csv] [--sure s] [--aralik n] [--cikti f.csv] "
                                 "[--baslangic-z m] [--yuk iki|f1|f2|bos]\n", argv[0]);
            return 2;
        }
    }
    if (!(sure > 0.0) || aralik < 1) {
        std::fprintf(stderr, "geçersiz süre ya da aralık\n");
        return 2;
    }
    std::vector<ProfilSatiri> profil;
    if (profil_yolu.empty()) {
        profil = yerlesik_profil();
    } else if (!profil_oku(profil_yolu, profil)) {
        std::fprintf(stderr, "profil okunamadı: %s\n", profil_yolu.c_str());
        return 1;
    }

    BaslangicDurumu b;
    if (yuk == "iki") b.yuklu_tupler = {"f1", "f2"};
    else if (yuk == "f1") b.yuklu_tupler = {"f1"};
    else if (yuk == "f2") b.yuklu_tupler = {"f2"};
    else if (yuk == "bos") b.yuklu_tupler = {};
    else {
        std::fprintf(stderr, "bilinmeyen yük durumu: %s\n", yuk.c_str());
        return 2;
    }

    const AracParametreleri arac = ida0_arac();
    const OrtamParametreleri ortam = havuz_ortami();
    Dunya dn;
    KurulumSonucu r = dn.kur(arac, ortam, b);
    if (r.tamam && z0 < 0.0) {
        // Varsayılan başlangıç: düz suda yüzer denge.
        b.konum[2] = dn.turetilmis().denge_konumu_z;
        r = dn.kur(arac, ortam, b);
    } else if (r.tamam) {
        b.konum[2] = z0;
        r = dn.kur(arac, ortam, b);
    }
    if (!r.tamam) {
        std::fprintf(stderr, "kurulum başarısız: %s\n", r.hata.c_str());
        return 1;
    }

    std::ofstream dosya;
    std::ostream* out = &std::cout;
    if (!cikti_yolu.empty()) {
        dosya.open(cikti_yolu);
        if (!dosya) {
            std::fprintf(stderr, "çıktı açılamadı: %s\n", cikti_yolu.c_str());
            return 1;
        }
        out = &dosya;
    }
    yazf(*out, "t_s,x_m,y_m,z_m,qw,qx,qy,qz,roll_rad,pitch_rad,yaw_rad,u,v,w,p,q,r,"
                      "fx_b,fy_b,fz_b,batik_oran");
    for (int k = 0; k < kIticiSayisi; ++k) yazf(*out, ",pwm%d", k);
    for (int k = 0; k < kIticiSayisi; ++k) yazf(*out, ",itki%d_N", k);
    yazf(*out, "\n");

    auto yaz = [&](const DurumAnlik& a) {
        const auto& q = a.yonelim;
        const double roll = std::atan2(2.0 * (q[0] * q[1] + q[2] * q[3]), 1.0 - 2.0 * (q[1] * q[1] + q[2] * q[2]));
        const double sp = std::fmax(-1.0, std::fmin(1.0, 2.0 * (q[0] * q[2] - q[3] * q[1])));
        const double yaw = std::atan2(2.0 * (q[0] * q[3] + q[1] * q[2]), 1.0 - 2.0 * (q[2] * q[2] + q[3] * q[3]));
        yazf(*out, "%.4f,%.6f,%.6f,%.6f,%.9f,%.9f,%.9f,%.9f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,"
                          "%.5f,%.5f,%.5f,%.6f",
                     static_cast<double>(a.t_ns) * 1e-9, a.konum_n[0], a.konum_n[1], a.konum_n[2], q[0], q[1], q[2],
                     q[3], roll, std::asin(sp), yaw, a.dogrusal_hiz_b[0], a.dogrusal_hiz_b[1],
                     a.dogrusal_hiz_b[2], a.acisal_hiz_b[0], a.acisal_hiz_b[1], a.acisal_hiz_b[2],
                     a.ozgul_kuvvet_b[0], a.ozgul_kuvvet_b[1], a.ozgul_kuvvet_b[2], a.batik_oran);
        for (double x : a.pwm_us) yazf(*out, ",%.1f", x);
        for (double x : a.itki) yazf(*out, ",%.4f", x);
        yazf(*out, "\n");
    };

    yaz(dn.durum());
    const double h = static_cast<double>(dn.adim_ns()) * 1e-9;
    const auto n = static_cast<std::uint64_t>(std::llround(sure / h));
    std::size_t sira = 0;
    for (std::uint64_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(dn.zaman_ns()) * 1e-9;
        bool degisti = false;
        while (sira < profil.size() && profil[sira].t <= t + 0.5 * h) {
            ++sira;
            degisti = true;
        }
        if (degisti || i == 0) {
            if (sira > 0) dn.itici_komutu_ver(dn.adim_no(), profil[sira - 1].pwm);
        }
        if (dn.adim() != AdimDurumu::tamam) {
            std::fprintf(stderr, "sayısal hata: t = %.4f s\n", t);
            return 1;
        }
        if (dn.adim_no() % static_cast<std::uint64_t>(aralik) == 0) yaz(dn.durum());
    }
    return 0;
}
