// İDA-0 varsayılanları. Değerler scenarios/ida0.toml (A5 araç dosyası; 04 §2.6) ile aynıdır;
// değişirse ikisi birlikte değişir (sim.fizik.ida0_toml_esitligi testi denetler).
#include "ida/fizik/parametreler.hpp"

namespace ida::fizik {

AracParametreleri ida0_arac()
{
    AracParametreleri a;
    a.ad = "IDA-0";
    a.varlik = "ida0";
    a.referans_noktasi =
        "Cercevenin geometrik merkezi; bos aracin agirlik merkezi bu noktada kabul edilir (r_g = 0); "
        "govdeye sabit, yuk ayrilinca degismez (iskelet §3.1; 04 §2.2)";

    a.kutle.kutle = 13.5;
    a.kutle.hacim = 0.0137;
    a.kutle.r_g = {0.0, 0.0, 0.0};
    a.kutle.r_b = {0.0, 0.0, -0.01};
    a.kutle.atalet = {{{0.26, 0.0, 0.0}, {0.0, 0.23, 0.0}, {0.0, 0.0, 0.37}}};
    a.kutle.trim_kutlesi = 0.0;

    auto& h = a.hidrodinamik;
    h.isaret_kurali = "pozitif_buyukluk";
    h.eklenmis_kutle = {6.36, 7.12, 18.68, 0.189, 0.135, 0.222};
    h.dogrusal_sonum = {13.7, 0.0, 33.0, 0.0, 0.8, 0.0};
    h.karesel_sonum = {141.0, 217.0, 190.0, 1.19, 0.47, 1.5};
    h.yuzey_sonum_orani = {0.3, 0.2, 0.4};
    h.ruzgar_direnc_katsayisi = 1.0;
    h.belirsizlik = {0.30, 1.00};

    const double k = 0.7071067811865475;
    a.iticiler[0] = {0, {0.156, 0.111, 0.085}, {k, -k, 0.0}, "yatay, on sancak"};
    a.iticiler[1] = {1, {0.156, -0.111, 0.085}, {k, k, 0.0}, "yatay, on iskele"};
    a.iticiler[2] = {2, {-0.156, 0.111, 0.085}, {k, k, 0.0}, "yatay, arka sancak"};
    a.iticiler[3] = {3, {-0.156, -0.111, 0.085}, {k, -k, 0.0}, "yatay, arka iskele"};
    a.iticiler[4] = {4, {0.120, 0.218, 0.0}, {0.0, 0.0, 1.0}, "dikey, on sancak"};
    a.iticiler[5] = {5, {0.120, -0.218, 0.0}, {0.0, 0.0, 1.0}, "dikey, on iskele"};
    a.iticiler[6] = {6, {-0.120, 0.218, 0.0}, {0.0, 0.0, 1.0}, "dikey, arka sancak"};
    a.iticiler[7] = {7, {-0.120, -0.218, 0.0}, {0.0, 0.0, 1.0}, "dikey, arka iskele"};

    auto& m = a.itici_modeli;
    m.tur = "t200_gecici";
    m.sekil_polinomu = {8.9, 176.0, -404.1, 389.9, -140.3};
    m.gerilim_noktalari = {12.0, 16.0, 20.0};
    m.azami_itki_ileri = {36.3827, 51.4849, 65.7046};
    m.azami_itki_geri = {28.6354, 40.2073, 49.5236};
    m.tam_gaz_akimi = {17.0, 24.0, 32.0};
    m.akim_ussu = 1.5;
    m.transfer_pay = {6136.0, 108700.0};
    m.transfer_payda = {1.0, 89.0, 9258.0, 108700.0};
    m.pervane_yaricapi = 0.038;
    m.havalanma_h_bolu_r = {0.0, 3.4};
    m.havalanma_beta = {0.0, 1.0};

    a.esc.pwm_notr_us = 1500;
    a.esc.olu_bant_us = 25;
    a.esc.pwm_min_us = 1100;
    a.esc.pwm_max_us = 1900;
    a.esc.baslatma_suresi_ns = 2'000'000'000;

    a.batarya.kip = BataryaKipi::ideal;
    a.batarya.v_sabit = 15.6;
    a.batarya.seri = 4;
    a.batarya.paralel = 6;
    a.batarya.kapasite = 64800.0;
    a.batarya.r_kablo = 0.01;
    a.batarya.aviyonik_guc = 20.0;

    a.temas.yay_sabiti = 3.0e4;
    a.temas.sonum_orani = 1.0;
    a.temas.surtunme_katsayisi = 0.5;

    a.torpido.varlik = "torpido_ida0";
    a.torpido.geometri = "prolate_sferoid";
    a.torpido.boy = 0.20;
    a.torpido.cap = 0.04;
    a.torpido.kutle = 0.175;
    a.torpido.r_g = {0.0, 0.0, 0.002};
    a.torpido.kanat_alani_duzlem = 0.0018;
    a.torpido.kanat_en_boy_orani = 2.0;
    a.torpido.kanat_x = -0.09;
    a.torpido.surukleme_katsayisi = 0.42;
    a.torpido.ayrilma_hizi = 2.0;

    a.firlaticilar[0] = {{0.0, 0.10, 0.10}, {1.0, 0.0, 0.0}, 0.0};
    a.firlaticilar[1] = {{0.0, -0.10, 0.10}, {1.0, 0.0, 0.0}, 0.0};
    return a;
}

OrtamParametreleri havuz_ortami()
{
    OrtamParametreleri o;
    o.tur = OrtamTuru::havuz;
    o.derinlik = 3.0;
    o.yercekimi = 9.82;
    o.atmosfer_basinci = 101325.0;
    o.hava_yogunlugu = 1.225;
    o.su.yogunluk = 1000.0;
    o.su.sicaklik_c = 20.0;
    o.su.tuzluluk = 0.0;
    o.su.ph = 8.0;
    return o;
}

}  // namespace ida::fizik
