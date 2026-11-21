// ÜRETİLDİ — elle değiştirme. Üreten: web/arac/uret-katalog.mjs
// Kaynak: docs/sozlesmeler/sema/a3-kanallar.json (A3 1.0, dondurulmuş)
// Kaynak özeti: sha256:9195d0f3dae3d19a16b648fa84e1bf968b5d4d8aeb1153573607191713330488
/* eslint-disable */

export const A3_SURUMU = "1.0";
export const WS_ALT_PROTOKOLU = "ida-a3.v1";
export const HTTP_TABAN_YOLU = "/api/v1";
/** İstemcinin derlendiği katalog dosyasının özeti (A3-K16 biçimi). */
export const KATALOG_OZETI = "sha256:9195d0f3dae3d19a16b648fa84e1bf968b5d4d8aeb1153573607191713330488";
export const BASLIK_BAYT = 24;

export type TelTipi = 'u8' | 'u16' | 'u32' | 'u64' | 'i32' | 'i64' | 'f32' | 'f64' | 'metin' | 'bayt_dizisi' | 'dizi';
export type YokEtiketi = 'NaN' | 'INT64_MIN' | '2^64-1' | '0xFFFFFFFF' | '255' | '-1' | '0';

export interface AlanTanimi {
  readonly ad: string;
  readonly tip: TelTipi;
  readonly adet?: number;
  readonly sayac?: 'u8' | 'u16' | 'u32';
  readonly azami?: number;
  readonly yok?: YokEtiketi;
  readonly aralik?: readonly [number, number];
  readonly degerler?: Readonly<Record<number, string>>;
  readonly birim: string;
  readonly oge?: readonly AlanTanimi[];
}

export type KanalSinifi = 'durum' | 'dizi' | 'olay';
export type ZamanAnlami = 'sim' | 'olcum' | 'alim' | 'gonderim';

export interface KanalTanimi {
  readonly kanal_id: number;
  readonly ad: string;
  readonly kanal_surumu: number;
  readonly yuk: YukAdi;
  readonly sinif: KanalSinifi;
  readonly azami_hz: number | null;
  readonly varsayilan_hz: number | null;
  readonly kuyruk_siniri: number | null;
  readonly zaman_anlami: ZamanAnlami;
  readonly anahtar: readonly string[];
  readonly zorunlu_abonelik: boolean;
  readonly yer_gercegi: boolean;
}

/** Yük düzenleri: alanlar tel sırasıyla (A3 §3.4). */
export const YUKLER = {
  yuk_calistirma_durum: [
    { ad: "durum", tip: "u8", degerler: { 0: "bos", 1: "yuklendi", 2: "hazirlaniyor", 3: "kosuyor", 4: "duraklatildi", 5: "bitti", 6: "durduruldu", 7: "hata", 8: "izleme" }, birim: "-" },
    { ad: "kip", tip: "u8", degerler: { 0: "yok", 1: "sil", 2: "hil_1x", 3: "hil_bariyer", 4: "tekrar_fizik", 5: "tekrar_girdi_sil", 6: "tekrar_yeniden_kurma" }, birim: "-" },
    { ad: "run_id", tip: "metin", sayac: "u16", azami: 32, birim: "bayt" },
    { ad: "adim_no", tip: "u64", birim: "-" },
    { ad: "hedef_carpan", tip: "f32", aralik: [0, 100], birim: "-" },
    { ad: "gerceklesen_carpan", tip: "f32", yok: "NaN", birim: "-" },
    { ad: "gecerli", tip: "u8", degerler: { 0: "gecersiz", 1: "gecerli" }, birim: "-" },
    { ad: "gercek_zamani_kacirdi", tip: "u8", degerler: { 0: "hayir", 1: "evet" }, birim: "-" },
    { ad: "gecerlilik_nedenleri", tip: "u32", birim: "-" },
    { ad: "tekrar_guvencesi", tip: "u8", degerler: { 0: "yok", 1: "izleme", 2: "fizik", 3: "girdi" }, birim: "-" },
    { ad: "kademe_gorunum", tip: "u8", degerler: { 0: "kapali", 1: "sade", 2: "tam" }, birim: "-" },
    { ad: "kademe_kamera", tip: "u8", degerler: { 0: "kapali", 1: "sade", 2: "tam" }, birim: "-" },
    { ad: "kademe_sonar", tip: "u8", degerler: { 0: "kapali", 1: "sade", 2: "tam" }, birim: "-" },
    { ad: "kademe_kayit", tip: "u8", degerler: { 0: "olaylar", 1: "telemetri", 2: "tam" }, birim: "-" },
    { ad: "a1_oturum", tip: "u8", degerler: { 0: "kapali", 1: "el_sikisma", 2: "hazir", 3: "kosuyor", 4: "kopuk", 255: "uygulanamaz" }, birim: "-" },
    { ad: "son_komut_no", tip: "u32", birim: "-" },
    { ad: "durum_rev", tip: "u64", birim: "-" },
  ],
  yuk_sayac_gercek_zaman: [
    { ad: "parti_gonderilen", tip: "u64", birim: "-" },
    { ad: "parti_kacirildi", tip: "u64", birim: "-" },
    { ad: "parti_kacirildi_birlesik", tip: "u64", birim: "-" },
    { ad: "bariyer_bekleme", tip: "u64", birim: "-" },
    { ad: "gercek_zaman_kaymasi_ns", tip: "i64", birim: "ns" },
    { ad: "gercek_zaman_kaymasi_en_buyuk_ns", tip: "i64", birim: "ns" },
    { ad: "tf_gec_yazim_ns", tip: "i64", birim: "ns" },
    { ad: "yanit_ret_seq_disi", tip: "u64", birim: "-" },
    { ad: "yanit_ret_yinelenen", tip: "u64", birim: "-" },
    { ad: "yanit_ret_gec", tip: "u64", birim: "-" },
    { ad: "yanit_ret_oturum", tip: "u64", birim: "-" },
    { ad: "yanit_iptal_esz", tip: "u64", birim: "-" },
    { ad: "esz_guvenli_uygulanan", tip: "u64", birim: "-" },
    { ad: "komut_suresi_doldu", tip: "u64", birim: "-" },
    { ad: "olay_alinan", tip: "u64", birim: "-" },
    { ad: "olay_gec", tip: "u64", birim: "-" },
    { ad: "olay_eksik", tip: "u64", birim: "-" },
    { ad: "olay_sonuc", tip: "u64", adet: 6, birim: "-" },
    { ad: "komut_birlestirildi", tip: "u64", birim: "-" },
    { ad: "komut_ertelendi", tip: "u64", birim: "-" },
    { ad: "komut_sonucsuz", tip: "u64", birim: "-" },
    { ad: "ping360_seyreltildi", tip: "u64", birim: "-" },
    { ad: "ping360_gecikme_en_buyuk", tip: "u64", birim: "-" },
    { ad: "pencere_tasma", tip: "u64", birim: "-" },
    { ad: "rx_crc_err", tip: "u64", birim: "-" },
    { ad: "rx_resync_bytes", tip: "u64", birim: "-" },
    { ad: "rx_unknown_msgid", tip: "u64", birim: "-" },
    { ad: "rx_bad_len", tip: "u64", birim: "-" },
    { ad: "rx_wrong_sys", tip: "u64", birim: "-" },
    { ad: "rx_iflag", tip: "u64", birim: "-" },
    { ad: "rx_mav1", tip: "u64", birim: "-" },
    { ad: "rx_seq_gap", tip: "u64", birim: "-" },
    { ad: "oturum_kurulan", tip: "u64", birim: "-" },
    { ad: "oturum_kopma", tip: "u64", birim: "-" },
    { ad: "beklenmeyen_sifirlama", tip: "u64", birim: "-" },
    { ad: "calistirma_gecersiz", tip: "u8", birim: "-" },
    { ad: "neden_bitleri", tip: "u32", birim: "-" },
    { ad: "son_tarih_kacirildi", tip: "u64", birim: "-" },
    { ad: "sim_gecikmesi", tip: "u64", birim: "-" },
    { ad: "yanit_ret_eski_uretim", tip: "u64", birim: "-" },
    { ad: "olay_rev_ret", tip: "u64", birim: "-" },
    { ad: "sonuc_kuyrugu_en_buyuk", tip: "u64", birim: "-" },
    { ad: "kritik_erteleme", tip: "u64", birim: "-" },
    { ad: "kritik_erteleme_en_buyuk", tip: "u64", birim: "-" },
    { ad: "cagri_izi_eksik", tip: "u64", birim: "-" },
    { ad: "yapilandirma_ret", tip: "u64", birim: "-" },
    { ad: "cagri_cakismasi", tip: "u64", birim: "-" },
    { ad: "sonuc_talebi_atildi", tip: "u64", birim: "-" },
    { ad: "komut_kovasi_bekleme", tip: "u64", birim: "-" },
    { ad: "emniyet_rev_doydu", tip: "u8", degerler: { 0: "hayir", 1: "evet" }, birim: "-" },
    { ad: "mesaj_retleri", tip: "dizi", sayac: "u8", azami: 64, birim: "-", oge: [
        { ad: "mesaj_kimligi", tip: "u32", birim: "-" },
        { ad: "alan_ret", tip: "u64", birim: "-" },
        { ad: "bag_ret", tip: "u64", birim: "-" },
    ] },
    { ad: "adim_son_tarih_ihlali", tip: "u64", birim: "-" },
    { ad: "kacirilan_adim", tip: "u64", birim: "-" },
    { ad: "azami_adim_gecikmesi_ns", tip: "i64", birim: "ns" },
    { ad: "sonar_gec", tip: "u64", birim: "-" },
    { ad: "sonar_kuyruk_dolu", tip: "u64", birim: "-" },
    { ad: "sensor_kuyruk_tasma", tip: "u64", birim: "-" },
    { ad: "donma_onceki_cikti_yok", tip: "u64", birim: "-" },
    { ad: "atama_celiskisi", tip: "u64", birim: "-" },
    { ad: "gevsetme_reddedildi", tip: "u64", birim: "-" },
    { ad: "bayat_yetki", tip: "u64", birim: "-" },
    { ad: "komut_bayat_yetki", tip: "u64", birim: "-" },
    { ad: "a3_halka_tasma", tip: "u64", birim: "-" },
    { ad: "kayit_dusen", tip: "u64", birim: "-" },
  ],
  yuk_olay: [
    { ad: "tur", tip: "u16", birim: "-" },
    { ad: "onem", tip: "u8", degerler: { 0: "bilgi", 1: "uyari", 2: "hata", 3: "emniyet" }, birim: "-" },
    { ad: "kaynak", tip: "u8", degerler: { 0: "sim", 1: "a3", 2: "kart", 3: "senaryo", 4: "kayit", 5: "surec", 6: "yer_istasyonu" }, birim: "-" },
    { ad: "adim_no", tip: "u64", yok: "2^64-1", birim: "-" },
    { ad: "komut_no", tip: "u32", birim: "-" },
    { ad: "istemci_id", tip: "u32", birim: "-" },
    { ad: "ayrinti", tip: "metin", sayac: "u16", azami: 8192, birim: "bayt" },
  ],
  yuk_fizik_durum: [
    { ad: "adim_no", tip: "u64", birim: "-" },
    { ad: "govdeler", tip: "dizi", sayac: "u16", azami: 64, birim: "-", oge: [
        { ad: "govde_id", tip: "u16", birim: "-" },
        { ad: "tur", tip: "u8", degerler: { 0: "arac", 1: "torpido", 2: "hedef", 3: "diger" }, birim: "-" },
        { ad: "kutle_durumu", tip: "u8", degerler: { 0: "iki_torpido", 1: "bir_torpido", 2: "bos", 255: "uygulanamaz" }, birim: "-" },
        { ad: "p_n", tip: "f64", adet: 3, birim: "m" },
        { ad: "q_nb", tip: "f64", adet: 4, birim: "-" },
        { ad: "nu", tip: "f64", adet: 6, birim: "m/s, rad/s" },
        { ad: "a_n", tip: "f64", adet: 3, birim: "m/s^2" },
    ] },
    { ad: "iticiler", tip: "dizi", sayac: "u8", azami: 8, birim: "-", oge: [
        { ad: "u", tip: "f32", aralik: [-1, 1], birim: "-" },
        { ad: "itki_n", tip: "f32", birim: "N" },
        { ad: "akim_a", tip: "f32", birim: "A" },
    ] },
    { ad: "bara_gerilimi_v", tip: "f32", birim: "V" },
    { ad: "paket_akimi_a", tip: "f32", birim: "A" },
    { ad: "soc", tip: "f32", aralik: [0, 1], birim: "-" },
  ],
  yuk_kart_durum: [
    { ad: "kart_t_ns", tip: "i64", birim: "ns" },
    { ad: "acilis_sayaci", tip: "u32", birim: "-" },
    { ad: "kip", tip: "u8", yok: "255", birim: "-" },
    { ad: "silahli", tip: "u8", degerler: { 0: "hayir", 1: "evet", 255: "bilinmiyor" }, birim: "-" },
    { ad: "saglik", tip: "u8", birim: "-" },
    { ad: "kestirim_gecerli", tip: "u8", degerler: { 0: "hayir", 1: "evet", 255: "bilinmiyor" }, birim: "-" },
    { ad: "p_n", tip: "f32", adet: 3, yok: "NaN", birim: "m" },
    { ad: "v_n", tip: "f32", adet: 3, yok: "NaN", birim: "m/s" },
    { ad: "q_nb", tip: "f32", adet: 4, yok: "NaN", birim: "-" },
    { ad: "derinlik_m", tip: "f32", yok: "NaN", birim: "m" },
    { ad: "batarya_soc", tip: "f32", yok: "NaN", aralik: [0, 1], birim: "-" },
    { ad: "adim_suresi_us", tip: "u32", yok: "0xFFFFFFFF", birim: "us" },
  ],
  yuk_atis_durum: [
    { ad: "fc_durumu", tip: "u8", yok: "255", degerler: { 0: "SAFE", 1: "ARMED", 2: "PERMIT_OPEN", 3: "FIRE_PENDING", 4: "TEST", 5: "FAULT_LATCHED" }, birim: "-" },
    { ad: "engel_maskesi", tip: "u32", yok: "0xFFFFFFFF", birim: "-" },
    { ad: "kart_son_olay_kimligi", tip: "u64", yok: "0", birim: "-" },
    { ad: "tupler", tip: "dizi", sayac: "u8", azami: 2, birim: "-", oge: [
        { ad: "tup", tip: "u8", aralik: [0, 1], birim: "-" },
        { ad: "dolu", tip: "u8", degerler: { 0: "bos", 1: "dolu" }, birim: "-" },
        { ad: "sureli_izin_gecerli", tip: "u8", degerler: { 0: "hayir", 1: "evet" }, birim: "-" },
        { ad: "izin_kalan_ns", tip: "i64", yok: "-1", birim: "ns" },
        { ad: "son_olay_sonucu", tip: "u8", degerler: { 0: "yok", 1: "uygulandi", 2: "yineleme", 3: "ret_emniyet", 4: "ret_suresi_gecmis", 5: "ret_diger" }, birim: "-" },
    ] },
  ],
  yuk_emniyet_girdiler: [
    { ad: "acil_durdurma", tip: "u8", degerler: { 0: "serbest", 1: "basili" }, birim: "-" },
    { ad: "silahlandirma_fisi", tip: "u8", adet: 2, degerler: { 0: "cikarilmis", 1: "takili" }, birim: "-" },
    { ad: "emniyet_pimi", tip: "u8", adet: 2, degerler: { 0: "cikarilmis", 1: "takili" }, birim: "-" },
    { ad: "son_degisim_adimi", tip: "u64", yok: "2^64-1", birim: "-" },
    { ad: "son_degisim_kaynagi", tip: "u8", degerler: { 0: "baslangic", 1: "senaryo", 2: "operator" }, birim: "-" },
    { ad: "kart_acil_durdurma", tip: "u8", yok: "255", birim: "-" },
    { ad: "kart_silahlandirma_fisi", tip: "u8", adet: 2, yok: "255", birim: "-" },
    { ad: "kart_emniyet_pimi", tip: "u8", adet: 2, yok: "255", birim: "-" },
    { ad: "geri_okuma_farki", tip: "u8", birim: "-" },
    { ad: "emniyet_rev", tip: "u32", aralik: [1, 4294967295], birim: "-" },
    { ad: "emniyet_rev_doydu", tip: "u8", degerler: { 0: "hayir", 1: "evet" }, birim: "-" },
  ],
  yuk_ariza_etkin: [
    { ad: "arizalar", tip: "dizi", sayac: "u16", azami: 256, birim: "-", oge: [
        { ad: "fault_id", tip: "metin", sayac: "u16", azami: 32, birim: "bayt" },
        { ad: "sinif", tip: "u8", degerler: { 0: "sensor", 1: "aktuator", 2: "guc", 3: "baglanti", 4: "protokol", 5: "sistem" }, birim: "-" },
        { ad: "hedef", tip: "metin", sayac: "u16", azami: 32, birim: "bayt" },
        { ad: "tur", tip: "metin", sayac: "u16", azami: 32, birim: "bayt" },
        { ad: "kaynak", tip: "u8", degerler: { 0: "senaryo", 1: "elle" }, birim: "-" },
        { ad: "baslangic_adimi", tip: "u64", birim: "-" },
        { ad: "bitis_adimi", tip: "u64", yok: "2^64-1", birim: "-" },
    ] },
  ],
  yuk_imu: [
    { ad: "sensor_id", tip: "u16", birim: "-" },
    { ad: "inst", tip: "u8", birim: "-" },
    { ad: "seq", tip: "u32", birim: "-" },
    { ad: "flags", tip: "u16", birim: "-" },
    { ad: "window_ns", tip: "i64", aralik: [0, 1000000000], birim: "ns" },
    { ad: "t_tx_ns", tip: "i64", yok: "INT64_MIN", birim: "ns" },
    { ad: "t_hazir_ns", tip: "i64", birim: "ns" },
    { ad: "t_teslim_ns", tip: "i64", birim: "ns" },
    { ad: "parti_step_seq", tip: "u32", yok: "0xFFFFFFFF", birim: "-" },
    { ad: "fault_mask", tip: "u32", birim: "-" },
    { ad: "quality", tip: "f32", yok: "NaN", aralik: [0, 1], birim: "-" },
    { ad: "model_version", tip: "u16", birim: "-" },
    { ad: "ivme_mps2", tip: "f32", adet: 3, birim: "m/s^2" },
    { ad: "acisal_hiz_radps", tip: "f32", adet: 3, birim: "rad/s" },
    { ad: "sicaklik_c", tip: "f32", yok: "NaN", birim: "degC" },
  ],
  yuk_manyetometre: [
    { ad: "sensor_id", tip: "u16", birim: "-" },
    { ad: "inst", tip: "u8", birim: "-" },
    { ad: "seq", tip: "u32", birim: "-" },
    { ad: "flags", tip: "u16", birim: "-" },
    { ad: "window_ns", tip: "i64", aralik: [0, 1000000000], birim: "ns" },
    { ad: "t_tx_ns", tip: "i64", yok: "INT64_MIN", birim: "ns" },
    { ad: "t_hazir_ns", tip: "i64", birim: "ns" },
    { ad: "t_teslim_ns", tip: "i64", birim: "ns" },
    { ad: "parti_step_seq", tip: "u32", yok: "0xFFFFFFFF", birim: "-" },
    { ad: "fault_mask", tip: "u32", birim: "-" },
    { ad: "quality", tip: "f32", yok: "NaN", aralik: [0, 1], birim: "-" },
    { ad: "model_version", tip: "u16", birim: "-" },
    { ad: "alan_t", tip: "f32", adet: 3, birim: "T" },
    { ad: "sicaklik_c", tip: "f32", yok: "NaN", birim: "degC" },
  ],
  yuk_basinc: [
    { ad: "sensor_id", tip: "u16", birim: "-" },
    { ad: "inst", tip: "u8", birim: "-" },
    { ad: "seq", tip: "u32", birim: "-" },
    { ad: "flags", tip: "u16", birim: "-" },
    { ad: "window_ns", tip: "i64", aralik: [0, 1000000000], birim: "ns" },
    { ad: "t_tx_ns", tip: "i64", yok: "INT64_MIN", birim: "ns" },
    { ad: "t_hazir_ns", tip: "i64", birim: "ns" },
    { ad: "t_teslim_ns", tip: "i64", birim: "ns" },
    { ad: "parti_step_seq", tip: "u32", yok: "0xFFFFFFFF", birim: "-" },
    { ad: "fault_mask", tip: "u32", birim: "-" },
    { ad: "quality", tip: "f32", yok: "NaN", aralik: [0, 1], birim: "-" },
    { ad: "model_version", tip: "u16", birim: "-" },
    { ad: "basinc_pa", tip: "f32", aralik: [0, 10000000], birim: "Pa" },
    { ad: "sicaklik_c", tip: "f32", yok: "NaN", birim: "degC" },
  ],
  yuk_gnss: [
    { ad: "sensor_id", tip: "u16", birim: "-" },
    { ad: "inst", tip: "u8", birim: "-" },
    { ad: "seq", tip: "u32", birim: "-" },
    { ad: "flags", tip: "u16", birim: "-" },
    { ad: "window_ns", tip: "i64", aralik: [0, 1000000000], birim: "ns" },
    { ad: "t_tx_ns", tip: "i64", yok: "INT64_MIN", birim: "ns" },
    { ad: "t_hazir_ns", tip: "i64", birim: "ns" },
    { ad: "t_teslim_ns", tip: "i64", birim: "ns" },
    { ad: "parti_step_seq", tip: "u32", yok: "0xFFFFFFFF", birim: "-" },
    { ad: "fault_mask", tip: "u32", birim: "-" },
    { ad: "quality", tip: "f32", yok: "NaN", aralik: [0, 1], birim: "-" },
    { ad: "model_version", tip: "u16", birim: "-" },
    { ad: "enlem_rad", tip: "f64", aralik: [-1.5707963267948966, 1.5707963267948966], birim: "rad" },
    { ad: "boylam_rad", tip: "f64", aralik: [-3.141592653589793, 3.141592653589793], birim: "rad" },
    { ad: "yukseklik_m", tip: "f32", birim: "m" },
    { ad: "hiz_ned_mps", tip: "f32", adet: 3, birim: "m/s" },
    { ad: "eph_m", tip: "f32", birim: "m" },
    { ad: "epv_m", tip: "f32", birim: "m" },
    { ad: "fix_turu", tip: "u8", degerler: { 0: "yok", 2: "iki_boyut", 3: "uc_boyut" }, birim: "-" },
    { ad: "uydu_sayisi", tip: "u8", birim: "-" },
  ],
  yuk_dvl: [
    { ad: "sensor_id", tip: "u16", birim: "-" },
    { ad: "inst", tip: "u8", birim: "-" },
    { ad: "seq", tip: "u32", birim: "-" },
    { ad: "flags", tip: "u16", birim: "-" },
    { ad: "window_ns", tip: "i64", aralik: [0, 1000000000], birim: "ns" },
    { ad: "t_tx_ns", tip: "i64", yok: "INT64_MIN", birim: "ns" },
    { ad: "t_hazir_ns", tip: "i64", birim: "ns" },
    { ad: "t_teslim_ns", tip: "i64", birim: "ns" },
    { ad: "parti_step_seq", tip: "u32", yok: "0xFFFFFFFF", birim: "-" },
    { ad: "fault_mask", tip: "u32", birim: "-" },
    { ad: "quality", tip: "f32", yok: "NaN", aralik: [0, 1], birim: "-" },
    { ad: "model_version", tip: "u16", birim: "-" },
    { ad: "hiz_sensor_mps", tip: "f32", adet: 3, yok: "NaN", birim: "m/s" },
    { ad: "irtifa_m", tip: "f32", yok: "NaN", birim: "m" },
    { ad: "fom_mps", tip: "f32", yok: "NaN", birim: "m/s" },
    { ad: "isin_gecerli", tip: "u8", birim: "-" },
  ],
  yuk_batarya: [
    { ad: "sensor_id", tip: "u16", birim: "-" },
    { ad: "inst", tip: "u8", birim: "-" },
    { ad: "seq", tip: "u32", birim: "-" },
    { ad: "flags", tip: "u16", birim: "-" },
    { ad: "window_ns", tip: "i64", aralik: [0, 1000000000], birim: "ns" },
    { ad: "t_tx_ns", tip: "i64", yok: "INT64_MIN", birim: "ns" },
    { ad: "t_hazir_ns", tip: "i64", birim: "ns" },
    { ad: "t_teslim_ns", tip: "i64", birim: "ns" },
    { ad: "parti_step_seq", tip: "u32", yok: "0xFFFFFFFF", birim: "-" },
    { ad: "fault_mask", tip: "u32", birim: "-" },
    { ad: "quality", tip: "f32", yok: "NaN", aralik: [0, 1], birim: "-" },
    { ad: "model_version", tip: "u16", birim: "-" },
    { ad: "gerilim_v", tip: "f32", birim: "V" },
    { ad: "akim_a", tip: "f32", birim: "A" },
  ],
  yuk_sizinti: [
    { ad: "sensor_id", tip: "u16", birim: "-" },
    { ad: "inst", tip: "u8", birim: "-" },
    { ad: "seq", tip: "u32", birim: "-" },
    { ad: "flags", tip: "u16", birim: "-" },
    { ad: "window_ns", tip: "i64", aralik: [0, 1000000000], birim: "ns" },
    { ad: "t_tx_ns", tip: "i64", yok: "INT64_MIN", birim: "ns" },
    { ad: "t_hazir_ns", tip: "i64", birim: "ns" },
    { ad: "t_teslim_ns", tip: "i64", birim: "ns" },
    { ad: "parti_step_seq", tip: "u32", yok: "0xFFFFFFFF", birim: "-" },
    { ad: "fault_mask", tip: "u32", birim: "-" },
    { ad: "quality", tip: "f32", yok: "NaN", aralik: [0, 1], birim: "-" },
    { ad: "model_version", tip: "u16", birim: "-" },
    { ad: "prob_maskesi", tip: "u8", birim: "-" },
  ],
  yuk_firlatici: [
    { ad: "sensor_id", tip: "u16", birim: "-" },
    { ad: "inst", tip: "u8", birim: "-" },
    { ad: "seq", tip: "u32", birim: "-" },
    { ad: "flags", tip: "u16", birim: "-" },
    { ad: "window_ns", tip: "i64", aralik: [0, 1000000000], birim: "ns" },
    { ad: "t_tx_ns", tip: "i64", yok: "INT64_MIN", birim: "ns" },
    { ad: "t_hazir_ns", tip: "i64", birim: "ns" },
    { ad: "t_teslim_ns", tip: "i64", birim: "ns" },
    { ad: "parti_step_seq", tip: "u32", yok: "0xFFFFFFFF", birim: "-" },
    { ad: "fault_mask", tip: "u32", birim: "-" },
    { ad: "quality", tip: "f32", yok: "NaN", aralik: [0, 1], birim: "-" },
    { ad: "model_version", tip: "u16", birim: "-" },
    { ad: "acil_durdurma", tip: "u8", degerler: { 0: "serbest", 1: "basili" }, birim: "-" },
    { ad: "tup_bitleri", tip: "u8", adet: 2, birim: "-" },
  ],
  yuk_ping360: [
    { ad: "sensor_id", tip: "u16", birim: "-" },
    { ad: "inst", tip: "u8", birim: "-" },
    { ad: "seq", tip: "u32", birim: "-" },
    { ad: "flags", tip: "u16", birim: "-" },
    { ad: "window_ns", tip: "i64", aralik: [0, 1000000000], birim: "ns" },
    { ad: "t_tx_ns", tip: "i64", yok: "INT64_MIN", birim: "ns" },
    { ad: "t_hazir_ns", tip: "i64", birim: "ns" },
    { ad: "t_teslim_ns", tip: "i64", birim: "ns" },
    { ad: "parti_step_seq", tip: "u32", yok: "0xFFFFFFFF", birim: "-" },
    { ad: "fault_mask", tip: "u32", birim: "-" },
    { ad: "quality", tip: "f32", yok: "NaN", aralik: [0, 1], birim: "-" },
    { ad: "model_version", tip: "u16", birim: "-" },
    { ad: "aci_grad", tip: "u16", aralik: [0, 399], birim: "grad" },
    { ad: "n_ornek", tip: "u16", aralik: [1, 1200], birim: "-" },
    { ad: "ornek_periyodu", tip: "u16", aralik: [80, 40000], birim: "25 ns" },
    { ad: "darbe_suresi_us", tip: "u16", birim: "us" },
    { ad: "frekans_khz", tip: "u16", birim: "kHz" },
    { ad: "kazanc", tip: "u8", birim: "-" },
    { ad: "kademe", tip: "u8", degerler: { 1: "sade", 2: "tam" }, birim: "-" },
    { ad: "ses_hizi_mps", tip: "f32", birim: "m/s" },
    { ad: "ornekler", tip: "bayt_dizisi", sayac: "u16", azami: 1200, birim: "bayt" },
  ],
  yuk_ping1d: [
    { ad: "sensor_id", tip: "u16", birim: "-" },
    { ad: "inst", tip: "u8", birim: "-" },
    { ad: "seq", tip: "u32", birim: "-" },
    { ad: "flags", tip: "u16", birim: "-" },
    { ad: "window_ns", tip: "i64", aralik: [0, 1000000000], birim: "ns" },
    { ad: "t_tx_ns", tip: "i64", yok: "INT64_MIN", birim: "ns" },
    { ad: "t_hazir_ns", tip: "i64", birim: "ns" },
    { ad: "t_teslim_ns", tip: "i64", birim: "ns" },
    { ad: "parti_step_seq", tip: "u32", yok: "0xFFFFFFFF", birim: "-" },
    { ad: "fault_mask", tip: "u32", birim: "-" },
    { ad: "quality", tip: "f32", yok: "NaN", aralik: [0, 1], birim: "-" },
    { ad: "model_version", tip: "u16", birim: "-" },
    { ad: "mesafe_m", tip: "f32", yok: "NaN", birim: "m" },
    { ad: "guven", tip: "f32", aralik: [0, 1], birim: "-" },
    { ad: "kademe", tip: "u8", degerler: { 1: "sade", 2: "tam" }, birim: "-" },
    { ad: "profil", tip: "bayt_dizisi", sayac: "u16", azami: 200, birim: "bayt" },
  ],
  yuk_algilama_kayit: [
    { ad: "kaynak", tip: "u8", degerler: { 1: "ping360", 2: "cam_fwd" }, birim: "-" },
    { ad: "inst", tip: "u8", birim: "-" },
    { ad: "batch_seq", tip: "u32", birim: "-" },
    { ad: "run_gen", tip: "u32", birim: "-" },
    { ad: "kayit_indeksi", tip: "u8", aralik: [0, 3], birim: "-" },
    { ad: "a8_major", tip: "u16", birim: "-" },
    { ad: "a8_minor", tip: "u16", birim: "-" },
    { ad: "sonuc", tip: "u8", degerler: { 0: "karta_iletildi", 1: "parti_reddedildi", 2: "gec" }, birim: "-" },
    { ad: "kayit", tip: "bayt_dizisi", sayac: "u16", azami: 512, birim: "bayt" },
  ],
  yuk_algilama_parti: [
    { ad: "kaynak", tip: "u8", degerler: { 1: "ping360", 2: "cam_fwd" }, birim: "-" },
    { ad: "inst", tip: "u8", birim: "-" },
    { ad: "batch_seq", tip: "u32", birim: "-" },
    { ad: "run_gen", tip: "u32", birim: "-" },
    { ad: "kare_sirasi", tip: "u32", birim: "-" },
    { ad: "window_ns", tip: "u32", birim: "ns" },
    { ad: "calib_ver", tip: "u16", birim: "-" },
    { ad: "producer_ver", tip: "u16", birim: "-" },
    { ad: "producer_id", tip: "u8", birim: "-" },
    { ad: "parti_bayraklari", tip: "u16", birim: "-" },
    { ad: "bilesen_durumu", tip: "u8", yok: "255", birim: "-" },
    { ad: "bilesen_kayit_sayisi", tip: "u8", birim: "-" },
    { ad: "eski_nesil", tip: "u8", degerler: { 0: "hayir", 1: "evet" }, birim: "-" },
    { ad: "a2_dusme_nedeni", tip: "u8", yok: "0", birim: "-" },
    { ad: "nihai_durum", tip: "u8", yok: "255", degerler: { 0: "OK", 1: "SKIPPED_BUSY", 2: "LATE", 3: "COMPONENT_DOWN", 4: "FRAME_DROPPED", 5: "FRAME_INVALID", 6: "CALIB_MISMATCH", 7: "CAMERA_OFF", 8: "COMPONENT_ERROR" }, birim: "-" },
    { ad: "ret_nedeni", tip: "u8", degerler: { 0: "yok", 1: "STRUCT", 2: "ECHO", 3: "CALIB", 4: "LINK", 5: "SEQ", 6: "TIME", 7: "KIND", 8: "FLAGS", 9: "VALUE", 10: "UNCERT" }, birim: "-" },
    { ad: "gec", tip: "u8", degerler: { 0: "hayir", 1: "evet" }, birim: "-" },
    { ad: "karta_kayit_sayisi", tip: "u8", birim: "-" },
    { ad: "a1_batch_seq", tip: "u32", yok: "0xFFFFFFFF", birim: "-" },
  ],
  yuk_algilama_sayaclari: [
    { ad: "akislar", tip: "dizi", sayac: "u8", azami: 4, birim: "-", oge: [
        { ad: "kaynak", tip: "u8", degerler: { 1: "ping360", 2: "cam_fwd" }, birim: "-" },
        { ad: "inst", tip: "u8", birim: "-" },
        { ad: "ret", tip: "u64", adet: 10, birim: "-" },
        { ad: "durum", tip: "u64", adet: 9, birim: "-" },
        { ad: "gec", tip: "u64", birim: "-" },
        { ad: "eski_nesil", tip: "u64", birim: "-" },
        { ad: "bilinmeyen_dusme_nedeni", tip: "u64", birim: "-" },
    ] },
  ],
  yuk_kart_baglanti_sayaclari: [
    { ad: "acilis_sayaci", tip: "u32", birim: "-" },
    { ad: "rx_bytes", tip: "u32", birim: "-" },
    { ad: "rx_frames", tip: "u32", birim: "-" },
    { ad: "rx_crc_err", tip: "u32", birim: "-" },
    { ad: "rx_unknown_msgid", tip: "u32", birim: "-" },
    { ad: "rx_iflag", tip: "u32", birim: "-" },
    { ad: "rx_bad_len", tip: "u32", birim: "-" },
    { ad: "rx_wrong_sys", tip: "u32", birim: "-" },
    { ad: "rx_mav1", tip: "u32", birim: "-" },
    { ad: "rx_overflow", tip: "u32", birim: "-" },
    { ad: "rx_resync_bytes", tip: "u32", birim: "-" },
    { ad: "rx_seq_gap", tip: "u32", birim: "-" },
    { ad: "rx_stale_session", tip: "u32", birim: "-" },
    { ad: "rx_outside_session", tip: "u32", birim: "-" },
    { ad: "stamp_coarse", tip: "u32", birim: "-" },
    { ad: "batch_closed", tip: "u32", birim: "-" },
    { ad: "batch_incomplete", tip: "u32", birim: "-" },
    { ad: "batch_skipped", tip: "u32", birim: "-" },
    { ad: "batch_old", tip: "u32", birim: "-" },
    { ad: "batch_dup", tip: "u32", birim: "-" },
    { ad: "batch_merged", tip: "u32", birim: "-" },
    { ad: "member_overflow", tip: "u32", birim: "-" },
    { ad: "ping360_incomplete", tip: "u32", birim: "-" },
    { ad: "ping360_bad", tip: "u32", birim: "-" },
    { ad: "ping360_orphan", tip: "u32", birim: "-" },
    { ad: "cmd_deferred", tip: "u32", birim: "-" },
    { ad: "cmd_dropped", tip: "u32", birim: "-" },
    { ad: "narrow_coalesced", tip: "u32", birim: "-" },
    { ad: "cmd_bad_field", tip: "u32", birim: "-" },
    { ad: "grid_missed", tip: "u32", birim: "-" },
    { ad: "out_fixed", tip: "u32", birim: "-" },
    { ad: "tx_frames", tip: "u32", birim: "-" },
    { ad: "tx_dropped", tip: "u32", birim: "-" },
    { ad: "tx_crit_stall", tip: "u32", birim: "-" },
    { ad: "events_sent", tip: "u32", birim: "-" },
    { ad: "events_resent", tip: "u32", birim: "-" },
    { ad: "sessions", tip: "u32", birim: "-" },
  ],
  yuk_kart_cekirdek_sayaclari: [
    { ad: "acilis_sayaci", tip: "u32", birim: "-" },
    { ad: "sample_rejected", tip: "u32", birim: "-" },
    { ad: "sample_future", tip: "u32", birim: "-" },
    { ad: "sample_conflict", tip: "u32", birim: "-" },
    { ad: "sample_dup_dropped", tip: "u32", birim: "-" },
    { ad: "narrow_from_rejected", tip: "u32", birim: "-" },
    { ad: "bg_bad_input", tip: "u32", birim: "-" },
    { ad: "imu_rejected", tip: "u32", birim: "-" },
    { ad: "imu_gap", tip: "u32", birim: "-" },
    { ad: "bad_input", tip: "u32", birim: "-" },
    { ad: "dup_seq", tip: "u32", birim: "-" },
    { ad: "old_seq", tip: "u32", birim: "-" },
    { ad: "wall_backstep", tip: "u32", birim: "-" },
    { ad: "cmd_rejected", tip: "u32", birim: "-" },
    { ad: "widen_blocked", tip: "u32", birim: "-" },
    { ad: "evres_ignored", tip: "u32", birim: "-" },
    { ad: "event_unknown", tip: "u32", birim: "-" },
    { ad: "det_rejected", tip: "u32", birim: "-" },
    { ad: "ping360_rejected", tip: "u32", birim: "-" },
    { ad: "bg_dropped", tip: "u32", birim: "-" },
    { ad: "bg_slice_mismatch", tip: "u32", birim: "-" },
    { ad: "log_dropped", tip: "u32", birim: "-" },
    { ad: "step_miss", tip: "u32", birim: "-" },
    { ad: "saglik", tip: "u8", birim: "-" },
  ],
  yuk_kayit_durum: [
    { ad: "acik", tip: "u8", degerler: { 0: "hayir", 1: "evet" }, birim: "-" },
    { ad: "kademe", tip: "u8", degerler: { 0: "olaylar", 1: "telemetri", 2: "tam" }, birim: "-" },
    { ad: "kamera_kareleri", tip: "u8", degerler: { 0: "kapali", 1: "pencereli", 2: "surekli" }, birim: "-" },
    { ad: "yazilan_bayt", tip: "u64", birim: "bayt" },
    { ad: "kayit_dusen", tip: "u64", birim: "-" },
    { ad: "tekrar_guvencesi", tip: "u8", degerler: { 0: "yok", 1: "izleme", 2: "fizik", 3: "girdi" }, birim: "-" },
    { ad: "dosya", tip: "metin", sayac: "u16", azami: 64, birim: "bayt" },
  ],
  yuk_surec_durum: [
    { ad: "surecler", tip: "dizi", sayac: "u8", azami: 4, birim: "-", oge: [
        { ad: "surec", tip: "u8", degerler: { 0: "gorsellestirici", 1: "algilama" }, birim: "-" },
        { ad: "durum", tip: "u8", degerler: { 0: "kapali_kademe", 1: "baslatiliyor", 2: "calisiyor", 3: "dustu", 4: "durduruldu" }, birim: "-" },
        { ad: "yeniden_baslatma", tip: "u32", birim: "-" },
        { ad: "son_kare_t_ns", tip: "i64", yok: "INT64_MIN", birim: "ns" },
    ] },
  ],
  yuk_baglanti_durum: [
    { ad: "a1_oturum", tip: "u8", degerler: { 0: "kapali", 1: "el_sikisma", 2: "hazir", 3: "kosuyor", 4: "kopuk", 255: "uygulanamaz" }, birim: "-" },
    { ad: "sess", tip: "u16", birim: "-" },
    { ad: "acilis_sayaci", tip: "u32", birim: "-" },
    { ad: "d_asagi_ort_ns", tip: "i64", yok: "INT64_MIN", birim: "ns" },
    { ad: "emulator", tip: "dizi", sayac: "u8", azami: 2, birim: "-", oge: [
        { ad: "yon", tip: "u8", degerler: { 0: "yer_to_arac", 1: "arac_to_yer" }, birim: "-" },
        { ad: "kuyruk_bayt", tip: "u32", birim: "bayt" },
        { ad: "dusen", tip: "u64", birim: "-" },
        { ad: "model_tutulamadi", tip: "u64", birim: "-" },
    ] },
    { ad: "bu_baglanti_ezilen_durum", tip: "u64", birim: "-" },
    { ad: "bu_baglanti_dusen_dizi", tip: "u64", birim: "-" },
    { ad: "bu_baglanti_olay_kuyrugu", tip: "u32", birim: "-" },
  ],
  yuk_yer_istasyonu_mavlink: [
    { ad: "yon", tip: "u8", degerler: { 0: "yer_to_arac", 1: "arac_to_yer" }, birim: "-" },
    { ad: "cerceve", tip: "bayt_dizisi", sayac: "u16", azami: 280, birim: "bayt" },
  ],
  yuk_kamera_halka_sayaclari: [
    { ad: "kamera_kimligi", tip: "u8", birim: "-" },
    { ad: "dusen_sim", tip: "u64", adet: 8, birim: "-" },
    { ad: "dusen_gorsellestirici", tip: "u64", adet: 7, birim: "-" },
    { ad: "gec_hazir", tip: "u64", birim: "-" },
    { ad: "tuketici_gec", tip: "u64", birim: "-" },
    { ad: "tuketici_koptu", tip: "u64", birim: "-" },
    { ad: "halka_emekliye_ayirma", tip: "u64", birim: "-" },
    { ad: "emekli_halka_yaniti", tip: "u64", birim: "-" },
    { ad: "tuketici_kamera_kapali", tip: "u64", birim: "-" },
    { ad: "tutulan_emekli_halka", tip: "u32", birim: "-" },
    { ad: "eski_nesil", tip: "u64", birim: "-" },
    { ad: "yok_sayilan_birakma", tip: "u64", birim: "-" },
    { ad: "seyreltilen_durum", tip: "u64", birim: "-" },
    { ad: "protokol_hatasi", tip: "u64", birim: "-" },
    { ad: "baglanti", tip: "u64", birim: "-" },
    { ad: "kopma", tip: "u64", birim: "-" },
    { ad: "istek_hazir_adet", tip: "u64", birim: "-" },
    { ad: "istek_hazir_toplam_ns", tip: "u64", birim: "ns" },
    { ad: "istek_hazir_en_buyuk_ns", tip: "u64", birim: "ns" },
    { ad: "istek_hazir_dagilim", tip: "u64", adet: 8, birim: "-" },
  ],
} as const satisfies Record<string, readonly AlanTanimi[]>;

export type YukAdi = keyof typeof YUKLER;

export interface YukSayacGercekZamanMesajRetleriOge {
  /** reddedilen kart çerçevesinin A1 mesaj kimliği */
  mesaj_kimligi: number;
  /** alan_ret_<mesaj> (A1 Tablo 3.11a, Tablo 4.6) */
  alan_ret: bigint;
  /** bag_ret_<mesaj> (A1 Tablo 3.11a, Tablo 4.6) */
  bag_ret: bigint;
}

export interface YukFizikDurumGovdelerOge {
  /** gövde kimliği (04) */
  govde_id: number;
  /** gövde türü Değerler: 0=arac, 1=torpido, 2=hedef, 3=diger. */
  tur: number;
  /** aracın kütle durumu (iskelet §5); araçtan başka gövdede 255 Değerler: 0=iki_torpido, 1=bir_torpido, 2=bos, 255=uygulanamaz. */
  kutle_durumu: number;
  /** [m] referans noktasının NED konumu */
  p_n: number[];
  /** gövdeden dünyaya birim kuaterniyon (w,x,y,z) */
  q_nb: number[];
  /** [m/s, rad/s] genelleştirilmiş hız (u,v,w,p,q,r) gövde çerçevesi, O_b */
  nu: number[];
  /** [m/s^2] O_b'nin NED eylemsiz ivmesi (04) */
  a_n: number[];
}

export interface YukFizikDurumIticilerOge {
  /** itici isteği (04 snapshot u) */
  u: number;
  /** [N] üretilen itki */
  itki_n: number;
  /** [A] itici akımı */
  akim_a: number;
}

export interface YukAtisDurumTuplerOge {
  /** tüp numarası */
  tup: number;
  /** sim'in yetkili durumu: tüpte torpido var mı Değerler: 0=bos, 1=dolu. */
  dolu: number;
  /** sim tarafında emniyet_ac süreli izni geçerli mi (03 §2.7; iskelet §3.2) Değerler: 0=hayir, 1=evet. */
  sureli_izin_gecerli: number;
  /** [ns] süreli iznin kalan süresi (sim zamanı); izin yoksa -1 Yok: -1 → null. */
  izin_kalan_ns: bigint | null;
  /** bu tüpe ait son tek seferlik olayın sonucu Değerler: 0=yok, 1=uygulandi, 2=yineleme, 3=ret_emniyet, 4=ret_suresi_gecmis, 5=ret_diger. */
  son_olay_sonucu: number;
}

export interface YukArizaEtkinArizalarOge {
  /** [bayt] arıza kimliği; senaryodan gelende A5 kimliği, elle gelende E<n> */
  fault_id: string;
  /** arıza sınıfı Değerler: 0=sensor, 1=aktuator, 2=guc, 3=baglanti, 4=protokol, 5=sistem. */
  sinif: number;
  /** [bayt] hedef (sensor_id, itici, bağlantı yönü); A5 arıza tanımındaki gösterim */
  hedef: string;
  /** [bayt] arıza türü adı (A5) */
  tur: string;
  /** arızanın kaynağı Değerler: 0=senaryo, 1=elle. */
  kaynak: number;
  /** uygulandığı fizik adımı */
  baslangic_adimi: bigint;
  /** planlanan bitiş adımı; süresizse 2^64-1 Yok: 2^64-1 → null. */
  bitis_adimi: bigint | null;
}

export interface YukAlgilamaSayaclariAkislarOge {
  /** A8 `source` Değerler: 1=ping360, 2=cam_fwd. */
  kaynak: number;
  /** A8 `inst` */
  inst: number;
  /** ret nedeni başına sayaç; sıra `STRUCT`, `ECHO`, `CALIB`, `LINK`, `SEQ`, `TIME`, `KIND`, `FLAGS`, `VALUE`, `UNCERT` (A8-K30) */
  ret: bigint[];
  /** karta giden durum başına sayaç; sıra A8 §3.4 değerleri 0-8 (A8-K30, K65) */
  durum: bigint[];
  /** geç yanıt sayısı (A8-K59) */
  gec: bigint;
  /** `eski_nesil` ile reddedilen yanıt (A8-K79) */
  eski_nesil: bigint;
  /** A8-K76 tablosunda olmayan A2 kodu (sim hatası) */
  bilinmeyen_dusme_nedeni: bigint;
}

export interface YukSurecDurumSureclerOge {
  /** yardımcı süreç Değerler: 0=gorsellestirici, 1=algilama. */
  surec: number;
  /** süreç durumu Değerler: 0=kapali_kademe, 1=baslatiliyor, 2=calisiyor, 3=dustu, 4=durduruldu. */
  durum: number;
  /** bu çalıştırmadaki yeniden başlatma sayısı */
  yeniden_baslatma: number;
  /** [ns] son kamera karesinin ölçüm zamanı; yoksa INT64_MIN Yok: INT64_MIN → null. */
  son_kare_t_ns: bigint | null;
}

export interface YukBaglantiDurumEmulatorOge {
  /** yön Değerler: 0=yer_to_arac, 1=arac_to_yer. */
  yon: number;
  /** [bayt] emülatör kuyruğundaki bayt */
  kuyruk_bayt: number;
  /** emülatörün düşürdüğü çerçeve */
  dusen: bigint;
  /** 06 §2.11 "model tutulamadı" işareti */
  model_tutulamadi: bigint;
}

export interface YukCalistirmaDurum {
  /** çalıştırma durumu (A3 Tablo 4.7a) Değerler: 0=bos, 1=yuklendi, 2=hazirlaniyor, 3=kosuyor, 4=duraklatildi, 5=bitti, 6=durduruldu, 7=hata, 8=izleme. */
  durum: number;
  /** zaman kipi Değerler: 0=yok, 1=sil, 2=hil_1x, 3=hil_bariyer, 4=tekrar_fizik, 5=tekrar_girdi_sil, 6=tekrar_yeniden_kurma. */
  kip: number;
  /** [bayt] çalıştırma kimliği (A6 §3.1); çalıştırma yoksa boş */
  run_id: string;
  /** tamamlanan fizik adımı sayısı; çalıştırma başında 0 */
  adim_no: bigint;
  /** istenen sim zamanı / duvar saati oranı; 0 = olabildiğince hızlı; HIL kiplerinde 1 */
  hedef_carpan: number;
  /** son 1 s duvar saati penceresinde gerçekleşen oran; hesaplanmadıysa NaN Yok: NaN → null. */
  gerceklesen_carpan: number | null;
  /** 1 = çalıştırma geçerli; bir kez 0 olursa o çalıştırmada 1'e dönmez Değerler: 0=gecersiz, 1=gecerli. */
  gecerli: number;
  /** iskelet §4 işareti: en az bir yanıt kendi L_act anına yetişmedi ya da HIL'de sim adımı kaçırıldı Değerler: 0=hayir, 1=evet. */
  gercek_zamani_kacirdi: number;
  /** geçersizlik neden bitleri (A3 Tablo 4.8); yalnız bit eklenir, silinmez */
  gecerlilik_nedenleri: number;
  /** bu çalıştırmanın kaydının şu anki tekrar oynatma güvencesi (A6 §4.6) Değerler: 0=yok, 1=izleme, 2=fizik, 3=girdi. */
  tekrar_guvencesi: number;
  /** iskelet §6 3B görünüm kademesi Değerler: 0=kapali, 1=sade, 2=tam. */
  kademe_gorunum: number;
  /** iskelet §6 kamera kademesi Değerler: 0=kapali, 1=sade, 2=tam. */
  kademe_kamera: number;
  /** iskelet §6 sonar kademesi Değerler: 0=kapali, 1=sade, 2=tam. */
  kademe_sonar: number;
  /** iskelet §6 kayıt kademesi Değerler: 0=olaylar, 1=telemetri, 2=tam. */
  kademe_kayit: number;
  /** A1 oturum durumu (06 §2.9; tanım A1) Değerler: 0=kapali, 1=el_sikisma, 2=hazir, 3=kosuyor, 4=kopuk, 255=uygulanamaz. */
  a1_oturum: number;
  /** son atanan A3 komut numarası; yoksa 0 */
  son_komut_no: number;
  /** durum revizyonu: sim süreci boyunca 1'den başlayan, uygulanan her çalıştırma kontrolü komutunda ve her durum geçişinde bir artan sayaç (A3-K122) */
  durum_rev: bigint;
}

export interface YukSayacGercekZaman {
  /** gönderilen TF sayısı (A1 Tablo 3.11a) */
  parti_gonderilen: bigint;
  /** uygulama anında yuvası tamamlanmamış ve kaçırılan parti; birleştirilen dahil (A1 Tablo 3.11a) */
  parti_kacirildi: bigint;
  /** bunların sonradan gelen bir yanıtın n_merged'i kapsadığı kısmi (A1 Tablo 3.11a) */
  parti_kacirildi_birlesik: bigint;
  /** bariyerde beklenen yuva sayısı (A1 Tablo 3.11a) */
  bariyer_bekleme: bigint;
  /** [ns] bariyer beklemelerinin toplam süresi (A1 Tablo 3.11a) */
  gercek_zaman_kaymasi_ns: bigint;
  /** [ns] en uzun tek bariyer beklemesi (A1'de `gercek_zaman_kaymasi_ns` adinin "en büyük" değeri; ayrı alan olduğu için sonek aldı) (A1 Tablo 3.11a) */
  gercek_zaman_kaymasi_en_buyuk_ns: bigint;
  /** [ns] TF yazım başlangıcının `W(t_n)`'den en büyük gecikmesi (A1 Tablo 3.11a) */
  tf_gec_yazim_ns: bigint;
  /** sıra dışı yanıt reddi (A1 Tablo 3.11a) */
  yanit_ret_seq_disi: bigint;
  /** yinelenen yanıt reddi (A1 Tablo 3.11a) */
  yanit_ret_yinelenen: bigint;
  /** uygulama anı geçmiş yanıt reddi (A1 Tablo 3.11a) */
  yanit_ret_gec: bigint;
  /** oturum dışı yanıt reddi (A1 Tablo 3.11a) */
  yanit_ret_oturum: bigint;
  /** eşzamansız güvenli çıktının iptal ettiği yuva (A1 Tablo 3.11a) */
  yanit_iptal_esz: bigint;
  /** uygulanan eşzamansız güvenli çıktı (A1 Tablo 3.11a) */
  esz_guvenli_uygulanan: bigint;
  /** kullanım süresi dolup nötre geçiş (A1 Tablo 3.11a) */
  komut_suresi_doldu: bigint;
  /** yeni olay (A1 Tablo 3.11a) */
  olay_alinan: bigint;
  /** geç uygulanmaya çalışılan olay (A1 Tablo 3.11a) */
  olay_gec: bigint;
  /** yuvası n_events tamamlanmadan kapanan olay (A1 Tablo 3.11a) */
  olay_eksik: bigint;
  /** sonuç kodu başına olay sayacı; sıra `APPLIED`, `DUPLICATE`, `REJ_EXPIRED`, `REJ_SAFE`, `REJ_TUBE_EMPTY`, `REJ_SESSION` (olay_sonuc_<kod>) (A1 Tablo 3.11a) */
  olay_sonuc: bigint[];
  /** sim'de birleştirilen daraltıcı (A1 Tablo 3.11a) */
  komut_birlestirildi: bigint;
  /** sonraki pencereye kalan toplu komut (A1 Tablo 3.11a) */
  komut_ertelendi: bigint;
  /** oturum bittiği için sonucu gelmeyen komut (A1 Tablo 3.11a) */
  komut_sonucsuz: bigint;
  /** atilan Ping360 satırı (A1 Tablo 3.11a) */
  ping360_seyreltildi: bigint;
  /** t_hazir'dan teslim partisine en büyük gecikme (periyot) (A1 Tablo 3.11a) */
  ping360_gecikme_en_buyuk: bigint;
  /** kritik ve atanmis üyelerin B_pencere'yi astigi pencere (A1 Tablo 3.11a) */
  pencere_tasma: bigint;
  /** sim alıcısının çerçeve hatası (A1 Tablo 3.11a) */
  rx_crc_err: bigint;
  /** sim alıcısının çerçeve dışı atilan baytı (A1 Tablo 3.11a) */
  rx_resync_bytes: bigint;
  /** sim alıcısının bilinmeyen kimlikli çerçevesi (A1 Tablo 3.11a) */
  rx_unknown_msgid: bigint;
  /** sim alıcısının len hatası (A1 Tablo 3.11a) */
  rx_bad_len: bigint;
  /** sim alıcısının sysid/compid hatası (A1 Tablo 3.11a) */
  rx_wrong_sys: bigint;
  /** sim alıcısının incompat_flags hatası (A1 Tablo 3.11a) */
  rx_iflag: bigint;
  /** sim alıcısının MAVLink 1 çerçevesi (A1 Tablo 3.11a) */
  rx_mav1: bigint;
  /** sim alıcısının başlık seq boşluğu (A1 Tablo 3.11a) */
  rx_seq_gap: bigint;
  /** kurulan oturum (A1 Tablo 3.11a) */
  oturum_kurulan: bigint;
  /** kopan oturum (A1 Tablo 3.11a) */
  oturum_kopma: bigint;
  /** kartın beklenmeyen sıfırlanması (A1 Tablo 3.11a) */
  beklenmeyen_sifirlama: bigint;
  /** çalıştırma zamanlama bakımından geçersiz (A1-K79) (A1 Tablo 3.11a) */
  calistirma_gecersiz: number;
  /** A1-K79 neden bitleri; bit numaralari A3 Tablo 4.8 (0-11) (A1 Tablo 3.11a) */
  neden_bitleri: number;
  /** yuvayı tamamlayan çerçevenin D_n'den sonra alınması (A1-K108) (A1 Tablo 3.11a) */
  son_tarih_kacirildi: bigint;
  /** sim'in fizik adımını planlanan duvar anından sonra hesaplaması (A1-K108) (A1 Tablo 3.11a) */
  sim_gecikmesi: bigint;
  /** out_seq'i artmayan sürekli çıktı (A1 Tablo 3.11a) */
  yanit_ret_eski_uretim: bigint;
  /** safety_rev damgasi güncel olmadığı için REJ_SAFE alan izin acan olay (A1 Tablo 3.11a) */
  olay_rev_ret: bigint;
  /** olay sonucu kuyruğunun en büyük doluluğu (A1 Tablo 3.11a) */
  sonuc_kuyrugu_en_buyuk: bigint;
  /** B_krit yüzünden kayan kritik üye (A1-K113) (A1 Tablo 3.11a) */
  kritik_erteleme: bigint;
  /** en büyük kritik erteleme (parti) (A1 Tablo 3.11a) */
  kritik_erteleme_en_buyuk: bigint;
  /** gelmeyen IDA_CALL_TRACE (A1-K95) (A1 Tablo 3.11a) */
  cagri_izi_eksik: bigint;
  /** A1-K104 ile reddedilen yapılandırma; ayrıntı `yapilandirma_reddedildi` olayında (A1 Tablo 3.11a) */
  yapilandirma_ret: bigint;
  /** CALL_CONFLICT bayrakli parti (A1-K117) (A1 Tablo 3.11a) */
  cagri_cakismasi: bigint;
  /** yer olmadığı için yanıtlanmayan saklı sonuç talebi (A1-K111) (A1 Tablo 3.11a) */
  sonuc_talebi_atildi: bigint;
  /** kova yüzünden bekleyen komut (A1 Tablo 3.11a) */
  komut_kovasi_bekleme: bigint;
  /** emniyet revizyonu doygunluğa ulaştı (A1 Tablo 3.11a; iskelet §3.2) Değerler: 0=hayir, 1=evet. */
  emniyet_rev_doydu: number;
  /** mesaj başına kart çerçevesi retleri; yalnız sıfırdan büyük olanlar, mesaj kimliği sırasıyla */
  mesaj_retleri: YukSayacGercekZamanMesajRetleriOge[];
  /** sim fizik adımının son tarihinden geç bitmesi (04 §2.11) */
  adim_son_tarih_ihlali: bigint;
  /** gecikmesi en az bir fizik adımı olan adım (04 §2.11) */
  kacirilan_adim: bigint;
  /** [ns] en büyük fizik adımı gecikmesi (04 §2.11) */
  azami_adim_gecikmesi_ns: bigint;
  /** 05 §4 */
  sonar_gec: bigint;
  /** 05 §4 */
  sonar_kuyruk_dolu: bigint;
  /** 05 §4 teslim kuyruğu taşması */
  sensor_kuyruk_tasma: bigint;
  /** A5 Tablo 3.10-A donma arıza sayacı */
  donma_onceki_cikti_yok: bigint;
  /** A5-K63 atama çelişkisi sayısı */
  atama_celiskisi: bigint;
  /** senaryonun reddedilen gevşeten atamasi (W1-uyum U3) */
  gevsetme_reddedildi: bigint;
  /** emülatörden çıkışta revizyonu değişmiş olduğu için atilan genişletici yer istasyonu komutu (A3-K101) */
  bayat_yetki: bigint;
  /** A1 gönderim kuyruğunda, tele yazılmadan önce revizyonu değişmiş olduğu için gönderilmeyen genişletici komut (A1 Tablo 3.11a; A1-K129) */
  komut_bayat_yetki: bigint;
  /** dünya -> A3 halka tamponunda düşen ileti (A3-K38) */
  a3_halka_tasma: bigint;
  /** dünya -> kayıt halka tamponunda düşen ileti (A6-K30) */
  kayit_dusen: bigint;
}

export interface YukOlay {
  /** olay türü kodu (x_ida_olay_turleri) */
  tur: number;
  /** olayın önemi Değerler: 0=bilgi, 1=uyari, 2=hata, 3=emniyet. */
  onem: number;
  /** olayı üreten taraf Değerler: 0=sim, 1=a3, 2=kart, 3=senaryo, 4=kayit, 5=surec, 6=yer_istasyonu. */
  kaynak: number;
  /** olayın bağlandığı fizik adımı; adımsız olayda 2^64-1 Yok: 2^64-1 → null. */
  adim_no: bigint | null;
  /** ilgili A3 komut numarası; yoksa 0 */
  komut_no: number;
  /** ilgili istemci; yoksa 0 */
  istemci_id: number;
  /** [bayt] UTF-8 JSON nesnesi; şeması olay türüne göre $defs içindedir */
  ayrinti: string;
}

export interface YukFizikDurum {
  /** snapshot'ın fizik adımı */
  adim_no: bigint;
  /** yer gerçeği gövde durumları */
  govdeler: YukFizikDurumGovdelerOge[];
  /** itici durumları, itici sırasıyla */
  iticiler: YukFizikDurumIticilerOge[];
  /** [V] bara gerilimi */
  bara_gerilimi_v: number;
  /** [A] batarya paket akımı */
  paket_akimi_a: number;
  /** şarj durumu */
  soc: number;
}

export interface YukKartDurum {
  /** [ns] kestirimin ait olduğu ölçüm zamanı (kartın bildirdiği; A1'den beklenen) */
  kart_t_ns: bigint;
  /** kartın açılış sayacı (iskelet §3.2; A1'den beklenen); bilinmiyorsa 0 */
  acilis_sayaci: number;
  /** otopilot kipi (A1 `IDA_STATUS.mode` = A4 `status.mode`); değer adları A4'te; bilinmiyorsa 255 Yok: 255 → null. */
  kip: number | null;
  /** otopilot arm durumu (atış kontrol silahlandırması değildir) Değerler: 0=hayir, 1=evet, 255=bilinmiyor. */
  silahli: number;
  /** sağlık bitleri (A1 `IDAW_HEALTH`): bit0 core_ok, bit1 data_ok, bit2 imu_rejected, bit3 imu_lag, bit4 input_dropped, bit5 narrowed, bit6 sample_conflict, bit7 bg_bad_input */
  saglik: number;
  /** kestirim geçerli mi Değerler: 0=hayir, 1=evet, 255=bilinmiyor. */
  kestirim_gecerli: number;
  /** [m] kestirilen NED konum Yok: NaN → null. */
  p_n: (number | null)[];
  /** [m/s] kestirilen NED hız Yok: NaN → null. */
  v_n: (number | null)[];
  /** kestirilen yönelim (w,x,y,z) Yok: NaN → null. */
  q_nb: (number | null)[];
  /** [m] kestirilen derinlik Yok: NaN → null. */
  derinlik_m: number | null;
  /** kartın bildirdiği şarj durumu Yok: NaN → null. */
  batarya_soc: number | null;
  /** [us] tanısal: kartın son adım süresi; bilinmiyorsa 0xFFFFFFFF Yok: 0xFFFFFFFF → null. */
  adim_suresi_us: number | null;
}

export interface YukAtisDurum {
  /** kartın bildirdiği emniyet zinciri durumu (A1 `IDA_FC_STATUS.state` = A4 `fc.state`); değer adları ve anlamları A4 §3.16; bilinmiyorsa 255 Değerler: 0=SAFE, 1=ARMED, 2=PERMIT_OPEN, 3=FIRE_PENDING, 4=TEST, 5=FAULT_LATCHED. Yok: 255 → null. */
  fc_durumu: number | null;
  /** kartın bildirdiği emniyet engeli bitleri (A1 `IDA_FC_STATUS.inhibit_mask` = A4 `fc.inhibit_mask`); bit adları ve anlamları A4 §3.16; bilinmiyorsa 0xFFFFFFFF Yok: 0xFFFFFFFF → null. */
  engel_maskesi: number | null;
  /** A1 `IDA_FC_STATUS.last_event_id`; yoksa 0 Yok: 0 → null. */
  kart_son_olay_kimligi: bigint | null;
  /** fırlatıcı tüplerinin durumu */
  tupler: YukAtisDurumTuplerOge[];
}

export interface YukEmniyetGirdiler {
  /** sanal acil durdurma yetkili durumu (tek) Değerler: 0=serbest, 1=basili. */
  acil_durdurma: number;
  /** tüp başına sanal silahlandırma fişi yetkili durumu; dizin 0 = tüp `f1`, dizin 1 = tüp `f2` (A5) Değerler: 0=cikarilmis, 1=takili. */
  silahlandirma_fisi: number[];
  /** tüp başına sanal emniyet pimi yetkili durumu; dizin 0 = tüp `f1`, dizin 1 = tüp `f2` (A5) Değerler: 0=cikarilmis, 1=takili. */
  emniyet_pimi: number[];
  /** son değişimin uygulandığı fizik adımı; hiç değişmediyse 2^64-1 Yok: 2^64-1 → null. */
  son_degisim_adimi: bigint | null;
  /** son değişimin kaynağı Değerler: 0=baslangic, 1=senaryo, 2=operator. */
  son_degisim_kaynagi: number;
  /** kartın bildirdiği etkin değer: A1 `IDA_FC_STATUS.inhibit_mask` bit 4 `ESTOP` (A4 §3.16; 1 = basili); `IDA_FC_STATUS` alınmadıysa 255 Yok: 255 → null. */
  kart_acil_durdurma: number | null;
  /** kartın bildirdiği etkin değer: `inhibit_mask` bit 5+i `PLUG_T<i>` kuruluysa 0 (cikarilmis), değilse 1 (takili) (A4 §3.16); dizin 0 = tüp `f1`, dizin 1 = tüp `f2` (A5); `IDA_FC_STATUS` alınmadıysa 255 Yok: 255 → null. */
  kart_silahlandirma_fisi: (number | null)[];
  /** kartın bildirdiği etkin değer: `inhibit_mask` bit 7+i `PIN_T<i>` kuruluysa 1 (takili), değilse 0 (cikarilmis) (A4 §3.16); dizin 0 = tüp `f1`, dizin 1 = tüp `f2` (A5); `IDA_FC_STATUS` alınmadıysa 255 Yok: 255 → null. */
  kart_emniyet_pimi: (number | null)[];
  /** yetkili durum ile kartın bildirdiği etkin değer arasındaki anlık fark bitleri: bit0 acil, bit1 fiş f1, bit2 fiş f2, bit3 pim f1, bit4 pim f2; kartın değeri bilinmiyorsa bit 0 */
  geri_okuma_farki: number;
  /** emniyet revizyonu (iskelet §3.2; A3-K117): 1'den başlar, sıfırlanmaz, `0xFFFFFFFF`'te doyar */
  emniyet_rev: number;
  /** revizyon doydu; bundan sonra bütün gevşetmeler reddedilir Değerler: 0=hayir, 1=evet. */
  emniyet_rev_doydu: number;
}

export interface YukArizaEtkin {
  /** etkin arızalar */
  arizalar: YukArizaEtkinArizalarOge[];
}

export interface YukImu {
  /** A5 `sensorler[]` kimliği */
  sensor_id: number;
  /** aynı türden sensörün örnek numarası */
  inst: number;
  /** sensörün kendi örnek sayacı; kayıp boşluk bırakır (05 MeasurementRecord.seq) */
  seq: number;
  /** bit0 geçerli, bit1 doymuş, bit2 sensor_arizasi; bit3-15 türe özgü (A1/A4 tanımlar) */
  flags: number;
  /** [ns] aralık boyunca toplanan ölçümde aralık süresi; anlık ölçümde 0 */
  window_ns: bigint;
  /** [ns] akustik ölçümde darbe gönderim anı (iskelet §3.1); yoksa INT64_MIN Yok: INT64_MIN → null. */
  t_tx_ns: bigint | null;
  /** [ns] modellenmiş veri hazır anı (05) */
  t_hazir_ns: bigint;
  /** [ns] modellenmiş teslim anı (05); fiilî teslim parti_step_seq alanındadır */
  t_teslim_ns: bigint;
  /** kaydın fiilen girdiği A1 partisinin sıra numarası; teslim edilmediyse 0xFFFFFFFF Yok: 0xFFFFFFFF → null. */
  parti_step_seq: number | null;
  /** o an bu kayda etkiyen arıza türleri bit maskesi (05 §2.7 bileşim sırası numarası = bit) */
  fault_mask: number;
  /** normalize kalite [0,1]; yoksa NaN Yok: NaN → null. */
  quality: number | null;
  /** sensör modelinin sürümü (05 §2.6) */
  model_version: number;
  /** [m/s^2] özgül kuvvet, sensör çerçevesi */
  ivme_mps2: number[];
  /** [rad/s] açısal hız, sensör çerçevesi */
  acisal_hiz_radps: number[];
  /** [degC] sensör sıcaklığı Yok: NaN → null. */
  sicaklik_c: number | null;
}

export interface YukManyetometre {
  /** A5 `sensorler[]` kimliği */
  sensor_id: number;
  /** aynı türden sensörün örnek numarası */
  inst: number;
  /** sensörün kendi örnek sayacı; kayıp boşluk bırakır (05 MeasurementRecord.seq) */
  seq: number;
  /** bit0 geçerli, bit1 doymuş, bit2 sensor_arizasi; bit3-15 türe özgü (A1/A4 tanımlar) */
  flags: number;
  /** [ns] aralık boyunca toplanan ölçümde aralık süresi; anlık ölçümde 0 */
  window_ns: bigint;
  /** [ns] akustik ölçümde darbe gönderim anı (iskelet §3.1); yoksa INT64_MIN Yok: INT64_MIN → null. */
  t_tx_ns: bigint | null;
  /** [ns] modellenmiş veri hazır anı (05) */
  t_hazir_ns: bigint;
  /** [ns] modellenmiş teslim anı (05); fiilî teslim parti_step_seq alanındadır */
  t_teslim_ns: bigint;
  /** kaydın fiilen girdiği A1 partisinin sıra numarası; teslim edilmediyse 0xFFFFFFFF Yok: 0xFFFFFFFF → null. */
  parti_step_seq: number | null;
  /** o an bu kayda etkiyen arıza türleri bit maskesi (05 §2.7 bileşim sırası numarası = bit) */
  fault_mask: number;
  /** normalize kalite [0,1]; yoksa NaN Yok: NaN → null. */
  quality: number | null;
  /** sensör modelinin sürümü (05 §2.6) */
  model_version: number;
  /** [T] manyetik alan, sensör çerçevesi */
  alan_t: number[];
  /** [degC] sensör sıcaklığı Yok: NaN → null. */
  sicaklik_c: number | null;
}

export interface YukBasinc {
  /** A5 `sensorler[]` kimliği */
  sensor_id: number;
  /** aynı türden sensörün örnek numarası */
  inst: number;
  /** sensörün kendi örnek sayacı; kayıp boşluk bırakır (05 MeasurementRecord.seq) */
  seq: number;
  /** bit0 geçerli, bit1 doymuş, bit2 sensor_arizasi; bit3-15 türe özgü (A1/A4 tanımlar) */
  flags: number;
  /** [ns] aralık boyunca toplanan ölçümde aralık süresi; anlık ölçümde 0 */
  window_ns: bigint;
  /** [ns] akustik ölçümde darbe gönderim anı (iskelet §3.1); yoksa INT64_MIN Yok: INT64_MIN → null. */
  t_tx_ns: bigint | null;
  /** [ns] modellenmiş veri hazır anı (05) */
  t_hazir_ns: bigint;
  /** [ns] modellenmiş teslim anı (05); fiilî teslim parti_step_seq alanındadır */
  t_teslim_ns: bigint;
  /** kaydın fiilen girdiği A1 partisinin sıra numarası; teslim edilmediyse 0xFFFFFFFF Yok: 0xFFFFFFFF → null. */
  parti_step_seq: number | null;
  /** o an bu kayda etkiyen arıza türleri bit maskesi (05 §2.7 bileşim sırası numarası = bit) */
  fault_mask: number;
  /** normalize kalite [0,1]; yoksa NaN Yok: NaN → null. */
  quality: number | null;
  /** sensör modelinin sürümü (05 §2.6) */
  model_version: number;
  /** [Pa] mutlak basınç */
  basinc_pa: number;
  /** [degC] su sıcaklığı Yok: NaN → null. */
  sicaklik_c: number | null;
}

export interface YukGnss {
  /** A5 `sensorler[]` kimliği */
  sensor_id: number;
  /** aynı türden sensörün örnek numarası */
  inst: number;
  /** sensörün kendi örnek sayacı; kayıp boşluk bırakır (05 MeasurementRecord.seq) */
  seq: number;
  /** bit0 geçerli, bit1 doymuş, bit2 sensor_arizasi; bit3-15 türe özgü (A1/A4 tanımlar) */
  flags: number;
  /** [ns] aralık boyunca toplanan ölçümde aralık süresi; anlık ölçümde 0 */
  window_ns: bigint;
  /** [ns] akustik ölçümde darbe gönderim anı (iskelet §3.1); yoksa INT64_MIN Yok: INT64_MIN → null. */
  t_tx_ns: bigint | null;
  /** [ns] modellenmiş veri hazır anı (05) */
  t_hazir_ns: bigint;
  /** [ns] modellenmiş teslim anı (05); fiilî teslim parti_step_seq alanındadır */
  t_teslim_ns: bigint;
  /** kaydın fiilen girdiği A1 partisinin sıra numarası; teslim edilmediyse 0xFFFFFFFF Yok: 0xFFFFFFFF → null. */
  parti_step_seq: number | null;
  /** o an bu kayda etkiyen arıza türleri bit maskesi (05 §2.7 bileşim sırası numarası = bit) */
  fault_mask: number;
  /** normalize kalite [0,1]; yoksa NaN Yok: NaN → null. */
  quality: number | null;
  /** sensör modelinin sürümü (05 §2.6) */
  model_version: number;
  /** [rad] enlem */
  enlem_rad: number;
  /** [rad] boylam */
  boylam_rad: number;
  /** [m] elipsoit yüksekliği */
  yukseklik_m: number;
  /** [m/s] NED hız */
  hiz_ned_mps: number[];
  /** [m] yatay konum belirsizliği (05) */
  eph_m: number;
  /** [m] düşey konum belirsizliği (05) */
  epv_m: number;
  /** çözüm türü Değerler: 0=yok, 2=iki_boyut, 3=uc_boyut. */
  fix_turu: number;
  /** kullanılan uydu sayısı */
  uydu_sayisi: number;
}

export interface YukDvl {
  /** A5 `sensorler[]` kimliği */
  sensor_id: number;
  /** aynı türden sensörün örnek numarası */
  inst: number;
  /** sensörün kendi örnek sayacı; kayıp boşluk bırakır (05 MeasurementRecord.seq) */
  seq: number;
  /** bit0 geçerli, bit1 doymuş, bit2 sensor_arizasi; bit3-15 türe özgü (A1/A4 tanımlar) */
  flags: number;
  /** [ns] aralık boyunca toplanan ölçümde aralık süresi; anlık ölçümde 0 */
  window_ns: bigint;
  /** [ns] akustik ölçümde darbe gönderim anı (iskelet §3.1); yoksa INT64_MIN Yok: INT64_MIN → null. */
  t_tx_ns: bigint | null;
  /** [ns] modellenmiş veri hazır anı (05) */
  t_hazir_ns: bigint;
  /** [ns] modellenmiş teslim anı (05); fiilî teslim parti_step_seq alanındadır */
  t_teslim_ns: bigint;
  /** kaydın fiilen girdiği A1 partisinin sıra numarası; teslim edilmediyse 0xFFFFFFFF Yok: 0xFFFFFFFF → null. */
  parti_step_seq: number | null;
  /** o an bu kayda etkiyen arıza türleri bit maskesi (05 §2.7 bileşim sırası numarası = bit) */
  fault_mask: number;
  /** normalize kalite [0,1]; yoksa NaN Yok: NaN → null. */
  quality: number | null;
  /** sensör modelinin sürümü (05 §2.6) */
  model_version: number;
  /** [m/s] dibe göre hız, sensör çerçevesi Yok: NaN → null. */
  hiz_sensor_mps: (number | null)[];
  /** [m] dibe yükseklik Yok: NaN → null. */
  irtifa_m: number | null;
  /** [m/s] liyakat sayısı (05) Yok: NaN → null. */
  fom_mps: number | null;
  /** bit0-3: dört ışının geçerliliği */
  isin_gecerli: number;
}

export interface YukBatarya {
  /** A5 `sensorler[]` kimliği */
  sensor_id: number;
  /** aynı türden sensörün örnek numarası */
  inst: number;
  /** sensörün kendi örnek sayacı; kayıp boşluk bırakır (05 MeasurementRecord.seq) */
  seq: number;
  /** bit0 geçerli, bit1 doymuş, bit2 sensor_arizasi; bit3-15 türe özgü (A1/A4 tanımlar) */
  flags: number;
  /** [ns] aralık boyunca toplanan ölçümde aralık süresi; anlık ölçümde 0 */
  window_ns: bigint;
  /** [ns] akustik ölçümde darbe gönderim anı (iskelet §3.1); yoksa INT64_MIN Yok: INT64_MIN → null. */
  t_tx_ns: bigint | null;
  /** [ns] modellenmiş veri hazır anı (05) */
  t_hazir_ns: bigint;
  /** [ns] modellenmiş teslim anı (05); fiilî teslim parti_step_seq alanındadır */
  t_teslim_ns: bigint;
  /** kaydın fiilen girdiği A1 partisinin sıra numarası; teslim edilmediyse 0xFFFFFFFF Yok: 0xFFFFFFFF → null. */
  parti_step_seq: number | null;
  /** o an bu kayda etkiyen arıza türleri bit maskesi (05 §2.7 bileşim sırası numarası = bit) */
  fault_mask: number;
  /** normalize kalite [0,1]; yoksa NaN Yok: NaN → null. */
  quality: number | null;
  /** sensör modelinin sürümü (05 §2.6) */
  model_version: number;
  /** [V] ölçülen gerilim */
  gerilim_v: number;
  /** [A] ölçülen akım */
  akim_a: number;
}

export interface YukSizinti {
  /** A5 `sensorler[]` kimliği */
  sensor_id: number;
  /** aynı türden sensörün örnek numarası */
  inst: number;
  /** sensörün kendi örnek sayacı; kayıp boşluk bırakır (05 MeasurementRecord.seq) */
  seq: number;
  /** bit0 geçerli, bit1 doymuş, bit2 sensor_arizasi; bit3-15 türe özgü (A1/A4 tanımlar) */
  flags: number;
  /** [ns] aralık boyunca toplanan ölçümde aralık süresi; anlık ölçümde 0 */
  window_ns: bigint;
  /** [ns] akustik ölçümde darbe gönderim anı (iskelet §3.1); yoksa INT64_MIN Yok: INT64_MIN → null. */
  t_tx_ns: bigint | null;
  /** [ns] modellenmiş veri hazır anı (05) */
  t_hazir_ns: bigint;
  /** [ns] modellenmiş teslim anı (05); fiilî teslim parti_step_seq alanındadır */
  t_teslim_ns: bigint;
  /** kaydın fiilen girdiği A1 partisinin sıra numarası; teslim edilmediyse 0xFFFFFFFF Yok: 0xFFFFFFFF → null. */
  parti_step_seq: number | null;
  /** o an bu kayda etkiyen arıza türleri bit maskesi (05 §2.7 bileşim sırası numarası = bit) */
  fault_mask: number;
  /** normalize kalite [0,1]; yoksa NaN Yok: NaN → null. */
  quality: number | null;
  /** sensör modelinin sürümü (05 §2.6) */
  model_version: number;
  /** sızıntı algılayan prob bitleri */
  prob_maskesi: number;
}

export interface YukFirlatici {
  /** A5 `sensorler[]` kimliği */
  sensor_id: number;
  /** aynı türden sensörün örnek numarası */
  inst: number;
  /** sensörün kendi örnek sayacı; kayıp boşluk bırakır (05 MeasurementRecord.seq) */
  seq: number;
  /** bit0 geçerli, bit1 doymuş, bit2 sensor_arizasi; bit3-15 türe özgü (A1/A4 tanımlar) */
  flags: number;
  /** [ns] aralık boyunca toplanan ölçümde aralık süresi; anlık ölçümde 0 */
  window_ns: bigint;
  /** [ns] akustik ölçümde darbe gönderim anı (iskelet §3.1); yoksa INT64_MIN Yok: INT64_MIN → null. */
  t_tx_ns: bigint | null;
  /** [ns] modellenmiş veri hazır anı (05) */
  t_hazir_ns: bigint;
  /** [ns] modellenmiş teslim anı (05); fiilî teslim parti_step_seq alanındadır */
  t_teslim_ns: bigint;
  /** kaydın fiilen girdiği A1 partisinin sıra numarası; teslim edilmediyse 0xFFFFFFFF Yok: 0xFFFFFFFF → null. */
  parti_step_seq: number | null;
  /** o an bu kayda etkiyen arıza türleri bit maskesi (05 §2.7 bileşim sırası numarası = bit) */
  fault_mask: number;
  /** normalize kalite [0,1]; yoksa NaN Yok: NaN → null. */
  quality: number | null;
  /** sensör modelinin sürümü (05 §2.6) */
  model_version: number;
  /** kartın okuyacağı sanal acil durdurma (`IDA_SAFETY.estop`; arızayla bozulabilir) Değerler: 0=serbest, 1=basili. */
  acil_durdurma: number;
  /** kartın okuyacağı tüp başına bitler (`IDA_SAFETY.tube[i]`): bit0 `LOADED`, bit1 `PLUG_IN`, bit2 `PIN_OUT`, bit3 `PERMIT_OPEN` (A4 `IDA_TUBE_*`); dizin 0 = tüp `f1`, dizin 1 = tüp `f2` (A5) */
  tup_bitleri: number[];
}

export interface YukPing360 {
  /** A5 `sensorler[]` kimliği */
  sensor_id: number;
  /** aynı türden sensörün örnek numarası */
  inst: number;
  /** sensörün kendi örnek sayacı; kayıp boşluk bırakır (05 MeasurementRecord.seq) */
  seq: number;
  /** bit0 geçerli, bit1 doymuş, bit2 sensor_arizasi; bit3-15 türe özgü (A1/A4 tanımlar) */
  flags: number;
  /** [ns] aralık boyunca toplanan ölçümde aralık süresi; anlık ölçümde 0 */
  window_ns: bigint;
  /** [ns] akustik ölçümde darbe gönderim anı (iskelet §3.1); yoksa INT64_MIN Yok: INT64_MIN → null. */
  t_tx_ns: bigint | null;
  /** [ns] modellenmiş veri hazır anı (05) */
  t_hazir_ns: bigint;
  /** [ns] modellenmiş teslim anı (05); fiilî teslim parti_step_seq alanındadır */
  t_teslim_ns: bigint;
  /** kaydın fiilen girdiği A1 partisinin sıra numarası; teslim edilmediyse 0xFFFFFFFF Yok: 0xFFFFFFFF → null. */
  parti_step_seq: number | null;
  /** o an bu kayda etkiyen arıza türleri bit maskesi (05 §2.7 bileşim sırası numarası = bit) */
  fault_mask: number;
  /** normalize kalite [0,1]; yoksa NaN Yok: NaN → null. */
  quality: number | null;
  /** sensör modelinin sürümü (05 §2.6) */
  model_version: number;
  /** [grad] kafa açısı, cihaz biriminde (A1 `angle_grad`); radyan = 2*pi*aci_grad/400 (A3 §3.4) */
  aci_grad: number;
  /** örnek sayısı (A1 `n_samples`) */
  n_ornek: number;
  /** [25 ns] örnek periyodu, 25 ns biriminde (A1 `sample_period`) */
  ornek_periyodu: number;
  /** [us] darbe süresi (A1 `tx_duration_us`) */
  darbe_suresi_us: number;
  /** [kHz] darbe frekansı (A1 `tx_freq_khz`) */
  frekans_khz: number;
  /** cihaz kazanç kodu (A1 `gain`) */
  kazanc: number;
  /** satırı üreten sonar kademesi Değerler: 1=sade, 2=tam. */
  kademe: number;
  /** [m/s] sim'in nominal ses hızı; yalnız panelin menzil dönüşümü için (A3 §3.4); kartın parametresi farklı olabilir */
  ses_hizi_mps: number;
  /** [bayt] kutu şiddetleri, 8 bit (A1 `data[n_samples]`) */
  ornekler: Uint8Array;
}

export interface YukPing1d {
  /** A5 `sensorler[]` kimliği */
  sensor_id: number;
  /** aynı türden sensörün örnek numarası */
  inst: number;
  /** sensörün kendi örnek sayacı; kayıp boşluk bırakır (05 MeasurementRecord.seq) */
  seq: number;
  /** bit0 geçerli, bit1 doymuş, bit2 sensor_arizasi; bit3-15 türe özgü (A1/A4 tanımlar) */
  flags: number;
  /** [ns] aralık boyunca toplanan ölçümde aralık süresi; anlık ölçümde 0 */
  window_ns: bigint;
  /** [ns] akustik ölçümde darbe gönderim anı (iskelet §3.1); yoksa INT64_MIN Yok: INT64_MIN → null. */
  t_tx_ns: bigint | null;
  /** [ns] modellenmiş veri hazır anı (05) */
  t_hazir_ns: bigint;
  /** [ns] modellenmiş teslim anı (05); fiilî teslim parti_step_seq alanındadır */
  t_teslim_ns: bigint;
  /** kaydın fiilen girdiği A1 partisinin sıra numarası; teslim edilmediyse 0xFFFFFFFF Yok: 0xFFFFFFFF → null. */
  parti_step_seq: number | null;
  /** o an bu kayda etkiyen arıza türleri bit maskesi (05 §2.7 bileşim sırası numarası = bit) */
  fault_mask: number;
  /** normalize kalite [0,1]; yoksa NaN Yok: NaN → null. */
  quality: number | null;
  /** sensör modelinin sürümü (05 §2.6) */
  model_version: number;
  /** [m] dibe mesafe; yoksa NaN Yok: NaN → null. */
  mesafe_m: number | null;
  /** güven */
  guven: number;
  /** sonar kademesi Değerler: 1=sade, 2=tam. */
  kademe: number;
  /** [bayt] profil örnekleri, 8 bit */
  profil: Uint8Array;
}

export interface YukAlgilamaKayit {
  /** A8 `source` Değerler: 1=ping360, 2=cam_fwd. */
  kaynak: number;
  /** A8 `inst` */
  inst: number;
  /** kaydın ait olduğu partinin `batch_seq`'i (kamerada `kare_sirasi`; A8-K16, K62) */
  batch_seq: number;
  /** partinin çalıştırma nesli (A8 `run_gen` = A2 `calisma_nesli`, A2-K80) */
  run_gen: number;
  /** partideki sıra, 0'dan */
  kayit_indeksi: number;
  /** A8 şema ana sürümü (`IDA_DET_SCHEMA_MAJOR`) */
  a8_major: number;
  /** A8 şema yan sürümü (`IDA_DET_SCHEMA_MINOR`) */
  a8_minor: number;
  /** kaydın akıbeti Değerler: 0=karta_iletildi, 1=parti_reddedildi, 2=gec. */
  sonuc: number;
  /** [bayt] algılama kaydının A8 §3.2'deki tel düzeni, değiştirilmeden; içindeki `class_id` kodlamasi A5-K113'un kapalı tablosudur */
  kayit: Uint8Array;
}

export interface YukAlgilamaParti {
  /** A8 `source` Değerler: 1=ping360, 2=cam_fwd. */
  kaynak: number;
  /** A8 `inst` */
  inst: number;
  /** A8 `batch_seq` (kamerada `kare_sirasi`; A8-K16, K62); `NO_FRAME`'de 0 */
  batch_seq: number;
  /** karenin ya da yanıtın çalıştırma nesli (A8 `run_gen`; A2-K80) */
  run_gen: number;
  /** karenin A2 `kare_sirasi`'i (A2-K49) */
  kare_sirasi: number;
  /** [ns] A8 `window_ns` */
  window_ns: number;
  /** A8 `calib_ver` */
  calib_ver: number;
  /** A8 `producer_ver` */
  producer_ver: number;
  /** A8 `producer_id` */
  producer_id: number;
  /** A8 parti `flags` (bit0 `NO_FRAME`) */
  parti_bayraklari: number;
  /** bileşenin bildirdiği `status`; yanıt yoksa ya da parti sim'de üretildiyse 255 Yok: 255 → null. */
  bilesen_durumu: number | null;
  /** bileşenin yanıtındaki `count`; yanıt yoksa 0 */
  bilesen_kayit_sayisi: number;
  /** kare ya da yanıt `M_k` anında güncel nesilde değil (A8-K63 adım 0, K79): karta başlık gitmez, `nihai_durum` 255 Değerler: 0=hayir, 1=evet. */
  eski_nesil: number;
  /** A2 kareyi teslim etmediyse özgün `SimKareDusmeNedeni` kodu (1-8, 33-39; A8-K76); etmediyse 0 Yok: 0 → null. */
  a2_dusme_nedeni: number | null;
  /** karta giden nihai kare durumu (A8-K63; A8 §3.4); karta başlık gitmediyse 255 Değerler: 0=OK, 1=SKIPPED_BUSY, 2=LATE, 3=COMPONENT_DOWN, 4=FRAME_DROPPED, 5=FRAME_INVALID, 6=CALIB_MISMATCH, 7=CAMERA_OFF, 8=COMPONENT_ERROR. Yok: 255 → null. */
  nihai_durum: number | null;
  /** sim'in A8-K27 ret nedeni (`IDA_DET_RJ_*`); ret yoksa 0 Değerler: 0=yok, 1=STRUCT, 2=ECHO, 3=CALIB, 4=LINK, 5=SEQ, 6=TIME, 7=KIND, 8=FLAGS, 9=VALUE, 10=UNCERT. */
  ret_nedeni: number;
  /** yanıt `M_k`'dan sonra alındı (A8-K59) Değerler: 0=hayir, 1=evet. */
  gec: number;
  /** karta giden partinin `count`'u */
  karta_kayit_sayisi: number;
  /** partinin karta iletildiği A1 partisi; iletilmediyse 0xFFFFFFFF Yok: 0xFFFFFFFF → null. */
  a1_batch_seq: number | null;
}

export interface YukAlgilamaSayaclari {
  /** akış başına sim'in kabul kapısı sayaçları (A8-K30, K65); bileşenin iç sayaçları dışa açılmaz */
  akislar: YukAlgilamaSayaclariAkislarOge[];
}

export interface YukKartBaglantiSayaclari {
  /** sayaçların ait olduğu kart açılışı (`boot_count`) */
  acilis_sayaci: number;
  /** A1 Tablo 3.11b #0 (`IDA_LINK_STATS.cnt[0]`); 2^32'de sarar */
  rx_bytes: number;
  /** A1 Tablo 3.11b #1 (`IDA_LINK_STATS.cnt[1]`); 2^32'de sarar */
  rx_frames: number;
  /** A1 Tablo 3.11b #2 (`IDA_LINK_STATS.cnt[2]`); 2^32'de sarar */
  rx_crc_err: number;
  /** A1 Tablo 3.11b #3 (`IDA_LINK_STATS.cnt[3]`); 2^32'de sarar */
  rx_unknown_msgid: number;
  /** A1 Tablo 3.11b #4 (`IDA_LINK_STATS.cnt[4]`); 2^32'de sarar */
  rx_iflag: number;
  /** A1 Tablo 3.11b #5 (`IDA_LINK_STATS.cnt[5]`); 2^32'de sarar */
  rx_bad_len: number;
  /** A1 Tablo 3.11b #6 (`IDA_LINK_STATS.cnt[6]`); 2^32'de sarar */
  rx_wrong_sys: number;
  /** A1 Tablo 3.11b #7 (`IDA_LINK_STATS.cnt[7]`); 2^32'de sarar */
  rx_mav1: number;
  /** A1 Tablo 3.11b #8 (`IDA_LINK_STATS.cnt[8]`); 2^32'de sarar */
  rx_overflow: number;
  /** A1 Tablo 3.11b #9 (`IDA_LINK_STATS.cnt[9]`); 2^32'de sarar */
  rx_resync_bytes: number;
  /** A1 Tablo 3.11b #10 (`IDA_LINK_STATS.cnt[10]`); 2^32'de sarar */
  rx_seq_gap: number;
  /** A1 Tablo 3.11b #11 (`IDA_LINK_STATS.cnt[11]`); 2^32'de sarar */
  rx_stale_session: number;
  /** A1 Tablo 3.11b #12 (`IDA_LINK_STATS.cnt[12]`); 2^32'de sarar */
  rx_outside_session: number;
  /** A1 Tablo 3.11b #13 (`IDA_LINK_STATS.cnt[13]`); 2^32'de sarar */
  stamp_coarse: number;
  /** A1 Tablo 3.11b #14 (`IDA_LINK_STATS.cnt[14]`); 2^32'de sarar */
  batch_closed: number;
  /** A1 Tablo 3.11b #15 (`IDA_LINK_STATS.cnt[15]`); 2^32'de sarar */
  batch_incomplete: number;
  /** A1 Tablo 3.11b #16 (`IDA_LINK_STATS.cnt[16]`); 2^32'de sarar */
  batch_skipped: number;
  /** A1 Tablo 3.11b #17 (`IDA_LINK_STATS.cnt[17]`); 2^32'de sarar */
  batch_old: number;
  /** A1 Tablo 3.11b #18 (`IDA_LINK_STATS.cnt[18]`); 2^32'de sarar */
  batch_dup: number;
  /** A1 Tablo 3.11b #19 (`IDA_LINK_STATS.cnt[19]`); 2^32'de sarar */
  batch_merged: number;
  /** A1 Tablo 3.11b #20 (`IDA_LINK_STATS.cnt[20]`); 2^32'de sarar */
  member_overflow: number;
  /** A1 Tablo 3.11b #21 (`IDA_LINK_STATS.cnt[21]`); 2^32'de sarar */
  ping360_incomplete: number;
  /** A1 Tablo 3.11b #22 (`IDA_LINK_STATS.cnt[22]`); 2^32'de sarar */
  ping360_bad: number;
  /** A1 Tablo 3.11b #23 (`IDA_LINK_STATS.cnt[23]`); 2^32'de sarar */
  ping360_orphan: number;
  /** A1 Tablo 3.11b #24 (`IDA_LINK_STATS.cnt[24]`); 2^32'de sarar */
  cmd_deferred: number;
  /** A1 Tablo 3.11b #25 (`IDA_LINK_STATS.cnt[25]`); 2^32'de sarar */
  cmd_dropped: number;
  /** A1 Tablo 3.11b #26 (`IDA_LINK_STATS.cnt[26]`); 2^32'de sarar */
  narrow_coalesced: number;
  /** A1 Tablo 3.11b #27 (`IDA_LINK_STATS.cnt[27]`); 2^32'de sarar */
  cmd_bad_field: number;
  /** A1 Tablo 3.11b #28 (`IDA_LINK_STATS.cnt[28]`); 2^32'de sarar */
  grid_missed: number;
  /** A1 Tablo 3.11b #29 (`IDA_LINK_STATS.cnt[29]`); 2^32'de sarar */
  out_fixed: number;
  /** A1 Tablo 3.11b #30 (`IDA_LINK_STATS.cnt[30]`); 2^32'de sarar */
  tx_frames: number;
  /** A1 Tablo 3.11b #31 (`IDA_LINK_STATS.cnt[31]`); 2^32'de sarar */
  tx_dropped: number;
  /** A1 Tablo 3.11b #32 (`IDA_LINK_STATS.cnt[32]`); 2^32'de sarar */
  tx_crit_stall: number;
  /** A1 Tablo 3.11b #33 (`IDA_LINK_STATS.cnt[33]`); 2^32'de sarar */
  events_sent: number;
  /** A1 Tablo 3.11b #34 (`IDA_LINK_STATS.cnt[34]`); 2^32'de sarar */
  events_resent: number;
  /** A1 Tablo 3.11b #35 (`IDA_LINK_STATS.cnt[35]`); 2^32'de sarar */
  sessions: number;
}

export interface YukKartCekirdekSayaclari {
  /** sayaçların ait olduğu kart açılışı (`boot_count`) */
  acilis_sayaci: number;
  /** A4 `ida_core_counters_t.sample_rejected` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  sample_rejected: number;
  /** A4 `ida_core_counters_t.sample_future` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  sample_future: number;
  /** A4 `ida_core_counters_t.sample_conflict` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  sample_conflict: number;
  /** A4 `ida_core_counters_t.sample_dup_dropped` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  sample_dup_dropped: number;
  /** A4 `ida_core_counters_t.narrow_from_rejected` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  narrow_from_rejected: number;
  /** A4 `ida_core_counters_t.bg_bad_input` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  bg_bad_input: number;
  /** A4 `ida_core_counters_t.imu_rejected` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  imu_rejected: number;
  /** A4 `ida_core_counters_t.imu_gap` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  imu_gap: number;
  /** A4 `ida_core_counters_t.bad_input` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  bad_input: number;
  /** A4 `ida_core_counters_t.dup_seq` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  dup_seq: number;
  /** A4 `ida_core_counters_t.old_seq` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  old_seq: number;
  /** A4 `ida_core_counters_t.wall_backstep` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  wall_backstep: number;
  /** A4 `ida_core_counters_t.cmd_rejected` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  cmd_rejected: number;
  /** A4 `ida_core_counters_t.widen_blocked` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  widen_blocked: number;
  /** A4 `ida_core_counters_t.evres_ignored` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  evres_ignored: number;
  /** A4 `ida_core_counters_t.event_unknown` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  event_unknown: number;
  /** A4 `ida_core_counters_t.det_rejected` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  det_rejected: number;
  /** A4 `ida_core_counters_t.ping360_rejected` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  ping360_rejected: number;
  /** A4 `ida_core_counters_t.bg_dropped` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  bg_dropped: number;
  /** A4 `ida_core_counters_t.bg_slice_mismatch` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  bg_slice_mismatch: number;
  /** A4 `ida_core_counters_t.log_dropped` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  log_dropped: number;
  /** A4 `ida_core_counters_t.step_miss` (A1 `IDA_CORE_COUNTERS`); 2^32'de sarar */
  step_miss: number;
  /** A4 `ida_health_t` (A1 `IDA_STATUS.health`, `IDAW_HEALTH`): bit0 core_ok, bit1 data_ok, bit2 imu_rejected, bit3 imu_lag, bit4 input_dropped, bit5 narrowed, bit6 sample_conflict, bit7 bg_bad_input */
  saglik: number;
}

export interface YukKayitDurum {
  /** kayıt dosyası açık mı Değerler: 0=hayir, 1=evet. */
  acik: number;
  /** kayıt kademesi Değerler: 0=olaylar, 1=telemetri, 2=tam. */
  kademe: number;
  /** kamera kare kaydı bayrağı (A6) Değerler: 0=kapali, 1=pencereli, 2=surekli. */
  kamera_kareleri: number;
  /** [bayt] dosyaya yazılan bayt */
  yazilan_bayt: bigint;
  /** düşen kayıt iletisi */
  kayit_dusen: bigint;
  /** kaydın şu anki güvencesi (A6) Değerler: 0=yok, 1=izleme, 2=fizik, 3=girdi. */
  tekrar_guvencesi: number;
  /** [bayt] kayıt dosyasının adı */
  dosya: string;
}

export interface YukSurecDurum {
  /** yardımcı süreçler */
  surecler: YukSurecDurumSureclerOge[];
}

export interface YukBaglantiDurum {
  /** A1 oturum durumu Değerler: 0=kapali, 1=el_sikisma, 2=hazir, 3=kosuyor, 4=kopuk, 255=uygulanamaz. */
  a1_oturum: number;
  /** A1 oturum numarası; yoksa 0 */
  sess: number;
  /** kartın açılış sayacı; yoksa 0 */
  acilis_sayaci: number;
  /** [ns] aşağı yön teslim gecikmesi kestiriminin ortalaması (06 §2.5); yoksa INT64_MIN Yok: INT64_MIN → null. */
  d_asagi_ort_ns: bigint | null;
  /** bağlantı emülatörü */
  emulator: YukBaglantiDurumEmulatorOge[];
  /** bu WebSocket bağlantısında gönderilmeden ezilen durum sınıfı çerçeve (A3-K40) */
  bu_baglanti_ezilen_durum: bigint;
  /** bu bağlantıda kuyruktan düşen dizi sınıfı çerçeve (A3-K41) */
  bu_baglanti_dusen_dizi: bigint;
  /** bu bağlantının olay kuyruğundaki çerçeve sayısı */
  bu_baglanti_olay_kuyrugu: number;
}

export interface YukYerIstasyonuMavlink {
  /** yön Değerler: 0=yer_to_arac, 1=arac_to_yer. */
  yon: number;
  /** [bayt] yer istasyonu düzlemindeki MAVLink 2 çerçevesi, emülatör çıktısı (A1) */
  cerceve: Uint8Array;
}

export interface YukKameraHalkaSayaclari {
  /** A2 `kamera_kimligi` */
  kamera_kimligi: number;
  /** sim nedenli düşen kare; sıra `SimKareDusmeNedeni` 1-8 (8 = `halka_emekli_hazir`) */
  dusen_sim: bigint[];
  /** görselleştirici nedenli düşen kare; sıra kod 33-39 */
  dusen_gorsellestirici: bigint[];
  /** A2-K48 */
  gec_hazir: bigint;
  /** A2-K59 */
  tuketici_gec: bigint;
  /** A2-K59 */
  tuketici_koptu: bigint;
  /** A2-K86 */
  halka_emekliye_ayirma: bigint;
  /** A2-K87 */
  emekli_halka_yaniti: bigint;
  /** A2-K89 */
  tuketici_kamera_kapali: bigint;
  /** su an tutulan emekli halka sayısı (A2-K88) */
  tutulan_emekli_halka: number;
  /** `eski_nesil` sonuç reddi (A2 §5) */
  eski_nesil: bigint;
  /** A2 §5 */
  yok_sayilan_birakma: bigint;
  /** A2 §5 */
  seyreltilen_durum: bigint;
  /** A2 §5 */
  protokol_hatasi: bigint;
  /** A2 bağlantı sayısı */
  baglanti: bigint;
  /** A2 kopma sayısı; A2 oturum kapanış nedeni `surec_durumu.a2_veda_nedeni`'nde */
  kopma: bigint;
  /** istekten hazıra süresi ölçülen kare (yuva başlığındaki duvar saatlerinden; A2 §5) */
  istek_hazir_adet: bigint;
  /** [ns] bu sürelerin toplamı */
  istek_hazir_toplam_ns: bigint;
  /** [ns] en büyük süre */
  istek_hazir_en_buyuk_ns: bigint;
  /** dağılım kovaları; sıra <5, <10, <20, <33, <50, <100, <200, >=200 ms (kova sınırları tahmin) */
  istek_hazir_dagilim: bigint[];
}

/** Kodlayıcı girdisi: 64 bit alanlarda güvenli tamsayı (number) da kabul edilir. */
export type KodlamaGirdisi<T> = T extends bigint
  ? bigint | number
  : T extends Uint8Array
    ? Uint8Array
    : T extends readonly (infer U)[]
      ? KodlamaGirdisi<U>[]
      : T extends object
        ? { [K in keyof T]: KodlamaGirdisi<T[K]> }
        : T;

/** Yük adından çözülmüş yük tipine. */
export interface YukTipleri {
  yuk_calistirma_durum: YukCalistirmaDurum;
  yuk_sayac_gercek_zaman: YukSayacGercekZaman;
  yuk_olay: YukOlay;
  yuk_fizik_durum: YukFizikDurum;
  yuk_kart_durum: YukKartDurum;
  yuk_atis_durum: YukAtisDurum;
  yuk_emniyet_girdiler: YukEmniyetGirdiler;
  yuk_ariza_etkin: YukArizaEtkin;
  yuk_imu: YukImu;
  yuk_manyetometre: YukManyetometre;
  yuk_basinc: YukBasinc;
  yuk_gnss: YukGnss;
  yuk_dvl: YukDvl;
  yuk_batarya: YukBatarya;
  yuk_sizinti: YukSizinti;
  yuk_firlatici: YukFirlatici;
  yuk_ping360: YukPing360;
  yuk_ping1d: YukPing1d;
  yuk_algilama_kayit: YukAlgilamaKayit;
  yuk_algilama_parti: YukAlgilamaParti;
  yuk_algilama_sayaclari: YukAlgilamaSayaclari;
  yuk_kart_baglanti_sayaclari: YukKartBaglantiSayaclari;
  yuk_kart_cekirdek_sayaclari: YukKartCekirdekSayaclari;
  yuk_kayit_durum: YukKayitDurum;
  yuk_surec_durum: YukSurecDurum;
  yuk_baglanti_durum: YukBaglantiDurum;
  yuk_yer_istasyonu_mavlink: YukYerIstasyonuMavlink;
  yuk_kamera_halka_sayaclari: YukKameraHalkaSayaclari;
}

/** Kanal kataloğu (A3 §3.3). */
export const KANALLAR = [
  { kanal_id: 1, ad: "calistirma/durum", kanal_surumu: 1, yuk: "yuk_calistirma_durum", sinif: "durum", azami_hz: 10, varsayilan_hz: 10, kuyruk_siniri: null, zaman_anlami: "sim", anahtar: [], zorunlu_abonelik: true, yer_gercegi: false },
  { kanal_id: 2, ad: "sayac/gercek_zaman", kanal_surumu: 1, yuk: "yuk_sayac_gercek_zaman", sinif: "durum", azami_hz: 10, varsayilan_hz: 10, kuyruk_siniri: null, zaman_anlami: "sim", anahtar: [], zorunlu_abonelik: true, yer_gercegi: false },
  { kanal_id: 3, ad: "olay", kanal_surumu: 1, yuk: "yuk_olay", sinif: "olay", azami_hz: null, varsayilan_hz: null, kuyruk_siniri: 4096, zaman_anlami: "sim", anahtar: [], zorunlu_abonelik: true, yer_gercegi: false },
  { kanal_id: 10, ad: "fizik/durum", kanal_surumu: 1, yuk: "yuk_fizik_durum", sinif: "durum", azami_hz: 50, varsayilan_hz: 25, kuyruk_siniri: null, zaman_anlami: "olcum", anahtar: [], zorunlu_abonelik: false, yer_gercegi: true },
  { kanal_id: 11, ad: "kart/durum", kanal_surumu: 1, yuk: "yuk_kart_durum", sinif: "durum", azami_hz: 50, varsayilan_hz: 25, kuyruk_siniri: null, zaman_anlami: "alim", anahtar: [], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 12, ad: "yer_istasyonu/durum", kanal_surumu: 1, yuk: "yuk_kart_durum", sinif: "durum", azami_hz: 50, varsayilan_hz: 10, kuyruk_siniri: null, zaman_anlami: "alim", anahtar: [], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 13, ad: "atis/durum", kanal_surumu: 1, yuk: "yuk_atis_durum", sinif: "durum", azami_hz: 10, varsayilan_hz: 10, kuyruk_siniri: null, zaman_anlami: "sim", anahtar: [], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 14, ad: "emniyet/girdiler", kanal_surumu: 1, yuk: "yuk_emniyet_girdiler", sinif: "durum", azami_hz: 10, varsayilan_hz: 10, kuyruk_siniri: null, zaman_anlami: "sim", anahtar: [], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 15, ad: "ariza/etkin", kanal_surumu: 1, yuk: "yuk_ariza_etkin", sinif: "durum", azami_hz: 2, varsayilan_hz: 2, kuyruk_siniri: null, zaman_anlami: "sim", anahtar: [], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 20, ad: "sensor/imu", kanal_surumu: 1, yuk: "yuk_imu", sinif: "durum", azami_hz: 50, varsayilan_hz: 10, kuyruk_siniri: null, zaman_anlami: "olcum", anahtar: ["sensor_id","inst"], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 21, ad: "sensor/manyetometre", kanal_surumu: 1, yuk: "yuk_manyetometre", sinif: "durum", azami_hz: 50, varsayilan_hz: 10, kuyruk_siniri: null, zaman_anlami: "olcum", anahtar: ["sensor_id","inst"], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 22, ad: "sensor/basinc", kanal_surumu: 1, yuk: "yuk_basinc", sinif: "durum", azami_hz: 50, varsayilan_hz: 10, kuyruk_siniri: null, zaman_anlami: "olcum", anahtar: ["sensor_id","inst"], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 23, ad: "sensor/gnss", kanal_surumu: 1, yuk: "yuk_gnss", sinif: "durum", azami_hz: 50, varsayilan_hz: 10, kuyruk_siniri: null, zaman_anlami: "olcum", anahtar: ["sensor_id","inst"], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 24, ad: "sensor/dvl", kanal_surumu: 1, yuk: "yuk_dvl", sinif: "durum", azami_hz: 50, varsayilan_hz: 10, kuyruk_siniri: null, zaman_anlami: "olcum", anahtar: ["sensor_id","inst"], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 25, ad: "sensor/batarya", kanal_surumu: 1, yuk: "yuk_batarya", sinif: "durum", azami_hz: 50, varsayilan_hz: 10, kuyruk_siniri: null, zaman_anlami: "olcum", anahtar: ["sensor_id","inst"], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 26, ad: "sensor/sizinti", kanal_surumu: 1, yuk: "yuk_sizinti", sinif: "durum", azami_hz: 50, varsayilan_hz: 10, kuyruk_siniri: null, zaman_anlami: "olcum", anahtar: ["sensor_id","inst"], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 27, ad: "sensor/firlatici", kanal_surumu: 1, yuk: "yuk_firlatici", sinif: "durum", azami_hz: 50, varsayilan_hz: 10, kuyruk_siniri: null, zaman_anlami: "olcum", anahtar: ["sensor_id","inst"], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 30, ad: "sonar/ping360", kanal_surumu: 1, yuk: "yuk_ping360", sinif: "dizi", azami_hz: null, varsayilan_hz: null, kuyruk_siniri: 256, zaman_anlami: "gonderim", anahtar: ["sensor_id","inst"], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 31, ad: "sonar/ping1d", kanal_surumu: 1, yuk: "yuk_ping1d", sinif: "durum", azami_hz: 50, varsayilan_hz: 10, kuyruk_siniri: null, zaman_anlami: "gonderim", anahtar: ["sensor_id","inst"], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 41, ad: "algilama/parti", kanal_surumu: 1, yuk: "yuk_algilama_parti", sinif: "dizi", azami_hz: null, varsayilan_hz: null, kuyruk_siniri: 64, zaman_anlami: "olcum", anahtar: [], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 42, ad: "algilama/sayaclar", kanal_surumu: 1, yuk: "yuk_algilama_sayaclari", sinif: "durum", azami_hz: 2, varsayilan_hz: 2, kuyruk_siniri: null, zaman_anlami: "sim", anahtar: [], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 54, ad: "kart/baglanti_sayaclari", kanal_surumu: 1, yuk: "yuk_kart_baglanti_sayaclari", sinif: "durum", azami_hz: 2, varsayilan_hz: 2, kuyruk_siniri: null, zaman_anlami: "alim", anahtar: [], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 55, ad: "kart/cekirdek_sayaclari", kanal_surumu: 1, yuk: "yuk_kart_cekirdek_sayaclari", sinif: "durum", azami_hz: 2, varsayilan_hz: 2, kuyruk_siniri: null, zaman_anlami: "alim", anahtar: [], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 56, ad: "kamera/halka_sayaclari", kanal_surumu: 1, yuk: "yuk_kamera_halka_sayaclari", sinif: "durum", azami_hz: 2, varsayilan_hz: 2, kuyruk_siniri: null, zaman_anlami: "sim", anahtar: [], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 40, ad: "algilama/kayit", kanal_surumu: 1, yuk: "yuk_algilama_kayit", sinif: "dizi", azami_hz: null, varsayilan_hz: null, kuyruk_siniri: 64, zaman_anlami: "olcum", anahtar: [], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 50, ad: "kayit/durum", kanal_surumu: 1, yuk: "yuk_kayit_durum", sinif: "durum", azami_hz: 2, varsayilan_hz: 2, kuyruk_siniri: null, zaman_anlami: "sim", anahtar: [], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 51, ad: "surec/durum", kanal_surumu: 1, yuk: "yuk_surec_durum", sinif: "durum", azami_hz: 2, varsayilan_hz: 2, kuyruk_siniri: null, zaman_anlami: "sim", anahtar: [], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 52, ad: "baglanti/durum", kanal_surumu: 1, yuk: "yuk_baglanti_durum", sinif: "durum", azami_hz: 10, varsayilan_hz: 10, kuyruk_siniri: null, zaman_anlami: "sim", anahtar: [], zorunlu_abonelik: false, yer_gercegi: false },
  { kanal_id: 53, ad: "yer_istasyonu/mavlink", kanal_surumu: 1, yuk: "yuk_yer_istasyonu_mavlink", sinif: "dizi", azami_hz: null, varsayilan_hz: null, kuyruk_siniri: 256, zaman_anlami: "sim", anahtar: [], zorunlu_abonelik: false, yer_gercegi: false },
] as const satisfies readonly KanalTanimi[];

export type KanalAdi = (typeof KANALLAR)[number]['ad'];

/** Kanal adından yük tipine. */
export interface KanalYuku {
  "calistirma/durum": YukCalistirmaDurum;
  "sayac/gercek_zaman": YukSayacGercekZaman;
  "olay": YukOlay;
  "fizik/durum": YukFizikDurum;
  "kart/durum": YukKartDurum;
  "yer_istasyonu/durum": YukKartDurum;
  "atis/durum": YukAtisDurum;
  "emniyet/girdiler": YukEmniyetGirdiler;
  "ariza/etkin": YukArizaEtkin;
  "sensor/imu": YukImu;
  "sensor/manyetometre": YukManyetometre;
  "sensor/basinc": YukBasinc;
  "sensor/gnss": YukGnss;
  "sensor/dvl": YukDvl;
  "sensor/batarya": YukBatarya;
  "sensor/sizinti": YukSizinti;
  "sensor/firlatici": YukFirlatici;
  "sonar/ping360": YukPing360;
  "sonar/ping1d": YukPing1d;
  "algilama/parti": YukAlgilamaParti;
  "algilama/sayaclar": YukAlgilamaSayaclari;
  "kart/baglanti_sayaclari": YukKartBaglantiSayaclari;
  "kart/cekirdek_sayaclari": YukKartCekirdekSayaclari;
  "kamera/halka_sayaclari": YukKameraHalkaSayaclari;
  "algilama/kayit": YukAlgilamaKayit;
  "kayit/durum": YukKayitDurum;
  "surec/durum": YukSurecDurum;
  "baglanti/durum": YukBaglantiDurum;
  "yer_istasyonu/mavlink": YukYerIstasyonuMavlink;
}

/** Olay türleri (A3 §3.5). */
export const OLAY_TURLERI = [
  { kod: 1, ad: "calistirma_durumu_degisti", varsayilan_onem: "bilgi" },
  { kod: 2, ad: "gecerlilik_dustu", varsayilan_onem: "uyari" },
  { kod: 3, ad: "gercek_zaman_kacirildi", varsayilan_onem: "uyari" },
  { kod: 4, ad: "parti_kacirildi_ozeti", varsayilan_onem: "uyari" },
  { kod: 5, ad: "komut_sonucu", varsayilan_onem: "bilgi" },
  { kod: 6, ad: "ariza_degisti", varsayilan_onem: "bilgi" },
  { kod: 7, ad: "emniyet_girdisi_degisti", varsayilan_onem: "emniyet" },
  { kod: 8, ad: "atis_olayi_sonucu", varsayilan_onem: "emniyet" },
  { kod: 9, ad: "a1_oturum_degisti", varsayilan_onem: "bilgi" },
  { kod: 10, ad: "kart_sifirlandi", varsayilan_onem: "uyari" },
  { kod: 11, ad: "yer_istasyonu_komutu", varsayilan_onem: "bilgi" },
  { kod: 12, ad: "yer_istasyonu_yaniti", varsayilan_onem: "bilgi" },
  { kod: 13, ad: "senaryo_yuklendi", varsayilan_onem: "bilgi" },
  { kod: 14, ad: "on_denetim_sonucu", varsayilan_onem: "bilgi" },
  { kod: 15, ad: "kayit_durumu", varsayilan_onem: "bilgi" },
  { kod: 16, ad: "kayit_eksik", varsayilan_onem: "uyari" },
  { kod: 17, ad: "surec_durumu", varsayilan_onem: "bilgi" },
  { kod: 18, ad: "olay_kaybi", varsayilan_onem: "hata" },
  { kod: 19, ad: "erisim_reddedildi", varsayilan_onem: "uyari" },
  { kod: 20, ad: "izleme_durumu", varsayilan_onem: "bilgi" },
  { kod: 21, ad: "senaryo_ogesi_uygulandi", varsayilan_onem: "bilgi" },
  { kod: 22, ad: "uyari", varsayilan_onem: "uyari" },
  { kod: 23, ad: "tekrar_mudahalesi", varsayilan_onem: "uyari" },
  { kod: 24, ad: "yapilandirma_reddedildi", varsayilan_onem: "hata" },
  { kod: 25, ad: "atama_celiskisi", varsayilan_onem: "uyari" },
  { kod: 26, ad: "gevsetme_reddedildi", varsayilan_onem: "emniyet" },
  { kod: 27, ad: "donma_onceki_cikti_yok", varsayilan_onem: "uyari" },
  { kod: 28, ad: "halka_emekliye_ayrildi", varsayilan_onem: "uyari" },
  { kod: 29, ad: "tuketici_kamera_kapali", varsayilan_onem: "uyari" },
  { kod: 30, ad: "emniyet_rev_doydu", varsayilan_onem: "emniyet" },
] as const;

/** Geçerlilik neden bitleri (A3 Tablo 4.8). */
export const GECERLILIK_NEDENLERI: Readonly<Record<number, string>> = {
  0: "parti_kacirildi",
  1: "son_tarih_kacirildi",
  2: "sim_gecikmesi",
  3: "cagri_cakismasi",
  4: "bariyer_bekleme",
  5: "olay_gec",
  6: "esz_guvenli_uygulanan",
  7: "yanit_ret_gec",
  8: "pencere_tasma",
  9: "planlanmamis_oturum_sonu",
  10: "beklenmeyen_sifirlama",
  11: "hil_duraklatma",
  16: "sonar_gec",
  17: "sonar_kuyruk_dolu",
  18: "sensor_kuyruk_tasma",
  19: "cagri_izi_eksik",
};

/** Hata kodları (A3 §3.9). */
export const HATA_KODLARI = [
  { kod: "erisim_reddedildi", http: 403 },
  { kod: "bulunamadi", http: 404 },
  { kod: "yontem_desteklenmiyor", http: 405 },
  { kod: "gecersiz_istek", http: 400 },
  { kod: "gecersiz_deger", http: 422 },
  { kod: "senaryo_gecersiz", http: 422 },
  { kod: "govde_cok_buyuk", http: 413 },
  { kod: "icerik_turu", http: 415 },
  { kod: "gecersiz_durum", http: 409 },
  { kod: "kipte_gecersiz", http: 409 },
  { kod: "beklenen_durum_uyusmuyor", http: 409 },
  { kod: "onay_gerekli", http: 409 },
  { kod: "onay_jetonu_gecersiz", http: 409 },
  { kod: "kimlik_cakismasi", http: 409 },
  { kod: "emniyet_rev_uyusmuyor", http: 409 },
  { kod: "durum_rev_uyusmuyor", http: 409 },
  { kod: "emniyet_rev_doydu", http: 409 },
  { kod: "kisitlama_hakim", http: 409 },
  { kod: "tekrar_kosulu_saglanmiyor", http: 409 },
  { kod: "tekrar_kipinde_yasak", http: 409 },
  { kod: "izleme_etkin", http: 409 },
  { kod: "canli_calistirma_yok", http: 409 },
  { kod: "komut_listede_yok", http: 422 },
  { kod: "kayit_yok", http: 404 },
  { kod: "kayit_okunamadi", http: 422 },
  { kod: "gecmis_yok", http: 410 },
  { kod: "mesgul", http: 503 },
  { kod: "sunucu_hatasi", http: 500 },
  { kod: "on_denetim_kaldi", http: 409 },
  { kod: "dosya_var", http: 409 },
] as const;

export type HataKodu = (typeof HATA_KODLARI)[number]['kod'];

/** WebSocket kapanış kodları (A3 §3.7). */
export const WS_KAPANIS_KODLARI: Readonly<Record<number, string>> = {
  1000: "olagan kapanis",
  1001: "sunucu kapaniyor",
  1002: "protokol hatasi",
  1003: "istemciden ikili ileti",
  1007: "gecersiz UTF-8",
  1009: "ileti cok buyuk",
  1011: "beklenmeyen ic hata",
  4001: "olay kuyrugu tasti",
  4002: "erisim jetonu gecersizlesti",
  4004: "canlilik yok",
};
