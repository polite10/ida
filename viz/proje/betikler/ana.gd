extends Node
## İDA görselleştirici, ana betik (W3-viz dilim 1).
##
## Düğüm ağacı (07 §2.2): A2 (IdaA2Istemci, GDExtension), Dünya (gövdeler ve statik sahne;
## NED → Godot dönüşümü eklentide), Su, Işıklar, Göstergeler, Operatör (SubViewport + Camera3D),
## HUD. Sahne A7 manifestinden kurulur (A2-K13, A7-K53); A7 denetimlerinin bu dilimde yapılan
## alt kümesi: K04 (BOM/CR), K05 (özet), K06 (dosya özeti ve boyutu), K11 (uzantılar), K14
## (eksenler), K21 (düğüm adları), K22 (get_scene_node). Yapılmayanlar raporda.
##
## Komut satırı (Godot'da "--" sonrası):
##   --a2-port <n>, --a2-operator-gecikme-us <n>   A2-K93 (eklenti okur)
##   --sahte                     pencerede "SAHTE VERİ" etiketi (sahte A2 kaynağıyla çalışırken)
##   --ekran-goruntusu=<dizin>   senaryonun belirli anlarında ekran görüntüsü kaydet
##   --ekran-plani=<t:ad:sapma:egim:uzak;...>  ekran görüntüsü anları (sim zamanı s, ad, yörünge sapma°,
##                               eğim°, uzaklık m); verilmezse sahte senaryonun çizelgesi. Son andan 1 s sonra çıkılır.
##   --olcum=<dosya.json>        kare süresi ölçümünü yaz ve senaryo bitince çık
##   --max-fps=<n>               kare sınırı (0 = sınırsız; ölçümde 120 fps sınırı kare süresini maskeler)

const GOK_UFUK := Color(0.62, 0.74, 0.86)
const GOK_TEPE := Color(0.18, 0.38, 0.72)
const Yukleyici := preload("res://betikler/varlik_yukleyici.gd")
# Görsel varsayılanlar (yalnız çizim; A5-K101: senaryo su optiğini/ortam ışığını vermemişse): kıyı suyu.
const ARAC_ISIK_CARPANI := 0.35   # aracın ön ışıkları (A7 KHR_lights_punctual) su altında beyaza taşmasın
const GORSEL_BETA_D := [0.42, 0.085, 0.07]
const GORSEL_BETA_B := [0.30, 0.08, 0.065]
const GORSEL_B_SONSUZ := [0.008, 0.10, 0.16]
const GORSEL_ORTAM_ISIGI := [0.32, 0.42, 0.56]

@onready var a2: IdaA2Istemci = $A2
@onready var dunya: Node3D = $"Dünya"
@onready var su = $Su
@onready var isiklar: Node3D = $"Işıklar"
@onready var gostergeler = $"Göstergeler"
@onready var gorus: SubViewport = $"Operatör/Görüş"
@onready var kamera = $"Operatör/Görüş/Kamera"
@onready var hud = $HUD

var sahte := false
var kademe := 2
var sim_kipi := 1
var yukleyici = Yukleyici.new()  # A7 kümesi: doğrulama, şablonlar, örnekler, malzemeler
var govdeler := {}               # kimlik → Node3D
var statik: Node3D
var ortam := {}
var etkin := {}                  # ortam + görsel varsayılanlar (_etkin_ortam)
var gunes: DirectionalLight3D
var env_ust: Environment
var env_alt: Environment
var son_islem: MeshInstance3D
var son_islem_malzeme: ShaderMaterial
var su_altinda := false
var gunes_enerjisi := 1.2
var isik_olcegi := 1.0
var _kare_ms := 0.0
var _varsayilan_notu := ""

# Otomasyon (ekran görüntüsü ve ölçüm)
var _goruntu_dizini := ""
var _olcum_dosyasi := ""
var _plan: Array = []
var _plan_arg := ""
var _bitis_t := 116.0
var _kareler := PackedFloat32Array()
var _gpu := PackedFloat32Array()
var _cpu := PackedFloat32Array()
var _son_kare_us := 0
var _ilk_nesil := -1
var _kayit_suruyor := false
var _cikiliyor := false


func _ready() -> void:
	for a in OS.get_cmdline_user_args():
		if a == "--sahte":
			sahte = true
		elif a.begins_with("--ekran-goruntusu="):
			_goruntu_dizini = a.get_slice("=", 1)
		elif a.begins_with("--ekran-plani="):
			_plan_arg = a.get_slice("=", 1)
		elif a.begins_with("--olcum="):
			_olcum_dosyasi = a.get_slice("=", 1)
		elif a.begins_with("--max-fps="):
			Engine.max_fps = maxi(0, int(a.get_slice("=", 1)))
	hud.sahte(sahte)
	_ortami_hazirla()
	a2.sahne_kurulmali.connect(_sahne_kur)
	a2.govde_olustur.connect(_govde_olustur)
	a2.govde_kaldir.connect(_govde_kaldir)
	a2.ortam_degisti.connect(_ortam_uygula)
	a2.sifirlandi.connect(func(_n): gostergeler.izi_temizle())
	a2.malzeme_degisti.connect(_malzeme_degisti)
	a2.baglanti_degisti.connect(func(d, ayr): print("ida-viz: bağlantı: ", d, " ", ayr))
	su.kur(a2, kademe)
	_kademe_uygula()
	if _goruntu_dizini != "" or _olcum_dosyasi != "":
		_plan_hazirla()
	RenderingServer.viewport_set_measure_render_time(gorus.get_viewport_rid(), true)
	RenderingServer.viewport_set_measure_render_time(get_viewport().get_viewport_rid(), true)
	get_tree().auto_accept_quit = false


func _exit_tree() -> void:
	# Ağaç dışında tutulan varlık şablonları (sızıntı uyarısı olmasın).
	yukleyici.temizle()


func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_CLOSE_REQUEST:
		a2.kapat()  # veda(olagan), A2-K67
		get_tree().quit()


# ------------------------------------------------------------------------------------
# Ortam: güneş, gökyüzü, su altı
# ------------------------------------------------------------------------------------

func _ortami_hazirla() -> void:
	gunes = DirectionalLight3D.new()
	gunes.name = "Güneş"
	gunes.light_angular_distance = 0.53
	gunes.shadow_enabled = true
	gunes.directional_shadow_mode = DirectionalLight3D.SHADOW_PARALLEL_4_SPLITS
	gunes.directional_shadow_max_distance = 60.0
	isiklar.add_child(gunes)

	var gok := ProceduralSkyMaterial.new()
	gok.sky_top_color = GOK_TEPE
	gok.sky_horizon_color = GOK_UFUK
	gok.sky_curve = 0.12
	gok.ground_horizon_color = GOK_UFUK.darkened(0.15)
	gok.ground_bottom_color = Color(0.03, 0.12, 0.18)
	gok.sun_angle_max = 18.0
	gok.sun_curve = 0.08
	var sky := Sky.new()
	sky.sky_material = gok
	env_ust = Environment.new()
	env_ust.background_mode = Environment.BG_SKY
	env_ust.sky = sky
	env_ust.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env_ust.reflected_light_source = Environment.REFLECTION_SOURCE_SKY
	env_ust.tonemap_mode = Environment.TONE_MAPPER_AGX
	env_ust.tonemap_exposure = 1.05
	env_ust.fog_enabled = true
	env_ust.fog_light_color = GOK_UFUK
	env_ust.fog_density = 0.0006
	env_ust.fog_aerial_perspective = 0.85
	env_ust.fog_sky_affect = 0.0

	env_alt = Environment.new()
	env_alt.background_mode = Environment.BG_COLOR
	env_alt.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env_alt.reflected_light_source = Environment.REFLECTION_SOURCE_DISABLED
	env_alt.tonemap_mode = Environment.TONE_MAPPER_AGX
	env_alt.tonemap_exposure = 1.15

	son_islem = MeshInstance3D.new()
	son_islem.name = "SuAltıSonİşlem"
	var q := QuadMesh.new()
	q.size = Vector2(2, 2)
	son_islem.mesh = q
	son_islem.extra_cull_margin = 16384.0
	son_islem.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	son_islem_malzeme = ShaderMaterial.new()
	son_islem_malzeme.shader = load("res://golgelendiriciler/su_alti_son.gdshader")
	son_islem_malzeme.render_priority = 100
	son_islem.material_override = son_islem_malzeme
	son_islem.visible = false
	kamera.add_child(son_islem)
	kamera.environment = env_ust
	_ortam_uygula({})


func _kademe_uygula() -> void:
	var tam := kademe == 2
	gunes.shadow_enabled = tam
	env_ust.glow_enabled = tam
	env_ust.glow_intensity = 0.6
	env_ust.glow_bloom = 0.05
	env_ust.glow_hdr_threshold = 1.2
	env_alt.glow_enabled = tam
	env_alt.glow_intensity = 0.7
	env_alt.glow_bloom = 0.08
	env_alt.fog_enabled = not tam   # sade: Godot sisi; tam: son işlem
	gorus.msaa_3d = Viewport.MSAA_4X if tam else Viewport.MSAA_DISABLED
	gorus.screen_space_aa = Viewport.SCREEN_SPACE_AA_DISABLED
	for l in _isiklar(dunya):
		l.shadow_enabled = false


func _isiklar(n: Node) -> Array:
	var sonuc := []
	for c in n.get_children():
		if c is Light3D:
			sonuc.append(c)
		sonuc.append_array(_isiklar(c))
	return sonuc


func _sifir_mu(a) -> bool:
	if a == null or a.size() != 3:   # Array ya da PackedFloat64Array (eklenti)
		return true
	return float(a[0]) == 0.0 and float(a[1]) == 0.0 and float(a[2]) == 0.0


## A5-K101: senaryo su optiğini ve ortam ışığını vermezse sim 0 yazar ("kalibre edilmedi", berrak su);
## A2 bunu aynen taşır. 0 katsayılı su görüntüde siyah arka plan ve ışıksız sahne demektir; senaryo
## yazarı değer vermelidir (A5-K101). Vermediyse görselleştirici KENDİ görsel varsayılanını kullanır
## (yalnız çizim; fiziğe ve sensörlere dokunmaz) ve bunu günlüğe yazar. W5-K60: sözleşme değişmez.
func _etkin_ortam(o: Dictionary) -> Dictionary:
	var e := o.duplicate()
	var notlar := []
	if _sifir_mu(o.get("beta_d")) and _sifir_mu(o.get("beta_b")) and _sifir_mu(o.get("b_sonsuz")):
		e["beta_d"] = GORSEL_BETA_D
		e["beta_b"] = GORSEL_BETA_B
		e["b_sonsuz"] = GORSEL_B_SONSUZ
		notlar.append("su optiği (senaryo vermedi: A5-K101)")
	if _sifir_mu(o.get("ortam_isigi")):
		e["ortam_isigi"] = GORSEL_ORTAM_ISIGI
		notlar.append("ortam ışığı (senaryo vermedi: A5-K101)")
	if not o.is_empty() and not notlar.is_empty() and _varsayilan_notu != str(notlar):
		_varsayilan_notu = str(notlar)
		print("ida-viz: görsel varsayılan kullanıldı: ", ", ".join(notlar))
	return e


func _ortam_uygula(o: Dictionary) -> void:
	if not o.is_empty():
		ortam = o
	var e := _etkin_ortam(ortam)
	etkin = e
	var yon: Vector3 = Vector3(0.45, 0.78, 0.43)
	if e.has("gunes_yonu_n"):
		yon = IdaA2Istemci.ned_vektor(e["gunes_yonu_n"]).normalized()
	var isinim: float = e.get("gunes_isinimi", 900.0)
	# Işınım → Godot ışık enerjisi (A2-A06 açık): görsel uyum için 700 W/m² = 1,0 (tahmin).
	gunes_enerjisi = isinim / 700.0
	isik_olcegi = isinim / 900.0
	gunes.light_energy = gunes_enerjisi
	var yukari := Vector3.UP if absf(yon.dot(Vector3.UP)) < 0.99 else Vector3.FORWARD
	gunes.global_transform = Transform3D(Basis.looking_at(-yon, yukari), Vector3.ZERO)
	var amb: Array = e.get("ortam_isigi", GORSEL_ORTAM_ISIGI)
	var amb_renk := Color(float(amb[0]), float(amb[1]), float(amb[2]))
	var amb_enerji := maxf(amb_renk.r, maxf(amb_renk.g, amb_renk.b))
	env_ust.ambient_light_color = (amb_renk / maxf(amb_enerji, 1e-6)).linear_to_srgb()
	env_ust.ambient_light_energy = amb_enerji
	var bs: Array = e.get("b_sonsuz", GORSEL_B_SONSUZ)
	var bsr := Color(float(bs[0]), float(bs[1]), float(bs[2]))
	env_alt.background_color = (bsr * isik_olcegi).linear_to_srgb()
	env_alt.ambient_light_color = Color(0.45, 0.75, 0.85)
	env_alt.ambient_light_energy = amb_enerji * 0.8
	var bb: Array = e.get("beta_b", GORSEL_BETA_B)
	env_alt.fog_light_color = (bsr * isik_olcegi).linear_to_srgb()
	env_alt.fog_density = (float(bb[0]) + float(bb[1]) + float(bb[2])) / 3.0
	env_alt.fog_sky_affect = 1.0
	su.optik(e, isik_olcegi, GOK_UFUK, GOK_TEPE, yon)
	if son_islem_malzeme:
		son_islem_malzeme.set_shader_parameter("beta_d", su._v3(e.get("beta_d", GORSEL_BETA_D)))
		son_islem_malzeme.set_shader_parameter("beta_b", su._v3(bb))
		son_islem_malzeme.set_shader_parameter("b_sonsuz", su._v3(bs))
		son_islem_malzeme.set_shader_parameter("isik_olcegi", isik_olcegi)
	_statik_kur()


func _su_alti_guncelle() -> void:
	var k: Vector3 = kamera.global_position
	var yuzey := a2.su_yuksekligi(k.x, k.z) if kademe == 2 else 0.0
	var alt := k.y < yuzey
	if alt != su_altinda:
		su_altinda = alt
		kamera.environment = env_alt if alt else env_ust
		son_islem.visible = alt and kademe == 2
	var d := maxf(0.0, yuzey - k.y)
	su.kamerayi_izle(k, su_altinda)
	if su_altinda:
		son_islem_malzeme.set_shader_parameter("kamera_derinligi", d)
		if kademe != 2:
			# Sade: aşağı inen güneş ışığının soğurulması kameranın derinliğiyle (yaklaşım).
			var bd: Array = etkin.get("beta_d", GORSEL_BETA_D)
			gunes.light_color = Color(exp(-float(bd[0]) * d), exp(-float(bd[1]) * d), exp(-float(bd[2]) * d))
			su.alt_malzeme.set_shader_parameter("sonum_rengi", Vector3(gunes.light_color.r, gunes.light_color.g, gunes.light_color.b))
	else:
		gunes.light_color = Color(1.0, 0.97, 0.92)


# ------------------------------------------------------------------------------------
# Sahne kurma (A2-K13, A7-K53)
# ------------------------------------------------------------------------------------

func _sahne_kur(bilgi: Dictionary) -> void:
	kademe = int(bilgi.get("kademe_3b", 2))
	sim_kipi = int(bilgi.get("sim_kipi", 1))
	su.kur(a2, kademe)
	_kademe_uygula()
	var yol: String = bilgi["manifest_yolu"]
	var t0 := Time.get_ticks_msec()
	# A2-K13, A7-K53: özet hosgeldin'dekiyle karşılaştırılır, her şey sahne kurulmadan doğrulanır.
	var sonuc: Array = yukleyici.yukle(yol, str(bilgi.get("manifest_ozeti", "")))
	var durum: int = sonuc[0]
	var ozet: PackedByteArray = sonuc[1]
	if durum == 0:
		# A7-K30: etkin optik değerler (malzeme_optik) ilk kurulumda uygulanır.
		var mh: String = yukleyici.malzeme_uygula(bilgi.get("malzeme_optik", []))
		if mh != "":
			a2.protokol_hatasi_bildir(mh)
			return
	print("ida-viz: sahne kuruldu: durum %d, %d varlık, %d ms, manifest %s" % [durum, yukleyici.varliklar.size(),
		Time.get_ticks_msec() - t0, yukleyici.manifest_ozeti])
	gostergeler.izi_temizle()
	if durum == 0:
		var oo: Dictionary = bilgi.get("ortam", {})
		print("ida-viz: ortam (A2): tür %s, güneş ışınımı %s W/m², ortam ışığı %s, β_d %s, β_b %s, B∞ %s" % [
			oo.get("tur", "?"), oo.get("gunes_isinimi", "?"), oo.get("ortam_isigi", "?"), oo.get("beta_d", "?"),
			oo.get("beta_b", "?"), oo.get("b_sonsuz", "?")])
		_ortam_uygula(oo)
		_kademe_uygula()
	a2.sahne_sonucu(durum, ozet)


func _malzeme_degisti(girdiler: Array) -> void:
	var mh: String = yukleyici.malzeme_uygula(girdiler)  # A7-K30
	if mh != "":
		a2.protokol_hatasi_bildir(mh)


func _govde_olustur(kimlik: int, varlik: int, _tur: int) -> void:
	if varlik < 0 or varlik >= yukleyici.varliklar.size():
		a2.protokol_hatasi_bildir("varlık indeksi %d manifest dışında" % varlik)  # §3.3
		return
	var kok := Node3D.new()
	kok.name = "Gövde[%d]" % kimlik
	var o: Dictionary = yukleyici.ornek(varlik)
	kok.add_child(o["model"])
	dunya.add_child(kok)
	gostergeler.eksen_ekle(kok)
	for l in _isiklar(kok):
		l.shadow_enabled = false
		l.light_energy *= ARAC_ISIK_CARPANI
	govdeler[kimlik] = kok
	a2.govde_kaydet(kimlik, kok, o["kosullu"])


func _govde_kaldir(kimlik: int) -> void:
	if govdeler.has(kimlik):
		var n: Node = govdeler[kimlik]
		if is_instance_valid(n):
			n.queue_free()
		govdeler.erase(kimlik)


func _statik_kur() -> void:
	if statik and is_instance_valid(statik):
		statik.queue_free()
	statik = null
	var i := int(ortam.get("statik_varlik", 0xFFFF))
	if i == 0xFFFF or i < 0 or i >= yukleyici.varliklar.size():
		return
	statik = Node3D.new()
	statik.name = "Statik"
	statik.transform = IdaA2Istemci.ned_donusum(ortam["statik_p_n"], ortam["statik_q_nb"])
	statik.add_child(yukleyici.ornek(i)["model"])
	dunya.add_child(statik)


# ------------------------------------------------------------------------------------
# Kare döngüsü
# ------------------------------------------------------------------------------------

func _process(delta: float) -> void:
	_kare_ms = lerpf(_kare_ms, delta * 1000.0, 0.05)
	var arac := a2.arac_kimligi()
	var bilgi := {"t": a2.t_op_s()}
	if arac >= 0 and govdeler.has(arac):
		var n: Node3D = govdeler[arac]
		var ileri := -n.global_transform.basis.z
		var sapma_g := atan2(-ileri.x, -ileri.z)
		kamera.takip_et(n.global_position, sapma_g)
		var ned := a2.govde_ned(arac)
		if ned.size() == 3:
			bilgi["derinlik"] = ned[2]
		bilgi["yon"] = fposmod(rad_to_deg(atan2(ileri.x, -ileri.z)), 360.0)
		if a2.bagli():
			gostergeler.iz_ekle(n.global_position, delta)
	_su_alti_guncelle()
	var kipler := {1: "SIL", 2: "HIL 1x", 3: "HIL 1x + bariyer", 4: "tekrar izleme"}
	bilgi["kip"] = "%s · 3B %s · kamera %s" % [kipler.get(a2.sim_kipi(), "?"), "tam" if kademe == 2 else "sade", kamera.kip_adi()]
	var uyari := ""
	if not a2.bagli():
		uyari = "BAĞLANTI YOK — son sahne donduruldu (%s)" % a2.durum_metni()
	elif a2.tutarsiz():
		uyari = "dalga kümesi tutarsız (A2-K28)"
	bilgi["uyari"] = uyari
	var s := a2.sayaclar()
	bilgi["sag"] = "kare %s ms · %d fps\n127.0.0.1:%d · nesil %d%s" % [hud.ondalik(_kare_ms, 1),
		Engine.get_frames_per_second(), a2.port(), s.get("calisma_nesli", 0), "\nsu altında" if su_altinda else ""]
	hud.guncelle(bilgi)
	if not _plan.is_empty() or _olcum_dosyasi != "" or _plan_arg != "":
		_otomasyon()


func _unhandled_input(olay: InputEvent) -> void:
	if olay is InputEventKey and olay.pressed and not olay.echo:
		match (olay as InputEventKey).keycode:
			KEY_C:
				kamera.kip_degistir()
			KEY_G:
				gostergeler.eksenleri_ac(not gostergeler.eksenler_acik)
			KEY_I:
				gostergeler.izi_ac(not gostergeler.iz_acik)
			KEY_H:
				hud.gizle_goster()
			KEY_F12:
				var yol := "user://ida_%d.png" % Time.get_unix_time_from_system()
				get_viewport().get_texture().get_image().save_png(yol)
				print("ida-viz: ekran görüntüsü ", ProjectSettings.globalize_path(yol))


# ------------------------------------------------------------------------------------
# Otomasyon: ekran görüntüleri (kabul md. 5) ve kare süresi (kabul md. 6)
# ------------------------------------------------------------------------------------

func _plan_hazirla() -> void:
	# [sim zamanı (s), ad, yörünge sapma°, eğim°, uzaklık m]; zamanlar sahte senaryonun çizelgesi.
	if _plan_arg != "":
		for oge in _plan_arg.split(";", false):
			var a := oge.split(":")
			if a.size() == 5:
				_plan.append([float(a[0]), a[1], float(a[2]), float(a[3]), float(a[4])])
				_bitis_t = float(a[0]) + 1.0
		return
	_plan = [
		[6.0, "yuzey", -38.0, 13.0, 1.55],
		[17.5, "dalis", 45.0, -18.0, 1.35],
		[45.0, "tur", -125.0, 24.0, 2.6],
		[104.0, "hedef", -28.0, 14.0, 1.7],
	]


func _otomasyon() -> void:
	if _kayit_suruyor or _cikiliyor:
		return
	var s := a2.sayaclar()
	var nesil := int(s.get("calisma_nesli", 0))
	if not a2.bagli() or nesil <= 0:
		return
	if _ilk_nesil < 0:
		_ilk_nesil = nesil
	if nesil != _ilk_nesil:
		_bitir()
		return
	var t := a2.t_op_s()
	var simdi := Time.get_ticks_usec()
	if _son_kare_us > 0 and t > 5.0:
		var yakin := false
		for p in _plan:
			if absf(t - float(p[0])) < 1.2:
				yakin = true
		if not yakin:
			_kareler.append(float(simdi - _son_kare_us) / 1000.0)
			_gpu.append(RenderingServer.viewport_get_measured_render_time_gpu(gorus.get_viewport_rid())
				+ RenderingServer.viewport_get_measured_render_time_gpu(get_viewport().get_viewport_rid()))
			_cpu.append(RenderingServer.viewport_get_measured_render_time_cpu(gorus.get_viewport_rid())
				+ RenderingServer.viewport_get_measured_render_time_cpu(get_viewport().get_viewport_rid())
				+ RenderingServer.get_frame_setup_time_cpu())
	_son_kare_us = simdi
	if not _plan.is_empty():
		var p: Array = _plan[0]
		if t >= float(p[0]) - 0.9 and not p.has("ayarlandi"):
			kamera.yorunge_ayarla(float(p[2]), float(p[3]), float(p[4]))
			p.append("ayarlandi")
		if t >= float(p[0]):
			_plan.pop_front()
			if _goruntu_dizini != "":
				_goruntu_al(str(p[1]))
	if t >= _bitis_t:
		_bitir()


func _goruntu_al(ad: String) -> void:
	_kayit_suruyor = true
	await RenderingServer.frame_post_draw
	var img := get_viewport().get_texture().get_image()
	DirAccess.make_dir_recursive_absolute(_goruntu_dizini)
	var yol := _goruntu_dizini.path_join("%s_%s.png" % ["tam" if kademe == 2 else "sade", ad])
	img.save_png(yol)
	print("ida-viz: ekran görüntüsü: ", yol, " (t = %.2f s, su altında: %s)" % [a2.t_op_s(), su_altinda])
	_kayit_suruyor = false


func _yuzdelik(a: PackedFloat32Array, q: float) -> float:
	if a.is_empty():
		return 0.0
	var b := a.duplicate()
	b.sort()
	return b[clampi(int(round(q * (b.size() - 1))), 0, b.size() - 1)]


func _bitir() -> void:
	if _cikiliyor:
		return
	_cikiliyor = true
	if _olcum_dosyasi != "":
		var toplam := 0.0
		for x in _kareler:
			toplam += x
		var sonuc := {
			"kademe": "tam" if kademe == 2 else "sade",
			"pencere_piksel": [get_viewport().get_visible_rect().size.x, get_viewport().get_visible_rect().size.y],
			"gorus_piksel": [gorus.size.x, gorus.size.y],
			"kare_sayisi": _kareler.size(),
			"kare_ms_ortalama": toplam / maxf(1.0, _kareler.size()),
			"kare_ms_p50": _yuzdelik(_kareler, 0.5),
			"kare_ms_p95": _yuzdelik(_kareler, 0.95),
			"kare_ms_p99": _yuzdelik(_kareler, 0.99),
			"kare_ms_azami": _yuzdelik(_kareler, 1.0),
			"gpu_ms_p50": _yuzdelik(_gpu, 0.5),
			"gpu_ms_p99": _yuzdelik(_gpu, 0.99),
			"cizim_cpu_ms_p50": _yuzdelik(_cpu, 0.5),
			"cizim_cpu_ms_p99": _yuzdelik(_cpu, 0.99),
			"gpu": RenderingServer.get_video_adapter_name(),
			"godot": Engine.get_version_info()["string"],
			"max_fps": Engine.max_fps,
		}
		var f := FileAccess.open(_olcum_dosyasi, FileAccess.WRITE)
		if f:
			f.store_string(JSON.stringify(sonuc, "  ") + "\n")
			f.close()
		print("ida-viz: ölçüm: ", JSON.stringify(sonuc))
	a2.kapat()
	get_tree().quit()
