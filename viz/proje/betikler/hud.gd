extends CanvasLayer
## Operatör göstergeleri: sim zamanı, derinlik, kip etiketi, bağlantı durumu, kare süresi,
## sahte veri etiketi. Yalnız pencerede; sahneye ve kamera karesine girmez.

var _sol: Label
var _rozet: Label
var _uyari: Label
var _sag: Label
var _yardim: Label
var _arka: PanelContainer


func _ready() -> void:
	_arka = PanelContainer.new()
	var stil := StyleBoxFlat.new()
	stil.bg_color = Color(0.02, 0.04, 0.06, 0.62)
	stil.corner_radius_top_left = 6
	stil.corner_radius_top_right = 6
	stil.corner_radius_bottom_left = 6
	stil.corner_radius_bottom_right = 6
	stil.content_margin_left = 14
	stil.content_margin_right = 14
	stil.content_margin_top = 10
	stil.content_margin_bottom = 10
	_arka.add_theme_stylebox_override("panel", stil)
	_arka.position = Vector2(18, 18)
	add_child(_arka)
	var kutu := VBoxContainer.new()
	kutu.add_theme_constant_override("separation", 4)
	_arka.add_child(kutu)
	var ust := HBoxContainer.new()
	ust.add_theme_constant_override("separation", 10)
	kutu.add_child(ust)
	var baslik := _etiket(22, Color(0.92, 0.96, 1.0))
	baslik.text = "İDA-0"
	ust.add_child(baslik)
	_rozet = _etiket(16, Color(0.06, 0.04, 0.0))
	_rozet.text = " SAHTE VERİ "
	var rs := StyleBoxFlat.new()
	rs.bg_color = Color(1.0, 0.68, 0.1)
	rs.corner_radius_top_left = 4
	rs.corner_radius_top_right = 4
	rs.corner_radius_bottom_left = 4
	rs.corner_radius_bottom_right = 4
	rs.content_margin_left = 6
	rs.content_margin_right = 6
	_rozet.add_theme_stylebox_override("normal", rs)
	_rozet.visible = false
	ust.add_child(_rozet)
	_sol = _etiket(17, Color(0.85, 0.92, 0.98))
	kutu.add_child(_sol)
	_uyari = _etiket(17, Color(1.0, 0.45, 0.35))
	kutu.add_child(_uyari)
	_sag = _etiket(14, Color(0.75, 0.82, 0.88))
	_sag.anchor_left = 1.0
	_sag.anchor_right = 1.0
	_sag.offset_left = -330
	_sag.offset_right = -18
	_sag.offset_top = 18
	_sag.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	add_child(_sag)
	_yardim = _etiket(14, Color(0.75, 0.82, 0.88, 0.85))
	_yardim.anchor_top = 1.0
	_yardim.anchor_bottom = 1.0
	_yardim.offset_left = 18
	_yardim.offset_top = -40
	_yardim.text = "C kamera kipi · G eksenler · I iz · H göstergeleri gizle"
	add_child(_yardim)


func _etiket(boyut: int, renk: Color) -> Label:
	var l := Label.new()
	var s := LabelSettings.new()
	s.font_size = boyut
	s.font_color = renk
	s.outline_size = 3
	s.outline_color = Color(0, 0, 0, 0.55)
	l.label_settings = s
	return l


func sahte(acik: bool) -> void:
	_rozet.visible = acik


static func ondalik(x: float, basamak: int) -> String:
	return (("%." + str(basamak) + "f") % x).replace(".", ",")


static func zaman_metni(t: float) -> String:
	var isaret := "-" if t < 0.0 else ""
	t = absf(t)
	var dk := int(t / 60.0)
	var sn := t - dk * 60.0
	return "%s%02d:%s" % [isaret, dk, ("%05.2f" % sn).replace(".", ",")]


func guncelle(bilgi: Dictionary) -> void:
	var satirlar := PackedStringArray()
	satirlar.append("sim zamanı  " + zaman_metni(bilgi.get("t", 0.0)))
	if bilgi.has("derinlik"):
		satirlar.append("derinlik    " + ondalik(bilgi["derinlik"], 2) + " m")
	else:
		satirlar.append("derinlik    —")
	if bilgi.has("yon"):
		satirlar.append("baş         %03d°" % int(round(bilgi["yon"])))
	satirlar.append("kip         " + str(bilgi.get("kip", "")))
	_sol.text = "\n".join(satirlar)
	var uyari := str(bilgi.get("uyari", ""))
	_uyari.text = uyari
	_uyari.visible = uyari != ""
	_sag.text = str(bilgi.get("sag", ""))


func gizle_goster() -> void:
	visible = not visible
