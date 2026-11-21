// A2 tel kodlaması. Sözleşme bölümleri her denetimin yanında.
#include "ida/viz/kodek.hpp"

#include <cmath>
#include <limits>

#include "ida/viz/sha256.hpp"

namespace ida::viz {

namespace {

using a2::IletiTuru;

bool sonlu(double x) { return std::isfinite(x); }

template <std::size_t N>
bool hepsi_sonlu(const double (&d)[N]) {
  for (double x : d) {
    if (!sonlu(x)) return false;
  }
  return true;
}

// a2_wire.h: kuaterniyon normu binary64'te w, x, y, z sırasıyla toplanır.
double norm_kare(const double (&q)[4]) { return q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]; }

bool birim_kuaterniyon(const double (&q)[4], double tolerans) {
  if (!hepsi_sonlu(q)) return false;
  // §3.4: |‖q‖ − 1| ≤ 1e-6 (norm); §3.6: |w²+x²+y²+z² − 1| ≤ 1e-5 (norm karesi).
  return std::fabs(std::sqrt(norm_kare(q)) - 1.0) <= tolerans;
}

bool birim_vektor(const double (&v)[3], double tolerans) {
  if (!hepsi_sonlu(v)) return false;
  return std::fabs(std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) - 1.0) <= tolerans;
}

void baslik_ekle(Baytlar& out, IletiTuru tur, std::uint32_t sira, std::size_t yuk_bayt) {
  a2::Baslik b{};
  b.sihirli = a2::kSihirliSayi;
  b.tur = static_cast<std::uint16_t>(tur);
  b.bayraklar = 0;
  b.yuk_bayt = static_cast<std::uint32_t>(yuk_bayt);
  b.sira = sira;
  yapi_ekle(out, b);
}

template <class T>
void sabit_ileti(Baytlar& out, IletiTuru tur, std::uint32_t sira, const T& v) {
  baslik_ekle(out, tur, sira, sizeof(T));
  yapi_ekle(out, v);
}

template <class T>
KodHata sabit_coz(BaytGorunumu yuk, T& out) {
  if (yuk.size() != sizeof(T)) return KodHata::uzunluk;
  out = yapi_oku<T>(yuk.data());
  return KodHata::yok;
}

KodHata govde_bilgisi_denetle(const a2::GovdeBilgisi& g) {
  if (g.govde_turu < 1 || g.govde_turu > 3) return KodHata::alan;
  // §3.3: dolu_tupler araç dışındaki gövdelerde 0.
  if (g.govde_turu != static_cast<std::uint8_t>(a2::GovdeTuru::arac) && g.dolu_tupler != 0) return KodHata::alan;
  return KodHata::yok;
}

KodHata govde_listesi_denetle(const std::vector<a2::GovdeBilgisi>& liste) {
  for (std::size_t i = 0; i < liste.size(); ++i) {
    const KodHata h = govde_bilgisi_denetle(liste[i]);
    if (h != KodHata::yok) return h;
    for (std::size_t j = 0; j < i; ++j) {
      if (liste[j].govde_kimligi == liste[i].govde_kimligi) return KodHata::alan;  // tekil kimlik
    }
  }
  return KodHata::yok;
}

// §3.6 kamera yapılandırması.
KodHata kamera_denetle(const a2::KameraYapilandirmasi& k, std::uint8_t kademe,
                       const std::vector<a2::GovdeBilgisi>& govdeler) {
  if (k.kamera_kimligi != 0) return KodHata::alan;
  if (k.piksel_bicimi != static_cast<std::uint8_t>(a2::PikselBicimi::rgb8_srgb)) return KodHata::alan;
  // Hız = pay / payda kademenin değerine eşit olmalı; çarpım 64 bitte (sol #2: u32 taşması
  // 30 × 2³¹ ≡ 0 ile 0 Hz'i kabul ettiriyordu).
  const bool tam = kademe == static_cast<std::uint8_t>(a2::Kademe::tam);
  const std::uint64_t hiz = tam ? a2::kTamHizHz : a2::kSadeHizHz;
  if (tam ? (k.genislik != a2::kTamGenislik || k.yukseklik != a2::kTamYukseklik)
          : (k.genislik != a2::kSadeGenislik || k.yukseklik != a2::kSadeYukseklik))
    return KodHata::alan;
  if (k.hiz_payda == 0 || static_cast<std::uint64_t>(k.hiz_pay) != hiz * static_cast<std::uint64_t>(k.hiz_payda))
    return KodHata::alan;
  if (k.yapilandirma_surumu < 1) return KodHata::alan;
  if (k.kalibrasyon_surumu < 1 || k.ayrilmis_kal != 0) return KodHata::alan;
  if (!sonlu(k.fx) || !sonlu(k.fy) || !sonlu(k.cx) || !sonlu(k.cy)) return KodHata::alan;
  if (!(k.fx > 0.0) || !(k.fy > 0.0)) return KodHata::alan;
  if (k.cx < -0.5 || k.cx > static_cast<double>(k.genislik) - 0.5) return KodHata::alan;
  if (k.cy < -0.5 || k.cy > static_cast<double>(k.yukseklik) - 0.5) return KodHata::alan;
  if (k.bozulma_modeli > 1) return KodHata::alan;
  if (!hepsi_sonlu(k.bozulma) || !hepsi_sonlu(k.montaj_l_b)) return KodHata::alan;
  for (double x : k.montaj_l_b) {
    if (std::fabs(x) > a2::kKameraAzamiMontajKonumuM) return KodHata::alan;
  }
  if (!hepsi_sonlu(k.montaj_q_bs) || std::fabs(norm_kare(k.montaj_q_bs) - 1.0) > a2::kKameraKuaterniyonToleransi)
    return KodHata::alan;
  if (k.pozlama_ns <= 0 || (k.pozlama_ns % 2) != 0 || k.pozlama_ns > a2::kKameraAzamiPozlamaNs) return KodHata::alan;
  if (!sonlu(k.yakin_m) || !sonlu(k.uzak_m) || !(k.yakin_m > 0.0) || !(k.yakin_m < k.uzak_m)) return KodHata::alan;
  bool arac_var = false;
  for (const auto& g : govdeler) {
    if (g.govde_kimligi == k.montaj_govde_kimligi && g.govde_turu == static_cast<std::uint8_t>(a2::GovdeTuru::arac))
      arac_var = true;
  }
  if (!arac_var) return KodHata::alan;
  return KodHata::yok;
}

}  // namespace

const char* hata_adi(KodHata h) {
  switch (h) {
    case KodHata::yok: return "yok";
    case KodHata::sihirli: return "sihirli sayı yanlış";
    case KodHata::tur_bilinmiyor: return "bilinmeyen ileti türü";
    case KodHata::yon_yanlis: return "ileti türü bu yönde gelmez";
    case KodHata::bayraklar: return "başlık bayrakları sıfır değil";
    case KodHata::yuk_buyuk: return "yük 1 MiB'den büyük";
    case KodHata::uzunluk: return "yük uzunluğu beklenenden farklı";
    case KodHata::metin: return "metin alanında NUL yok";
    case KodHata::sinir: return "sayı alanı sınırı aşıyor";
    case KodHata::alan: return "alan değeri geçersiz";
    case KodHata::ozet: return "dalga kümesi özeti tutmuyor";
  }
  return "?";
}

TurBilgisi tur_bilgisi(std::uint16_t tur) {
  TurBilgisi b;
  b.bilinen = true;
  switch (static_cast<IletiTuru>(tur)) {
    case IletiTuru::merhaba: b.yon = Yon::gorsel_sim; b.sabit_boyut = sizeof(a2::Merhaba); b.ad = "merhaba"; break;
    case IletiTuru::hazir: b.yon = Yon::gorsel_sim; b.sabit_boyut = sizeof(a2::Hazir); b.ad = "hazir"; break;
    case IletiTuru::kare_hazir: b.yon = Yon::gorsel_sim; b.sabit_boyut = sizeof(a2::KareHazir); b.ad = "kare_hazir"; break;
    case IletiTuru::kare_dustu: b.yon = Yon::gorsel_sim; b.sabit_boyut = sizeof(a2::KareDustu); b.ad = "kare_dustu"; break;
    case IletiTuru::goruntuleyici_nabzi:
      b.yon = Yon::gorsel_sim; b.sabit_boyut = sizeof(a2::GoruntuleyiciNabzi); b.ad = "goruntuleyici_nabzi"; break;
    case IletiTuru::halka_eslendi:
      b.yon = Yon::gorsel_sim; b.sabit_boyut = sizeof(a2::HalkaEslendi); b.ad = "halka_eslendi"; break;
    case IletiTuru::hosgeldin:
      b.yon = Yon::sim_gorsel; b.sabit_boyut = sizeof(a2::Hosgeldin); b.degisken = true; b.ad = "hosgeldin"; break;
    case IletiTuru::ret: b.yon = Yon::sim_gorsel; b.sabit_boyut = sizeof(a2::Ret); b.ad = "ret"; break;
    case IletiTuru::dalga_kumesi:
      b.yon = Yon::sim_gorsel; b.sabit_boyut = sizeof(a2::DalgaKumesi); b.degisken = true; b.ad = "dalga_kumesi"; break;
    case IletiTuru::ortam: b.yon = Yon::sim_gorsel; b.sabit_boyut = sizeof(a2::Ortam); b.ad = "ortam"; break;
    case IletiTuru::malzeme_optik:
      b.yon = Yon::sim_gorsel; b.sabit_boyut = sizeof(a2::MalzemeOptik); b.degisken = true; b.ad = "malzeme_optik"; break;
    case IletiTuru::durum:
      b.yon = Yon::sim_gorsel; b.sabit_boyut = sizeof(a2::Durum); b.degisken = true; b.ad = "durum"; break;
    case IletiTuru::sahne_olayi:
      b.yon = Yon::sim_gorsel; b.sabit_boyut = sizeof(a2::SahneOlayi); b.ad = "sahne_olayi"; break;
    case IletiTuru::kamera_istegi:
      b.yon = Yon::sim_gorsel; b.sabit_boyut = sizeof(a2::KameraIstegi); b.degisken = true; b.ad = "kamera_istegi"; break;
    case IletiTuru::kamera_iptal:
      b.yon = Yon::sim_gorsel; b.sabit_boyut = sizeof(a2::KameraIptal); b.ad = "kamera_iptal"; break;
    case IletiTuru::sim_nabzi: b.yon = Yon::sim_gorsel; b.sabit_boyut = sizeof(a2::SimNabzi); b.ad = "sim_nabzi"; break;
    case IletiTuru::halka_yenile:
      b.yon = Yon::sim_gorsel; b.sabit_boyut = sizeof(a2::HalkaBilgisi); b.ad = "halka_yenile"; break;
    case IletiTuru::govde_tablosu:
      b.yon = Yon::sim_gorsel; b.sabit_boyut = sizeof(a2::GovdeTablosu); b.degisken = true; b.ad = "govde_tablosu"; break;
    case IletiTuru::veda: b.yon = Yon::iki_yon; b.sabit_boyut = sizeof(a2::Veda); b.ad = "veda"; break;
    default: b.bilinen = false; break;
  }
  return b;
}

// ---------------------------------------------------------------------------

void metin_yaz(char* alan, std::size_t boyut, std::string_view s) {
  std::memset(alan, 0, boyut);
  if (boyut == 0) return;
  std::size_t n = s.size() < boyut - 1 ? s.size() : boyut - 1;
  // UTF-8 dizisinin ortasında kesme: devam baytlarını (10xxxxxx) geri al.
  if (n < s.size()) {
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0u) == 0x80u) --n;
  }
  std::memcpy(alan, s.data(), n);
}

bool metin_gecerli(const char* alan, std::size_t boyut) {
  return std::memchr(alan, 0, boyut) != nullptr;
}

std::string metin_oku(const char* alan, std::size_t boyut) {
  const void* nul = std::memchr(alan, 0, boyut);
  const std::size_t n = nul ? static_cast<std::size_t>(static_cast<const char*>(nul) - alan) : boyut;
  return std::string(alan, n);
}

bool zaman_adimla_tutarli(std::int64_t t_ns, std::uint64_t adim_no, std::int64_t adim_ns) {
  if (adim_ns <= 0 || t_ns < 0) return false;
  const auto adim = static_cast<std::uint64_t>(adim_ns);
  if (adim_no > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) / adim) return false;
  return adim_no * adim == static_cast<std::uint64_t>(t_ns);
}

bool govde_durumu_gecerli(const a2::GovdeDurumu& g) {
  if ((g.bayraklar & a2::kGovdeGecerli) == 0) return false;
  if (!hepsi_sonlu(g.p_n) || !hepsi_sonlu(g.v_n) || !hepsi_sonlu(g.w_b)) return false;
  return birim_kuaterniyon(g.q_nb, a2::kKuaterniyonNormToleransi);
}

// ---------------------------------------------------------------------------
// Çerçeveleme
// ---------------------------------------------------------------------------

bool CerceveOkuyucu::ekle(const Bayt* veri, std::size_t n) {
  if (bas_ > 0 && bas_ == tampon_.size()) {
    tampon_.clear();
    bas_ = 0;
  }
  if (n > kAzamiBekleyenBayt || bekleyen() > kAzamiBekleyenBayt - n) return false;
  if (bas_ > 0 && tampon_.size() + n > kAzamiBekleyenBayt) {  // tüketilmiş baştakileri at
    tampon_.erase(tampon_.begin(), tampon_.begin() + static_cast<std::ptrdiff_t>(bas_));
    bas_ = 0;
  }
  tampon_.insert(tampon_.end(), veri, veri + n);
  return true;
}

void CerceveOkuyucu::temizle() {
  tampon_.clear();
  bas_ = 0;
}

bool CerceveOkuyucu::sonraki(a2::Baslik& baslik, Baytlar& yuk, KodHata& hata) {
  hata = KodHata::yok;
  if (bekleyen() < a2::kBaslikBoyutu) return false;
  baslik = yapi_oku<a2::Baslik>(tampon_.data() + bas_);
  // A2-K06: her başlık denetimi, yükü beklemeden yapılır.
  if (baslik.sihirli != a2::kSihirliSayi) {
    hata = KodHata::sihirli;
    return false;
  }
  const TurBilgisi tb = tur_bilgisi(baslik.tur);
  if (!tb.bilinen) {
    hata = KodHata::tur_bilinmiyor;
    return false;
  }
  if (tb.yon != Yon::iki_yon && tb.yon != alici_) {
    hata = KodHata::yon_yanlis;
    return false;
  }
  if (baslik.bayraklar != 0) {
    hata = KodHata::bayraklar;
    return false;
  }
  if (baslik.yuk_bayt > a2::kAzamiYuk) {
    hata = KodHata::yuk_buyuk;
    return false;
  }
  if (tb.degisken ? baslik.yuk_bayt < tb.sabit_boyut : baslik.yuk_bayt != tb.sabit_boyut) {
    hata = KodHata::uzunluk;
    return false;
  }
  if (bekleyen() < a2::kBaslikBoyutu + baslik.yuk_bayt) return false;
  const Bayt* p = tampon_.data() + bas_ + a2::kBaslikBoyutu;
  yuk.assign(p, p + baslik.yuk_bayt);
  bas_ += a2::kBaslikBoyutu + baslik.yuk_bayt;
  // Tüketilmiş baştaki baytları ara sıra at (tampon sınırsız büyümesin).
  if (bas_ > 65536 && bas_ * 2 > tampon_.size()) {
    tampon_.erase(tampon_.begin(), tampon_.begin() + static_cast<std::ptrdiff_t>(bas_));
    bas_ = 0;
  }
  return true;
}

void ileti_yaz(Baytlar& out, a2::IletiTuru tur, std::uint32_t sira, BaytGorunumu yuk) {
  baslik_ekle(out, tur, sira, yuk.size());
  out.insert(out.end(), yuk.begin(), yuk.end());
}

// ---------------------------------------------------------------------------
// Çözücüler: sim → görselleştirici
// ---------------------------------------------------------------------------

KodHata coz_hosgeldin(BaytGorunumu yuk, HosgeldinIleti& out) {
  if (yuk.size() < sizeof(a2::Hosgeldin)) return KodHata::uzunluk;
  out.h = yapi_oku<a2::Hosgeldin>(yuk.data());
  const auto& h = out.h;
  if (h.govde_sayisi > a2::kAzamiGovde || h.kamera_sayisi > a2::kAzamiKamera) return KodHata::sinir;
  const std::size_t beklenen = sizeof(a2::Hosgeldin) + h.govde_sayisi * sizeof(a2::GovdeBilgisi) +
                               h.kamera_sayisi * (sizeof(a2::KameraYapilandirmasi) + sizeof(a2::HalkaBilgisi));
  if (yuk.size() != beklenen) return KodHata::uzunluk;
  if (!metin_gecerli(h.manifest_yolu, sizeof h.manifest_yolu)) return KodHata::metin;
  if (h.oturum_kimligi == 0) return KodHata::alan;
  if (h.surum_ana != a2::kSurumAna) return KodHata::alan;
  if (h.sim_kipi < 1 || h.sim_kipi > 4) return KodHata::alan;
  if (h.kademe_3b < 1 || h.kademe_3b > 2) return KodHata::alan;  // kapalıda A2 çalışmaz (A2-K02)
  if (h.kademe_kamera > 2) return KodHata::alan;
  if ((h.kamera_sayisi == 0) != (h.kademe_kamera == 0)) return KodHata::alan;
  if (h.adim_ns != 1'250'000) return KodHata::alan;
  if (!zaman_adimla_tutarli(h.t_ns, h.adim_no, h.adim_ns)) return KodHata::alan;
  if (h.durum_hiz_pay != a2::kDurumHizPay || h.durum_hiz_payda != a2::kDurumHizPayda) return KodHata::alan;
  if (h.calisma_nesli < 1) return KodHata::alan;
  if (h.manifest_yolu[0] == '\0') return KodHata::alan;

  const Bayt* p = yuk.data() + sizeof(a2::Hosgeldin);
  out.govdeler.resize(h.govde_sayisi);
  for (auto& g : out.govdeler) {
    g = yapi_oku<a2::GovdeBilgisi>(p);
    p += sizeof(a2::GovdeBilgisi);
  }
  out.kameralar.resize(h.kamera_sayisi);
  for (auto& k : out.kameralar) {
    k = yapi_oku<a2::KameraYapilandirmasi>(p);
    p += sizeof(a2::KameraYapilandirmasi);
  }
  out.halkalar.resize(h.kamera_sayisi);
  for (auto& r : out.halkalar) {
    r = yapi_oku<a2::HalkaBilgisi>(p);
    p += sizeof(a2::HalkaBilgisi);
  }
  KodHata e = govde_listesi_denetle(out.govdeler);
  if (e != KodHata::yok) return e;
  for (const auto& k : out.kameralar) {
    e = kamera_denetle(k, h.kademe_kamera, out.govdeler);
    if (e != KodHata::yok) return e;
  }
  for (const auto& r : out.halkalar) {
    if (!metin_gecerli(r.ad, sizeof r.ad)) return KodHata::metin;
    if (r.yuva_sayisi < a2::kYuvaAsgari || r.yuva_sayisi > a2::kYuvaAzami) return KodHata::alan;
  }
  return KodHata::yok;
}

KodHata coz_dalga(BaytGorunumu yuk, DalgaIleti& out) {
  if (yuk.size() < sizeof(a2::DalgaKumesi)) return KodHata::uzunluk;
  out.d = yapi_oku<a2::DalgaKumesi>(yuk.data());
  const auto& d = out.d;
  if (d.bilesen_sayisi > a2::kAzamiDalgaBileseni) return KodHata::sinir;
  if (yuk.size() != sizeof(a2::DalgaKumesi) + d.bilesen_sayisi * sizeof(a2::DalgaBileseni)) return KodHata::uzunluk;
  if (d.surum < 1) return KodHata::alan;
  if (d.derin_su > 1) return KodHata::alan;
  if (!sonlu(d.g) || !(d.g > 0.0) || !sonlu(d.rho) || !(d.rho > 0.0)) return KodHata::alan;
  // §3.4: derin_su = 1 ise derinlik_m yok sayılır; yalnız 0 iken denetlenir (Okuma).
  if (d.derin_su == 0 && (!sonlu(d.derinlik_m) || !(d.derinlik_m > 0.0))) return KodHata::alan;
  const Bayt* p = yuk.data() + sizeof(a2::DalgaKumesi);
  const std::size_t dizi_bayt = d.bilesen_sayisi * sizeof(a2::DalgaBileseni);
  const Ozet o = sha256(p, dizi_bayt);
  if (std::memcmp(o.data(), d.ozet_sha256, 32) != 0) return KodHata::ozet;
  out.bilesenler.resize(d.bilesen_sayisi);
  for (auto& b : out.bilesenler) {
    b = yapi_oku<a2::DalgaBileseni>(p);
    p += sizeof(a2::DalgaBileseni);
    if (!sonlu(b.a) || b.a < 0.0) return KodHata::alan;
    if (!sonlu(b.omega) || !(b.omega > 0.0)) return KodHata::alan;
    if (!sonlu(b.k) || !(b.k > 0.0)) return KodHata::alan;
    if (!sonlu(b.beta) || !sonlu(b.phi)) return KodHata::alan;
  }
  return KodHata::yok;
}

KodHata coz_ortam(BaytGorunumu yuk, a2::Ortam& out) {
  const KodHata e = sabit_coz(yuk, out);
  if (e != KodHata::yok) return e;
  const auto& o = out;
  if (o.ortam_surumu < 1) return KodHata::alan;
  if (o.ortam_turu < 1 || o.ortam_turu > 2) return KodHata::alan;
  if (!hepsi_sonlu(o.statik_p_n) || !birim_kuaterniyon(o.statik_q_nb, a2::kKuaterniyonNormToleransi))
    return KodHata::alan;
  if (!birim_vektor(o.gunes_yonu_n, 1e-6)) return KodHata::alan;
  if (!sonlu(o.gunes_isinimi_w_m2) || o.gunes_isinimi_w_m2 < 0.0) return KodHata::alan;
  for (int i = 0; i < 3; ++i) {
    if (!sonlu(o.ortam_isigi_rgb_dogrusal[i]) || o.ortam_isigi_rgb_dogrusal[i] < 0.0) return KodHata::alan;
    if (!sonlu(o.beta_d_m[i]) || o.beta_d_m[i] < 0.0) return KodHata::alan;
    if (!sonlu(o.beta_b_m[i]) || o.beta_b_m[i] < 0.0) return KodHata::alan;
    if (!sonlu(o.b_sonsuz_rgb_dogrusal[i]) || o.b_sonsuz_rgb_dogrusal[i] < 0.0 || o.b_sonsuz_rgb_dogrusal[i] > 1.0)
      return KodHata::alan;
  }
  if (!sonlu(o.bulaniklik_olcegi) || o.bulaniklik_olcegi < 0.0) return KodHata::alan;
  return KodHata::yok;
}

KodHata coz_malzeme(BaytGorunumu yuk, MalzemeIleti& out) {
  if (yuk.size() < sizeof(a2::MalzemeOptik)) return KodHata::uzunluk;
  out.m = yapi_oku<a2::MalzemeOptik>(yuk.data());
  if (out.m.girdi_sayisi > a2::kAzamiMalzemeOptik) return KodHata::sinir;
  if (yuk.size() != sizeof(a2::MalzemeOptik) + out.m.girdi_sayisi * sizeof(a2::MalzemeOptikGirdisi))
    return KodHata::uzunluk;
  const Bayt* p = yuk.data() + sizeof(a2::MalzemeOptik);
  out.girdiler.resize(out.m.girdi_sayisi);
  for (auto& g : out.girdiler) {
    g = yapi_oku<a2::MalzemeOptikGirdisi>(p);
    p += sizeof(a2::MalzemeOptikGirdisi);
    if ((g.maske & ~0x7u) != 0) return KodHata::alan;
    if (g.maske & 0x1u) {
      for (double c : g.taban_rengi_dogrusal) {
        if (!sonlu(c) || c < 0.0 || c > 1.0) return KodHata::alan;
      }
    }
    if ((g.maske & 0x2u) && (!sonlu(g.metaliklik) || g.metaliklik < 0.0 || g.metaliklik > 1.0)) return KodHata::alan;
    if ((g.maske & 0x4u) && (!sonlu(g.puruzluluk) || g.puruzluluk < 0.0 || g.puruzluluk > 1.0)) return KodHata::alan;
  }
  return KodHata::yok;
}

KodHata coz_durum(BaytGorunumu yuk, std::int64_t adim_ns, DurumIleti& out) {
  if (yuk.size() < sizeof(a2::Durum)) return KodHata::uzunluk;
  out.d = yapi_oku<a2::Durum>(yuk.data());
  const auto& d = out.d;
  if (d.govde_sayisi > a2::kAzamiGovde) return KodHata::sinir;
  if (yuk.size() != sizeof(a2::Durum) + d.govde_sayisi * sizeof(a2::GovdeDurumu)) return KodHata::uzunluk;
  if (!zaman_adimla_tutarli(d.t_ns, d.adim_no, adim_ns)) return KodHata::alan;
  if (d.calisma_durumu < 1 || d.calisma_durumu > 3) return KodHata::alan;
  if ((d.bayraklar & ~a2::kDurumTakvimDisi) != 0) return KodHata::alan;
  const Bayt* p = yuk.data() + sizeof(a2::Durum);
  out.govdeler.resize(d.govde_sayisi);
  for (auto& g : out.govdeler) {
    g = yapi_oku<a2::GovdeDurumu>(p);
    p += sizeof(a2::GovdeDurumu);
    if (g.govde_turu < 1 || g.govde_turu > 3) return KodHata::alan;
  }
  for (std::size_t i = 0; i < out.govdeler.size(); ++i) {
    for (std::size_t j = 0; j < i; ++j) {
      if (out.govdeler[i].govde_kimligi == out.govdeler[j].govde_kimligi) return KodHata::alan;
    }
  }
  return KodHata::yok;
}

KodHata coz_sahne_olayi(BaytGorunumu yuk, std::int64_t adim_ns, a2::SahneOlayi& out) {
  const KodHata e = sabit_coz(yuk, out);
  if (e != KodHata::yok) return e;
  if (out.tur < 1 || out.tur > 8) return KodHata::alan;
  if (!zaman_adimla_tutarli(out.t_ns, out.adim_no, adim_ns)) return KodHata::alan;
  if (out.calisma_nesli < 1) return KodHata::alan;
  switch (static_cast<a2::SahneOlayiTuru>(out.tur)) {
    case a2::SahneOlayiTuru::govde_dogdu:
      if (out.govde_turu < 1 || out.govde_turu > 3) return KodHata::alan;
      if (out.kaynak_tup > 2) return KodHata::alan;
      break;
    case a2::SahneOlayiTuru::govde_oldu:
      if (out.neden < 1 || out.neden > 4) return KodHata::alan;
      break;
    case a2::SahneOlayiTuru::isabet:
      if (!hepsi_sonlu(out.nokta_n) || !hepsi_sonlu(out.normal_n) || !hepsi_sonlu(out.bagil_hiz_n))
        return KodHata::alan;
      break;
    default:
      break;
  }
  return KodHata::yok;
}

KodHata coz_kamera_istegi(BaytGorunumu yuk, KameraIstegiIleti& out) {
  if (yuk.size() < sizeof(a2::KameraIstegi)) return KodHata::uzunluk;
  out.k = yapi_oku<a2::KameraIstegi>(yuk.data());
  if (out.k.govde_sayisi > a2::kAzamiGovde) return KodHata::sinir;
  if (yuk.size() != sizeof(a2::KameraIstegi) + out.k.govde_sayisi * sizeof(a2::GovdeDurumu)) return KodHata::uzunluk;
  const Bayt* p = yuk.data() + sizeof(a2::KameraIstegi);
  out.govdeler.resize(out.k.govde_sayisi);
  for (auto& g : out.govdeler) {
    g = yapi_oku<a2::GovdeDurumu>(p);
    p += sizeof(a2::GovdeDurumu);
  }
  return KodHata::yok;
}

KodHata coz_kamera_iptal(BaytGorunumu yuk, a2::KameraIptal& out) { return sabit_coz(yuk, out); }

KodHata coz_sim_nabzi(BaytGorunumu yuk, a2::SimNabzi& out) {
  const KodHata e = sabit_coz(yuk, out);
  if (e != KodHata::yok) return e;
  if (out.calisma_durumu < 1 || out.calisma_durumu > 3) return KodHata::alan;
  if (out.sim_kipi < 1 || out.sim_kipi > 4) return KodHata::alan;
  return KodHata::yok;
}

KodHata coz_halka_bilgisi(BaytGorunumu yuk, a2::HalkaBilgisi& out) {
  const KodHata e = sabit_coz(yuk, out);
  if (e != KodHata::yok) return e;
  if (!metin_gecerli(out.ad, sizeof out.ad)) return KodHata::metin;
  return KodHata::yok;
}

KodHata coz_govde_tablosu(BaytGorunumu yuk, std::int64_t adim_ns, GovdeTablosuIleti& out) {
  if (yuk.size() < sizeof(a2::GovdeTablosu)) return KodHata::uzunluk;
  out.t = yapi_oku<a2::GovdeTablosu>(yuk.data());
  if (out.t.govde_sayisi > a2::kAzamiGovde) return KodHata::sinir;
  if (yuk.size() != sizeof(a2::GovdeTablosu) + out.t.govde_sayisi * sizeof(a2::GovdeBilgisi)) return KodHata::uzunluk;
  if (!zaman_adimla_tutarli(out.t.t_ns, out.t.adim_no, adim_ns)) return KodHata::alan;
  if (out.t.calisma_nesli < 1) return KodHata::alan;
  const Bayt* p = yuk.data() + sizeof(a2::GovdeTablosu);
  out.govdeler.resize(out.t.govde_sayisi);
  for (auto& g : out.govdeler) {
    g = yapi_oku<a2::GovdeBilgisi>(p);
    p += sizeof(a2::GovdeBilgisi);
  }
  return govde_listesi_denetle(out.govdeler);
}

KodHata coz_ret(BaytGorunumu yuk, a2::Ret& out) {
  const KodHata e = sabit_coz(yuk, out);
  if (e != KodHata::yok) return e;
  if (!metin_gecerli(out.ayrinti, sizeof out.ayrinti)) return KodHata::metin;
  if (out.neden < 1 || out.neden > 6) return KodHata::alan;
  return KodHata::yok;
}

KodHata coz_veda(BaytGorunumu yuk, a2::Veda& out) {
  const KodHata e = sabit_coz(yuk, out);
  if (e != KodHata::yok) return e;
  if (out.neden > 5) return KodHata::alan;
  return KodHata::yok;
}

// ---------------------------------------------------------------------------
// Çözücüler: görselleştirici → sim
// ---------------------------------------------------------------------------

KodHata coz_merhaba(BaytGorunumu yuk, a2::Merhaba& out) {
  const KodHata e = sabit_coz(yuk, out);
  if (e != KodHata::yok) return e;
  if (!metin_gecerli(out.godot_surumu, sizeof out.godot_surumu) ||
      !metin_gecerli(out.uzanti_surumu, sizeof out.uzanti_surumu) || !metin_gecerli(out.gpu_adi, sizeof out.gpu_adi))
    return KodHata::metin;
  // platform ve çizici geçersizse sim ret(protokol) gönderir (§3.3); burada alan hatası.
  if (out.platform < 1 || out.platform > 2) return KodHata::alan;
  if (out.cizici != 1) return KodHata::alan;
  return KodHata::yok;
}

KodHata coz_hazir(BaytGorunumu yuk, a2::Hazir& out) {
  const KodHata e = sabit_coz(yuk, out);
  if (e != KodHata::yok) return e;
  if (out.durum > 7) return KodHata::alan;
  return KodHata::yok;
}

KodHata coz_goruntuleyici_nabzi(BaytGorunumu yuk, a2::GoruntuleyiciNabzi& out) { return sabit_coz(yuk, out); }
KodHata coz_kare_hazir(BaytGorunumu yuk, a2::KareHazir& out) { return sabit_coz(yuk, out); }
KodHata coz_kare_dustu(BaytGorunumu yuk, a2::KareDustu& out) { return sabit_coz(yuk, out); }
KodHata coz_halka_eslendi(BaytGorunumu yuk, a2::HalkaEslendi& out) { return sabit_coz(yuk, out); }

// ---------------------------------------------------------------------------
// Kodlayıcılar
// ---------------------------------------------------------------------------

void kodla_merhaba(Baytlar& out, std::uint32_t sira, const a2::Merhaba& m) {
  sabit_ileti(out, IletiTuru::merhaba, sira, m);
}
void kodla_hazir(Baytlar& out, std::uint32_t sira, const a2::Hazir& h) { sabit_ileti(out, IletiTuru::hazir, sira, h); }
void kodla_goruntuleyici_nabzi(Baytlar& out, std::uint32_t sira, const a2::GoruntuleyiciNabzi& n) {
  sabit_ileti(out, IletiTuru::goruntuleyici_nabzi, sira, n);
}
void kodla_kare_hazir(Baytlar& out, std::uint32_t sira, const a2::KareHazir& k) {
  sabit_ileti(out, IletiTuru::kare_hazir, sira, k);
}
void kodla_kare_dustu(Baytlar& out, std::uint32_t sira, const a2::KareDustu& k) {
  sabit_ileti(out, IletiTuru::kare_dustu, sira, k);
}
void kodla_halka_eslendi(Baytlar& out, std::uint32_t sira, const a2::HalkaEslendi& h) {
  sabit_ileti(out, IletiTuru::halka_eslendi, sira, h);
}
void kodla_veda(Baytlar& out, std::uint32_t sira, a2::VedaNedeni neden) {
  a2::Veda v{};
  v.neden = static_cast<std::uint32_t>(neden);
  sabit_ileti(out, IletiTuru::veda, sira, v);
}

void kodla_hosgeldin(Baytlar& out, std::uint32_t sira, const HosgeldinIleti& h) {
  a2::Hosgeldin sabit = h.h;
  sabit.govde_sayisi = static_cast<std::uint16_t>(h.govdeler.size());
  sabit.kamera_sayisi = static_cast<std::uint8_t>(h.kameralar.size());
  const std::size_t n = sizeof(a2::Hosgeldin) + h.govdeler.size() * sizeof(a2::GovdeBilgisi) +
                        h.kameralar.size() * sizeof(a2::KameraYapilandirmasi) +
                        h.halkalar.size() * sizeof(a2::HalkaBilgisi);
  baslik_ekle(out, IletiTuru::hosgeldin, sira, n);
  yapi_ekle(out, sabit);
  for (const auto& g : h.govdeler) yapi_ekle(out, g);
  for (const auto& k : h.kameralar) yapi_ekle(out, k);
  for (const auto& r : h.halkalar) yapi_ekle(out, r);
}

void kodla_ret(Baytlar& out, std::uint32_t sira, a2::RetNedeni neden, std::string_view ayrinti) {
  a2::Ret r{};
  r.neden = static_cast<std::uint32_t>(neden);
  metin_yaz(r.ayrinti, sizeof r.ayrinti, ayrinti);
  sabit_ileti(out, IletiTuru::ret, sira, r);
}

void dalga_ozeti_hesapla(DalgaIleti& d) {
  Sha256 s;
  for (const auto& b : d.bilesenler) s.ekle(&b, sizeof b);
  const Ozet o = s.bitir();
  std::memcpy(d.d.ozet_sha256, o.data(), 32);
}

void kodla_dalga(Baytlar& out, std::uint32_t sira, const DalgaIleti& d) {
  a2::DalgaKumesi sabit = d.d;
  sabit.bilesen_sayisi = static_cast<std::uint16_t>(d.bilesenler.size());
  baslik_ekle(out, IletiTuru::dalga_kumesi, sira, sizeof sabit + d.bilesenler.size() * sizeof(a2::DalgaBileseni));
  yapi_ekle(out, sabit);
  for (const auto& b : d.bilesenler) yapi_ekle(out, b);
}

void kodla_ortam(Baytlar& out, std::uint32_t sira, const a2::Ortam& o) { sabit_ileti(out, IletiTuru::ortam, sira, o); }

void kodla_malzeme(Baytlar& out, std::uint32_t sira, const MalzemeIleti& m) {
  a2::MalzemeOptik sabit = m.m;
  sabit.girdi_sayisi = static_cast<std::uint16_t>(m.girdiler.size());
  baslik_ekle(out, IletiTuru::malzeme_optik, sira, sizeof sabit + m.girdiler.size() * sizeof(a2::MalzemeOptikGirdisi));
  yapi_ekle(out, sabit);
  for (const auto& g : m.girdiler) yapi_ekle(out, g);
}

void kodla_durum(Baytlar& out, std::uint32_t sira, const DurumIleti& d) {
  a2::Durum sabit = d.d;
  sabit.govde_sayisi = static_cast<std::uint16_t>(d.govdeler.size());
  baslik_ekle(out, IletiTuru::durum, sira, sizeof sabit + d.govdeler.size() * sizeof(a2::GovdeDurumu));
  yapi_ekle(out, sabit);
  for (const auto& g : d.govdeler) yapi_ekle(out, g);
}

void kodla_sahne_olayi(Baytlar& out, std::uint32_t sira, const a2::SahneOlayi& o) {
  sabit_ileti(out, IletiTuru::sahne_olayi, sira, o);
}

void kodla_kamera_istegi(Baytlar& out, std::uint32_t sira, const KameraIstegiIleti& k) {
  a2::KameraIstegi sabit = k.k;
  sabit.govde_sayisi = static_cast<std::uint16_t>(k.govdeler.size());
  baslik_ekle(out, IletiTuru::kamera_istegi, sira, sizeof sabit + k.govdeler.size() * sizeof(a2::GovdeDurumu));
  yapi_ekle(out, sabit);
  for (const auto& g : k.govdeler) yapi_ekle(out, g);
}

void kodla_kamera_iptal(Baytlar& out, std::uint32_t sira, const a2::KameraIptal& k) {
  sabit_ileti(out, IletiTuru::kamera_iptal, sira, k);
}
void kodla_sim_nabzi(Baytlar& out, std::uint32_t sira, const a2::SimNabzi& n) {
  sabit_ileti(out, IletiTuru::sim_nabzi, sira, n);
}
void kodla_halka_yenile(Baytlar& out, std::uint32_t sira, const a2::HalkaBilgisi& h) {
  sabit_ileti(out, IletiTuru::halka_yenile, sira, h);
}

void kodla_govde_tablosu(Baytlar& out, std::uint32_t sira, const GovdeTablosuIleti& t) {
  a2::GovdeTablosu sabit = t.t;
  sabit.govde_sayisi = static_cast<std::uint16_t>(t.govdeler.size());
  baslik_ekle(out, IletiTuru::govde_tablosu, sira, sizeof sabit + t.govdeler.size() * sizeof(a2::GovdeBilgisi));
  yapi_ekle(out, sabit);
  for (const auto& g : t.govdeler) yapi_ekle(out, g);
}

}  // namespace ida::viz
