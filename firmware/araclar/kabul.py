"""Kart sınamalarının yargı işlevleri (yalnız standart kitaplık; seri porta dokunmaz).

sahte_sim.py bunları kullanır; test_kabul.py kart olmadan sınar.
"""

ASYNC = 16
THR_VALID = 1
IS_SAFE = 2
ESC_NOTR_US = 1500

# T_bağ (A1-K22, §3.9): kart son geçerli çerçeveden 500 ms sonra oturumu bitirir.
T_BAG_ALT_S = 0.45
T_BAG_UST_S = 0.8
# Son TF'den ilk ızgara tick'ine: E + T_tf_gec = 6 ms; ardından 10 ms'lik ızgara (A1-K117).
# PC tarafının ölçüm belirsizliğiyle ilk eşzamansız güvenli çıktı için üst sınır.
ILK_GUVENLI_UST_S = 0.05


def tbag_degerlendir(asenk, t_idle_s, son_parti, yeniden_el_sikisma):
    """T_bağ sınamasının yargısı (sol bulgu 5).

    asenk: [(t_s, act_flags, pwm_us[8], batch_seq)] — parti akışı durduktan sonra (t = 0) ve
    IDLE duyurusundan önce gelen ASYNC bayraklı IDA_ACTUATOR'lar, geliş sırasıyla.
    t_idle_s: IDLE duyurusunun anı (None: gelmedi). son_parti: gönderilen son partinin batch_seq'i.
    Dönüş: (gecti, ayrinti sözlüğü).
    """
    a = {"asenk_aktuator": len(asenk), "duyuruya_kadar_s": t_idle_s, "hatalar": []}
    h = a["hatalar"]
    if t_idle_s is None:
        h.append("IDLE duyurusu yok")
    elif not T_BAG_ALT_S <= t_idle_s <= T_BAG_UST_S:
        h.append("IDLE duyurusu T_bağ penceresinde değil: %.3f s" % t_idle_s)
    if not asenk:
        h.append("eşzamansız güvenli çıktı yok (en az bir tane zorunlu; A1-K44)")
    else:
        t0, f0, p0, b0 = asenk[0]
        a["ilk_guvenli_s"] = t0
        a["ilk_bayrak"] = f0
        a["ilk_pwm"] = list(p0)
        a["ilk_batch_seq"] = b0
        if t0 > ILK_GUVENLI_UST_S:
            h.append("ilk güvenli çıktı geç: %.3f s > %.3f s" % (t0, ILK_GUVENLI_UST_S))
        for t, f, p, b in asenk:
            if (f & (THR_VALID | IS_SAFE | ASYNC)) != (THR_VALID | IS_SAFE | ASYNC):
                h.append("t=%.3f s: THR_VALID/IS_SAFE/ASYNC eksik (bayrak 0x%X)" % (t, f))
                break
            if any(x != ESC_NOTR_US for x in p):
                h.append("t=%.3f s: darbe genişliği nötr değil %s" % (t, list(p)))
                break
            if b != son_parti:
                h.append("t=%.3f s: batch_seq %d, son kapanan parti %d değil (A1-K44)" % (t, b, son_parti))
                break
            if t_idle_s is not None and t > t_idle_s:
                h.append("t=%.3f s: IDLE'dan sonra eşzamansız çıktı" % t)
                break
    if not yeniden_el_sikisma:
        h.append("yeniden el sıkışma kurulamadı")
    return (not h), a
