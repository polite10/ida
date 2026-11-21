#include "baglanti.hpp"

namespace ida::viz {

IstemciBaglantisi::IstemciBaglantisi(IstemciOturumu::Ayarlar ayarlar, std::uint16_t port)
    : oturum_(std::move(ayarlar)), port_(port) {
  soketleri_hazirla();
}

void IstemciBaglantisi::kopar(const std::string& neden, std::int64_t simdi_ns) {
  soket_.kapat();
  giden_.clear();
  son_hata_ = neden;
  oturum_.baglanti_koptu(neden);
  if (oturum_.yeniden_baglanabilir()) {
    durum_ = Durum::bekliyor;
    sonraki_deneme_ns_ = simdi_ns + a2::kYenidenBaglanmaAraligiNs;
  } else {
    durum_ = Durum::durdu;
  }
}

void IstemciBaglantisi::gonder() {
  Baytlar yeni = oturum_.giden_al();
  giden_.insert(giden_.end(), yeni.begin(), yeni.end());
  if (giden_.empty() || soket_.durum() != TcpSoket::Durum::bagli) return;
  std::string h;
  soket_.yaz(giden_, h);
}

void IstemciBaglantisi::adim(std::int64_t simdi_ns, std::uint32_t cizim_hizi_mhz, bool pencere_cizilebilir) {
  switch (durum_) {
    case Durum::durdu:
      return;
    case Durum::bekliyor: {
      if (simdi_ns < sonraki_deneme_ns_) return;
      std::string h;
      if (!soket_.baglan_baslat(port_, h)) {
        son_hata_ = h;
        sonraki_deneme_ns_ = simdi_ns + a2::kYenidenBaglanmaAraligiNs;
        return;
      }
      durum_ = Durum::baglaniyor;
      sonraki_deneme_ns_ = simdi_ns + a2::kYenidenBaglanmaAraligiNs;  // bağlanma için süre
      [[fallthrough]];
    }
    case Durum::baglaniyor: {
      std::string h;
      const TcpSoket::Durum d = soket_.durum_yokla(h);
      if (d == TcpSoket::Durum::kapali) {
        son_hata_ = h.empty() ? "bağlanılamadı" : h;
        durum_ = Durum::bekliyor;
        sonraki_deneme_ns_ = simdi_ns + a2::kYenidenBaglanmaAraligiNs;
        return;
      }
      if (d == TcpSoket::Durum::baglaniyor) {
        if (simdi_ns >= sonraki_deneme_ns_) {  // 1 s içinde tamamlanmadı: yeniden dene
          soket_.kapat();
          son_hata_ = "bağlantı zaman aşımı";
          durum_ = Durum::bekliyor;
        }
        return;
      }
      durum_ = Durum::bagli;
      ++baglanti_sayisi_;
      giden_.clear();
      oturum_.baglandi(simdi_ns, pencere_cizilebilir);
      gonder();
      [[fallthrough]];
    }
    case Durum::bagli: {
      std::string h;
      bool acik = true;
      // sol #1: okuma bütçeli; her parça hemen çerçevelenip denetlenir. Oturum kapanırsa
      // (ör. bozuk başlık) okuma o parçada durur.
      std::size_t butce = kAdimOkumaButcesi;
      while (butce > 0 && acik && !oturum_.kapatilmali()) {
        gelen_.clear();
        acik = soket_.oku(gelen_, butce < kParcaBoyutu ? butce : kParcaBoyutu, h);
        okunan_bayt_ += gelen_.size();
        butce -= gelen_.size();
        if (!gelen_.empty()) oturum_.veri_geldi(gelen_.data(), gelen_.size(), simdi_ns);
        if (gelen_.size() < kParcaBoyutu) break;  // soket şimdilik boş
      }
      oturum_.zaman(simdi_ns, cizim_hizi_mhz, pencere_cizilebilir);
      gonder();
      if (oturum_.kapatilmali()) {
        // Oturum kendisi kapattı (protokol hatası, veda, ret, nabız): veda gönderildi.
        oturum_.kapatildi();
        kopar(oturum_.son_kapanis_nedeni(), simdi_ns);
        return;
      }
      if (!acik) kopar(h, simdi_ns);
      return;
    }
  }
}

void IstemciBaglantisi::olagan_kapat() {
  if (durum_ == Durum::bagli) {
    oturum_.olagan_kapat();
    gonder();
  }
  soket_.kapat();
  durum_ = Durum::durdu;
}

}  // namespace ida::viz
