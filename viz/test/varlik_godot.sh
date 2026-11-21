#!/bin/bash
# Varlık yükleyicisi sınaması (Godot başsız): viz/proje/test/varlik_testi.gd.
# Ortam: GODOT (yoksa ${IDA_CACHE:-$HOME/.cache/ida}/godot/4.7.2/...). Godot yoksa 77 döner
# (CTest bunu "atlandı" sayar).
set -u
BURADA=$(cd "$(dirname "$0")" && pwd)
KOK=$(cd "$BURADA/../.." && pwd)
GODOT=${GODOT:-${IDA_CACHE:-$HOME/.cache/ida}/godot/4.7.2/app/Godot.app/Contents/MacOS/Godot}
if [ ! -x "$GODOT" ]; then
  echo "Godot bulunamadı: $GODOT (atlandı)"
  exit 77
fi
if [ ! -f "$KOK/viz/proje/.godot/extension_list.cfg" ]; then
  "$GODOT" --headless --path "$KOK/viz/proje" --import > /dev/null 2>&1 || true
fi
"$GODOT" --headless --path "$KOK/viz/proje" --script res://test/varlik_testi.gd -- \
  "$KOK/viz/test/varlik" "$KOK/assets/manifest.schema.json" 2>&1 | grep -v '^ *at: \|GDScript backtrace\|^ *\[[0-9]'
exit "${PIPESTATUS[0]}"
