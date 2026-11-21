extends SceneTree
## Varlık yükleyicisi sınaması (sol #6–#10; A7-K06, K10–K14, K19–K23, K28, K30, K53; A2-K13).
## Başsız koşar:
##   godot --headless --path viz/proje --script res://test/varlik_testi.gd -- <viz/test/varlik> <assets/manifest.schema.json>
## Çıkış kodu: 0 hepsi geçti, 1 başarısız.

const Yukleyici := preload("res://betikler/varlik_yukleyici.gd")

var _denetim := 0
var _hata := 0


func denetle(kosul: bool, ad: String) -> void:
	_denetim += 1
	if not kosul:
		_hata += 1
		printerr("DENETİM BAŞARISIZ: ", ad)


func _init() -> void:
	var args := OS.get_cmdline_user_args()
	var kok: String = args[0]
	var sema: String = args[1]
	var gecici := OS.get_cache_dir().path_join("ida_varlik_testi_%d" % Time.get_ticks_usec())

	# sol #6 (A7-K20): sıfır olmayan REF, ayrı kökler. Görsel düğüm REF ile aynı yerde:
	# gövde düğümüne göre yerel konumu 0 olmalı.
	var y = Yukleyici.new()
	var r: Array = y.yukle(kok.path_join("deneme_ref/manifest.json"), "", sema)
	denetle(r[0] == 0, "deneme_ref yüklenir (durum %d, %s)" % [r[0], y.son_neden])
	if r[0] == 0:
		var o: Dictionary = y.ornek(0)
		var model: Node3D = o["model"]
		var kutu: Node3D = model.find_child("kutu", true, false)
		var t := Transform3D.IDENTITY
		var n: Node = kutu
		while n != null:
			t = (n as Node3D).transform * t
			if n == model:
				break
			n = n.get_parent()
		print("REF sınaması: kutu'nun gövdeye göre konumu ", t.origin)
		denetle(t.origin.length() < 1e-5, "A7-K20: C·M·(p − t_REF) (konum %s)" % t.origin)
		# sol #10 (A7-K30): maskeli etkin değerler glTF malzemesine uygulanır, geri alınır.
		var mi := kutu as MeshInstance3D
		var mat := mi.mesh.surface_get_material(0) as StandardMaterial3D
		var ilk := mat.albedo_color
		var hata: String = y.malzeme_uygula([{"malzeme_indeksi": 0, "maske": 7, "taban_rengi": [1.0, 0.0, 0.0, 1.0],
			"metaliklik": 0.5, "puruzluluk": 0.25}])
		denetle(hata == "", "malzeme uygulanır (%s)" % hata)
		denetle(mat.albedo_color.is_equal_approx(Color(1, 0, 0, 1).linear_to_srgb()), "taban rengi uygulandı (%s)" % mat.albedo_color)
		denetle(is_equal_approx(mat.metallic, 0.5) and is_equal_approx(mat.roughness, 0.25), "metaliklik/pürüzlülük uygulandı")
		y.malzeme_uygula([])
		denetle(mat.albedo_color.is_equal_approx(ilk), "maske kalkınca glTF değeri geri gelir")
		denetle(y.malzeme_uygula([{"malzeme_indeksi": 5, "maske": 1, "taban_rengi": [1, 1, 1, 1]}]) != "",
			"manifest dışı malzeme indeksi reddedilir")
		model.free()

	# sol #8 (A2-K13, A7-K53): manifest özeti hosgeldin'dekinden farklıysa hiçbir varlık
	# yüklenmez; hesaplanan özet hata sonucuyla döner.
	var y2 = Yukleyici.new()
	var gercek := Yukleyici.sha256(FileAccess.get_file_as_bytes(kok.path_join("deneme_ref/manifest.json")))
	var r2: Array = y2.yukle(kok.path_join("deneme_ref/manifest.json"), "00".repeat(32), sema)
	denetle(r2[0] != 0, "özet uyuşmazlığında başarı bildirilmez (durum %d)" % r2[0])
	denetle(y2.varliklar.is_empty(), "özet uyuşmazlığında varlık yüklenmez")
	denetle(r2[1] == gercek, "hesaplanan özet döner")

	# sol #7 (A7-K53): olumsuz kümeler; beklenen hazir.durum kodu
	for d in [["bozuk_olcek", 5], ["bozuk_uri", 5], ["bozuk_sema", 2], ["bozuk_rol", 5], ["bozuk_malzeme", 5]]:
		var yb = Yukleyici.new()
		var rb: Array = yb.yukle(kok.path_join(d[0]).path_join("manifest.json"), "", sema)
		print("%s: durum %d (%s)" % [d[0], rb[0], yb.son_neden])
		denetle(rb[0] == d[1], "%s → hazir.durum %d (bulunan %d)" % [d[0], d[1], rb[0]])
		yb.temizle()

	# sol #9 (A7-K06): önbellek isabetinde de dosya denetlenir; kaynak dizini önbellek kimliğinde.
	DirAccess.make_dir_recursive_absolute(gecici.path_join("deneme_ref"))
	for ad in ["manifest.json", "deneme.glb"]:
		DirAccess.copy_absolute(kok.path_join("deneme_ref").path_join(ad), gecici.path_join("deneme_ref").path_join(ad))
	var y3 = Yukleyici.new()
	denetle(y3.yukle(gecici.path_join("deneme_ref/manifest.json"), "", sema)[0] == 0, "geçici kopya yüklenir")
	var r3: Array = y3.yukle(kok.path_join("deneme_ref/manifest.json"), "", sema)
	denetle(r3[0] == 0 and not y3.son_onbellekten, "başka dizindeki aynı manifest önbellekten gelmez")
	var y4 = Yukleyici.new()
	denetle(y4.yukle(gecici.path_join("deneme_ref/manifest.json"), "", sema)[0] == 0, "ilk yükleme")
	var f := FileAccess.open(gecici.path_join("deneme_ref/deneme.glb"), FileAccess.READ_WRITE)
	f.seek(f.get_length() - 1)
	f.store_8(0x5A)
	f.close()
	var r4: Array = y4.yukle(gecici.path_join("deneme_ref/manifest.json"), "", sema)
	denetle(r4[0] == 4, "önbellek varken değişen .glb yakalanır (durum %d)" % r4[0])
	DirAccess.remove_absolute(gecici.path_join("deneme_ref/deneme.glb"))
	var r5: Array = y4.yukle(gecici.path_join("deneme_ref/manifest.json"), "", sema)
	denetle(r5[0] == 3, "önbellek varken silinen .glb yakalanır (durum %d)" % r5[0])
	for yy in [y, y2, y3, y4]:
		yy.temizle()

	print("varlik_testi: %d denetim, %d başarısız" % [_denetim, _hata])
	quit(0 if _hata == 0 else 1)
