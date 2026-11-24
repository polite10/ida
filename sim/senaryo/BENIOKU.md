# sim/senaryo — A5 senaryo çözümleyicisi ve doğrulayıcısı

A5 sözleşmesinin (`docs/sozlesmeler/A5-senaryo.md`, 1.0) senaryo ve araç dosyalarını okuyan,
doğrulayan ve tek bir çözümlenmiş senaryo üreten kitaplık (`ida_senaryo`) ve komut satırı aracı
(`ida_senaryo_dogrula`). İş belgesi: `docs/gorevler/W4-senaryo.md`.

## Akış

1. **Aşama 1** (her dosya için; A5-K90): UTF-8, TOML 1.0 sözdizimi (toml++), yinelenen anahtar,
   sayıların binary64'e çevrilmesi (A5-K96; kaynak metinden yeniden çevrilir), tarih/saat reddi
   (A5-K97), tamsayı sınırı, şema (`scenarios/sema/a5.schema.json`), sürüm (A5-K03, K04), araç
   dosyası başvurusu (A5-K12), `x_` uyarıları (A5-K07).
2. **Aşama 2**: birleştirme (A5-K13–K15), birleşmiş aracın şeması, desteklenmeyenler (A5-K102),
   A7 manifesti (A5-K115), çözümleme (A5-K16; varsayılanlar `veri/ek_e.toml`'dan), §4.3–§4.13
   kuralları ve A7 atıfları, çözümlenmiş biçimin şeması.
3. Geçerliyse RFC 8785 kanonik baytları ve SHA-256 davranış özeti (A5-K17).

Aşama 3 (ön denetim) A1'in ve sim'in işidir. Senaryonun yürütülmesi bu dizinin işi değildir.

## Gömülü veri

- `scenarios/sema/a5.schema.json` derleme zamanında gömülür ve çalışma anında yorumlanır
  (`src/sema.cpp`; şema ilk kullanımda düğüm ağacına derlenir).
- `veri/ek_e.toml` A5 Ek E bloğunun bayt bayt kopyasıdır; `sim.senaryo.ek_e_suruklenme` testi
  sözleşmedeki blokla aynı olduğunu denetler. Sözleşmede Ek E değişirse bu dosya yeniden
  çıkarılır.

## Araç

```
ida_senaryo_dogrula <senaryo.toml> [--cozumlenmis <çıktı.json>] [--rapor <çıktı.json>] [--kok <depo kökü>]
```

Çıkış kodları: 0 geçerli, 1 geçersiz, 2 kullanım hatası, 3 G/Ç hatası. Rapor (A5 §3.14)
`--rapor` yoksa standart çıktıya yazılır; bulgular ve özet standart hataya.

## Uygulama sınırları (dayanıklılık; sözleşme dışı)

Dosya boyu en çok 4 MiB, iç içe tablo/dizi derinliği en çok 48 (`kAzamiDosyaBoyu`,
`kAzamiDerinlik`); aşan dosya A5-K01 ile reddedilir.

toml++ depo içi vcpkg overlay port'undan gelir (`vcpkg-overlay/tomlplusplus`, kök
`vcpkg-configuration.json` ile kayıtlı): 3.4.0 + üst akımda birleşmiş dört düzeltme (PR #242,
#301, #305, #321; adresler `portfile.cmake`'te). Bu yamalar bozuk girdinin kitaplıktaki tanımsız
davranış yollarına (`TOML_UNREACHABLE`, `TOML_ASSERT_ASSUME`) ulaşmasını olağan ayrıştırma
hatasına çevirir; güvenlik sınırı kitaplığın kendisidir, `toml_okuma.cpp`'deki ön tarama değil.
3.5.x kullanılmaz: TOML 1.1.0'ı koşulsuz ayrıştırır, A5-K01 ise TOML 1.0.0 ister.

## Testler

`ctest -R sim.senaryo` (test kodu `test/`; bozuk dosyalar bellekte, örnek dosyalardan üretilir).

## Sonar kurulumu (W4-bagla-sonar)

`sonar_kur(DogrulanmisSenaryo, DosyaKaynagi, DogrulamaRaporu)` (`include/ida/senaryo/sonar_yukleyici.hpp`, hedef `ida_senaryo_sonar`; yalnız `sim/sonar` varken): `sensor_kur` ile aynı kural (yalnız doğrulanmış senaryo). Sonarlar `arac.sensorler`'in `ping1d` ve `ping360` girdileri (A5 §3.6 alan adları); ortam `ortam.su` (`ses_hizi` verilmemişse W4-K1 formülü sim'de); `kalite.sonar` yalnız `sade` (`tam` hata). Sahne (W4-sonar-sahne; `sonar::AgSahnesi`): su yüzeyi (z = 0; sonar modelinin yüzey malzemesi) + `ortam.sahne` A7 statik varlığı `ortam.sahne_poz` pozunda + `hedefler` (varlıklarının ağları `konum`/`yonelim` pozunda; yalnız akustik yansıtıcı cisim). Ağlar A7-K27'ye göre seçilir (`akustik[]` doluysa SON_*, boşsa COL_* kaynak ağları birebir; dışbükey parçalar değil), malzeme üçgen başına (A7-K28; A7 §3.4 akustik alanları, `kalibrasyon.malzeme.<kimlik>.akustik` geçersiz kılar, A5-K13); taban geometriden gelir (A7-K56), `ortam.derinlik` sahneye girmez. Hedef hareketinden sabit, sabit_hiz ve sinus sahnede uygulanır (A5 §3.10 formülleri); rastgele_yuruyus ve `yonelim_rastgele = true` A5 Ek D akışı ister, bu dilimde açık hatayla reddedilir. A7 manifesti `assets/<varliklar.kume>/manifest.json`'dan aynı kaynakla yeniden okunur; baytların SHA-256'sı doğrulama raporundaki kaynak özetine koşulsuz, `varliklar.manifest_ozeti` verilmişse ona da eşit olmalı, beyan edilen `varliklar.kume_surumu` manifestteki değerle aynı kalmalıdır (A5-K115; W4-bagla-sonar sol #3). Varlıklar `a7_varlik_yukle` ile okunur (aşağıda); yüklenemeyen varlık A7'nin koduyla açık hatadır (sessiz atlama yok; A7-K51). Uygunluk örneği `ornek`in ikili dosyaları olmadığından onu gösteren senaryolar (`ornek-havuz.toml`, `z1.toml`) sonar açıkken `EKSIK_VARLIK` ile reddedilir; sonarlı havuz senaryosu `sonar-havuz.toml` küme `havuz`u kullanır. Etkin sonar yoksa manifest okunmaz, sahne boş kalır. Sınama: `sim.senaryo.sonar_yukleyici`, `sim.senaryo.sonar_sahne_havuz_duvari`, `sonar_sahne_hedef`, `sonar_sahne_k27`.

### A7 varlık okuyucusu (W4-sonar-sahne)

`a7_varlik_yukle(manifest, kume, kimlik, DosyaKaynagi, A7Hata&)` (`include/ida/senaryo/a7_varlik.hpp`, `ida_senaryo` içinde): A7'nin izin verdiği en küçük glTF alt kümesini okur (GLB 2, `mode` = 4, float32 VEC3 `POSITION`, u8/u16/u32 indeksli ya da indekssiz; TRS ya da birim ölçekli `matrix`) ve sonarın kullandığı ağları gövde çerçevesine çıkarır (`p_b = M (p_kök − t_REF)`). Denetimler ve kodlar: A7-K03 `SEMA`, K06 `EKSIK_VARLIK`/`OZET_UYUSMAZ` (ayrıştırmadan önce), K10–K12 `GLTF`/`UZANTI`, K13 `BIRIM`, K14 `EKSEN`, K19 `ERISILEMEZ`, K20–K21 `DUGUM`, K23 `BIRIM`, K24 `TUR` (araç tüp kuralı A5 ister, yok), K26 `KAPALI_DEGIL`/`DISBUKEY_DEGIL`, K28 `MALZEME`, K56 `TABAN`. Yapılmayanlar: K04–K05 ve şema (çözümleyici), K25 ve K52 (yüzerlik), K29 (bağlantı noktaları), K40–K46 (türetilmiş parçalar). Hata iletisi A7 §5'teki bağlamı taşır (kod, küme, varlık, düğüm indeksi ve adı, ayrıntı). Fizik bugün bu okuyucuyu kullanmaz: çarpışma şekli A7 araç varlığının sınır kutusudur (A5-K34); A7-K50'nin "tek yükleme" kuralı fizik ağları okumaya başladığında birleştirilecek (açık konu). Sınama: `sim.senaryo.a7_varlik_havuz`, `a7_varlik_olumsuz` (bellekte kutu varlıkları, `test/glb_yazici.hpp`). Küme `havuz`ın üreticisi `viz/arac/model-uret/havuz.py`; `sim.senaryo.havuz_kumesi_uretici` üretilenin depodakiyle bayt bayt aynı olduğunu sınar.
