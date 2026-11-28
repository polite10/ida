# W5-atis-platform — uygulama raporu

Tarih: 2026-11-28. Dal: `is/w5-atis-platform`. Görev belgesi: `docs/gorevler/W5-atis-platform.md`. Bağımsız inceleme raporu değildir. Bu dilimde uygulayıcı, test yazarı ve kayıt aynı oturumda yürütüldü (kullanıcı talimatı: alt agent kullanılmasın); bağımsız kabul testleri ayrı agent'a verilmedi — açık iş olarak aşağıda yazılıdır.

## 1. G-K izlenebilirlik tablosu

Dosya adları `protocol/ida_link/src/` altındadır; testler `ctest --preset host-debug` adlarıdır. "Kanıt" sütunu uygulamanın yeri, "test" sütunu bu davranışı koşturan mevcut testtir.

| Madde | Kanıt | Test |
|---|---|---|
| G-K01 daraltıcı mandalı hiç atılmaz | `ust_komut.c` `mandala_al`, `ust_komut_al` | `protokol.birim.ust_komut`, `kabul.oturum.A1-K63.komut_sirasi` |
| G-K02 mandalda bekleyen genişleticiler atılır | `ust_komut.c` `genisleticileri_at` | `kabul.oturum.A1-K63.komut_sirasi` |
| G-K03 çağrı başına 8 komut, erteleme/atma | `ust_komut.c` `ust_komut_cek` (erteleme tamponu) | `protokol.birim.ust_komut`, `kabul.oturum.A1-K63.erteleme` |
| G-K04 ilk kabul kapısı | `ust_cikti.c` `kapi_*` | `protokol.birim.ust_cikti`, `kabul.oturum.A1-K67.*` |
| G-K05 `thr_valid` çıktı kapısı | `ust_cikti.c` (~382) | `protokol.birim.ust_cikti` |
| G-K06 olaylar `IDA_ACTUATOR`'tan önce | `ust_cikti.c` `ida_link_ust_put_out` | `protokol.birim.ust_cikti` |
| G-K07 olay yinelemesi ve sonuç payı | `ust_cikti.c` (`T_olay_yin`, `EXPIRED_NO_ACK`) | `protokol.birim.ust_cikti`, `protokol.birim.ust_sol` |
| G-K08 oturum sonu sırası | `ust_oturum.c` `ust_olay_oturum_sonu` | `protokol.birim.ust_oturum`, `protokol.birim.ust_uctan_uca` |
| G-K09 emniyet örneği hiç atılmaz; >16 → PROTOCOL | `ust_parti.c` (~316) | `protokol.birim.ust_parti` |
| G-K10 `boot_count` kesin artar / `boot_count_ok` | `firmware/app/kalici.c` | `firmware.birim.kalici` |
| G-K11 `core_ok = 0` iken IWDG beslenmez | `firmware/app/cagirici.c` + `saglik.c` | `firmware.birim.saglik` |
| G-K12 ilk gönderim kovaları | `ust_oturum.c` (~453) | `protokol.birim.ust_cikti` |
| G-K13 birleştirme sayacı | `ust_komut.c` `mandala_al` (`coalesced`) | `kabul.oturum.A1-K63.komut_sirasi` |
| G-K14 biçimi bozuk `FC_DISARM`/`FC_ABORT` mandala alınır | **bu dilimde değişti:** `ust_esleme.c` FC dalı + `ust_komut.c` sayaç | **yeni:** `test_gk14_bozuk_daraltici`; güncellenen `kabul.oturum.A1-K63.komut_sirasi` |
| G-K15 nihai `NARROWED` kimlikleri | `ust_komut.c` `nihai_mi`/`nihai_ekle` | `protokol.birim.ust_sol` (b6), `kabul.oturum.A1-K63` |
| G-K16 `EVENT_OVERFLOW` | `ust_cikti.c` (~328) | `protokol.birim.ust_cikti`, `kabul.oturum.A1-K80` |
| G-K17 `safety_rev` damgası | `ust_cikti.c` (~338), `ust_esleme.c` | `protokol.birim.ust_cikti` |
| G-K18 olay sonucu tam bir kez | `ust_parti.c` `evres`, `ust_cikti.c` | `protokol.birim.ust_parti` |
| G-K19 taşınan örneğin `VALID = 0` olması | `ust_ic.h` `UST_HF_VALID_SIL`, `ust_esleme.c` | `protokol.birim.ust_esleme` |
| G-K20 silahlandırma kalıcı belleğe yazılmaz | `firmware/app/kalici.h` (alan yok) | `firmware.birim.kalici` |

Bilinen boşluk **G-K14** bu dilimde kapatıldı. Diğer 19 madde uygulanmıştı; kanıtları yukarıdadır.

## 2. Bu dilimde değişenler

- **G-K14 / W5-K7:** `FC_DISARM` ve `FC_ABORT` için `tube ≠ 255` ya da `arg0`/`arg1` ≠ 0 artık komutu reddetmiyor: mandala alınıyor, bütün tüplere uygulanıyor, sonuç `ACCEPTED` oluyor; biçim hatası `IDA_LINK_SAY_CMD_BAD_FIELD` ve `cnt_cmd_bad_field` ile ayrıca görünüyor. `LINK_BAD_FIELD` yalnız `op ∉ {2,3}` işlemlere ve genişletici işlemlerin bozuk alanlarına uygulanıyor (`ust_esleme.c`, `ust_komut.c`, `ust_ic.h`).
- **W5-K70:** `ida_link_ust_cagri_gecerli` eklendi; `ida_link_ust_take` step için `tf.present = 1` şartını uygular ve ihlalde çağrıyı ızgara sayar (`IDA_LINK_SAY_STEP_TF_YOK`). Üç çağrı yolu (`firmware/app/main.c`, `sim/sil/src/kosucu.cpp`, `sim/kopru/src/karsi.cpp`) savunma denetimini kullanır; çekirdeğe zaman çerçevesiz step gitmez.
- **W5-K5:** sim, `FIRE` olayını yalnız kendi ilettiği ve kartın `ACCEPTED` sonucunu verdiği bir `FC_FIRE` ile (aynı tüp) eşleştiriyorsa uygular. Yetki `Kopru::komut_fc`'de kaydedilir, `sonuc_al`'da kabulde kurulur, `izin_kapat`'ta düşer; eşleşmeyen `FIRE` `REJ_SAFE` alır (`sim/kopru/src/kopru.cpp`, `kopru_sinir.cpp`, `kopru_parti.cpp`, `kopru_ic.hpp`).
- **W5-K69 (karar):** etkin acil durdurma `ARM`'ın ön koşulu değildir (W3-K1 korunur); bu dilimde çekirdeğe yeni ret koşulu eklenmedi. Mevcut `test_md2_emniyet_girdileri_arm_kosulu_degil` değişmeden geçer.
- Testler: `protocol/ida_link/test/oturum/test_komut.c` yeni G-K14 testi; `test_sol.c` ve `tests/kabul/oturum/kabul_komut_sirasi.c` eski (G-K14 öncesi) beklentileri W5-K7'ye göre güncellendi; `sim/kopru/test/test_destek.hpp` FC komut/sonuç yardımcıları; `test_emniyet.cpp` ve `test_emniyet3.cpp` FIRE'tan önce kabul edilmiş `FC_FIRE` gönderir.

## 3. Kabul maddeleri

| Madde | Sonuç |
|---|---|
| 1 — G-K tablosu | Yukarıda; 20/20 kanıtlı, G-K14 boşluğu kapatıldı. |
| 2 — G-K14/W5-K7 | Uygulandı ve test edildi (`test_gk14_bozuk_daraltici`; kabul testi güncellendi). |
| 3 — W5-K5 | Uygulandı; `sim.kopru.olay_fire_kabul` ve `eksik_kume_tutarli` kabul edilmiş `FC_FIRE` ile geçiyor. |
| 4 — Zaman çerçevesiz step | `ida_link_ust_cagri_gecerli`; take içinde savunma; üç çağrı yolu. Test: `protokol.birim.ust_parti` (sayaç) ve mevcut uçtan uca testler. |
| 5 — W5-K69 | Karar: yeni davranış yok; mevcut test değişmeden geçiyor. |
| 6 — Tam tur | `host-debug`: `ctest -E host.cekirdek_siniri` → **510 testin 508'i geçti**, 2 düştü (`host.cmake_moduller.iki_dizin`, `host.cmake_moduller.bos_yol`; bu Mac'in bozuk CommandLineTools SDK'sı), 2 atlandı (`sim.a3.*web`). `host.cekirdek_siniri.*` (10 test) aynı SDK sorunu nedeniyle dışlandı. `host-release` ve `fw-release` koşulmadı; CI'ya bırakıldı. |
| 7 — Kısa HIL | **Koşulmadı.** Bu worktree'de `ida_sim`/`fw-release` derlemesi yapılmadı; kart koşusu ayrı iş. |
| 8 — Çağrı süresi | **Ölçülmedi.** |

## 4. Ortam

- Bu Mac'te varsayılan SDK (`/Library/Developer/CommandLineTools/SDKs/MacOSX27.2.sdk`) ile linker `ld: tapi error: malformed file` veriyor. Derleme `-DCMAKE_OSX_SYSROOT=$(xcrun --sdk macosx --show-sdk-path)` (Xcode SDK'sı) ile yapıldı. Aynı nedenle `host.cekirdek_siniri.*` ve `host.cmake_moduller.*` testleri geçici projeleri varsayılan SDK ile yapılandırıp düşüyor; ürün kusuru değil, ortam. CI'da bu testler normal koşar.
- ASan bu Mac'te koşturulmadı (W5-K43).

## 5. Açık işler

- **Bağımsız kabul testleri** (`tests/kabul/platform/**`) yazılmadı; kullanıcı talimatıyla bu dilimde alt agent (test yazarı) kullanılmadı. İncelemede veya ayrı işte eklenmeli.
- Bağımsız inceleme yapılmadı; dilim emniyet sınıfındadır (K21/K27). Birleştirmeden önce `ida-inceleyici` ya da sol incelemesi gerekir.
- `host-release` tam tur, `fw-release` derlemesi ve kısa HIL koşusu CI'ya/kart işine bırakıldı.
- Sözleşmeye küçük sürüm notu: A1-K64'ün W5-K7 biçim okuması (zaten W5-ortak not tablosunda).
