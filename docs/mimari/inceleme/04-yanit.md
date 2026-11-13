# 04 numaralı bölümün inceleme yanıtı

İnceleme: [04-sol.md](04-sol.md). Bölüm: [../04-simulator-cekirdegi.md](../04-simulator-cekirdegi.md), sürüm 0.2. Fizik ve matematik düzeltmeleri uygulanmadan önce yeniden türetildi; sayısal sonuçlar bu turda yeniden hesaplandı. von Benzon makalesinin PDF'i indirildi ancak bu ortamda metni çıkarılamadı; kaynağa dayanan iki madde (5, 17) bu yüzden inceleme kaydına dayanılarak uygulandı ve bölümde öyle işaretlendi.

| # | Sonuç | Bölümde değişen yer | Not |
|---|---|---|---|
| 1 | giderildi | §2.10, §2.11, §6, §7 (madde 2, 14, 15), §8 | Adım 1,25 ms; itici RK4 çarpanları 0,98368 / 0,95371; çözülmüş 400 Hz önerisi ve kademe notu §8'den çıkarıldı. |
| 2 | giderildi | §2.4 "Sonlu derinlik alanı" | Potansiyelden türetildi: `ζ = a cos θ` için `w_yukarı = −aω sinh/sinh · sin θ`, `∂_t w = −aω² … cos θ`; yüzey kinematik koşulu ve süreklilik denetlendi. NED dönüşümü ayrıca yazıldı. |
| 3 | giderildi | §2.2 (`O_b`), §2.3 "Su hızı ve türevi", §3 `IEnvironmentView` | Bütün genelleştirilmiş büyüklükler `O_b`'de; `ν̇_w` dönme ve `(∇v)ṗ` terimleriyle; FK için Euler ivmesi ayrı; arayüze `water_acceleration_local` ve `water_velocity_gradient` eklendi. |
| 4 | giderildi | §2.4 "Kırpma ve kuvvet" | Kuvvet basınç merkezinde uygulanır, moment `O_b`'ye göre: `r_P × Rᵀ dF`. İncelemedeki `r_b ×` yazımı yerine üçgenin uygulama noktası kullanıldı (yalnız `r_b` her üçgen için yanlış olurdu). |
| 5 | giderildi | §2.7.2–2.7.3 | Sıra `F = K(s) F_ss`. Kaynak bu turda bağımsız okunamadı; inceleme kaydına dayanıldığı bölümde belirtildi. Ters sıranın küçük sinyal kazancını ≈ 0,29'a indirdiği bağımsız olarak denetlendi. |
| 6 | giderildi | §2.6 "Kütle durumları" tablosu, §2.8 fırlatma adım 1 | Üç durum için `m`, `r_g`, `∇`, `r_b`, atalet ve net yüzerlik bu turda hesaplandı; `O_b` boş araç CG'si olarak sabit; torpido ayrılma adımında araçtan çıkar, iki gövdede birden sayılmaz; başlangıç hızları aracın rijit hızı. |
| 7 | giderildi | §2.8 "Fırlatma: darbe modeli", "Sonraki kademe", §7 madde 9 | İskelet 0.3'e göre tek anlık darbe; 200 ms'ye yayma kaldırıldı. Test dış kuvvetsiz, sabit kütleli, durgun referans durumla; beklenen değerler yeniden hesaplandı. Sonlu süreli tüp modeli tek paragrafta sonraki kademe. |
| 8 | giderildi | §2.8 torpido tablosu ve "Kararlılık" | Tek geometri: prolate sferoid (hacim 1,6755·10⁻⁴ m³, kütle 0,175 kg). Atalet, CG/CB, eklenmiş kütle ve kanat verileri eklendi; statik koşul hesaplandı (oran 1,51). |
| 9 | giderildi | §2.7.7, §4, §6 | Toplam yükü (aviyonik `P/V` dahil) içeren `f(V)` ve `[7 V, E]` aralığı; yedek bir adım gecikme yerine deterministik ikiye bölme; kök yoksa güç çökmesi ve `model_dışı_durum`; geçerlilik kuralı yazıldı. Kalıntı eşiğinin ADC gerekçesi kaldırıldı. |
| 10 | giderildi | §2.7.6 | `Q = 64 800 C`, `OCV_p = 4 OCV_h`, `R₁,p = (4/6) R₁,h`, `C₁,p = (6/4) C₁,h`; başlangıç SoC ve `V₁` tanımlandı. |
| 11 | giderildi | §2.7.4 | Akım ilk sürümde negatif olmaz; rejenerasyonun gerilim ve enerji etkisi açıkça "modellenmez"; 7–12, 20–26 ve 7 V altı alanlar bayrakla tanımlandı. |
| 12 | giderildi | §2.1, §2.10 "her aşamada / adım başında tutulanlar", "Mertebe" | Bara gerilimi ve batarya ayrık; bağlaşık sistemin mertebesi bir olarak yazıldı; 16 kat beklentisi düzgün alt sistemle sınırlandı. Temas yayı aşamalarda çözüldüğü için bu listeden çıktı. |
| 13 | giderildi, kısmen farklı | §2.9 "Tepki" | `1/m_eff = wᵀM⁻¹w`, normal yönü ve yaklaşma hızı tanımlandı. Farklı çözüm: temas geometrisi adım başında sabit, derinlik ve hız her RK4 aşamasında güncellenir; adım başında sabit kuvvetin sönümsüz yayda enerji eklediği (`det = 1 + (ωh)²/2`) türetildi. Köşe ve dört köşe hesabına göre `k_n` 10⁵'ten 3·10⁴'e indirildi; çoklu temas testi eklendi. |
| 14 | giderildi | §2.4 "Nokta modeline geçiş" | Ölçüt: yüzey kesişmesinin olanaksızlığı (`d_üst ≥ Σ|a_i|`) ve her bileşen için sonlu derinlik `F_p ≤ ε` ya da `k L_maks ≤ 2π/5` uzaysal ölçütü. |
| 15 | giderildi | §7 madde 5–6 | Roll periyodu `B` ile (3,630 / 3,585 s); uç hız testinde ağırlık ve yüzerlik kapalı, net yüzerlikli heave ayrıca verildi. |
| 16 | giderildi | §2.6 itici yerleşimi | "Bağımsız" ifadesi kaldırıldı; tam matrisin rankı 6, yatay grubun çapraz momentleri, rankın doyumda garanti olmadığı ve dikey pozitif yön dönüşümü yazıldı. |
| 17 | giderildi | §2.6 boş araç tablosu, yer tutucu ağlar | Nitelikler: kütle ölçülmüş, atalet ve CB CAD, CG tanım. Dördüncü etiket kaldırıldı. Kaynak bu turda bağımsız okunamadı; inceleme kaydına dayanıldığı belirtildi. 1,20 oranı bağımsız seçim; çarpışma kutusu için 0,46 × 0,58 × 0,38 m. |
| 18 | giderildi | §2.11 zamanlayıcı ve tablo | En yakın adım, eşitlikte önceki adım; 30 Hz 26/27 adım; en çok 0,625 ms sapma. |
| 19 | giderildi | §2.11 "Sayaçlar" | Her kipte ham son tarih ihlali, azami gecikme ve dağılım, kaçırılan adım; geçersizlik eşiği ayrı; SIL'de tanısal anlam. |
| 20 | giderildi | §2.12, §5 | "Etkilemez" kesinliği kaldırıldı; etki M11/M15 ile ölçülecek. |
| 21 | giderildi | §3 `WorldSnapshot`, `OneShotEvent`, `StepResult`; §2.9 süzgeçler; §2.8 isabet | Değişmez, paylaşımlı sahipli snapshot halkası (K = 64); kinematik, kuaterniyon sırası, `ν̇` ve `a_n` anlamı; kütle durumu alanı; bağlı/ayrılan torpido çarpışma süzgeci; hareketli hedefe bağıl süpürme ve sorgu zamanı. |
| 22 | giderildi | §2.1 adım 3, §2.7.6 kip tablosu, §6 | Atıf §2.7.7'ye düzeltildi; ideal, B0, B1 kiplerinin denklemleri, zorunlu girdileri ve izin koşulları tek tabloda. |

**İskelet 0.3 ile gelen ek uyumlar:** ortak kabuller (§2.2), sürekli komut ömrü ve tek seferlik olaylar (§2.7.1, §3), zaman çerçevesi için kontrol sınırı görevi (§2.11), tekrar oynatma kaydı (§2.13, `StepResult`), üç kütle durumu ve darbe modeli (§2.6, §2.8).
