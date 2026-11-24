// İDA fizik çekirdeği (W3-fizik, dilim 1): tek aracın dünyası.
//
// Başsız ve belirlenimcidir: iş parçacığı, dosya, saat ve ağ kullanmaz; aynı ikili ve aynı
// girdiyle aynı durum dizisini bit düzeyinde üretir. Sim zamanı tamsayı nanosaniyedir; fizik
// sabit adımla ilerler (varsayılan 1,25 ms; iskelet §4). Adım içindeki sıra 04 §2.1'dir: adım
// başında aktüatör komutu uygulanır (sıfırıncı derece tutma), sonra gövde RK4 ile ilerletilir,
// kuaterniyon normalleştirilir, anlık görüntü üretilir. Dilim 1'de olay, güç, temas ve dalga
// adımları yoktur.
#pragma once

#include "ida/fizik/kutle_durumu.hpp"
#include "ida/fizik/parametreler.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace ida::fizik {

// Bir fizik adımının sonundaki durum (ölçüm zamanı t_ns). Dizinler kanal sırasıdır
// (`arac.iticiler.<tN>.kanal`; kartın gördüğü sıra).
struct DurumAnlik {
    std::int64_t t_ns = 0;
    std::uint64_t adim_no = 0;        // tamamlanan adım sayısı; t_ns = adim_no · adım
    Vek3 konum_n{};                   // O_b, NED, m
    Kuaterniyon yonelim{1.0, 0.0, 0.0, 0.0};  // gövdeden NED'e (w, x, y, z)
    Vek3 dogrusal_hiz_b{};            // (u, v, w), O_b, yere göre, gövde çerçevesi, m/s
    Vek3 acisal_hiz_b{};              // (p, q, r), rad/s
    Vek3 dogrusal_ivme_b{};           // ν̇₁:₃ (gövde çerçevesinde zaman türevi), m/s²
    Vek3 acisal_ivme_b{};             // ν̇₄:₆ = ω̇, rad/s²
    Vek3 ozgul_kuvvet_b{};            // O_b'de (ν̇₁:₃ + ω × v) − Rᵀ g, m/s² (IMU 05'te türetir)
    std::array<double, kIticiSayisi> itki{};         // N; gerçekleşen itki (havalanma ve itki çarpanı dahil)
    std::array<double, kIticiSayisi> itki_istegi{};  // N; durağan itki isteği F_ss
    std::array<double, kIticiSayisi> havalanma{};    // β(h/R)
    std::array<double, kIticiSayisi> pwm_us{};       // yürürlükteki komut
    Vek6 itici_kuvvet_momenti{};      // τ_itici, gövde, O_b (N, N·m)
    double batik_oran = 0.0;          // s = ∇_batık / ∇_tam
    double batik_hacim = 0.0;         // m³
    double yuzey_sonumu_agirligi = 0.0;  // Otter yüzey sönümünün ağırlığı [0, 1]
    // ESC başlatıldı (kanal başına): komut `baslatma_suresi_ns` boyunca kesintisiz nötr
    // (A5 §3.3: pwm_notr_us ± olu_bant_us) tutuldu. Hazır olmayan kanal itki üretmez.
    std::array<bool, kIticiSayisi> esc_hazir{};
};

enum class AdimDurumu {
    tamam,
    kurulmadi,      // kur() başarıyla çağrılmadı
    sayisal_hata,   // durum sonlu değil; dünya durur, son geçerli durum korunur (04 §4)
};

struct KurulumSonucu {
    bool tamam = false;
    std::string hata;   // tamam değilse insan okur ileti (A5 kural numarasıyla)
};

// Kurulumda hesaplanan, kütle durumuna bağlı türetilmiş büyüklükler (tanı ve test için).
struct TuretilmisBuyuklukler {
    KutleDurumu kutle{};
    std::array<std::array<double, 6>, 6> kutle_matrisi{};   // M_RB + M_A
    double tam_hacim_ag = 0.0;        // yüzerlik ağlarının toplam hacmi, m³
    Vek3 tam_yuzerlik_merkezi_ag{};   // ağdan, O_b'ye göre, m
    bool yuzeyde_denge_var = false;   // düz suda yüzer denge bulundu
    double denge_konumu_z = 0.0;      // düz yönelimde dengedeki O_b derinliği, m
    double denge_su_hatti_alani = 0.0;  // m²
    Vek3 yuzey_dogal_frekans{};       // ω₃, ω₄, ω₅ (rad/s), yüzey dengesinde
    Vek3 yuzey_sonum_katsayisi{};     // 2 ζ_i ω_i M_ii (heave, roll, pitch)
};

class Dunya {
public:
    Dunya();
    ~Dunya();
    Dunya(Dunya&&) noexcept;
    Dunya& operator=(Dunya&&) noexcept;
    Dunya(const Dunya&) = delete;
    Dunya& operator=(const Dunya&) = delete;

    // Parametreleri doğrular, kütle durumunu (`baslangic.yuklu_tupler`) türetir, yüzerlik
    // ağlarını kurar ve durumu başlangıca getirir (t = 0, adım 0, komut nötr). Hata olursa
    // dünya kurulmamış kalır.
    KurulumSonucu kur(const AracParametreleri& arac, const OrtamParametreleri& ortam,
                      const BaslangicDurumu& baslangic, const SayisalAyarlar& sayisal = {});

    // Bir fizik adımı ilerler.
    AdimDurumu adim();

    // `adim_no` numaralı adımın (t = adim_no · adım anında başlayan) başından itibaren
    // yürürlüğe girecek itici komutu; dizin kanal sırasıdır, değer µs. Adım `adim_no`
    // hesaplanmışsa (adim_no < this->adim_no()) reddedilir ve false döner. Aynı adıma verilen
    // ikinci komut öncekinin yerine geçer (aynı fizik sınırına düşen çıktıların sonuncusu
    // yürürlükte kalır; sıralama köprünündür). Komutun ömrü ve nötre dönüş köprünün işidir
    // (A1-K74, A1-K77): fizik yeni bir komut verilene kadar son komutu uygular.
    bool itici_komutu_ver(std::uint64_t adim_no, const std::array<double, kIticiSayisi>& pwm_us);

    // İtici arızası (A5 Tablo 3.10-A `aktuator/itki_carpani`; W4-K20): `adim_no` numaralı adımın
    // başından itibaren itici başına mekanik kuvvet çarpanı; dizin kanal sırasıdır, değer [0, 1].
    // Çarpan süzgeç ve havalanma (β) sonrasındaki kuvvete uygulanır (F_i ← c_i · β · K(s) F_ss);
    // süzgeç durumu ve elektrik tarafı (`itki_istegi`, F_ss) arızasız komutu görür. Yeni bir
    // çarpan verilene kadar kalır; varsayılan 1 (bit düzeyinde arızasız davranış). Adım
    // hesaplanmışsa ya da değer aralık dışıysa reddedilir (false).
    bool itki_carpani_ver(std::uint64_t adim_no, const std::array<double, kIticiSayisi>& carpan);

    bool kuruldu() const;
    std::int64_t zaman_ns() const;
    std::uint64_t adim_no() const;
    std::int64_t adim_ns() const;
    const DurumAnlik& durum() const;
    const TuretilmisBuyuklukler& turetilmis() const;

    // Toplam mekanik enerji (J): ½ νᵀ M ν + yerçekimi ve hidrostatik potansiyel
    // (−m g z_G + ρ g ∇_batık z_c). Akıntısız durumda anlamlıdır (test ve tanı için).
    double mekanik_enerji() const;

    // Normalleştirmeden önce kuaterniyon normunun 1'den 1e-9'dan fazla saptığı adım sayısı
    // (04 §4 "olay").
    std::uint64_t kuaterniyon_sapma_sayaci() const;

private:
    struct Ic;
    std::unique_ptr<Ic> ic_;
};

}  // namespace ida::fizik
