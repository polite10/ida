// Ortam akustiği: ses hızı ve soğurma (05 §2.4.1, §2.4.2; W4-K1).
//
// Sensör kümesi (DVL) ve sonar modelleri aynı işlevleri kullanır; `ida_sonar_akustik` kitaplığı
// başka hiçbir İDA kitaplığına bağlı değildir.
//
// Ses hızı (W4-K1, 05 §2.4.1):
//  - A5 `ortam.su.ses_hizi` verilmişse o sabit kullanılır.
//  - Tatlı suda UNESCO denklemi (Chen–Millero 1977; Fofonoff ve Millard 1983 katsayıları).
//    Girdi sıcaklığı ITS-90'dır; katsayılar IPTS-68 içindir: T68 = 1,00024 · T90 dönüşümüyle
//    değerlendirilir. Wong–Zhu (1995) ITS-90 katsayılarının bu yoldan farkı ≤ 0,024 m/s'dir
//    (Wong ve Zhu 1995 özeti). Wong–Zhu katsayıları bu işte bağımsız doğrulanamadığı için
//    kullanılmadı (sim/sonar/BENIOKU.md).
//  - Deniz suyunda Mackenzie (1981) dokuz terimli denklemi.
//  - Ayrım tuzlulukla yapılır: S ≥ 25 ‰ (Mackenzie'nin geçerlilik alanının alt ucu) deniz suyu,
//    altı tatlı su sayılır [T].
//  - Sahne boyunca tek ses hızı kullanılır; kırılma ve derinlikle değişim ihmal edilir
//    (05 §2.4.1 [T]); değerlendirme derinliği `z_m` (sonar ve sensör kümesi 0 m kullanır).
//
// Soğurma (05 §2.4.2): Ainslie–McColm (1998) biçimi, 05'teki yazımıyla. Formülün kendisi ve
// tatlı su uygulaması bağımsız doğrulanmadı (05 §2.4.2; A5-K108 uyarısı).
#pragma once

#include <optional>

namespace ida::sonar {

// UNESCO 1983 ses hızı polinomu (Chen–Millero 1977); T68 °C (IPTS-68), S ‰ (PSU),
// P bar (gösterge basıncı; yüzeyde 0). m/s.
double ses_hizi_unesco_t68(double t68_c, double s_ppt, double p_bar);

// Aynı denklem, ITS-90 sıcaklığıyla (T68 = 1,00024 · T90).
double ses_hizi_chen_millero(double t90_c, double s_ppt, double p_bar);

// Mackenzie (1981): T °C, S ‰, D m (derinlik). m/s.
double ses_hizi_mackenzie(double t_c, double s_ppt, double d_m);

enum class SesHiziKaynagi { sabit, chen_millero, mackenzie };

struct SesHizi {
    double c = 0.0;                                   // m/s
    SesHiziKaynagi kaynak = SesHiziKaynagi::sabit;
    // Girdi formülün geçerlilik alanının dışında: sonuç doğrulanmamış dışdeğerlemedir
    // (reddedilmez; A5'in ortam aralığı daha geniştir, W4-K18/sol #8). Alanlar:
    //  - Mackenzie (1981): 2–30 °C, 25–40 ‰, 0–8000 m.
    //  - UNESCO (Chen–Millero 1977; Fofonoff ve Millard 1983): 0–40 °C, 0–40 ‰, 0–1000 bar.
    bool dislama = false;
};

// `dislama` için kurulum uyarısının metni (boş: alan içinde).
const char* ses_hizi_uyarisi(const SesHizi& s);

// Deniz suyu ayrımının tuzluluk eşiği (‰).
inline constexpr double kDenizSuyuEsigiPpt = 25.0;

// Ortamın ses hızı (W4-K1). `tuzluluk` A5 birimiyle kg/kg; `sabit` A5 `ortam.su.ses_hizi`.
// `z_m` değerlendirme derinliği (m, aşağı pozitif); basınç `yogunluk · yercekimi · z`.
SesHizi ortam_ses_hizi(double sicaklik_c, double tuzluluk, const std::optional<double>& sabit,
                       double z_m = 0.0, double yogunluk = 1000.0, double yercekimi = 9.82);

// Soğurma, dB/m (05 §2.4.2, Ainslie–McColm biçimi): f Hz, T °C, tuzluluk kg/kg, z m, pH.
double sogurma_db_m(double f_hz, double t_c, double tuzluluk, double z_m, double ph);

}  // namespace ida::sonar
