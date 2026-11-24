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
