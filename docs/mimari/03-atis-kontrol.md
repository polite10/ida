# 03 — Atış kontrol

Sürüm: 0.1 (2026-11-13). Durum: taslak; mimar denetimi ve kullanıcı onayı bekliyor.

Bu bölüm [00-iskelet.md](00-iskelet.md) (sürüm 0.4) Bölüm 9, satır 03'ün kapsamını yazar: sonar verisinde hedef algılama, takip, atış çözümü, durum makinesi, emniyet zinciri, algılama kaydı ve torpidonun simülasyondaki modeli için gerekler. İskeletle çeliştiği yerde iskelet geçerlidir. Dayanak [../research/00-duzeltmeler-ve-kararlar.md](../research/00-duzeltmeler-ve-kararlar.md) belgesidir (bundan sonra "00"). Atıf biçimi: `07.n` = 00 Bölüm 4'teki düzeltme satırı; `Kn`, `Ön`, `Mn`, `Çn`, `An` = 00'daki karar, ölçüm, çelişki ve açık soru; `D07 #k`, `D07 Yk`, `D07 Nk`, `D07 E` = [../research/dogrulama/07-codex.md](../research/dogrulama/07-codex.md) maddeleri; `07 §x` = ham rapor bölümü; `02 §x`, `04 §x`, `05 §x`, `06 §x`, `01 §x` = komşu mimari bölümler; `02-sol md.k` = [inceleme/02-sol.md](inceleme/02-sol.md) maddesi.

**Değerlerin niteliği.** Bu bölümdeki hiçbir süre, işlemci yükü, bellek miktarı, isabet ya da algılama başarımı ölçülmemiştir. **(tahmin)** işaretli her sayı başlangıç değeridir ve parametredir; toleranslar simülasyonda senaryolarla belirlenir (Bölüm 7). Torpido ve araç hakkındaki sayılar 04'ün mimari varsayımı olan varsayılanlarından hesaplanmıştır (K10). Yarışma kuralları kullanılmaz (K7); 07 numaralı ham raporun Rust'a ve yarışma şartnamelerine özgü kısımları yok sayıldı (K3, K7).

---

## 1. Amaç ve sorumluluklar

Atış kontrol modülü (FC), ortak çekirdeğin parçası olan ayrı bir C17 modülüdür (iskelet §2). Kartta ve SIL'de aynı kaynakla koşar, donanım ve işletim sistemi görmez, bellek ayırmaz, saat okumaz (02 §1 ilkeleri). Torpido laboratuvarda simüle edilen bir öğedir; gerçek fırlatıcı, gerçek tetik devresi ve gerçek sensör tedariki kapsam dışıdır (K8).

Sorumluluklar:

1. **Algılama kaydı:** kart içi sonar algılamasının ürettiği ve PC'deki kamera algılamasının A1 üzerinden gönderdiği kayıtların ortak biçimi, doğrulanması ve tek giriş noktası (2.2, 3.2).
2. **Sonar verisinde algılama:** Ping360 tarama satırlarından aday çıkarma, eşikleme, kümeleme, onaylama; kontrol adımını geciktirmeyen arka plan işi olarak (2.3).
3. **Takip:** iz durumları, filtre, kapılama, veri ilişkilendirme, iz yaşam döngüsü, seyrüsefer sıfırlamasında davranış (2.4).
4. **Atış çözümü:** hizalanma ölçütleri, menzil kapısı, aracın kendi hareketi ve akıntı düzeltmesi, 04'ün torpido modeliyle tutarlı yörünge öngörüsü, gecikme telafisi, otopilottan hizalanma isteği (2.5).
5. **Durum makinesi ve emniyet zinciri:** kipler, geçişler, zaman aşımları, tek seferlik atış olaylarının üretimi ve tekilleştirilmesi, güvenli durum (2.6, 2.7).
6. **Simülasyondan beklenenler:** hedef ve torpido modeli, isabet değerlendirmesi, sanal emniyet girdileri, tekrarlanabilir atış senaryoları (2.8).

**Yazmadıkları ve beklenen yer:**

| Konu | Bölüm | Bu bölümün ondan beklediği |
|---|---|---|
| Torpido fiziği, fırlatma darbe modeli, isabet sorgusu, hedef kinematiği | 04 | 2.8'deki fırlatıcı ve olay gerekleri |
| Ping360 ve Ping1D modeli, profil zamanlaması, fırlatıcı durum geri okumasının üretimi | 05 | 2.8; profil başlığında gönderim anı |
| Kamera görüntüsü ve kamera ile algılama bileşeninin içi | 07 | 3.2'deki kayıt alanlarının doldurulması |
| A1 mesajlarının taşınması, parçalanması, olay onayı | 06 | 3.4'teki mesaj içerikleri |
| Kestirim, kontrol, kip yönetimi, adım sırası | 02 | 3.1'deki girdiler ve sıra kuralı |
| Arka plan işinin ana döngüde çağrılması, gözetim zamanlayıcısı | 01 | 2.9'daki eşzamanlılık sözleşmesi |
| Senaryo, sanal emniyet girdilerinin paneli, atış değerlendirme raporu | 08 | 2.8 |

**Temel ilkeler.**
1. **Sınır ölçüm düzeyindedir.** Takip ve atış mantığı yalnız algılama kaydını tüketir; algılayıcının kartta, PC'de ya da başka bir yerde koşması atış mantığını değiştirmez (iskelet §3; Ç5; 07 §3.2).
2. **Emniyet çözümden önce gelir.** İzni daraltan her girdi, aynı adımda atış kararından önce uygulanır (iskelet §3.2). Kabul ölçütü sıfır yanlış atıştır (07 §2.7.4).
3. **Tek seferlik olay.** Atış ve emniyet değişiklikleri benzersiz kimlikli olaydır; yeniden gönderim yeni atış üretmez (iskelet §3.2).
4. **İki saat ayrı tutulur.** Verinin tazeliği ölçüm zamanında, koşulların süresi ve zaman aşımları duvar saatinde değerlendirilir (iskelet §4).
5. **Simülasyondan habersizlik.** Yer gerçeği FC'ye hiçbir yoldan girmez; isabet değerlendirmesi sim tarafındadır (02 §3.2 md.5).

---

## 2. İç yapı

### 2.1 Modüller ve veri akışı

| Modül (`core/fc/…`) | Bağlam | Görev |
|---|---|---|
| `fc_det` | adım | Algılama kayıtlarının doğrulanması, sıralama tamponu, sensör → dünya dönüşümü (tek yer) |
| `fc_sonar` | arka plan | Ping360 profili → tepe listesi (eşikleme, açı içi tepe); saf işlev |
| `fc_clust` | adım | Tepeler → kümeler → sonar algılama kayıtları |
| `fc_trk` | adım | İzler, kapılama, ilişkilendirme, yaşam döngüsü |
| `fc_sol` | adım | Atış çözümü, yörünge tablosu, hizalanma isteği |
| `fc_sm` | adım | Durum makinesi, kipler, jeton, ön izin |
| `fc_safe` | adım | Komut doğrulama ve sıralama, çıktı kapısı, olay kutusu, canlılık |

```
 Ping360 profili (02 girdisi) ──kopya──► iş yuvası ──► fc_sonar (arka plan) ──► tepe listesi
                                                                                  │ (K_bg adım sonra)
 A1 IDA_DETECTION (kamera, PC) ─────────────┐                                     ▼
                                            ▼                                 fc_clust
                     fc_det: doğrula ─► sıralama tamponu ◄── sonar algılama kaydı ┘
                                            │ ölçüm zamanı sırasıyla
                     nav geçmişi (02) ─────►│ sensör → gövde → dünya
                                            ▼
                                         fc_trk ─► izler ─► fc_sol ─► çözüm, hizalanma isteği (02'ye)
                                                               │
 FC komutları, izin (02), fırlatıcı geri okuması, olay onayları ─► fc_safe ─► fc_sm ─► olaylar (A1), durum
```

**Adım içindeki sıra** (`ida_fc_step` her kontrol adımında, 400 Hz; iskelet §4):
1. Girdi alınır; 02 bu adımın DISARM, kip ve arıza eylemlerini **önceden** uygulamış ve güncel izni (`ida_fc_permit_t`) kurmuş olmalıdır (iskelet §3.2; 02-sol md.11). 02'nin taslak sırası (fcif adım 5, kip adım 6) buna uymuyor; Bölüm 8.
2. `fc_safe`: FC komutları sıralanır: önce daraltanlar (`FC_DISARM`, `FC_ABORT`), sonra genişletenler (`FC_ARM`, `FC_SELECT`, `FC_FIRE`). Aynı adımda daraltan bir komut ya da izin kaybı varsa o adımın `FC_FIRE` komutu reddedilir.
3. Arka plan sonucu tüketimi (`due_step` gelmiş iş yuvaları) ve `fc_clust`.
4. `fc_det`: sıralama tamponundan zamanı gelen kayıtlar; `fc_trk` güncellemesi.
5. `fc_sol`: zamanı geldiyse çözüm (50 Hz, tahmin; ağır gruplar faz kaydırılır, 02 §2.2).
6. `fc_sm`: durum makinesi her adımda; olay üretimi.
7. `fc_safe`: çıktı kapısı (2.7 md.5), canlılık bayrakları, telemetri.

Durum makinesi her adımda koşar; böylece daraltma ile çözüm arasına en çok bir adım (2,5 ms) girmez. Takip ve çözüm daha seyrek koşar, çünkü hızlı değişen girdiler (açısal hız, izin) atış adımında yeniden denetlenir (2.5).

### 2.2 Algılama kaydı

Sonar algılaması kartta, kamera algılaması PC'de üretilir (iskelet §3). İkisi aynı `ida_detection_t` kaydını üretir; alanlar 3.2'dedir. Kaydın dört kuralı algılamanın yerini atış mantığından ayırır:

1. **Ölçüm sensör çerçevesinde verilir.** Algılayıcı kendi geometrisini (yön, yükseliş, menzil) sensör çerçevesinde verir; gövdeye ve dünyaya dönüşümü yalnız `fc_det` yapar. Böylece PC tarafı seyrüsefer durumunu ya da yer gerçeğini kullanmaz ve aynı ölçüm her üreticide aynı dönüşümden geçer.
2. **Kovaryans yalnız ölçüm belirsizliğini taşır.** Seyrüsefer belirsizliği takipte tek yerde eklenir (2.4); kayıtta ikinci kez sayılmaz. Güven puanı kovaryansın yerine geçmez (07 §3.2).
3. **Zaman yakalama anıdır.** `t_capture` ölçümün geçerli olduğu andır: kamerada pozlama ortası (05 §2.2), sonarda yansıma anı `t_tx + r/c` (iskelet §3.1). İşlem bitişi ya da alım anı kullanılmaz (07 §2.4.3). `t_sigma` zaman belirsizliğidir.
4. **Kalibrasyon sürümü taşınır.** Kayıt, üreticinin kullandığı iç kalibrasyonun ve beklediği montajın sürümünü taşır; kartın `ida_params_t` sensör grubundaki sürümle uyuşmazsa o kaynağın kayıtları reddedilir (D07 "Mimari için net sonuçlar" md.2).

`source` alanı sensörü (Ping360, ön kamera) tanımlar, algılayıcının yerini tanımlamaz; yer yalnız `producer_id` alanında görünür ve yalnız telemetri ve kayıt için kullanılır. Kamera algılaması ileride karta taşınsa kayıt, dönüşüm ve atış mantığı değişmez; yalnız üretici değişir. Kart içi sonar kayıtları A1'e değerlendirme için de gönderilir (06 `IDA_DETECTION` k→s).

### 2.3 Sonar verisinde algılama (kartta)

**Profil girişi.** 02, adım başına en çok bir `ida_ping360_t` verir; işaretçi yalnız çağrı boyunca geçerlidir (02 §3.1). `ida_fc_step` profili FC'nin iş yuvasına kopyalar (en çok 1200 B) ve aynı anda gereken seyrüsefer örneklerini geçmişten alıp iş yuvasına koyar: gönderim anı `t_tx` ve dinleme sonu `t_tx + T_dinleme` (02-sol md.15, 01-sol md.14'ün sahiplik sorusunun yanıtı). Arka plan işi çekirdek durumuna hiç erişmez.

**Hat** (`fc_sonar`, saf işlev: aynı iş yuvası her zaman aynı tepe listesini verir):
1. **Menzil ekseni.** Kutu `i`'nin menzili `r_i = c_p · (i + ½) · sample_period · 25 ns / 2`; `c_p` kartın ses hızı parametresidir, gerçek cihazdaki gibi (05 §2.4.4). Pencereler kutu değil metre cinsinden tanımlanır ve her profilde kutuya çevrilir; böylece sade kademenin 600 örneği ile tam kademenin 1200 örneği aynı parametreyle çalışır (05 §5).
2. **Maske.** Kör bölge `r < 0,75 m` (Ping360 en kısa menzili; D07 #5) ve yakın alan bandı `r < r_yakın` (varsayılan 1,0 m, tahmin; 05 §2.4.3 md.6'daki Hasan 2024 gözlemi) maskelenir. Doymuş kutular (255) işaretlenir.
3. **Eşikleme: CA-CFAR.** Menzil yönünde, test hücresinin iki yanında koruma ve eğitim hücreleri; eşik `T = α · P̂_n`. Koruma genişliği en az bir darbe uzunluğu (`tx_duration · c_p / 2`) olur, çünkü komşu kutular aynı darbeyle ilintilidir. `α = N(Pfa^(−1/N) − 1)` bağıntısı bağımsız, homojen eğitim örnekleri ve tek bakışlı kare-kanun güç istatistiği varsayar; Ping360'ın u8 şiddet örnekleri bu dağılımı taşımaz (bilinmeyen TVG ve kazanç, darbe ilintisi, nicemleme, doyum, duvar yankıları). Bu yüzden `α` bir **parametre** olarak tutulur; ayarlanan Pfa gerçekleşen yanlış alarm oranı sayılmaz ve oran simülasyonda hedefsiz sahnelerde ölçülerek kalibre edilir (07.5; D07 #24, Y6). Duvar kenarında maskelenme için OS-CFAR yedektir (07 §2.2.4). Şiddetin güce çevrimi (`u8` → genlik mi logaritmik mi) bilinmediği için seçim parametredir (varsayılan genliğin karesi; tahmin).
4. **Açı içi tepe.** Ardışık eşik aşımları tek tepeye indirgenir (şiddet ağırlıklı merkez, tepe şiddeti, genişlik).

**Kümeleme ve onaylama** (`fc_clust`, adımda; sonuç tüketilirken). Her tepe kendi ölçüm zamanındaki pozla dünyaya taşınır; mekanik tarama boyunca araç hareket ettiği için tarama tek anlık görüntü sayılmaz (07 §2.2.4 md.4). Kümeleme dünya çerçevesinde ızgara tabanlı bağlı bileşenle yapılır (komşuluk eşiği `max(2 kutu, 1,5 · r · Δθ_adım)`; tahmin). Tarama kümenin açısından ışın genişliği (2°) ve bir adım kadar uzaklaştığında ya da tarama yönü döndüğünde küme kapanır. Kapanan küme en az iki açı adımında görülmüşse (yakın menzilde parametre), maskesiz bölgedeyse ve yalnız doymuş kutulardan oluşmuyorsa algılama kaydı olur: küme merkezi, kümenin referans anındaki seyrüsefer pozuyla sensör çerçevesine geri çevrilir ve `RB2` (menzil, yön) türünde verilir; ileri ve geri dönüşüm aynı pozla yapıldığı için birbirini götürür. Yükseliş ölçülmez (25° dikey ışın; D07 #5); izin dikey bileşeni ışın sınırından gelen önsel bilgiyle başlatılır (2.4). `t_capture` şiddet ağırlıklı yansıma anı ortalaması, `t_sigma` kümenin zaman yayılımının `1/√12` katıdır (düzgün dağılım varsayımı; tahmin).

**Ölçüm modeli ve belirsizlik.** Sensör ölçümünden dünya konumu `p_W = p_B + R_WB (R_BS p_S + t_BS)` ile elde edilir; sensör–gövde dönüşümü (`R_BS`, `t_BS`) ölçüm modelinin parçasıdır, yalnız atış çözümünün değil (07.7; D07 #29, N8). Kovaryans Jacobian'larla taşınır: ölçüm `R` bileşeni, montaj belirsizliği `Σ_e` ve zaman belirsizliğinin katkısı `J_t σ_t² J_tᵀ` (`J_t`: ölçümün zamana göre değişimi; aracın açısal ve öteleme hızından). Seyrüsefer hatası bütün ölçümlerde ortaktır ve aralarında korelasyon yaratır; bu yüzden her ölçüme bağımsız gürültü olarak eklenmez, takipte ayrı ele alınır (2.4).

**Son yaklaşmada kör bölge.** Ping360 0,75 m'nin altında, yakın alan bandında 1 m civarına kadar kullanılamaz (D07 E; 05 §2.4.3). İskeletteki Ping1D dibe bakar (iskelet §5); öne bakan ikinci bir menzil kaynağı yoktur. Tasarımdaki karşılığı:
- İz dünya çerçevesinde tutulduğu için kör bölgede de yaşar; seyrüsefer sürüklenmesiyle büyüyen kovaryansla ileri kestirilir (2.4).
- Kör bölgede sonar fırsatı sayılmaz; iz bu yüzden kaçırma biriktirip silinmez (2.4).
- Atış izninin tazelik ölçütü kaynak başına en kısa menzille denetlenir: kör bölgedeki hedef için taze ölçüm yalnız kameradan gelebilir; kamera yoksa ya da kapalıysa izin, son sonar ölçümünden bu yana göreli kovaryansın eşiği aşmadığı süreyle sınırlıdır (2.5 md.6).
- 04 varsayılanlarıyla etkili atış menzili sonarın kör bölgesine yakındır (2.5); bu çakışma Bölüm 8'de mimara bildirilmiştir.

**Bütçe (hepsi tahmin; M5 ve Bölüm 6'daki yeni ölçüm).** Kayan toplamlı CA-CFAR profil başına O(n): 1200 kutu için birkaç on bin çevrim, tepe ve dönüşümle birlikte profil başına 0,1–0,3 ms. Profil hızının üst sınırı 7 m menzilde yalnız akustik beklemeyle ≈ 107 profil/s'dir (05 §2.4.4; motor ve iletim bunu düşürür); bu sınırda işlemci payı ≈ %1–3. İş, 01'in ana döngüsündeki 300 µs'lik arka plan dilimine sığacak biçimde kutu blokları halinde dilimlenebilir (01 §2.4). Bellek: 4 iş yuvası × (1200 B profil + seyrüsefer örnekleri) ≈ 5 KB, CFAR çalışma alanı ≈ 5 KB (`float`), tepe ve küme tabloları ≈ 3 KB, 16 iz ≈ 4 KB, sıralama tamponu 32 kayıt ≈ 2,5 KB, yörünge tablosu ≈ 1 KB; toplam < 32 KB, SRAM3'te (01 §2.6).

### 2.4 Takip

**Durum ve model.** İz durumu yerel NED'de tutulur (07 §2.3.1). İki model vardır, hedef sınıfına bağlı parametreyle seçilir:
- `P3` (sabit hedef): `p` (3); süreç gürültüsü yalnız seyrüsefer sürüklenmesi ve küçük hedef payı.
- `CV6` (hareketli ya da sınıfı bilinmeyen): `p, v` (6), beyaz ivme gürültülü sabit hız modeli. α-β filtresi kullanılmaz: kazançları yalnız uygun model ve kararlı durumda Kalman'a denktir ve düzensiz ölçüm aralığında kovaryans vermez (07.6; D07 #27).

**Filtre.** Genişletilmiş Kalman filtresi, ölçüm sensör çerçevesinde kutupsal olarak işlenir: `h(x) = kutupsal(R_BSᵀ(R_WBᵀ(p − p_B) − t_BS))`, poz ölçümün `t_capture` anındaki seyrüsefer geçmişinden (02 §3.6). Kutupsal → Kartezyen dönüşüm taraflılığı böylece doğmaz (07 §2.3.1); açı yeniliği sarılır. Bütün sayılar `float`; Joseph formu ve simetrikleştirme (02 §2.3 ile aynı kural). Ping360 kaydında yükseliş olmadığı için ilk `z`, araç derinliği ve montajdan ışın sınırına (±12,5°) yayılmış önsel değerle başlar.

**Seyrüsefer belirsizliği ve korelasyon.** Atış için gereken büyüklük hedefin araca göre konumudur. Ölçüm anlarında ortak olan seyrüsefer hatası (konum ve yön ofseti) iz konumuna da aynen geçer ve göreli konumda büyük ölçüde birbirini götürür; ölçüm anı ile atış anı arasındaki **sürüklenme** götürmez (07.7'nin "dünya çerçevesi belirsizliği kaldırmaz" düzeltmesinin göreli geometrideki karşılığı). Bu yüzden: (a) ortak bileşen her ölçüme bağımsız gürültü olarak eklenmez, çünkü N ölçümde ortalama alınıp yapay olarak küçülür; (b) sürüklenme izin süreç gürültüsüne girer: iki güncelleme arasında 02'nin `p_sigma` ve `att_sigma` artışından türetilen `Q_nav` (yön sürüklenmesi `r · δψ` olarak yanal bileşene); (c) mutlak seyrüsefer σ'sı ayrıca raporlanır ama atış ölçütüne girmez. Bu bir yaklaşımdır; kesin karşılığı seyrüsefer ofsetini durum vektörüne alıp güncellemeyen Schmidt filtresidir (yedek, Bölüm 6). Tutarlılık SIL'de yer gerçeğiyle NEES/NIS testiyle sınanır.

**Kapılama.** Mahalanobis uzaklığı `d² = νᵀ S⁻¹ ν`, `S = H P Hᵀ + R_eff`. Eşik ki-kare %99: 2 serbestlik derecesinde 9,210, 3'te 11,345 (D07 #25). Eşikler kovaryansların tutarlı olduğunu varsayar; NIS ortalaması sağlık metriği olarak telemetride yayımlanır.

**Veri ilişkilendirme.** Ölçümler parti halinde ilişkilendirilir (bir sonar kümesi grubu ya da bir kamera karesinin kayıtları; en çok 8 kayıt, 16 iz; tahmin). Yöntem küresel en iyi atamalı GNN'dir: maliyet `d² + ln|S|`, kapı dışı çiftler yasak, her ölçüm için "yeni iz" kukla sütunu; çözüm Hungarian (Munkres) algoritmasıyla, sabit boyutlu (8 × 24) matrisle. Açgözlü eşleme kullanılmaz, çünkü küresel en küçük maliyeti garanti etmez (`[[1,2],[2,100]]` örneğinde açgözlü 101, en iyi 4; 07.6, D07 #26). Bu boyutta Munkres'in maliyeti küçüktür (birkaç on bin işlem; tahmin). Yoğun yankı ortamında JPDA yedektir (Bölüm 6).

**Sıralama.** Kamera ve sonar kayıtlarının teslim gecikmeleri farklıdır. `fc_det` kayıtları `t_capture` sırasıyla, sabit `D_sıra` gecikmeli bir tamponla izlere verir (varsayılan 60 ms ölçüm zamanında, tahmin; iki kaynağın gecikme farkından SIL'de seçilir). İzin son güncellemesinden eski kalan kayıt reddedilir ve sayılır; geri kestirimli güncelleme yedektir. Gecikme, çözümün ileri kestiriminde telafi edilir (2.5).

**İz yaşam döngüsü.**

| Durum | Giriş | Çıkış |
|---|---|---|
| `TENTATIVE` | İlişkilendirilmemiş `RB2`/`RB3` kaydı (menzilsiz `B2` iz başlatmaz) | M-of-N: son 5 fırsatın 3'ünde güncelleme → `CONFIRMED` (07 §2.3.2; tahmin); 3 fırsatta güncelleme yoksa silinir |
| `CONFIRMED` | M-of-N | K = 4 ardışık kaçırılmış fırsat, kovaryans izi sınırı, ölçüm zamanında 30 s güncellemesiz (tahmin) → silinir |
| `RECONFIRM` | Seyrüsefer sıfırlaması (aşağıda) | Sıfırlamadan sonraki `N_r` = 2 güncelleme (tahmin) → `CONFIRMED` |

**Fırsat** yalnız sensörün hedefi görebileceği anlardır: sonarda tarama izin kestirilen yönünü ışın genişliği içinde geçerken ve menzil maskesiz bölgedeyken; kamerada kestirilen konum görüş alanı ve kamera menzil sınırları içindeyken. Mekanik tarama başka yöne bakarken ya da hedef kör bölgedeyken iz kaçırma biriktirmez.

**Seyrüsefer sıfırlaması.** 02'nin `reset_counter` değeri değişince kestiricinin çerçevesi sıçramış demektir (02 §2.3). Hedefin gerçek yeri değişmediği ama aracın inanılan konumu Δ kadar kaydığı için izler de aynı Δ ile kaydırılır ve kovaryansları sıfırlama belirsizliğiyle büyütülür; Δ bildirilmezse kayma yapılmaz, kovaryans büyütülür. Bütün `CONFIRMED` izler `RECONFIRM` olur ve yeniden onaylanana kadar atış izni verilmez (07 §4 R12). `t_capture` anı sıfırlamadan önce kalan kayıtlar reddedilir; geçmiş sorgusu sıfırlama sınırı üzerinden ara değerleme yapmamalıdır (02-sol md.17; 02'den beklenen).

### 2.5 Atış çözümü

**Tüp çerçevesi.** Tüp `i`'nin çıkış noktası `r_t,i` ve ekseni `ε_i` gövde çerçevesinde parametredir (04 §2.8 varsayılanı: `+x`, `r_t = (0, ±0,10, 0,10)` m). Değerler 02'nin otopilot parametrelerindedir ve fizikteki gerçek değerden bilinçli olarak farklı verilebilir (02 §8.1 md.2).

**Yörünge öngörüsü.** 04'te torpido, kanatlı, sürüklemeli, hafif negatif yüzerlikli ayrı bir 6-SD gövdedir ve darbe modeliyle ayrılır (04 §2.8). FC bunun basit karşılığını kullanır:
1. **Tablo.** SIL'de başsız sim sunucusuyla, 04'ün torpido modeli üzerinden, durgun araç ve akıntısız su için üretilir: eksen boyunca uzaklık `s` → uçuş süresi `t(s)`, eksenel hız, düşey sapma, yanal hız sönüm katsayısı, saçılım (Monte Carlo σ). Tablo, üretildiği A5 torpido ve fırlatıcı grubunun özetini taşır; kart bunu kendi parametre özetiyle karşılaştırır, uyuşmazlıkta çözüm üretmez.
2. **Bileşim.** Suya göre başlangıç hızı `v_B + ω × r_t − v_akıntı + u₀ ε` (araç hızı, tüpün dönme kaynaklı hızı, ayrılma hızı); su çerçevesinde tablo yörüngesi, yere göre `+ v_akıntı · t` (07 §2.4.2). Yanal başlangıç hızı tablodaki sönümle azaltılır; torpido akıntı hızına anında ulaşmaz (D07 #32'nin düzeltmesi: anında ulaşma varsayımı sapmayı abartır).
3. **İsabet anı.** Kestirilen yörüngenin hedefe en yakın geçtiği an `t_hit`; hareketli hedef `CV6` ile `t_f + t_hit` anına ileri kestirilir. Sonuç: hedef düzleminde yanal ve düşey ıska `e = (e_y, e_z)` ve kovaryansı `Σ_e = J Σ_göreli Jᵀ + Σ_tablo + Σ_gecikme + Σ_akıntı`.

Bileşim, doğrusal olmayan sürüklemede yaklaşıktır; hatası SIL Monte Carlo'sunda 04'ün gerçek yörüngesiyle karşılaştırılır ve `Σ_tablo`'ya eklenir. Yedek, kartta doğrudan sayısal entegrasyondur (Bölüm 6).

**04 varsayılanlarıyla ölçek (hesap, tahmin; tolerans değil).** Eksenel sürüklemede `u(s) = u₀ e^(−s/L)`, `L = m_e / (½ ρ C_D A)`; 04 §2.8'deki değerlerle (`m_e` = 0,1849 kg, `C_D` = 0,42, `A` = 1,257·10⁻³ m², `u₀` ≈ 2 m/s) `L` ≈ 0,70 m; 1 m'de `t` ≈ 1,1 s ve `u` ≈ 0,48 m/s, 1,5 m'de `t` ≈ 2,6 s. Net batma kuvveti (−0,073 N) için sürüklemesiz üst sınır 1 m'de ≈ 0,14 m düşüştür. Sonuç: bu varsayılanlarla atış kısa menzilli olur ve düşey sapma çözümde ihmal edilemez. `C_D` 04'te yer tutucudur (04.8) ve bu sayılar yalnız ölçeği gösterir. Ham rapordaki düşüş ve akıntı sapma rakamları da 07 doğrulamasına göre **tasarım toleransı olarak kullanılamaz** (07.8; D07 N5); burada tolerans olarak kullanılmaz.

**Hizalanma ölçütleri.** Bütün ölçütler aynı anda ve `T_dwell` süresince (duvar saati) sağlanırsa çözüm geçerlidir (07 §2.4.1). Her ölçütün benzersiz kimliği vardır ve durumu telemetride yayımlanır (NASA, 07 §2.6.1).

| # | Ölçüt | Dayanak |
|---|---|---|
| 1 | `|e_y| + k σ_y ≤ ρ_hedef` ve `|e_z| + k σ_z ≤ ρ_hedef`; `ρ_hedef` hedef sınıfının isabet bölgesinden (A5), `k` parametre | 07 §2.4.1 md.1 |
| 2 | Menzil kapısı `R_min ≤ R ≤ R_max`. `R_min`: torpidonun ayrılıp kararlı hale geldiği mesafe ve aracın hedefe çarpmaması. `R_max`: SIL Monte Carlo'sunda isabet olasılığının `P_gerekli`'nin altına düştüğü menzil; tablo olarak yüklenir | 07 §2.4.1 md.2, §2.7.3 |
| 3 | Açısal hızlar `|ω_i| ≤ ω_max`; tüpün dönmesi torpidoya `ω × r_t` yanal hız verir | 07 §2.4.1 md.3 |
| 4 | Yalpa ve baş-kıç sınır içinde | 07 §2.4.1 md.4 |
| 5 | Göreli öteleme hızı `≤ v_max` | 07 §2.4.1 md.5 |
| 6 | İz `CONFIRMED` (`RECONFIRM` değil); kaynak başına tazelik: hedefin menzilini görebilen bir kaynaktan son kaydın yaşı `≤ T_taze` (ölçüm zamanında) ya da kör bölgede son kayıttan bu yana göreli kovaryans eşik altında; zorunlu kaynak kümesi parametre (varsayılan boş) | 07 §2.4.1 md.6; 2.3 |
| 7 | Seyrüsefer `EST_OK`; rejim dalış; tüp derinliği `≥ d_min` (tüp ve yörünge su altında; dalga payıyla); `failsafe_clear` | 07 §2.6.4 |
| 8 | `t_hit ≤ T_uçuş_max`; tablo özeti uyumlu | 2.5 |

**Toleranslar simülasyonda belirlenir.** `k`, `R_max`, `ω_max`, `v_max`, `T_dwell`, `T_taze` için bu bölüm sayı vermez. Değerler, ayar tohumlarıyla koşulan senaryo ailelerinden (menzil × yanal hata × akıntı × gecikme) isabet olasılığı haritası çıkarılarak seçilir (07 §2.7.3) ve kabul senaryolarından önce dondurulur (Bölüm 7).

**Gecikme.** Zincir: yakalama → teslim (kamerada `L_kam`, sonarda dinleme, işlem ve iletim; 05 §2.2) → 06'nın toplu kısmı (1 adım) → arka plan (`K_bg` adım) → sıralama tamponu (`D_sıra`) → çözüm periyodu → durum makinesi → operatör kipinde onay → olayın sim'e ulaşması ve uygulanması (adım + `L_act`; 06 §2.4) → darbe. Telafi: (1) iz atış anına `t_f = t_şimdi + L_çıkış` ileri kestirilir; `L_çıkış` kayda yazılan sabit parametredir; (2) araç pozu `t_f`'ye güncel `v` ve `ω` ile ileri kestirilir; (3) kalan belirsizlik `Σ_gecikme ≈ J (Σ_v τ² + (ω τ R)²) Jᵀ` olarak ıska kovaryansına girer (07 §2.4.3'teki `Δv·τ`, `ω·τ·R` ilişkileri). Açısal hız ölçütü (md.3) atış adımında güncel `ω` ile yeniden denetlenir. Gecikmenin gerçek dağılımı M1, M12, M17 ölçümlerine bağlıdır.

**Hizalanma isteği.** Çözüm, otopilottan tüp ekseninin nişan noktasından geçtiği bir duruş ister (Boulder'ın tüp ekseni izdüşümü yöntemi; 07 §2.1.3, D07 #18). Nişan noktası `a` = hedefin kestirilen konumu − kestirilen düşey sapma − akıntı sürüklenmesi. FC, 02'nin `ida_align_req_t` alanlarını doldurur: `yaw_ref` (tüpün yanal ofseti `y_t` için `atan2` yönüne `−asin(y_t / d_yatay)` düzeltmesi), `depth_ref` (`a_z − z_t`, yalpa ve baş-kıç ≈ 0 varsayımıyla), `hold_ne` (nişan doğrusu üzerinde istenen menzil `R_istenen` kadar gerideki nokta), `max_speed`, `tol_yaw`, `tol_pos`, `dwell`. İstek çözüm hızında yenilenir; referans değişimi histerezisle sınırlanır. `valid = false` hizalanmayı bırakır. Kip yöneticisi isteği yalnız izin koşullarında uygular, arıza eylemi her zaman önceliklidir (02 §3.6).

### 2.6 Durum makinesi

Durum değişkeni ve kritik bayraklar (kip, silah, kalan atış hakkı) özel bit desenleriyle ve tamamlayıcı kopyayla tutulur; "hep 0" ya da "hep 1" geçerli değer değildir, geçersiz desen `FAULT_LATCHED`'e götürür (07 §2.5'teki NASA deseni). Her durum × olay çifti tanımlıdır ve tablo olarak kodlanır; tablo birim testiyle eksiksiz taranır (02 §2.6'daki kuralla aynı). Kart her açılışta `SAFE` başlar; silah durumu kalıcı belleğe yazılmaz (01 §2.7).

**Kipler.** `OPERATÖR` ve `OTONOM`; aralarındaki tek fark atış izninin kaynağıdır, bütün diğer engeller iki kipte aynıdır (07 §2.5).
- **Operatör onaylı kip:** `SOLUTION_VALID`'e girerken FC bir **jeton** üretir (`u32`; açılış tuzu ve çözüm sırasından türetilir; SIL'de tuz senaryo tohumundan gelir) ve `IDA_FC_STATUS` ile yayımlar. Jeton tek kullanımlıdır ve (iz, tüp, çözüm sırası) üçlüsüne bağlıdır. `FC_FIRE` yalnız geçerli jetonla kabul edilir; jeton kullanılınca, `SOLUTION_VALID`'den çıkılınca, iz ya da tüp değişince ve `T_jeton` dolunca geçersizdir. Onay tetik anını belirlemez; `T_pencere` süreli bir pencere açar ve tetik, pencere içinde bütün ölçütlerin yeniden sağlandığı ilk adımda verilir (07 §2.4.3 md.5). `T_pencere = 0` yalnız onay adımında tetik demektir.
- **Otonom kip:** izin, görevle birlikte atomik yüklenen **ön izinden** gelir (07 §2.5): görev adımı kimliği, hedef sınıfı, menzil kapısı (genel kapının alt kümesi), en çok atış sayısı, tüp maskesi, görev adımı başlangıcından itibaren zaman penceresi (duvar saati). Her atış sayacı azaltır; görev adımı değişince, pencere dolunca ya da görev durdurulunca ön izin geçersizdir. Otonom kip yalnız `FC_ARM` komutunda açıkça seçilir.

**Durum tablosu** (zaman aşımları duvar saatinde, değerler tahmin; tazelik ölçütleri ölçüm zamanında):

| Durum | Giriş koşulu | İzinli çıkışlar | Yasak çıkışlar | Zaman aşımı | Normal çıkış |
|---|---|---|---|---|---|
| `SAFE` | Açılış, sıfırlama, her evrensel geçiş | `emniyet_kapat`; durum | `emniyet_aç`, `ateşle`, hizalanma isteği, jeton | — | Geçerli `FC_ARM` (özel 32 bit desen, kip) ve izin → `ARMING` |
| `ARMING` | `FC_ARM` | `emniyet_kapat`; durum | `emniyet_aç`, `ateşle`, jeton | 2 s → `SAFE` | Ön denetim (izin, sanal fiş geri okuması, tüp dolu, rejim, derinlik, `EST_OK`, tablo özeti, otonomda ön izin) → `ARMED` |
| `ARMED` | Ön denetim geçti | `emniyet_kapat`; durum | `emniyet_aç`, `ateşle`, jeton | Otonomda ön izin penceresi → `SAFE` | Seçili onaylı iz (operatör `FC_SELECT` ya da ön izin sınıfıyla seçim) → `TRACKING` |
| `TRACKING` | Seçili iz `CONFIRMED` | Hizalanma isteği; durum | `emniyet_aç`, `ateşle`, jeton | Hizalanma süresi 60 s → `ARMED` | Ölçütler `T_dwell` boyunca → `SOLUTION_VALID` |
| `SOLUTION_VALID` | Ölçütler sağlandı | Hizalanma isteği; jeton (operatör kipi) | `emniyet_aç`, `ateşle` | `T_jeton` 10 s → `TRACKING` | Geçerli `FC_FIRE` ya da ön izin → `FIRE_AUTHORIZED` |
| `FIRE_AUTHORIZED` | İzin alındı | Hizalanma; `emniyet_aç` (süreli, yenilenir); ölçütler ve emniyet açık geri okuması sağlanırsa tek `ateşle` | İkinci `ateşle`, başka tüpe olay | `T_pencere` 300 ms → `TRACKING` (jeton harcanmış, `emniyet_kapat`) | `ateşle` gönderildi → `FIRING` |
| `FIRING` | `ateşle` olayı kutuda | Aynı olayın **aynı kimlikle** yeniden gönderimi; hizalanma | Yeni kimlikli `ateşle`, `emniyet_aç` yenilemesi | Tüp boşaldı geri okuması 500 ms içinde yoksa → `FAULT_LATCHED` | Tüp boşaldı → `ASSESS` |
| `ASSESS` | Fırlatma görüldü | `emniyet_kapat`; istasyon tutma isteği; atış kaydı | `emniyet_aç`, `ateşle` | 1 s → aşağıdaki çıkış | Torpido ve hak kaldıysa ve silahlıysa `ARMED`, yoksa `SAFE` |
| `FAULT_LATCHED` | Bölüm 4'teki kilitli arızalar | `emniyet_kapat`; durum ve neden | Diğer bütün olaylar, hizalanma, jeton | — | Yalnız `FC_RESET` (özel desen) + koşullar temiz → `SAFE` |
| `TEST` | Yalnız `SAFE`'ten; sanal bakım girdisi etkin ve iki tüp boş geri okuması | Kuru zincir sınaması: `emniyet_aç`, `ateşle` (sim boş tüpte olayı yok sayar ve sayar, 04 §4) | Tüp dolu görünürse her olay | Bakım girdisi kalkınca → `SAFE`; çıkışta engellerin yeniden etkin olduğu doğrulanır (07 §2.5) | `SAFE` |

**Evrensel geçişler.** Aşağıdakiler her durumdan, aynı adımda uygulanır:

| Koşul | Hedef durum |
|---|---|
| `FC_DISARM`; 02'nin `armed = false`; `failsafe_clear = false` (02'de `HOLD` ve üstü); `EST_BAD`; rejim dalış değil ya da tüp derinliği `< d_min`; sanal fiş ya da acil durdurma geri okuması "kesik"; operatör kipinde yer istasyonu kalp atışı `T_hb_fc` = 1 s (tahmin) gelmedi; `ida_core_tick_wall` çağrısı (IMU kesildi) | `SAFE` (`emniyet_kapat`, jeton ve ön izin iptal) |
| `FC_ABORT`; iz kaybı ya da `RECONFIRM`; `mode_ok = false`; hizalanma isteğinin reddi | `ARMED` (yalnız `TRACKING` … `FIRE_AUTHORIZED`'dan; jeton iptal, `emniyet_kapat`) |
| Geçersiz durum deseni; olay onayı alınamadı; tekleme; beklenmeyen tüp boşalması; geri okuma çelişkisi | `FAULT_LATCHED` |

Otonom kipte yer istasyonu bağlantısının kaybı tek başına `SAFE` sebebi değildir (su altında beklenen durum, 02 §4); ön izin penceresi sınırlı olduğu için atış yetkisi süreyle kendiliğinden biter (07 §2.6.4).

### 2.7 Emniyet zinciri

Zincir üç katmandır. Hangi engelin kimden bağımsız olduğu açıkça yazılır, çünkü iki adımlı komut ya da aynı yazılımdaki ikinci denetim bir engel sayılmaz (D07 #6; 07 §2.6.1).

**1. Sim sunucusundaki sanal fiziksel engeller** (kartın dışında; kart yazılımından bağımsız):
- **Sanal acil durdurma:** etkinken sim iticileri ve fırlatıcı enerjisini keser; kartın gönderdiği hiçbir olay fırlatma üretmez.
- **Sanal silahlandırma fişi** (fırlatıcı başına): takılı değilse fırlatıcı enerjisizdir.
- **Sanal mekanik emniyet pimi** (tüp başına): takılıyken mekanizma kilitlidir.

Bu girdiler 08'in panelinden ya da senaryonun zaman çizelgesinden (A5) değişir ve A6'ya yazılır. Kart bunları yalnız geri okuma olarak görür; geri okuma hata enjeksiyonuyla bozulabilir, böylece kartın emniyet için ona güvenmediği sınanır. Bağımsızlıkları yalnız simülasyonda geçerlidir: gerçek bir araçta bağımsızlık donanım tasarımına bağlıdır ve bu aşamanın konusu değildir (K8). Bakımda çıkarılan fiş ya da pimin çalışma sırasında bağımsız engel sağladığı da söylenemez; bağımsızlık her işletim durumunda ayrı incelenir (07.11).

**2. Fırlatıcı modelinde süreli izin** (kartın yazılım engelinin sim tarafındaki karşılığı). `emniyet_aç` olayı bir geçerlilik süresi taşır (`T_izin`, varsayılan 200 ms sim zamanında, tahmin); süre dolunca sim fırlatıcıyı kendiliğinden kilitler. Kart izni `FIRE_AUTHORIZED` süresince, sağlık değerlendirmesinden sonra ve yeni olay kimliğiyle yeniler. Serbest çalışan bir zamanlayıcının, PWM'in ya da DMA'nın ürettiği sinyal yazılım sağlığının kanıtı değildir (07.11; D07 #34): CPU ilerlemese de sürebilir. Bu yüzden izin, ilerleyen yazılımın yeniden kurması gereken süreli bir izindir ve kart kilitlenince en geç `T_izin` sonunda düşer.

**3. Kart yazılımındaki engeller:** durum makinesi; ayrı silahlandırma ve atış komutları (`FC_ARM` özel 32 bit desenli, `FC_FIRE` jetonlu; 07 §3.3); çıktıda ayrı `emniyet_aç` ve `ateşle` olayları; çekirdeğin izni (`ida_fc_permit_t`); 01'in platform kapısı (01 §2.7). Bunlar **birbirinden bağımsız sayılmaz**: aynı işlemcide, aynı bellekte, aynı saatle, aynı derleme ve aynı girdilerle (örneğin aynı hatalı seyrüsefer verisiyle) çalışırlar; ortak nedenli tek bir arıza (bellek bozulması, derleyici hatası, ortak girdi hatası) hepsini birden kaldırabilir. NASA tanımında asıl ölçüt budur (D07 #6). Değerleri mantık hatalarına karşı derinlemesine savunmadır; arıza toleransı iddiası 1. katmana dayanır.

**Kurallar:**
1. **Ayrı silahlandırma ve atış.** Silahlandırma (`FC_ARM` → `emniyet_aç`) ve atış (`FC_FIRE` → `ateşle`) ayrı komutlar ve ayrı olaylardır. Sim, `ateşle`'yi yalnız süreli izin geçerliyken uygular.
2. **Daraltan önce.** 02, aynı adımda DISARM, kip ve arıza eylemlerini FC'den önce uygular; FC kendi komutlarında daraltanları önce işler (2.1). Sim tarafında da emniyeti artıran olaylar aynı adımda ateşlemeden önce uygulanır (04 §3 `OneShotEvent`).
3. **Tekilleştirme.** Her olay `(boot_id, olay_id)` ile tekildir; `olay_id` açılıştan beri artan `u32`'dir. Olay, karar adımında bir kez üretilir ve onaylanana ya da süresi dolana kadar FC'nin olay kutusunda **aynı kimlikle** tutulur; yeniden gönderim (aktarım yinelemesi, 02'nin `DUP_SEQ` yolunda önceki çıktının tekrarı) aynı kimliği taşır ve sim onu yalnız bir kez uygular (iskelet §3.2; 04 §3). Operatörün `FC_FIRE` komutu da `cmd_id` ile tekildir; yinelenen komut ilk sonucun onayını yeniden alır, yeni etki üretmez. Jeton zaten tek kullanımlıktır.
4. **Son geçerlilik.** `ateşle` olayı bir son geçerlilik anı taşır (karar adımının ölçüm zamanı + `T_geç`, varsayılan 20 ms, tahmin); sim bu andan sonra ulaşan olayı uygulamaz ve sayar. Bağlantı takılmasından sonra gecikmiş bir atış gerçekleşmez.
5. **Çıktı kapısı.** Adım sonunda `fc_safe`, üretilen her olayı o adımın güncel durumu ve izniyle yeniden denetler; tutarsızlık olayı siler ve `FAULT_LATCHED`'e götürür. 02, son çıktının güncel arm, kip ve arıza izniyle tutarlı olduğunu ayrıca garanti eder (02-sol md.11, md.12; 02'den beklenen). 01'in platform kapısı üçüncü denetimdir.
6. **Görünürlük.** Her engelin benzersiz kimliği ve durumu `IDA_FC_STATUS`'ta bit maskesi olarak yayımlanır; ön koşulu tutmayan komut nedeniyle reddedilir (07 §2.6.1).

**Gözetim.** Gözetim zamanlayıcısının yalnız döngüyü değil kritik işlerin canlılığını izlemesi için FC, 02'nin `alive_ok` bayrağına üç koşul verir (02 §4; 01 §2.7): (a) `ida_fc_step` bu adımda tamamlandı; (b) iç tutarlılık: durum ve kritik bayrakların tamamlayıcı kopyaları uyumlu, iş yuvası sahiplik alanları geçerli; (c) arka plan ilerlemesi: bekleyen iş varken son `T_bg` = 100 ms (duvar saati, tahmin) içinde en az biri tamamlandı. (a) ya da (b) bozulursa `alive_ok = false` olur ve IWDG kartı sıfırlar. (c) tek başına yük kaynaklı olabileceği için sıfırlatmaz; FC'yi `SAFE`'e alır ve sayar (01 §2.7'deki dış neden ile iç arıza ayrımı). IWDG'nin gerçek aralığı nominal 32 kHz LSI'de ≈ 125 µs – 131 s'dir (12 bit yeniden yükleme, en çok 1024 bölücü); ham rapordaki 16 ms – 4096 s yanlıştır (07.10). Çalışma süresi 01'dedir (100 ms, tahmin; M19).

**Reset, kilitlenme, bağlantı kopması.**

| Olay | Kart | Sim (04, 06'dan beklenen) |
|---|---|---|
| Kart sıfırlandı (IWDG, hata, yazılım) | `SAFE` açılır; silah, jeton, ön izin, olay sayacı sıfır; yeni silahlandırma ve görev yüklemesi gerekir | Yeni `boot_id` görünce bütün fırlatıcıları kilitler; eski `boot_id`'li olayları atar |
| Ana döngü kilitlendi | Olay ve yenileme üretilmez; IWDG sıfırlar | Süreli izin en geç `T_izin` sonunda düşer |
| A1 koptu | IMU kesilir; `ida_core_tick_wall` → FC `SAFE` | `KOPUK` anında fırlatıcıları kilitler, o oturumun bekleyen olaylarını atar |
| Yer istasyonu bağlantısı koptu | Operatör kipinde `T_hb_fc` sonra `SAFE`; otonomda ön izin penceresiyle sınırlı devam | — |
| HIL'de sim duraklatıldı | IMU kesilmesi → `SAFE` | Çalıştırma geçersiz (06 §2.7) |

### 2.8 Simülasyondan beklenenler

| Gerek | Bölüm | İçerik |
|---|---|---|
| Hedef modelleri | 04, A7 | Statik ve kinematik hedefler (sabit hız, rastgele yürüyüş, sinüs; 07 §2.7.1); şekil ve isabet bölgesi A7'den; hedef sınıfı senaryoda tanımlanır, yarışma panosu değildir (K7). Yönelim rastgeleleştirilebilir |
| Sonar imzası | 05, A7 | Hedef ağı ve akustik malzemesi [K] (05 §2.4.1); hedef ve torpido Embree örnekleri olarak her pingte kendi pozuyla. Hedef yansıtıcılığı bilinmediği için algılama başarımı iddiası yalnız modelin parametreleri için geçerlidir (07 §2.2.4 "dürüst değerlendirme") |
| Kamera imzası ve algılama | 07 | Hedefin görsel malzemesi aynı A7 kimliğiyle; algılama bileşeni 3.2'deki kaydı sensör çerçevesinde, kovaryans ve yakalama anıyla üretir; seyrüsefer ya da yer gerçeği pozu kullanmaz |
| Torpido ve fırlatıcı | 04 | Darbe modeli (04 §2.8); olayların `(boot_id, olay_id)` ile tekilleştirilmesi; `emniyet_aç`'ın süreli izni; `ateşle`'nin son geçerliliği ve yalnız izin geçerliyken uygulanması; olay kabul ya da ret yanıtı; boş tüp, su üstünde namlu ve sanal engel nedeniyle reddin ayrı nedenleri; kart sıfırlaması ve kopmada kilitleme. Torpido ve fırlatıcı parametre özeti A6'ya |
| İsabet değerlendirmesi | 04, 08 | 04'ün isabet olayı (nokta, normal, bağıl hız) hedef yerel çerçevesinde isabet bölgesine göre sınıflanır: temiz geçiş, temaslı geçiş, çarpma, ıska; ıskada en yakın geçiş vektörü. Her atış için yanlış atış denetimi (Bölüm 7) |
| Fırlatıcı geri okuması | 05, 06 | Tüp başına dolu, emniyet durumu, fiş ve pim durumu; gerçek bir aracın anahtar ve varlık algılayıcısı gibi bir sensör örneği; hata enjeksiyonuyla bozulabilir |
| Sanal emniyet girdileri | 08 | Acil durdurma, fiş, pim, bakım girdisi; panelden ve senaryo zaman çizelgesinden; A6'ya |
| Tekrarlanabilir atış senaryoları | 08, 09 | Tohumlu senaryo aileleri; ayar ve kabul tohumları ayrı; Monte Carlo koşuları SIL'de, bir süreçte birden çok çekirdekle (02 §2.8); tablo üretimi |

### 2.9 Çekirdek adımıyla eşzamanlılık sözleşmesi

**Sahiplik.** `ida_fc_t` iki bölümdür: adımın sahip olduğu durum (izler, durum makinesi, olay kutusu, sıralama tamponu, küme tablosu) ve iş yuvaları (`ida_fc_bg_t`, `J` = 4; tahmin). Arka plan işi yalnız iş yuvalarına dokunur, adımın durumuna hiç dokunmaz. Her yuvanın tek yazarı vardır ve sahiplik bir alanla devredilir: `BOŞ → GÖNDERİLDİ` (adım yazar: profil ve seyrüsefer kopyası, `submit_step`, `due_step = submit_step + K_bg`), `GÖNDERİLDİ → BİTTİ` (arka plan yazar: tepe listesi), `BİTTİ → BOŞ` (adım tüketir).

**Tüketim adımı sabittir.** Sonuç yalnız `due_step` adımında tüketilir; bitmiş olsa da daha erken tüketilmez. `due_step` geldiğinde bitmemişse iş düşürülür, geç biten sonuç atılır ve `bg_late` sayılır. Böylece arka plan sonucunun hangi adımda kullanıldığı yalnız girdilerle belirlenir (06 §2.4'ün beklentisi). `K_bg` varsayılanı 8 adımdır (400 Hz'te 20 ms; tahmin, Bölüm 6). Bütün yuvalar doluyken gelen profil düşürülür ve `sonar_drop` sayılır.

**Kipler.** SIL koşucusu her adımdan önce `ida_fc_background`'u iş kalmayana kadar çağırır; `bg_late` SIL'de oluşmaz ve koşu bit düzeyinde tekrarlanır. Kartta RTOS'suz ana döngü arka planı dilimler halinde çağırır (01 §2.4); adım ve arka plan aynı bağlamda olduğu için yarış yoktur. HIL'de `bg_late > 0` A6'ya düşürülen veri olarak yazılır ve bariyerli çalıştırmayı tekrarlanamaz işaretler (iskelet A6). FreeRTOS yedeğinde sahiplik alanının devri bir bellek bariyeri ister; çekirdek `<stdatomic.h>` kullanmadığı için (02 §2.7) bu bariyeri platform sağlar (01'den beklenen).

**Kayıt.** Arka plan sonucu adımın girdisinin parçası sayılır: tekrar oynatma için her tüketilen yuvanın `(submit_step, due_step, bitti/düştü)` bilgisi kayda yazılır (02-sol md.16).

---

## 3. Arayüzler

İmza ve alan listesi düzeyindedir; sözleşmeler ayrı işte dondurulur (iskelet §3). Zamanlar `int64` ns; `ida_stime_t` ölçüm zamanı, `ida_wtime_t` duvar saatidir (02 §3.1).

### 3.1 Ortak çekirdekle (A4 içinde)

```c
void       ida_fc_init(ida_fc_t *fc, const ida_fc_params_t *p, uint32_t boot_salt);
void       ida_fc_step(ida_fc_t *fc, const ida_fc_in_t *in, ida_fc_out_t *out);
ida_bg_rc  ida_fc_background(ida_fc_bg_t *bg);       /* yalnız iş yuvaları (2.9); en çok bir dilim; IDLE | MORE */
ida_rc     ida_nav_hist_at(const ida_nav_hist_t *h, ida_stime_t t,
                           ida_nav_t *nav, uint8_t *reset_seg); /* 02 sağlar; sınır ötesinde hata */
```

- **`ida_fc_in_t`:** `t_now` (adımın IMU ölçüm zamanı), `now_w`, `nav` (`ida_nav_t`; 02 §3.6), `hist` (salt okunur geçmiş görünümü; ufuk ≥ `T_det_max` + `D_sıra` + pay; reset sınırını bildirir), `permit` (`ida_fc_permit_t`; 02'den, bu adımın daraltmaları uygulanmış), `regime` (yüzey, geçiş, dalış), `v_current` + `v_current_valid` (akıntı kestirimi; yoksa geçersiz), `ping360` (isteğe bağlı `const` işaretçi), `n_det` + `det[8]` (A1'den gelen kayıtlar), `n_cmd` + `cmd[4]` (`ida_fc_cmd_t`), `launcher[2]` (geri okuma), `n_ack` + `ack[4]` (olay yanıtları), `align_status` (`ida_align_status_t`), `mission_step_id` ve `preauth` (görev modülünden), `tick_wall` (IMU'suz çağrı bayrağı).
- **`ida_fc_out_t`:** `align_req` (`ida_align_req_t`; 02 §3.6), `n_evt` + `evt[4]` (`ida_fc_event_t`), `status` (3.4'teki `IDA_FC_STATUS` alanları), `n_det_tx` + `det_tx[4]` (değerlendirme için kart içi sonar kayıtları), `alive` (2.7'deki koşullar), sayaçlar.
- **`ida_fc_permit_t`** (02'nin alanlarına ek gerekler): `armed`, `mode_ok`, `est_ok`, `failsafe_clear`, `est_level`, `reset_counter`, `gcs_link_ok` (duvar saatinde, 02'nin kalp atışı damgasından).
- **`ida_fc_event_t`:** `olay_id` (u32), `tür` (`EMNIYET_AC`, `EMNIYET_KAPAT`, `ATESLE`), `tüp` (u8), `t_karar` (ölçüm zamanı), `geçerlilik_ns` (yalnız `EMNIYET_AC`), `son_geçerlilik` (yalnız `ATESLE`), `iz_id`, `çözüm_seq`. 02'nin `ida_step_out_t` içindeki `fire[2]` alanının yerini alır (Bölüm 8).
- **`ida_fc_cmd_t`:** `cmd_id` (u32), `tür` ve yük: `ARM {kip, desen}`, `DISARM`, `ABORT`, `SELECT {iz_id, tüp}`, `FIRE {jeton, iz_id, tüp}`, `RESET {desen}`, `TEST_ENTER`, `TEST_EXIT`. Her komut `IDA_FC_COMMAND_ACK {cmd_id, sonuç, neden}` ile yanıtlanır.
- **`ida_fc_launcher_t`** (geri okuma): `dolu`, `emniyet_açık`, `fiş_takılı`, `pim_çıkık`, `acil_durdurma`, `bakım`, `t_meas`, `seq`.
- **`ida_align_req_t` / `ida_align_status_t`:** 02 §3.6'daki alanlar. Ek gerek: durumun FC'ye hangi adımda döndüğü (02-sol md.13; 02'den beklenen) ve `accepted = false` nedeni.

### 3.2 Algılama kaydı `ida_detection_t` (A1 ve A4)

| Alan | Tip | Anlamı |
|---|---|---|
| `seq` | u32 | Üretici başına artan; tekrar ve geri gitme reddedilir |
| `t_capture` | `ida_stime_t` | Yakalama anı (2.2 md.3) |
| `t_sigma_ns` | u32 | Zaman belirsizliği (1σ) |
| `source` | u8 | Sensör: `PING360`, `CAM_FWD` (genişletilebilir) |
| `inst` | u8 | Sensör örneği; montaj bu kimlikle `ida_params_t`'den |
| `frame` | u8 | İlk sürümde yalnız `SENSOR`; başka çerçeve reddedilir |
| `kind` | u8 | `RB2` (menzil, yön), `B2` (yön, yükseliş), `RB3` (menzil, yön, yükseliş). Poz (6B) türü ilk sürümde yok |
| `z[3]` | f32 | Türe göre ilk 2 ya da 3 bileşen; m, rad |
| `cov[6]` | f32 | `z`'nin 3×3 kovaryansının üst üçgeni (yalnız ölçüm belirsizliği); kullanılmayan satırlar 0 |
| `class` | u8 | Hedef sınıfı (senaryoda tanımlı); 0 = bilinmiyor |
| `confidence` | f32 | [0, 1]; yalnız iz başlatma ön koşulu ve telemetri |
| `calib_ver` | u16 | Üreticinin iç kalibrasyonu ve beklediği montaj sürümü |
| `producer_id`, `producer_ver` | u8, u16 | Algoritma ve sürüm; yer bilgisi yalnız burada |
| `flags` | u8 | Geçerli, kör bölge sınırında, doymuş kutu içeriyor |

06'nın taslağındaki `values[6]`, `cov[6]` düzeni bu tabloya göre daraltılmalı; `t_sigma_ns` ve `calib_ver` eklenmelidir (06-sol md.21; Bölüm 8).

### 3.3 Parametreler (`ida_fc_params_t`, 02'nin kalıcı parametre bloğunda)

Grup ve örnek alanlar: `sonar` (`c_p`, maske menzilleri, CFAR pencereleri (m), `α`, güç dönüşümü, küme eşikleri), `trk` (model seçimi sınıf tablosu, gürültü yoğunlukları, M-of-N, K, `D_sıra`, `T_det_max`), `sol` (tüp geometrisi, yörünge tablosu ve özeti, `k`, `R_min`, `R_max` tablosu, `ω_max`, `v_max`, `T_dwell`, `T_taze`, `L_çıkış`, `σ_akıntı`), `sm` (bütün zaman aşımları, `T_izin`, `T_geç`, `T_hb_fc`, `K_bg`), `policy` (zorunlu kaynak kümesi, tüp sırası). Değiştirilebilirlik: `sm` ve `policy` yalnız `SAFE` iken; diğerleri `ARMED` altında (02-sol md.23 ilkesi).

### 3.4 A1 mesajlarının içeriği (taşıma 06'nın)

- **`IDA_FC_COMMAND`** (yer istasyonu → kart): `ida_fc_cmd_t` alanları; kaynak `sysid` ve oturum zarfı 06'dan.
- **`IDA_FC_COMMAND_ACK`:** `cmd_id`, `sonuç` (KABUL, RED, YİNELEME), `neden` (engel kimliği).
- **`IDA_FC_STATUS`** (10 Hz): `durum`, `kip`, seçili `iz_id` ve iz durumu, `tüp`, kalan torpido, çözüm (`R`, `t_hit`, `e_y`, `e_z`, `σ_y`, `σ_z`), ölçüt bit maskesi, engel bit maskesi, `dwell` ilerlemesi, `jeton` (yalnız `SOLUTION_VALID`'de), son olay kimliği ve onay durumu, NIS ortalaması, sayaçlar (`bg_late`, `sonar_drop`, neden başına algılama reddi).
- **Olay mesajı** (kart → sim, HIL düzlemi): `boot_id`, `ida_fc_event_t` alanları; sürekli `IDA_ACTUATOR` çerçevesinden ayrı ve onaylı taşınır. **Olay yanıtı** (sim → kart): `olay_id`, `sonuç` (UYGULANDI, YİNELEME, GEÇ, İZİNSİZ, BOŞ_TÜP, ENGEL, MODEL_DIŞI).
- **Fırlatıcı durumu** (sim → kart, sensör örneği): `ida_fc_launcher_t`.
- **`IDA_DETECTION`:** 3.2.

---

## 4. Hata durumları ve davranışı

| Durum | Tespit | Güvenli davranış |
|---|---|---|
| Bayat algılama | `t_now − t_capture > T_det_max` (ölçüm zamanında; 500 ms, tahmin) ya da geçmiş ufku dışında | Reddedilir ve sayılır; iz güncellenmez; tazelik ölçütü düşer, atış izni yok |
| Donmuş ya da gelecekteki zaman damgası | Aynı üreticide `t_capture` ilerlemiyor; `t_capture > t_now + pay` | Reddedilir; kaynak "sağlıksız" işaretlenir; tazelik o kaynaktan sağlanamaz |
| Kalibrasyon sürümü, çerçeve ya da tür uyuşmazlığı | `fc_det` | O kaynağın bütün kayıtları reddedilir; durum ve neden yayımlanır |
| Geçersiz kovaryans | NaN/Inf, simetrik değil, 3×3 Cholesky başarısız | Reddedilir |
| Yinelenen ya da geri giden `seq` | Üretici başına | Atılır, sayılır |
| Arka plan geç kaldı; yuvalar dolu | `due_step`; yuva durumu | İş ya da profil düşürülür, sayılır; HIL'de A6'ya; kontrol adımı beklemez |
| Seyrüsefer sıfırlaması | `reset_counter` değişti | İzler kaydırılır, `RECONFIRM`; `TRACKING` ve üstü → `ARMED`; sıfırlamadan önceki kayıtlar atılır |
| Kestirim kötüleşmesi | `est_level` | `DEGRADED`: atış ölçütü 7 düşer → `SOLUTION_VALID` ve üstünden `TRACKING`; `BAD` → `SAFE` |
| Aynı adımda silahsızlandırma (ya da izin kaybı) ve atış | `fc_safe` sıralaması | Daraltma uygulanır, `FC_FIRE` "aynı adımda daraltıldı" nedeniyle reddedilir |
| Tekrarlanan `FC_FIRE` | `cmd_id` görülmüş | İlk sonucun ACK'i tekrarlanır; yeni etki yok |
| Bayat ya da yanlış jeton | `fc_sm` | Reddedilir, nedeni bildirilir; durum değişmez |
| Olay onayı gelmedi | `T_ack` = 50 ms (duvar saati, tahmin) içinde yanıt yok | Aynı kimlikle yeniden gönder; `ATESLE` son geçerliliğini geçtiyse ya da 3 deneme dolduysa `FAULT_LATCHED` + `emniyet_kapat` (fırlatıcı durumu bilinmiyor) |
| Tekleme | `FIRING`'de tüp dolu kaldı | `FAULT_LATCHED`; `emniyet_kapat`; tüp kullanım dışı |
| Beklenmeyen tüp boşalması | `FIRING` dışında `dolu` düştü | `FAULT_LATCHED`; olay kaydı |
| Geri okuma çelişkisi | FC açmamışken `emniyet_açık`; fiş "takılı" ama acil durdurma etkin | `FAULT_LATCHED`; `emniyet_kapat` |
| İzde sayısal hata | NaN/Inf, negatif varyans | İz silinir; seçili izse → `ARMED`; aynı adımda tekrarı → `FAULT_LATCHED` |
| Durum değişkeni bozuk | Geçersiz desen ya da tamamlayıcı kopya uyuşmazlığı | `FAULT_LATCHED`; `alive_ok = false` (IWDG sıfırlar) |
| Hizalanma reddedildi ya da süresi doldu | `align_status`, 60 s | → `ARMED` |
| Yörünge tablosu özeti uyuşmuyor | Açılışta ve `ARMING`'de | Silahlandırma reddedilir |
| A1 kopması, kart sıfırlaması, yer istasyonu kopması | 2.7 tablosu | 2.7 tablosu |

---

## 5. Kalite kademelerinin etkisi

FC kademeyi bilmez (05 §5; 02 §5). Etkiler dolaylıdır:
- **Sonar sade/tam:** profilin içeriği değişir (600/1200 örnek, eşevresiz/eşevreli, çok yollu yok/var). Pencereler metre cinsinden olduğu için parametreler aynı kalır; ama gerçekleşen yanlış alarm oranı kademeye göre değişebilir. `α`, iki kademede de ölçülen orana göre en kötü durum için seçilir; kademeye özel parametre karta gönderilmez.
- **Kamera kapalı:** dışarıdan kayıt gelmez; FC yalnız sonarla çalışır. Zorunlu kaynak kümesinde kamera varsa çözüm hiç geçerli olmaz ve neden "zorunlu kaynak yok" diye yayımlanır. Kör bölgedeki atışlar bu durumda yalnız kovaryans koşuluyla sınırlıdır (2.5 md.6).
- **Kayıt kademesi:** "yalnızca olaylar"da atış senaryolarının tekrar oynatma güvencesi yoktur (iskelet A6); kabul koşuları en az "telemetri" kademesinde yapılır.

---

## 6. Ölçüme bağlı kararlar

| Karar | Birincil | Yedek | Ölçüm |
|---|---|---|---|
| Arka plan yürütme | RTOS'suz ana döngüde dilimli (01 §2.4) | FreeRTOS düşük öncelikli görev, platform bariyeriyle devir | M5; yeni: FC hızlı kısmının ve profil işinin en kötü süresi (DWT) |
| `K_bg` | 8 adım (400 Hz) | 16 adım; 200 Hz'te 4 | M5, aynı yeni ölçüm |
| Çözüm hızı | 50 Hz | 25 Hz | M5 |
| Eşikleme | CA-CFAR, `α` kalibre | OS-CFAR | Yeni (SIL): hedefsiz sahnede gerçekleşen yanlış alarm oranı, kademe başına; açık Ping360 kayıtlarıyla karşılaştırma (M14'ün K8 ile daralan kapsamı, 05 §8) |
| İlişkilendirme | GNN + Munkres | Sınırlı hipotezli JPDA | SIL: yanlış iz oranı ve iz kopması, yoğun yankılı havuz senaryosunda |
| Seyrüsefer belirsizliği | Göreli yaklaşım (`Q_nav` sürüklenme) | Schmidt filtresi | SIL: NEES/NIS tutarlılığı |
| Sıralama | `D_sıra` tamponu | Geri kestirimli güncelleme | SIL; M12, M30 (kamera gecikmesi) |
| Yörünge öngörüsü | 04 modelinden tablo + bileşim | Kartta sayısal entegrasyon | M16 (torpido `C_D`, fırlatma); SIL Monte Carlo |
| Gecikme telafisi | Sabit `L_çıkış` | Ölçülen dağılımdan kovaryans | M1, M17 |
| Gözetim süresi | 01'in değeri (100 ms) | — | M19 |

Bu bölüm, 00 Bölüm 7'ye iki yeni ölçüm konusu önerir: "FC hızlı kısım ve sonar işi en kötü süresi (kartta, M5 ile)" ve "CFAR gerçekleşen yanlış alarm oranı kalibrasyonu (SIL)".

---

## 7. Test yaklaşımı ve kabul ölçütleri

**Ayar ile kabulün ayrılması.** Toleranslar (`k`, `R_max`, `ω_max`, `v_max`, `T_dwell`, `T_taze`, `α`) ayar tohumlarıyla koşulan senaryolarla seçilir ve kabul koşularından **önce** dondurulur; kabul koşuları ayrı tohumlarla yapılır. Kabulden sonra eşik gevşetilmez (02-sol md.24'ün dersi).

**Birim testleri** (aynı testler kartta ve iki işletim sisteminde): durum makinesinin her durum × olay çifti; geçersiz bit desenleri; jetonun tek kullanımı ve bağlandığı üçlü; ön iznin sayaç ve pencere sınırı; komut sıralaması (daraltan önce); olay kutusunun aynı kimlikle yeniden gönderimi; CFAR'ın bağımsız üstel gürültüde nominal Pfa'yı (iki tohumda, istatistiksel aralık içinde) verdiği ve ilintili, nicemlenmiş, doymuş girdide sapmanın raporlandığı (varsayımın sınaması, 07.5); Munkres'in rastgele 8 × 24 matrislerde kaba kuvvet aramasıyla aynı maliyeti verdiği ve `[[1,2],[2,100]]` örneğinde 4 bulduğu (07.6); kapı eşiklerinin 9,210 ve 11,345 olduğu; kutupsal EKF güncellemesinin analitik durumla karşılaştırması; açı sarma; dönüşüm zincirinin (`p_W = p_B + R_WB(R_BS p_S + t_BS)`) bilinen montajla sınaması; sıfırlamada iz kaydırması; arka plan sonucunun yalnız `due_step`'te tüketilmesi.

**SIL senaryoları:** sabit hedefe yaklaşma, hizalanma ve atış; hareketli hedefte iz sürekliliği; kör bölgeden geçiş (iz silinmez, sonar fırsatı sayılmaz); akıntıda atış; kamera kapalı; yoğun yankılı havuz (duvar, çok yollu, tam kademe); iki hedefte doğru hedef seçimi; seyrüsefer sürüklenmesi altında NEES/NIS. Ölçütler: menzile göre algılama olasılığı, gerçekleşen yanlış alarm oranı, konum hatası, iz onay süresi, iz kopması, isabet oranı, ıska dağılımı (07 §2.7.4).

**Arıza enjeksiyonu test seti** (zorunlu; SIL'de her biri, HIL'de en az bir kez; 07 §3.5 md.3'ün genişletilmişi):

| # | Enjeksiyon | Beklenen |
|---|---|---|
| F1 | Aynı A1 partisinde `DISARM` + `FC_FIRE`; aynı adımda kip değişikliği + `FC_FIRE`; arıza eylemi kenarı + ön izinli atış | Atış olayı üretilmez; `SAFE`/`ARMED`; ACK nedeni "aynı adımda daraltıldı" |
| F2 | Tetiğin tekrarı: `FC_FIRE`'ın aynı `cmd_id` ile yinelenmesi; olay mesajının 06 hata enjektörüyle çoğaltılması; IMU `DUP_SEQ` ile önceki çıktının yeniden gönderimi; olayın gecikmeli teslimi | Sim tam bir fırlatma uygular ya da son geçerlilik geçtiyse hiç uygulamaz; yinelemeler sayılır |
| F3 | A1 kopması `FIRE_AUTHORIZED`, `FIRING` ve `ASSESS` içinde; yer istasyonu kopması operatör ve otonom kipte | Kopmadan sonra fırlatma yok; sim kilitler; kart `SAFE`; otonomda ön izin sınırı |
| F4 | Kestirim sıfırlaması `SOLUTION_VALID` ve `FIRE_AUTHORIZED` içinde | `ARMED`; yeniden onaydan önce atış yok |
| F5 | Bayat algılama: kamera kayıtlarının `T_det_max`'tan fazla geciktirilmesi; `t_capture` donması; gelecekteki damga; yanlış `calib_ver` | Kayıtlar reddedilir; tazelik ölçütü düşer; atış yok |
| F6 | Kart sıfırlaması (yazılım, IWDG) her durumda; ana döngü kilitlenmesi | `SAFE` açılış; süreli izin düşer; eski `boot_id` olayları uygulanmaz |
| F7 | Geri okuma bozulması (fiş, emniyet, dolu); tekleme; olay yanıtı kaybı | `FAULT_LATCHED`; sanal engeller fırlatmayı yine engeller |
| F8 | Sanal acil durdurma `FIRING` sırasında; pim takılıyken atış | Fırlatma yok; olay yanıtı ENGEL |
| F9 | Bozuk durum deseni (bellek enjeksiyonu, SIL); arka planın aç kalması | `FAULT_LATCHED` + sıfırlama; aç kalmada `SAFE`, sıfırlama yok |
| F10 | Bayat ya da başka çözüme ait jeton; sıra dışı ve CRC hatalı komut | Reddedilir; durum değişmez |

**Kabul ölçütleri:**
1. **Sıfır yanlış atış.** Yanlış atış: sim'in uyguladığı ve (a) geçerli izin zinciriyle (jeton ya da ön izin) eşlenemeyen, (b) uygulandığı adımda kartın izninin ya da bir sanal engelin kapalı olduğu, (c) yinelenen kimlikten doğan ya da (d) son geçerliliği geçmiş her fırlatma. Bütün arıza testlerinde ve sayısı koşulmadan önce sabitlenen Monte Carlo koşularında sıfır (sayı 09'da kesinleşir).
2. Her arızada tabloda yazılan duruma geçiş; daraltmada aynı adımda, kopmada `T_hb_fc` + 1 adım içinde.
3. Aynı ikilide iki SIL koşusunun FC çıktıları ve olayları bit düzeyinde aynı; A6'dan tekrar oynatma aynı (iskelet A6).
4. Dondurulmuş toleranslarla kabul senaryolarında isabet oranı ve ıska dağılımı, ayar aşamasında yazılan değerleri karşılar.
5. M5 ölçümünde FC hızlı kısmının en kötü süresi 02'nin adım bütçesi içinde (hedef ≤ 150 µs; tahmin) ve arka plan dilimi 01'in 300 µs'ı içinde.

---

## 8. Açık noktalar ve iskelete öneriler

### 8.1 Komşu bölümlerle uyumsuzluklar (bölüm iskelete uydu)

1. **06: atış seviye sinyali.** 06 §2.2 `IDA_ACTUATOR.launcher[2]` alanında atışı yükselen kenarlı seviye olarak taşıyor; iskelet §3.2 benzersiz kimlikli tek seferlik olay istiyor (06-sol md.2). Bu bölüm iskelete uydu: atış ve emniyet ayrı, onaylı olay mesajıyla taşınır (3.4). 06 bunu değiştirmeli.
2. **02 ve 01: `fire[2]`.** 02 §3.1'deki `ida_step_out_t.fire[2]` ve 01 §2.7'deki "yeniden gönderimde tetik sıfırlanır" yaklaşımı iskelet §3.2'yi karşılamaz (01-sol md.1, 02-sol md.12). Yerine `ida_fc_event_t` olay listesi ve aynı kimlikle yeniden gönderim (2.7 md.3).
3. **02: adım sırası.** 02 §2.2'de `fcif` (5) kip yöneticisinden (6) önce. İskelet §3.2 gereği DISARM, kip ve arıza eylemleri FC'den önce uygulanmalı (02-sol md.11).
4. **02: eksik girdiler.** `ida_fc_permit_t`'de rejim, `est_level`, `reset_counter`, yer istasyonu bağlantı durumu yok; geçmiş sorgusu reset sınırını bildirmiyor (02-sol md.17); akıntı kestirimi ilk sürümde yok (02 §2.3), bu yüzden akıntı düzeltmesi parametre ve `σ_akıntı` ile yapılıyor; hizalanma durumunun dönüş yolu tanımsız (02-sol md.13).
5. **Ping360 ölçüm zamanı.** İskelet §3.1 (0.4) akustik ölçümde ölçüm zamanını yansıma anı sayıyor; 05 §2.2 ve 06 §2.2 profil damgasını gönderim anı `t_tx` olarak tanımlıyor. Çok kutulu bir profilin tek yansıma anı yoktur. Bu bölüm profil başlığını `t_tx` kabul eder ve her algılamanın yansıma anını `t_tx + r/c` olarak kendisi hesaplar. İskelete öneri aşağıda.
6. **06 `IDA_DETECTION`:** `values[6]` / `cov[6]` düzeni 3 bileşene indirilmeli; `t_sigma_ns`, `calib_ver` ve çerçeve kuralı eklenmeli (06-sol md.21). Kamera hızına bağlı bütçe (06 §2.3) toplu kısımda sonarla ortak hesaplanmalı (06-sol md.8).
7. **04 `OneShotEvent`:** `geçerlilik_ns`, `son_geçerlilik`, `boot_id` kapsamı, olay yanıtı ve kopma ya da kart sıfırlamasında kilitleme eksik (2.8). 04'teki "tüp dolu ve emniyet kapalı değilse" ifadesi emniyetin anlamı bakımından belirsiz; "süreli izin geçerliyse" olarak netleşmeli.

### 8.2 İskelete öneriler

1. **§3.1:** "Bir profil boyunca toplanan akustik ölçümlerde başlık gönderim anını taşır; her yankının ölçüm zamanı `t_tx + r/c` ile türetilir."
2. **§5 tablosu:** sistem sensörlerine "fırlatıcı durumu (tüp dolu, emniyet, fiş, pim)" eklenmesi; "sanal emniyet girdileri (acil durdurma, silahlandırma fişi, emniyet pimi, bakım)" satırının sim sunucusunun girdisi olarak yazılması.
3. **§3.2:** tek seferlik olayların oturum kapsamı (`boot_id`), son geçerlilik ve alıcı yanıtı; "izin veren olay süreli olur ve yenilenmezse düşer" kuralı.
4. **§3:** algılama kaydının çerçeve kuralı: "ölçüm sensör çerçevesinde; dünya dönüşümü yalnız kartta".

### 8.3 Mimara bildirilen bulgu

**Atış menzili ile sonar kör bölgesi çakışıyor.** 04'ün torpido varsayılanlarıyla (2.5) torpido 1 m'yi ≈ 1,1 s'de alır ve hızı ≈ 0,48 m/s'ye düşer; düşey sapma ihmal edilemez. Etkili atış menzili bu yüzden 1 m mertebesindedir. Ping360 bu menzilde kör ya da yakın alan bandındadır ve iskelette öne bakan başka bir menzil kaynağı yoktur (Ping1D dibe bakar). Son yaklaşmada konum yalnız kamera ve seyrüsefer sürüklenmesiyle sürdürülür (2.3). Seçenekler mimarın kararıdır: (a) bu kısıtı kabul edip kör bölgede kamerayı zorunlu kaynak yapmak; (b) 04'ün torpido varsayılanlarını (ayrılma hızı, `C_D`, yüzerlik) daha uzun menzil verecek biçimde gözden geçirmek; (c) simüle edilen sensör setine öne bakan ikinci bir tek ışınlı menzil ölçer eklemek (iskelet §5 değişikliği; gerçek sensör tedariki değildir).

### 8.4 Açık noktalar

- **Anlamsal algılama üreteci** (05 §8): SIL'de atış mantığını sonar algoritmasından bağımsız sınamak için yer gerçeğinden gürültülü kayıt üreten bir sensör modeli. Kayıt biçimi aynıdır ve `producer_id` ile ayrılır; yer gerçeğinin çekirdeğe girmeme kuralıyla (02 §3.2 md.5) ilişkisi 05 ve 07 ile birlikte kararlaştırılmalı.
- **Hedef sınıfları ve isabet bölgeleri** senaryo verisidir; A5'teki biçimi 08'in işidir.
- **Sürekli itkili torpido** ilk sürümde yok (04'ün sonraki kademesi; 00 Bölüm 6); yörünge tablosu yalnız pasif torpido içindir.
- **Kamera kayıt hızı ve sayısı** A24'e bağlı; sıralama tamponu ve A1 bütçesi buna göre kesinleşir.
- **Tetik gecikmesinin dağılımı** (gerçek tetik devresi) bu aşamada yok; `L_çıkış` yalnız protokol ve sim uygulama gecikmesini temsil eder.
