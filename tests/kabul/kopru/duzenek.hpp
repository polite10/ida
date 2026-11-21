// tests/kabul/kopru — köprü düzeneği: ida::kopru::Kopru (yalnız açık başlığı) + sahte kart,
// bellek içi taşıma (ida::kopru::bellek_cifti) üzerinden.
//
// Zaman: fizik sınırı b'nin sim zamanı t = b·h. Her sınırda, köprünün açık başlığındaki çağrı
// sırasıyla (kopru.hpp): zamanla(duvar) → sinir_isle(b) → (kontrol sınırıysa) IMU ölçümü +
// parti_kapat(t) → baytların iki yönde taşınması. Duvar saati sim zamanından türetilir
// (duvar = duvar0 + t; HIL 1x kipinde de gerçek saat kullanılmaz, böylece vaka belirlenimcidir).
//
// Sahte kartın bir çerçevesi t_birak ≤ t olan ilk taşımada bırakılır; t_birak = (B − 1)·h
// verilen çerçeve sinir_isle(B − 1)'den sonra, sinir_isle(B)'den önce köprüye ulaşır.
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "ida/kopru/kopru.hpp"
#include "ida/kopru/tasima.hpp"
#include "sahte_kart.hpp"

namespace kabul {

class Duzenek {
public:
    using Yanitci = std::function<void(const GelenTf&, std::int64_t)>;

    explicit Duzenek(ida::kopru::CalistirmaKipi kip = ida::kopru::CalistirmaKipi::hil_1x,
                     const ida::kopru::KopruAyari* ek = nullptr,
                     const SahteKart::Ayar* kart_ayari = nullptr);

    // El sıkışma: KOSUYOR'a kadar (en çok ~2 s duvar). Başarı döner.
    bool kur();

    // Bir fizik sınırını işler (b → b + 1).
    void adim();
    void ilerle(int n);
    // Sim zamanı t'ye kadar (t dahil) bütün sınırları işler.
    void ilerle_kadar(std::int64_t t);
    // Sıradaki kontrol sınırına kadar ilerler (o sınır dahil); kartın aldığı son TF.
    GelenTf sonraki_tf();
    // Bütün yönlerde bekleyen baytları taşır.
    void pompa();

    // Senaryo emniyet ataması (sonraki sinir_isle'de uygulanır).
    void emniyet(std::optional<bool> acil, std::optional<bool> fis0, std::optional<bool> fis1,
                 std::optional<bool> pim0, std::optional<bool> pim1);
    // Olağan izin veren durum: acil durdurma yok, iki fiş takılı, iki pim çıkık.
    void izinli_durum();

    // Sıradaki TF için tek seferlik yanıtçı (yoksa varsayılan: geçerli yanıt, olay yok).
    void sonraki_tf_icin(Yanitci f) { tek_seferlik_.push_back(std::move(f)); }
    void varsayilan_yanit(const GelenTf& tf, std::int64_t t);

    // Oturum bittikten sonra yeniden KOSUYOR'a kadar fizik sınırlarını işler.
    bool yeniden_kur(int en_cok_sinir = 4000);

    // sinir_isle(b)'den hemen sonra, o sınırın taşımasından önce çağrılır (b'den sonraki
    // sınırda uygulanacak atamalar ve olayların geliş sırası için).
    std::function<void(std::uint64_t)> sinir_sonrasi;
    // Sınır b işlendikten sonraki yetkili durum (dis_durum().emniyet).
    const ida::sensor::EmniyetDurumu& emniyet_b(std::uint64_t b) const { return emn_.at(b); }
    // Oturum kapalıyken köprü kendiliğinden el sıkışmaya başlamadıysa el_sikisma_baslat çağrılır.
    bool otomatik_yeniden = true;
    // Sınır b işlendikten sonraki oturum durumu.
    ida::kopru::OturumDurumu durum_b(std::uint64_t b) const { return durum_.at(b); }
    // Tek seferlik yanıtçı yokken kullanılan kalıcı yanıtçı (boşsa varsayılan yanıt).
    Yanitci kalici_yanitci;

    std::uint64_t b() const { return b_; }                 // işlenecek sıradaki sınır
    std::int64_t t_sonraki() const { return static_cast<std::int64_t>(b_) * kH; }
    std::int64_t duvar(std::int64_t t) const { return duvar0_ + t; }
    static std::uint64_t sinir(std::int64_t t) { return static_cast<std::uint64_t>(t / kH); }
    const ida::kopru::SinirSonucu& sonuc_b(std::uint64_t b) const { return sinirlar_.at(b); }
    bool islendi(std::uint64_t b) const { return b < sinirlar_.size() && islendi_.at(b); }

    // A1-K74: o sınırda uygulanan yürürlükteki komutun ilk kanalı (nötrde ESC nötrü).
    double pwm0(std::uint64_t b) const { return sinirlar_.at(b).pwm_us[0]; }
    bool hepsi(std::uint64_t b, double deger) const;

    // Sensör kümesinin emniyet geri okuma kaydı (taklit): kopru.hpp'ye göre emniyet örneği
    // köprüye sensör kümesinden olcum_ekle ile gelir.
    void emniyet_kaydi_ekle(std::int64_t t, const ida::sensor::EmniyetDurumu& e);
    static std::uint8_t tup_bitleri(const ida::sensor::EmniyetDurumu& e, int i);

    ida::kopru::Kopru k;
    SahteKart kart;
    std::uint16_t varsayilan_pwm = 1600;
    bool varsayilan_acik = true;   // false: TF'ye hiç yanıt verilmez (tek seferlikler dışında)
    bool emniyet_kaydi_uret = true;
    // Verilirse her sınırda sinir_isle(b)'den sonra ölçüm kayıtlarını üretir (köprüye
    // olcum_ekle ile verir); taklit emniyet kaydı ve IMU üretilmez.
    std::function<void(std::uint64_t, std::int64_t)> olcum_uretici;
    std::uint64_t parti_sayisi = 0;
    std::uint64_t emniyet_kaydi_sayisi = 0;

private:
    void emniyet_kaydi(std::int64_t t);

    bool son_emniyet_var_ = false;
    bool son_estop_ = false;
    std::array<std::uint8_t, 2> son_tup_{};
    std::uint32_t son_rev_ = 0;
    std::int64_t son_emniyet_t_ = 0;
    std::uint32_t emniyet_seq_ = 0;
    std::unique_ptr<ida::kopru::Tasima> sim_uc_;
    std::unique_ptr<ida::kopru::Tasima> kart_uc_;
    std::uint64_t b_ = 0;
    std::int64_t duvar0_ = 0;
    std::int64_t t_simdi_ = 0;
    std::uint32_t imu_seq_ = 0;
    std::vector<ida::kopru::SinirSonucu> sinirlar_;
    std::vector<ida::sensor::EmniyetDurumu> emn_;
    std::vector<ida::kopru::OturumDurumu> durum_;
    std::vector<bool> islendi_;
    std::deque<Yanitci> tek_seferlik_;
    ida::kopru::CalistirmaKipi kip_;
};

}  // namespace kabul
