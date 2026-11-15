# MP6a: Godot 4.7.2'de görünmeyen pencerede kamera karesi üretimi (Mac ön turu).
# Atılabilir ölçüm prototipi; ürün koduna taşınmaz.
#
# Kullanıcı argümanları ("--" sonrasına):
#   --rol=olcum|ortu        ortu: başka pencereyi örten yardımcı süreç
#   --yol=a|b               a: olağan çizim döngüsü + SubViewport UPDATE_ONCE
#                           b: RenderingServer.force_draw(false, dt), ana iş parçacığından
#   --durum=gorunur|ortulu|gizli|simge|tamekran   (yalnız kayıt ve geçiş mantığı için)
#   --sure=<s>              ölçüm süresi (durum oluştuktan sonra)
#   --isinma=<s>            ısınma süresi (görünür pencerede, kayda girmez)
#   --cikti=<mutlak dizin>  kayıt dizini
#   --cozunurluk=<GxY>      kamera SubViewport boyutu (varsayılan 640x480)
#   --geri=esz|async        esz: ViewportTexture.get_image() (eşzamanlı)
#                           async: RenderingDevice.texture_get_data_async
#   --ornek=<n>             her n. kareyi ham RGBA olarak dök (0: dökme)
extends Node3D

const HZ := 30.0
const T_US := 1000000.0 / HZ
const BIT_SAYISI := 24
const BIT_PX := 8
const KUBRE_R := 0.25
const GOLGE_ESIK := 0.75  # gölge var: lum(gölge noktası) / lum(aydınlık nokta) <= 0.75 (ölçümden önce donduruldu)

var rol := "olcum"
var yol := "a"
var durum := "gorunur"
var sure_s := 300.0
var isinma_s := 8.0
var cikti := ""
var cw := 640
var ch := 480
var geri := "esz"
var ornek := 30
var cift := false

var vp: SubViewport
var vp_rid: RID
var cam: Camera3D
var kure: MeshInstance3D
var bit_ust := []
var bit_alt := []
var etiket: Label
var rd: RenderingDevice
var rd_tex: RID
var basliksiz := false
var zemin_ci: RID

var faz := "isinma"
var t_faz := 0            # fazın başladığı an (usec)
var t0 := 0               # yuva kökeni (usec)
var k_son := -1
var bekleyen := {}        # yol a: {k, t_sched, t_req}
var async_bekleyen := {}  # k -> true
var son_cizilen := -1
var t_son_cizim_degisim := 0
var durum_dogrulandi := false
var durum_dogrulama := ""

var f_kare: FileAccess
var f_durum: FileAccess
var sayac := {}
var gecikmeler := PackedInt64Array()
var geri_okuma := PackedInt64Array()
var get_image_sure := PackedInt64Array()
var t_son_durum_satiri := 0
var son_pf := 0
var son_fd := 0
var golge_ps := Vector3.ZERO
var golge_pl := Vector3.ZERO
var golge_ps_px := Vector2.ZERO
var golge_pl_px := Vector2.ZERO
var bitti := false


func _argumanlar() -> void:
	for a in OS.get_cmdline_user_args():
		if not a.begins_with("--") or a.find("=") < 0:
			continue
		var kv: PackedStringArray = a.substr(2).split("=", true, 1)
		var k: String = kv[0]
		var v: String = kv[1]
		match k:
			"rol": rol = v
			"yol": yol = v
			"durum": durum = v
			"sure": sure_s = float(v)
			"isinma": isinma_s = float(v)
			"cikti": cikti = v
			"cozunurluk":
				var p := v.split("x")
				cw = int(p[0])
				ch = int(p[1])
			"geri": geri = v
			"ornek": ornek = int(v)
			"cift": cift = v == "1"


func _ready() -> void:
	_argumanlar()
	basliksiz = DisplayServer.get_name() == "headless"
	if rol == "ortu":
		_ortu_kur()
		return
	DirAccess.make_dir_recursive_absolute(cikti)
	_sahne_kur()
	if yol == "a":
		RenderingServer.frame_post_draw.connect(_cizim_sonrasi)
	rd = RenderingServer.get_rendering_device()
	if rd != null:
		rd_tex = RenderingServer.texture_get_rd_texture(vp.get_texture().get_rid())
	for s in ["istek", "uretildi", "dongu_gecikti", "cizim_yok", "icerik_bayat", "icerik_gecersiz", "geri_okuma_bos", "async_gelmedi", "async_hata"]:
		sayac[s] = 0
	f_durum = FileAccess.open(cikti.path_join("durum.csv"), FileAccess.WRITE)
	f_durum.store_line("t_us,faz,process_frames,frames_drawn,d_process,d_drawn,pencere_kipi,odak,k_son,uretildi,istek")
	f_kare = FileAccess.open(cikti.path_join("kare.csv"), FileAccess.WRITE)
	f_kare.store_line("k,t_sched_us,t_req_us,t_ready_us,gecikme_us,geri_okuma_us,get_image_us,cozulen_id,md5,cx,cy,gx,gy,golge_orani,sonuc,frames_drawn,process_frames,ikinci_us")
	var t := Time.get_ticks_usec()
	t_faz = t
	t0 = t
	t_son_cizim_degisim = t
	_bilgi_yaz()


func _ortu_kur() -> void:
	var cl := CanvasLayer.new()
	add_child(cl)
	var r := ColorRect.new()
	r.color = Color(0.15, 0.15, 0.18)
	r.set_anchors_preset(Control.PRESET_FULL_RECT)
	cl.add_child(r)
	var l := Label.new()
	l.text = "IDA MP6a olcum ortusu\nkendiliginden kapanir"
	l.position = Vector2(20, 20)
	cl.add_child(l)
	get_tree().create_timer(sure_s).timeout.connect(func(): get_tree().quit())
	# Örtünün kipi koşu boyunca 15 s'de bir yazılır (tam ekranın sürdüğünün iç kanıtı).
	var zam := Timer.new()
	zam.wait_time = 15.0
	zam.autostart = true
	zam.timeout.connect(func(): print("ortu t_us=%d pencere_kipi=%d odak=%s" % [Time.get_ticks_usec(), DisplayServer.window_get_mode(), str(DisplayServer.window_is_focused())]))
	add_child(zam)
	get_tree().create_timer(3.0).timeout.connect(func(): print("ortu pencere_kipi=%d" % DisplayServer.window_get_mode()))


func _sahne_kur() -> void:
	# Ortam: sabit arka plan, düşük ortam ışığı, doğrusal ton eşleme; zamansal etki yok.
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.10, 0.16, 0.24)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(1, 1, 1)
	env.ambient_light_energy = 0.25
	env.tonemap_mode = Environment.TONE_MAPPER_LINEAR
	var we := WorldEnvironment.new()
	we.environment = env
	add_child(we)

	var isik := DirectionalLight3D.new()
	isik.rotation_degrees = Vector3(-55.0, 35.0, 0.0)
	isik.shadow_enabled = true
	isik.light_energy = 1.0
	add_child(isik)

	var zemin := MeshInstance3D.new()
	var pm := PlaneMesh.new()
	pm.size = Vector2(30, 30)
	zemin.mesh = pm
	var zm := StandardMaterial3D.new()
	zm.albedo_color = Color(0.7, 0.7, 0.7)
	zemin.material_override = zm
	add_child(zemin)

	var direk := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = Vector3(0.4, 2.0, 0.4)
	direk.mesh = bm
	var dm := StandardMaterial3D.new()
	dm.albedo_color = Color(0.4, 0.5, 0.9)
	direk.material_override = dm
	var direk_pos := Vector3(3.2, 1.0, -1.2)
	direk.position = direk_pos
	add_child(direk)

	kure = MeshInstance3D.new()
	var sm := SphereMesh.new()
	sm.radius = KUBRE_R
	sm.height = 2.0 * KUBRE_R
	sm.radial_segments = 48
	sm.rings = 24
	kure.mesh = sm
	var km := StandardMaterial3D.new()
	km.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	km.albedo_color = Color(1, 0, 0)
	kure.material_override = km
	kure.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
	add_child(kure)

	# Operatör görünümü (ana pencere).
	var op_cam := Camera3D.new()
	op_cam.position = Vector3(5.0, 5.0, 7.0)
	add_child(op_cam)
	op_cam.look_at(Vector3(0, 0.5, 0))
	op_cam.current = true
	var cl := CanvasLayer.new()
	add_child(cl)
	etiket = Label.new()
	etiket.position = Vector2(8, 8)
	cl.add_child(etiket)

	# Kamera SubViewport: operatörle aynı World3D (own_world_3d = false).
	vp = SubViewport.new()
	vp.size = Vector2i(cw, ch)
	vp.transparent_bg = false
	vp.msaa_3d = Viewport.MSAA_DISABLED
	vp.screen_space_aa = Viewport.SCREEN_SPACE_AA_DISABLED
	vp.use_taa = false
	vp.use_debanding = false
	vp.render_target_update_mode = SubViewport.UPDATE_DISABLED
	add_child(vp)
	vp_rid = vp.get_viewport_rid()
	cam = Camera3D.new()
	cam.fov = 60.0
	cam.position = Vector3(0.0, 3.0, 6.0)
	vp.add_child(cam)
	cam.look_at(Vector3(0, 0.5, 0))
	cam.current = true

	# Kare kimliği şeridi: üst satır k'nin 24 biti, alt satır tümleyeni (8x8 px blok).
	var kl := CanvasLayer.new()
	vp.add_child(kl)
	var n2 := Node2D.new()
	kl.add_child(n2)
	zemin_ci = RenderingServer.canvas_item_create()
	RenderingServer.canvas_item_set_parent(zemin_ci, n2.get_canvas_item())
	RenderingServer.canvas_item_add_rect(zemin_ci, Rect2(0, 0, BIT_SAYISI * BIT_PX, 2 * BIT_PX), Color(0, 0, 0))
	for i in BIT_SAYISI:
		for satir in 2:
			var ci := RenderingServer.canvas_item_create()
			RenderingServer.canvas_item_set_parent(ci, n2.get_canvas_item())
			RenderingServer.canvas_item_add_rect(ci, Rect2(i * BIT_PX, satir * BIT_PX, BIT_PX, BIT_PX), Color(1, 1, 1))
			# Kardeş sırası RS'de belirlenimci değil; bitler siyah zeminin üstünde kalsın.
			RenderingServer.canvas_item_set_z_index(ci, 1)
			if satir == 0:
				bit_ust.append(ci)
			else:
				bit_alt.append(ci)

	# Gölge sınaması: direğin orta yüksekliğindeki noktanın zemindeki gölgesi (gölgede)
	# ve 1,2 m kameraya doğru (+z) zemin noktası (aydınlık; direk gölge bandından ~0,7 m,
	# kürenin ve gölgesinin yolundan uzak; kamera ışını direk ve küreye çarpmaz).
	var d: Vector3 = -isik.global_transform.basis.z
	var orta := Vector3(direk_pos.x, 1.0, direk_pos.z)
	golge_ps = orta + d * (1.0 / -d.y)
	golge_ps.y = 0.0
	golge_pl = golge_ps + Vector3(0.0, 0.0, 1.2)


func _bilgi_yaz() -> void:
	var bilgi := {
		"rol": rol, "yol": yol, "durum": durum, "sure_s": sure_s, "isinma_s": isinma_s,
		"cozunurluk": [cw, ch], "geri": geri, "ornek": ornek, "cift": cift, "hz": HZ,
		"godot": Engine.get_version_info(),
		"os": OS.get_name(), "os_surum": OS.get_version(),
		"display_server": DisplayServer.get_name(),
		"adaptor": RenderingServer.get_video_adapter_name(),
		"adaptor_api": RenderingServer.get_video_adapter_api_version(),
		"rendering_method": ProjectSettings.get_setting("rendering/renderer/rendering_method"),
		"thread_model": ProjectSettings.get_setting("rendering/driver/threads/thread_model"),
		"frame_queue_size": ProjectSettings.get_setting("rendering/rendering_device/vsync/frame_queue_size"),
		"vsync_mode": ProjectSettings.get_setting("display/window/vsync/vsync_mode"),
		"max_fps": Engine.max_fps,
		"low_processor": OS.is_in_low_processor_usage_mode(),
		"pid": OS.get_process_id(),
		"rd_var": rd != null,
		"golge_ps": [golge_ps.x, golge_ps.y, golge_ps.z],
		"golge_pl": [golge_pl.x, golge_pl.y, golge_pl.z],
		"golge_esik": GOLGE_ESIK,
		"golge_ps_px": [cam.unproject_position(golge_ps).x, cam.unproject_position(golge_ps).y],
		"golge_pl_px": [cam.unproject_position(golge_pl).x, cam.unproject_position(golge_pl).y],
	}
	var f := FileAccess.open(cikti.path_join("bilgi.json"), FileAccess.WRITE)
	f.store_string(JSON.stringify(bilgi, "  "))
	f.close()


func _durum_ayarla(k: int) -> Vector2:
	var ph := TAU * float(k) / 240.0
	var pos := Vector3(2.0 * sin(ph), 0.6 + 0.3 * sin(TAU * float(k) / 90.0), 1.0 * cos(ph))
	# Doğrudan RenderingServer'a: düğüm dönüşüm bildirimi _process içinde henüz yayılmaz,
	# force_draw aynı çağrıda çizer.
	RenderingServer.instance_set_transform(kure.get_instance(), Transform3D(Basis(), pos))
	var id := k & 0xFFFFFF
	for i in BIT_SAYISI:
		var b := (id >> i) & 1
		RenderingServer.canvas_item_set_visible(bit_ust[i], b == 1)
		RenderingServer.canvas_item_set_visible(bit_alt[i], b == 0)
	return cam.unproject_position(pos)


func _process(_dt: float) -> void:
	if rol == "ortu" or bitti:
		return
	var now := Time.get_ticks_usec()
	var fd := Engine.get_frames_drawn()
	if fd != son_cizilen:
		son_cizilen = fd
		t_son_cizim_degisim = now
	match faz:
		"isinma":
			_yuva(now, false)
			if now - t_faz >= int(isinma_s * 1e6):
				faz = "gecis"
				t_faz = now
				var f := FileAccess.open(cikti.path_join("hazir"), FileAccess.WRITE)
				f.store_line(str(now))
				f.close()
				if durum == "simge":
					DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_MINIMIZED)
		"gecis":
			_yuva(now, false)
			var olustu := false
			if durum == "gorunur":
				olustu = now - t_faz >= 1000000 and now - t_son_cizim_degisim < 100000
				durum_dogrulama = "frames_drawn ilerliyor"
			else:
				# Godot'nun kendi ölçütü: Main::iteration çizimi atlıyorsa frames_drawn durur.
				olustu = now - t_son_cizim_degisim >= 1500000
				durum_dogrulama = "frames_drawn 1,5 s durdu"
			if olustu or now - t_faz >= 30000000:
				durum_dogrulandi = olustu
				if not olustu:
					durum_dogrulama = "30 s içinde oluşmadı"
				if durum == "simge":
					durum_dogrulama += "; window_get_mode=" + str(DisplayServer.window_get_mode())
				faz = "olcum"
				t_faz = now
				t0 = now
				k_son = -1
				bekleyen.clear()
				async_bekleyen.clear()
				son_pf = Engine.get_process_frames()
				son_fd = Engine.get_frames_drawn()
				t_son_durum_satiri = now
				f_durum.store_line("%d,olcum_basladi,%d,%d,0,0,%d,%s,-1,0,0" % [now, son_pf, son_fd, DisplayServer.window_get_mode(), str(DisplayServer.window_is_focused())])
		"olcum":
			# Yuvalar k = 0 .. sure*HZ - 1; son yuvanın zamanı geçince koşu biter.
			if now - t0 >= int(sure_s * 1e6):
				_bitir(now)
				return
			_yuva(now, true)
			if now - t_son_durum_satiri >= 1000000:
				_durum_satiri(now)
	if not basliksiz and (Engine.get_process_frames() % 10) == 0:
		etiket.text = "MP6a yol=%s durum=%s faz=%s\nk=%d uretildi=%d" % [yol, durum, faz, k_son, sayac["uretildi"]]


func _durum_satiri(now: int) -> void:
	var pf := Engine.get_process_frames()
	var fd := Engine.get_frames_drawn()
	f_durum.store_line("%d,olcum,%d,%d,%d,%d,%d,%s,%d,%d,%d" % [now, pf, fd, pf - son_pf, fd - son_fd, DisplayServer.window_get_mode(), str(DisplayServer.window_is_focused()), k_son, sayac["uretildi"], sayac["istek"]])
	son_pf = pf
	son_fd = fd
	t_son_durum_satiri = now


func _yuva(now: int, kayit: bool) -> void:
	var k := int(floor(float(now - t0) / T_US))
	if k <= k_son:
		return
	if kayit:
		for j in range(k_son + 1, k):
			sayac["istek"] += 1
			_dusur(j, int(t0 + j * T_US), "dongu_gecikti")
	if yol == "a" and not bekleyen.is_empty():
		if kayit:
			_dusur(bekleyen["k"], bekleyen["t_sched"], "cizim_yok")
		bekleyen.clear()
	k_son = k
	if kayit:
		sayac["istek"] += 1
	_iste(k, int(t0 + k * T_US), kayit)


func _iste(k: int, t_sched: int, kayit: bool) -> void:
	var gt := _durum_ayarla(k)
	var t_req := Time.get_ticks_usec()
	RenderingServer.viewport_set_update_mode(vp_rid, RenderingServer.VIEWPORT_UPDATE_ONCE)
	var istek := {"k": k, "t_sched": t_sched, "t_req": t_req, "gt": gt, "kayit": kayit}
	if yol == "b":
		RenderingServer.force_draw(false, 1.0 / HZ)
		_geri_oku(istek)
	else:
		bekleyen = istek


func _cizim_sonrasi() -> void:
	if bitti or bekleyen.is_empty():
		return
	var istek := bekleyen
	bekleyen = {}
	_geri_oku(istek)


func _geri_oku(istek: Dictionary) -> void:
	if geri == "async" and rd != null:
		var t_a := Time.get_ticks_usec()
		var err := rd.texture_get_data_async(rd_tex, 0, _async_geldi.bind(istek, t_a))
		if err != OK:
			if istek["kayit"]:
				_dusur(istek["k"], istek["t_sched"], "async_hata")
			return
		if istek["kayit"]:
			async_bekleyen[istek["k"]] = true
		return
	if geri == "rdesz" and rd != null:
		# Eşzamanlı, dönüşümsüz RGBA8: RenderingDevice.texture_get_data.
		var t1 := Time.get_ticks_usec()
		var ham := rd.texture_get_data(rd_tex, 0)
		var t2 := Time.get_ticks_usec()
		# Hazır olma zamanı ilk okumadan hemen sonra (ikinci okumadan önce) kaydedilir.
		istek["t_hazir"] = t2
		if ham.is_empty():
			if istek["kayit"]:
				_dusur(istek["k"], istek["t_sched"], "geri_okuma_bos")
			return
		if cift:
			# Yeni çizim olmadan yinelenen eşzamanlı geri okuma (aynı doku). Saf aktarım süresi değildir:
			# her eşzamanlı okuma yeni ara tampon, GPU kopyası ve _flush_and_stall_for_all_frames içerir.
			var t3 := Time.get_ticks_usec()
			rd.texture_get_data(rd_tex, 0)
			istek["ikinci_us"] = Time.get_ticks_usec() - t3
		if istek["kayit"]:
			_isle(istek, ham, t2 - t1, t2 - t1, cw, ch, Image.FORMAT_RGBA8)
		return
	var ta := Time.get_ticks_usec()
	var img: Image = vp.get_texture().get_image()
	var tb := Time.get_ticks_usec()
	if img == null or img.is_empty():
		if istek["kayit"]:
			_dusur(istek["k"], istek["t_sched"], "geri_okuma_bos")
		return
	var veri := img.get_data()
	var tc := Time.get_ticks_usec()
	istek["t_hazir"] = tc
	if cift:
		var t4 := Time.get_ticks_usec()
		vp.get_texture().get_image().get_data()
		istek["ikinci_us"] = Time.get_ticks_usec() - t4
	if istek["kayit"]:
		_isle(istek, veri, tc - ta, tb - ta, img.get_width(), img.get_height(), img.get_format())


func _async_geldi(veri: PackedByteArray, istek: Dictionary, t_a: int) -> void:
	if bitti or not istek["kayit"]:
		return
	async_bekleyen.erase(istek["k"])
	var t := Time.get_ticks_usec()
	istek["t_hazir"] = t
	if veri.size() != cw * ch * 4:
		_dusur(istek["k"], istek["t_sched"], "geri_okuma_bos")
		return
	_isle(istek, veri, t - t_a, -1, cw, ch, Image.FORMAT_RGBA8)


func _dusur(k: int, t_sched: int, neden: String) -> void:
	sayac[neden] += 1
	f_kare.store_line("%d,%d,,,,,,,,,,,,,%s,%d,%d," % [k, t_sched, neden, Engine.get_frames_drawn(), Engine.get_process_frames()])


func _lum(veri: PackedByteArray, w: int, bpp: int, p: Vector2) -> float:
	var x0 := int(p.x) - 2
	var y0 := int(p.y) - 2
	var t := 0.0
	for y in range(y0, y0 + 5):
		for x in range(x0, x0 + 5):
			var i := (y * w + x) * bpp
			t += 0.2126 * veri[i] + 0.7152 * veri[i + 1] + 0.0722 * veri[i + 2]
	return t / 25.0


func _isle(istek: Dictionary, veri: PackedByteArray, geri_us: int, getimg_us: int, w: int, h: int, fmt: int) -> void:
	var t_ready: int = istek.get("t_hazir", Time.get_ticks_usec())
	var k: int = istek["k"]
	# get_image(), saydam olmayan SubViewport'ta RGBA8 dokuyu CPU'da RGB8'e çevirir (texture_2d_get).
	var bpp := 4
	if fmt == Image.FORMAT_RGB8:
		bpp = 3
	if (fmt != Image.FORMAT_RGBA8 and fmt != Image.FORMAT_RGB8) or veri.size() != w * h * bpp:
		if sayac["icerik_gecersiz"] == 0:
			printerr("bicim uyusmazligi: fmt=%d boyut=%d w=%d h=%d" % [fmt, veri.size(), w, h])
		_dusur(k, istek["t_sched"], "icerik_gecersiz")
		return
	# Kare kimliği.
	var ust := 0
	var alt := 0
	var gecerli := true
	for i in BIT_SAYISI:
		var x := i * BIT_PX + BIT_PX / 2
		var a := veri[(BIT_PX / 2 * w + x) * bpp]
		var b := veri[((BIT_PX + BIT_PX / 2) * w + x) * bpp]
		if a > 200 and b < 55:
			ust |= (1 << i)
		elif a < 55 and b > 200:
			alt |= (1 << i)
		else:
			gecerli = false
	if gecerli and (ust | alt) != 0xFFFFFF:
		gecerli = false
	var sonuc := "uretildi"
	if not gecerli:
		sonuc = "icerik_gecersiz"
	elif ust != (k & 0xFFFFFF):
		sonuc = "icerik_bayat"
	sayac[sonuc] += 1
	# İçerik özeti.
	var hc := HashingContext.new()
	hc.start(HashingContext.HASH_MD5)
	hc.update(veri)
	var md5 := hc.finish().hex_encode()
	# Kürenin piksel ağırlık merkezi (kırmızı maske), yer gerçeği izdüşümü çevresinde.
	var gt: Vector2 = istek["gt"]
	var r := 40
	var sx := 0.0
	var sy := 0.0
	var n := 0
	var x0 := maxi(0, int(gt.x) - r)
	var x1 := mini(w - 1, int(gt.x) + r)
	var y0 := maxi(2 * BIT_PX, int(gt.y) - r)
	var y1 := mini(h - 1, int(gt.y) + r)
	for y in range(y0, y1 + 1):
		var sat := y * w
		for x in range(x0, x1 + 1):
			var i := (sat + x) * bpp
			if veri[i] > 200 and veri[i + 1] < 60 and veri[i + 2] < 60:
				sx += x + 0.5
				sy += y + 0.5
				n += 1
	var cx := -1.0
	var cy := -1.0
	if n > 0:
		cx = sx / n
		cy = sy / n
	# Gölge oranı.
	var ps := cam.unproject_position(golge_ps)
	var pl := cam.unproject_position(golge_pl)
	var oran := _lum(veri, w, bpp, ps) / maxf(_lum(veri, w, bpp, pl), 1e-3)
	var gecikme := t_ready - int(istek["t_req"])
	if sonuc == "uretildi":
		gecikmeler.append(gecikme)
		geri_okuma.append(geri_us)
		if getimg_us >= 0:
			get_image_sure.append(getimg_us)
	f_kare.store_line("%d,%d,%d,%d,%d,%d,%d,%d,%s,%.3f,%.3f,%.3f,%.3f,%.4f,%s,%d,%d,%d" % [
		k, istek["t_sched"], istek["t_req"], t_ready, gecikme, geri_us, getimg_us, ust if gecerli else -1, md5,
		cx, cy, gt.x, gt.y, oran, sonuc, Engine.get_frames_drawn(), Engine.get_process_frames(), istek.get("ikinci_us", -1)])
	if ornek > 0 and k % ornek == 0:
		var f := FileAccess.open(cikti.path_join("kare_%07d.%s" % [k, "rgb" if bpp == 3 else "rgba"]), FileAccess.WRITE)
		f.store_buffer(veri)
		f.close()


func _bitir(now: int) -> void:
	bitti = true
	if yol == "a" and not bekleyen.is_empty():
		_dusur(bekleyen["k"], bekleyen["t_sched"], "cizim_yok")
		bekleyen.clear()
	# Döngü süre sonunu atlayarak geldiyse verilemeyen son yuvalar.
	var n_yuva := int(round(sure_s * HZ))
	for j in range(k_son + 1, n_yuva):
		sayac["istek"] += 1
		_dusur(j, int(t0 + j * T_US), "dongu_gecikti")
	for k in async_bekleyen.keys():
		_dusur(k, int(t0 + k * T_US), "async_gelmedi")
	_durum_satiri(now)
	f_kare.close()
	f_durum.close()
	var ozet := {
		"sayac": sayac,
		"durum_dogrulandi": durum_dogrulandi,
		"durum_dogrulama": durum_dogrulama,
		"olcum_baslangic_us": t0,
		"olcum_bitis_us": now,
		"son_k": k_son,
	}
	var f := FileAccess.open(cikti.path_join("ozet.json"), FileAccess.WRITE)
	f.store_string(JSON.stringify(ozet, "  "))
	f.close()
	for ci in bit_ust + bit_alt + [zemin_ci]:
		RenderingServer.free_rid(ci)
	get_tree().quit()
