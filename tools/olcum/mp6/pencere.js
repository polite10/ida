// MP6a yardımcısı (JXA, osascript -l JavaScript ile çalışır).
// Başka uygulamaya Apple Event göndermez; yalnız AppKit ve CoreGraphics
// sorgularını kendi sürecinde yapar. Erişilebilirlik ya da otomasyon izni gerekmez.
//
// Kullanım:
//   osascript -l JavaScript pencere.js durum <pid>    -> JSON: gizli mi, ön planda mı, ekrandaki pencereler
//   osascript -l JavaScript pencere.js gizle <pid>    -> NSRunningApplication.hide
//   osascript -l JavaScript pencere.js goster <pid>   -> NSRunningApplication.unhide
ObjC.import('AppKit');
ObjC.import('CoreGraphics');

function windowsOf(pid, onScreenOnly) {
  // kCGWindowListOptionOnScreenOnly = 1, kCGWindowListOptionAll = 0; kCGNullWindowID = 0
  var opt = onScreenOnly ? 1 : 0;
  var ref = $.CGWindowListCopyWindowInfo(opt | 16 /* ExcludeDesktopElements */, 0);
  var arr = ObjC.deepUnwrap(ObjC.castRefToObject(ref));
  var out = [];
  for (var i = 0; i < arr.length; i++) {
    var w = arr[i];
    out.push({
      order: i,
      pid: w.kCGWindowOwnerPID,
      layer: w.kCGWindowLayer,
      onscreen: w.kCGWindowIsOnscreen ? true : false,
      alpha: w.kCGWindowAlpha,
      id: w.kCGWindowNumber,
      b: w.kCGWindowBounds
    });
  }
  if (pid === null) return out;
  return out.filter(function (w) { return w.pid === pid; });
}

function run(argv) {
  var cmd = argv[0];
  var pid = argv.length > 1 ? parseInt(argv[1], 10) : null;
  var app = pid !== null ? $.NSRunningApplication.runningApplicationWithProcessIdentifier(pid) : null;
  if (cmd === 'gizle') {
    return JSON.stringify({ ok: app.hide });
  }
  if (cmd === 'goster') {
    return JSON.stringify({ ok: app.unhide });
  }
  if (cmd === 'durum') {
    var res = {
      t: Date.now() / 1000.0,
      pid: pid,
      hidden: app ? app.hidden : null,
      active: app ? app.active : null,
      onscreen_all: windowsOf(null, true).filter(function (w) { return w.layer === 0 || w.layer === 3; })
    };
    return JSON.stringify(res);
  }
  return JSON.stringify({ hata: 'bilinmeyen komut' });
}
