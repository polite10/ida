extends Camera3D
## Operatör kamerası (07 §2.2 `Operatör`): araç takip ve serbest kipler.
##   C          kip değiştir (takip ↔ serbest)
##   takip:     sol fare sürükle → aracın çevresinde dön, tekerlek → uzaklık
##   serbest:   sağ fare sürükle → bakış, W/A/S/D ileri/yan, Q/E aşağı/yukarı, Shift hızlı

enum Kip { TAKIP, SERBEST }

var kip := Kip.TAKIP
var hedef := Vector3.ZERO           # takip edilen gövdenin Godot konumu
var hedef_sapma := 0.0              # takip edilen gövdenin baş açısı (Godot, rad)
var hedef_var := false
var yorunge_sapma := deg_to_rad(-35.0)   # aracın arkasına göre
var yorunge_egim := deg_to_rad(16.0)     # yatayın üstü +
var mesafe := 1.9
var _yumusak_hedef := Vector3.ZERO
var _yumusak_sapma := 0.0
var _ilk := true
var _serbest_sapma := 0.0
var _serbest_egim := 0.0
var _surukle_sol := false
var _surukle_sag := false


func takip_et(konum: Vector3, sapma: float) -> void:
	hedef = konum
	hedef_sapma = sapma
	hedef_var = true


func kip_adi() -> String:
	return "takip" if kip == Kip.TAKIP else "serbest"


func kip_degistir() -> void:
	if kip == Kip.TAKIP:
		kip = Kip.SERBEST
		var e := global_transform.basis.get_euler()
		_serbest_egim = e.x
		_serbest_sapma = e.y
	else:
		kip = Kip.TAKIP
		_ilk = true


## Ekran görüntüsü otomasyonu için: takip kipinde yörüngeyi doğrudan ayarlar.
func yorunge_ayarla(sapma_derece: float, egim_derece: float, uzaklik: float) -> void:
	kip = Kip.TAKIP
	yorunge_sapma = deg_to_rad(sapma_derece)
	yorunge_egim = deg_to_rad(egim_derece)
	mesafe = uzaklik
	_ilk = true


func _unhandled_input(olay: InputEvent) -> void:
	if olay is InputEventMouseButton:
		var b := olay as InputEventMouseButton
		if b.button_index == MOUSE_BUTTON_LEFT:
			_surukle_sol = b.pressed
		elif b.button_index == MOUSE_BUTTON_RIGHT:
			_surukle_sag = b.pressed
		elif b.pressed and b.button_index == MOUSE_BUTTON_WHEEL_UP:
			mesafe = maxf(0.5, mesafe * 0.9)
		elif b.pressed and b.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			mesafe = minf(60.0, mesafe * 1.1)
	elif olay is InputEventMouseMotion:
		var m := olay as InputEventMouseMotion
		if kip == Kip.TAKIP and _surukle_sol:
			yorunge_sapma -= m.relative.x * 0.006
			yorunge_egim = clampf(yorunge_egim + m.relative.y * 0.005, deg_to_rad(-80.0), deg_to_rad(85.0))
		elif kip == Kip.SERBEST and _surukle_sag:
			_serbest_sapma -= m.relative.x * 0.004
			_serbest_egim = clampf(_serbest_egim - m.relative.y * 0.004, deg_to_rad(-89.0), deg_to_rad(89.0))


func _process(delta: float) -> void:
	if kip == Kip.TAKIP:
		_takip(delta)
	else:
		_serbest(delta)


func _takip(delta: float) -> void:
	if not hedef_var:
		return
	if _ilk:
		_yumusak_hedef = hedef
		_yumusak_sapma = hedef_sapma
		_ilk = false
	var k := 1.0 - exp(-delta * 6.0)
	_yumusak_hedef = _yumusak_hedef.lerp(hedef, k)
	_yumusak_sapma = lerp_angle(_yumusak_sapma, hedef_sapma, 1.0 - exp(-delta * 2.0))
	var aci := _yumusak_sapma + yorunge_sapma
	# Godot'da baş açısı ψ_G: ileri = (−sin ψ, 0, −cos ψ); arkadan bakış için +.
	var yatay := Vector3(sin(aci), 0.0, cos(aci))
	var konum := _yumusak_hedef + (yatay * cos(yorunge_egim) + Vector3.UP * sin(yorunge_egim)) * mesafe
	global_position = konum
	look_at(_yumusak_hedef + Vector3(0, 0.03, 0), Vector3.UP)


func _serbest(delta: float) -> void:
	var hiz := 2.5
	if Input.is_key_pressed(KEY_SHIFT):
		hiz *= 4.0
	var yon := Vector3.ZERO
	if Input.is_key_pressed(KEY_W):
		yon.z -= 1.0
	if Input.is_key_pressed(KEY_S):
		yon.z += 1.0
	if Input.is_key_pressed(KEY_A):
		yon.x -= 1.0
	if Input.is_key_pressed(KEY_D):
		yon.x += 1.0
	if Input.is_key_pressed(KEY_Q):
		yon.y -= 1.0
	if Input.is_key_pressed(KEY_E):
		yon.y += 1.0
	basis = Basis.from_euler(Vector3(_serbest_egim, _serbest_sapma, 0.0))
	if yon != Vector3.ZERO:
		global_position += global_transform.basis * yon.normalized() * hiz * delta
