// Sade kademe ışın demeti ve biriktirme (05 §2.4.2–§2.4.4; iskelet §6, §8).
//
// Hat: ışın örnekleme (katmanlı, titreşimli; sayaç tabanlı RNG) → sahne kesişimi (doğrudan yol)
// → ışın enerji ağırlığı → gövdeye sabit saçıcı hücrelerinde enerji toplamı (iskelet §8
// "Sonar saçıcı kuralı") → kesirli gecikmeli biriktirme (ince ızgara T_s/4) → darbe/alıcı
// çekirdeğinin karesiyle evrişim → kutulara indirgeme. Çıktı alıcı öncesi kutu enerjisidir
// (normalize yoğunluk Ĩ); benek, gürültü ve alıcı zinciri modelde uygulanır.
#pragma once

#include "ida/sensor/rastgele.hpp"
#include "ida/sonar/sahne.hpp"

#include <cstdint>
#include <vector>

namespace ida::sonar::ic {

// Ayrılabilir sinc deseni (05 §2.4.2 [T]): B(θ, φ) = sinc(θ/θ0) · sinc(φ/φ0), tek yönlü genlik;
// θ0 = (−3 dB yarı genişliği) / 0,44295. Sade kademede koni ana lobdur: |θ| ≤ θ0, |φ| ≤ φ0.
struct Desen {
    double teta0 = 0.0;   // rad
    double fi0 = 0.0;     // rad
    static Desen tam_genislikten(double yatay_3db_rad, double dusey_3db_rad);
};

double sinc(double x);

// Saçıcı hücresi kimliği (gövde, yüzey, i, j) ve yol türü (sade: yalnız doğrudan, 0).
struct HucreAnahtari {
    std::uint32_t govde = 0;
    std::uint32_t yuzey = 0;
    std::int32_t i = 0;
    std::int32_t j = 0;
    std::uint8_t yol = 0;
};

struct DemetGirdisi {
    const Sahne* sahne = nullptr;
    std::int64_t t_ns = 0;            // sahne pozlarının anı (gönderim anı)
    Vek3 konum{};                     // dönüştürücü, NED
    fizik::Mat3 r_nh{};               // kafa çerçevesinden NED'e (kafa +x ışın ekseni)
    Desen desen{};
    int isin_sayisi = 5000;
    std::uint64_t ping_no = 0;        // RNG sayacı n
    const sensor::Akis* isin_akisi = nullptr;   // `sensor/<ad>/sonar_isin`
    double c = 1500.0;                // m/s
    double alfa_db_m = 0.0;
    double hucre_kenari = 0.01;       // m (MP7 kuralı, Δ_h)
    // Zaman ekseni (iki yönlü süre, s): kutu i = [tau0 + i·T_s, tau0 + (i+1)·T_s).
    double tau0 = 0.0;
    double t_s = 1e-5;
    int n_kutu = 1;
    double darbe_s = 0.0;             // darbe süresi; çekirdek yarı genişliği
};

struct DemetSonucu {
    std::vector<double> ham;          // n_kutu
    std::uint32_t isin = 0;
    std::uint32_t isabet = 0;
};

// Bir saçıcı hücresinin katkısı: iki yönlü gecikme (s) ve enerji.
struct Katki {
    double tau = 0.0;
    double enerji = 0.0;
};

struct BiriktirmeTamponu {
    std::vector<double> ince;
    std::vector<double> evr;
    std::vector<double> cekirdek;
};

// Katkıları kutulara biriktirir (05 §2.4.4): ince ızgara T_s/4, doğrusal kesirli gecikme;
// darbe ⊗ alıcı süzgeci = üçgen (yarı genişlik darbe süresi, sıfır fazlı), sade kademede karesi;
// kutuya indirgeme (4 ince örneğin ortalaması). İnce dizi çekirdek desteği kadar iki yönde
// genişletilir: pencerenin hemen dışındaki ekonun taşan kısmı da pencereye düşer; pencere
// evrişimden sonra kırpılır (sol #5).
void biriktir(const std::vector<Katki>& katkilar, double tau0, double t_s, int n_kutu, double darbe_s,
              BiriktirmeTamponu& tampon, std::vector<double>& ham);

// Dinlenen en uzak iki yönlü süre (çekirdek payıyla), s.
double dinleme_sonu(double tau0, double t_s, int n_kutu, double darbe_s);

// Yeniden kullanılan çalışma tamponları (ping başına ayırma yapmamak için).
class Demet {
public:
    void izle(const DemetGirdisi& g, DemetSonucu& s);
    // `g` ile yapılacak `izle` çağrısının tamponlarını önceden ayırır (ışın izlemez; W4-K27 (5)).
    // Kapasite yalnız büyür; aynı boyutlu sonraki `izle` çağrıları bellek ayırmaz.
    void hazirla(const DemetGirdisi& g, DemetSonucu& s);

private:
    struct Hucre {
        HucreAnahtari k;
        double sw = 0.0;      // Σ w_j
        double swt = 0.0;     // Σ w_j τ_j
    };
    std::vector<std::int32_t> tablo_;
    std::vector<Hucre> hucreler_;
    std::vector<Katki> katkilar_;
    BiriktirmeTamponu tampon_;
};

}  // namespace ida::sonar::ic
