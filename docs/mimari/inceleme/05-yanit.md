# 05-sensorler-ve-sonar: sol incelemesine yanıt

İncelenen sürüm 0.1; yanıtlanan sürüm 0.2 (iskelet 0.7). Madde numaraları [05-sol.md](05-sol.md) tablosundaki numaralardır. Bütün denklem ve hesap düzeltmeleri uygulanmadan önce bu turda yeniden türetildi: özgül kuvvette `R_bsᵀ`, yoğunluk konvansiyonunda `ASD/√(2Δt)` ve `N = ASD/√2`, Gauss–Markov sürecinde `q = 2σ_∞²/τ` limiti, DVL'de `c_ayar/c_gerçek` oranı, simetrik dört ışın kovaryansı (`1,848 σ_b`, `0,541 σ_b`), CEP'in varyans paylaşımı (`1,611 m`, `0,537 m`), Ping360 menzil/örnek/`sp` sınırları. Hepsi inceleyicinin sonucuyla aynı çıktı. Yalnızca TDK veri sayfasındaki 100 Hz RMS değeri bu oturumda bağımsız okunmadı; bölümde böyle işaretlendi.

| # | Sonuç | Bölümde değişen yer | Not / gerekçe |
|---|---|---|---|
| 1 | giderildi | 2.3 "Ortak gösterim", 3 `Mount` | Montaj doğrudan sabit `O_b`'ye göre; hareketli referans düzeltmesi kaldırıldı (iskelet 0.4+ §3.1) |
| 2 | giderildi | 2.2 zaman alanları tablosu ve sensör tablosu, 3 `MeasurementRecord` | `t_meas` (aralık sonu / anlık geçerlilik / yansıma / gönderim), `window_ns`, `t_tx`, `t_hazır`, `t_teslim` tek sözleşmede; A1'e giden alanlar yazıldı |
| 3 | giderildi | 2.2 nedensellik kuralları; 2.3.3, 2.3.6, 2.5 | Batarya ve basınçta `t_meas` aralık sonu, teslim aralık bitince; kamerada `t_teslim = t_meas + L_sonrası` (ortadan `T_poz/2 + L_sonrası`). İnceleyicinin "merkezden 51 ms" önerisi iskeletin aralık sonu kuralıyla eşdeğer biçimde uygulandı |
| 4 | giderildi | 2.2 tablosu, 2.4.6, 3 | Ping1D `t_meas` = yansıma anı `t_tx + t_eko/2`, `t_tx` ayrı alan. Ping360'ta iskelet 0.5 §3.1 gereği `t_meas` = gönderim anı, kutu yansıma anı `t_tx + (i+½)T_s/2` |
| 5 | giderildi | 2.2 "Adım ve bağlama"; 8 | En yakın sınır, ≤ 0,625 ms; tamamlanma olayları fiziksel bitişten sonraki ilk sınırda. 04 ile ilgili eski uyumsuzluk kayıtları kaldırıldı |
| 6 | giderildi | 2.3.1 yer gerçeği; 7 IMU satırı | `f_s = R_bsᵀ f_b`, jiroskopta `R_bsᵀ`; kimlik olmayan montajla kabul testi eklendi |
| 7 | giderildi | 2.3 gürültü sınıfları; 2.3.1 tablosu; 7 | Tek taraflı ASD, `σ = ASD/√(2Δt)`, `N = ASD/√2`; 400 ve 200 Hz değerleri yeniden hesaplandı |
| 8 | giderildi | 2.3 Gauss–Markov | `q` tanımı, rastgele yürüyüşün ayrı süreç olduğu, durağan başlangıç |
| 9 | giderildi | 2.3 gürültü sınıfları; 2.3.1–2.3.6 | Yoğunluk ile örnek σ'sı ayrıldı; IMU süzgecinin ayrık güncellemesi ve sırası; doğrusalsızlık denklemde; basınç, manyetometre ve batarya sıcaklık yasaları; DVL `σ_b(r, u)` |
| 10 | giderildi | 2.3.2 | İç birim T, nT girişinde `10⁻⁹`; besleme akımı ile motor yönü ayrıldı, yön bağımlılığı `K_k⁺`, `K_k⁻` |
| 11 | giderildi | 2.3.4 hata bütçesi | Varyans paylaşımı; düşey ve hız hatasının zaman modeli yazıldı |
| 12 | giderildi | 2.3.4 | Uydu havuzu, uydu başına histerezisli izleme, uydu sayısıyla bozulan doğruluk, suya yeniden girişte edinimin sıfırlanması; 2/24 s alt sınır değil medyan, dağılım [K] |
| 13 | giderildi | 2.3.5 ışın hızı | Oran `(c_ayar/c_gerçek)^κ`; kendi türetimimle doğrulandı; dönüştürücü tipi [S] |
| 14 | giderildi | 2.3.5 geometri, geçerlilik, çözüm, mod | Işın kimlikleri ve vektörleri (azimutlar [T]), irtifa ile eğimli menzil ayrımı, alt ışın ağırlığı, `altitude` tanımı, histerezis ve kilit kuralları |
| 15 | giderildi | 2.3.5 ışın hızı ve zaman zinciri | Yansıtıcı yüzeye göre bağıl hız; `t_tx → t_meas → t_rapor → t_teslim` nedensel zinciri; kilitsiz rapor; `r̄/c` yaklaşımının sınırı |
| 16 | giderildi | 2.3.3; 7 basınç satırı | 15 adım = 18,75 ms; gürültü testi ofset ve sıcaklık çıkarılmış artık üzerinde |
| 17 | giderildi | 2.4.2 | Normalize `p̃` ve `Ĩ` adlandırıldı, dB referansı ve sıfır tabanı; desen `B_tx B_rx = B²` (genlik), `B⁴` (yoğunluk). Fiziksel yoğunluk yerine inceleyicinin önerdiği seçeneklerden normalize büyüklük seçildi |
| 18 | giderildi | 2.4.3; 6 yakınsama satırı | Tam kademede yüzeye bağlı kalıcı saçıcı hücreleri; sade kademede ilintisizlik bilinen sınır olarak yazıldı; yakınsama topluluk üzerinde; mutlak faz doğruluğu iddia edilmiyor |
| 19 | giderildi | 2.4.4 yol tablosu ve genlik; 5 hesap yükü | Yol türleri, speküler/difüz etkileşim, açınık uzunluklarla genlik; kesişim ve görünürlük ayrı sayıldı. Dönüşte sekme yolu, karşılıklılık gereği gidişte sekmeyle aynı katkıyı verdiği için ayrı ışınla değil ×2 ile eklenir |
| 20 | giderildi | 2.4.4 biriktirme ve alıcı zinciri | Darbe/alıcı çekirdeği iki kademede; alıcı zinciri sırası; çıktı ölçeği [S]; Rayleigh testi log öncesi doğrusal genlikte |
| 21 | kısmen | 2.4.2 soğurma | Tam ifade, birimler, pH girdisi yazıldı; tatlı su uygulaması [T]. Formülün birincil kaynaktan doğrulanması bu turda da yapılamadı; 8. başlıkta açık |
| 22 | giderildi | 2.4.5 parametreler ve varsayılan katsayılar; 7 Ping360 satırı | `sp` tamsayı sınırı ve gerçekleşen menzil; 1 m üretici süreleri uydurma ve kabul dışında; 50 m yalnız uydurma noktası, kabul değil |
| 23 | kısmen | 2.4.5 komut ve durum tablosu; 8 md.5 | Durum tablosu yazıldı (çakışma seçimleri [T]); 06'nın `IDA_PING360_CONFIG`'inde hedef açı, `transmit` ve durdurma alanı olmadığı için A1 eşlemesi 06'ya bildirildi |
| 24 | giderildi | 2.4.6; 7 Ping1D satırı | %0,5 çözünürlük doğruluk sayılmıyor; `T_dinleme` en uzak dinlenen mesafeyi kapsıyor |
| 25 | kısmen | 2.4.1 değişmezlik, iş paketi, sınırlar; 2.2 iş parçacığı | İşe özel üst sahne, değişmez paylaşımlı alt sahneler, bekleyen iş sınırı, `epoch` ile iptal. Alt sahnenin eşzamanlı birden çok üst sahnede örneklenmesinin Embree'de izinli olduğu belgeden doğrulanmadı; yedek (işçi başına kopya) tanımlı, 8. başlıkta açık |
| 26 | giderildi | 2.1; 2.6; 2.7; 3 `raw()`, `solve()` | Temel ölçüm ve ham veri koşulsuz; kısmi arıza ham kopyada, çözüm saf işlevle yeniden; durumlu arızalar ayrı sınıf; RNG sayaç alanları tanımlandı |
| 27 | giderildi | 2.7; 7 ortak testler | Sabit bileşim sırası, parametre denetimi, `k ≥ 1`, donma sonrası davranış, durumlu arızada toparlanma sözleşmesi; bit eşitliği yalnız temel dizi ve durumsuz arıza için |
| 28 | giderildi | 3 A1 alanları ve `drain` | `seq` A1'de (06 v0.2 `H.seq`); 05 teslim kuyruğunu deterministik sırayla verir, parti içi iletim sırasının sahibi 06 |
| 29 | giderildi | 2.4.7; 5; 6 | Kademe kullanıcı ayarı, ölçüme bağlı yedek ayrı; kademe zamanlamayı değiştirmez, içerik çözünürlüğünü değiştirir; `sp` yuvarlama 06 §2.7 ile tek sözleşme |
| 30 | giderildi | 7 ilkeler, ortak testler, veri kümeleri | Kalibrasyon ve kabul verisi ayrı (Hasan 2024 senaryo bölmesi), etkiler tek tek, ≥ 20 tohum ve güven aralığı, mutlak + bağıl tolerans, sonarda topluluk istatistiği, eşiklerin önceden dondurulması |
| 31 | giderildi | Giriş etiket tablosu; ilgili parametreler | [S] yapısal bilinmez, [G] çevresel girdi, [C] yapılandırma eklendi; [K] varsayılanları ayrıca [T]. Görev tanımının dört etiketi korunup inceleme gereği ayrıntılandırıldı |

**Bu turda ayrıca:** iskelet 0.7'ye göre torpidonun itkisiz olduğu kabul edildi; bölümde itkili torpidoya bağlı ifade yoktu, itkiyle ilişkili "torpido kabarcık izi" açık noktası kaldırıldı. Fırlatıcı ve emniyet geri okuması sensör olarak eklendi (2.3.8): yetkili durum sim tarafında, geri okuma bozulabilir, tazelik `seq` ve `t_meas` ile.
