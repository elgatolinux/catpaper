import io
import json
import os
import shlex
import shutil
import subprocess
import threading
import time
from pathlib import Path

from PIL import Image
from PySide6.QtCore import (QFileSystemWatcher, QMetaObject, QObject, Qt,
                            QTimer, Property, Signal, Slot)
from PySide6.QtWidgets import QFileDialog

from search import fetch, run_search_thread
from thumbs import make_thumb, work_extract

HOME = Path(os.path.expanduser("~"))
CACHE_ROOT = HOME / ".cache" / "wallpaper_picker"
CONFIG_PATH = HOME / ".config" / "wallpaper_picker" / "config.conf"
DEFAULT_DIR = HOME / "Pictures" / "Wallpapers"

TRANSITIONS = ["simple", "fade", "left", "right", "top", "bottom", "wipe",
               "grow", "center", "outer", "random", "wave"]


class Config:
    def __init__(self, path=None):
        self.raw = {}
        p = Path(path) if path else Path(CONFIG_PATH)
        try:
            for line in p.read_text(errors="ignore").splitlines():
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                k, _, v = line.partition("=")
                self.raw[k.strip()] = v.strip()
        except Exception:
            pass

    def get(self, key, default=""):
        return self.raw.get(key, default)


class Paths(QObject):
    @Property(str, constant=True)
    def logDir(self):
        return str(CACHE_ROOT)

    @Slot(str)
    def getCacheDir(self, name):
        return str(HOME / ".cache" / name)

    @Slot(str)
    def getRunDir(self, name):
        return str(CACHE_ROOT / name)


class Backend(QObject):
    monitorsReady = Signal(list)
    wallpaperApplied = Signal(str)
    noticeChanged = Signal()
    downloadStateChanged = Signal()
    wallpaperDirChanged = Signal()
    localCacheReset = Signal()
    thumbsRefreshed = Signal()
    searchRefreshed = Signal()
    markersRefreshed = Signal()
    srcRefreshed = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.cfg = Config()
        wd = os.environ.get("WALLPAPER_DIR") or self.cfg.get("wallpaper_dir") or str(DEFAULT_DIR)
        self._wallpaper_dir = str(Path(wd).expanduser())
        self._notice = ""
        self._downloading = False
        self._downloading_name = ""
        self._setter = None
        self._stop_event = threading.Event()
        self._pause_event = threading.Event()
        self._search_thread = None
        self._extracting = False
        self._notice_timer = QTimer(self)
        self._notice_timer.setSingleShot(True)
        self._notice_timer.timeout.connect(self._clear_notice)

        self._watcher = QFileSystemWatcher(self)
        self._watcher.directoryChanged.connect(self._on_dir_changed)

    # ------------------------------------------------------------------ props
    @Property(str, notify=noticeChanged)
    def notice(self):
        return self._notice

    @Property(bool, notify=downloadStateChanged)
    def isDownloading(self):
        return self._downloading

    @Property(str, notify=downloadStateChanged)
    def downloadingName(self):
        return self._downloading_name

    @Property(str, constant=True)
    def homeDir(self):
        return "file://" + str(HOME)

    @Property(str, constant=True)
    def thumbDir(self):
        return "file://" + str(CACHE_ROOT / "thumbs")

    @Property(str, constant=True)
    def searchDir(self):
        return "file://" + str(CACHE_ROOT / "search_thumbs")

    @Property(str, notify=wallpaperDirChanged)
    def wallpaperDir(self):
        return self._wallpaper_dir

    def _set_notice(self, msg):
        self._notice = msg
        self.noticeChanged.emit()
        QMetaObject.invokeMethod(self, "_schedule_clear", Qt.ConnectionType.QueuedConnection)

    @Slot()
    def _schedule_clear(self):
        self._notice_timer.start(3500)

    def _clear_notice(self):
        self._notice = ""
        self.noticeChanged.emit()

    def _set_downloading(self, name="", value=False):
        self._downloading = value
        self._downloading_name = name
        self.downloadStateChanged.emit()

    # ---------------------------------------------------------- dirs & config
    @Slot()
    def ensureDirs(self):
        for d in (CACHE_ROOT / "thumbs", CACHE_ROOT / "search_thumbs",
                  CACHE_ROOT / "colors_markers", Path(self._wallpaper_dir)):
            try:
                d.mkdir(parents=True, exist_ok=True)
            except Exception:
                pass
        self._setup_watchers()

    def _watch_dirs(self):
        return {str(v): sig for v, sig in [
            (CACHE_ROOT / "thumbs", self.thumbsRefreshed),
            (CACHE_ROOT / "search_thumbs", self.searchRefreshed),
            (CACHE_ROOT / "colors_markers", self.markersRefreshed),
            (Path(self._wallpaper_dir), self.srcRefreshed),
        ]}

    def _setup_watchers(self):
        watched = self._watcher.directories()
        for p in self._watch_dirs():
            if p not in watched:
                self._watcher.addPath(p)

    def _on_dir_changed(self, path):
        sig = self._watch_dirs().get(path)
        if sig is not None:
            sig.emit()

    # ------------------------------------------------------- wallpaper dir
    @Slot()
    def chooseWallpaperDir(self):
        chosen = QFileDialog.getExistingDirectory(None, "Selecciona la carpeta de wallpapers",
                                                  self._wallpaper_dir)
        if not chosen:
            return
        chosen = str(Path(chosen).expanduser())
        if Path(chosen) == Path(self._wallpaper_dir):
            return
        self._wallpaper_dir = chosen
        self._persist_config()
        self._reset_local_cache()
        self._setup_watchers()
        self.wallpaperDirChanged.emit()
        self.localCacheReset.emit()
        self.extractColors()

    def _persist_config(self):
        try:
            p = Path(CONFIG_PATH)
            p.parent.mkdir(parents=True, exist_ok=True)
            lines = p.read_text(errors="ignore").splitlines() if p.exists() else []
            out, found = [], False
            for line in lines:
                if line.strip().startswith("wallpaper_dir"):
                    out.append("wallpaper_dir=%s" % self._wallpaper_dir)
                    found = True
                else:
                    out.append(line)
            if not found:
                out.append("wallpaper_dir=%s" % self._wallpaper_dir)
            p.write_text("\n".join(out) + "\n")
        except Exception:
            pass

    def _reset_local_cache(self):
        for d in ("thumbs", "colors_markers"):
            shutil.rmtree(str(CACHE_ROOT / d), ignore_errors=True)
        (CACHE_ROOT / "thumbs").mkdir(parents=True, exist_ok=True)
        (CACHE_ROOT / "colors_markers").mkdir(parents=True, exist_ok=True)

    # ---------------------------------------------------------------- monitors
    @Slot()
    def loadMonitors(self):
        names = self._monitors()
        self.monitorsReady.emit(names)

    def _monitors(self):
        candidates = [
            (["niri", "msg", "outputs"], lambda d: [o.get("name") for o in d if o.get("name")]),
            (["hyprctl", "monitors", "-j"], lambda d: [o.get("name") for o in d if o.get("name")]),
            (["swaymsg", "-t", "get_outputs", "-r"],
             lambda d: [o.get("name") for o in d if o.get("name") and o.get("active")]),
        ]
        for argv, proj in candidates:
            try:
                r = subprocess.run(argv, capture_output=True, text=True, timeout=3)
                if r.returncode != 0:
                    continue
                names = [n for n in proj(json.loads(r.stdout)) if n]
                if names:
                    return names
            except Exception:
                continue
        return []

    # --------------------------------------------------------------- wallpaper
    @Slot(str, bool, str)
    def applyWallpaper(self, safeName, isVideo, outputs):
        threading.Thread(target=self._apply_worker, args=(safeName, isVideo, outputs),
                         daemon=True).start()

    def _apply_worker(self, safeName, isVideo, outputs):
        try:
            src = Path(self._wallpaper_dir) / safeName
            from_search = not src.is_file()
            if from_search:
                url = self._map_url(safeName)
                if not url:
                    self._set_notice("No se encuentra el archivo: " + safeName)
                    return
                self._set_downloading(safeName, True)
                try:
                    src = self._download_search_image(safeName, url)
                except Exception as e:
                    self._set_notice("Descarga fallida: %s" % e)
                    self._set_downloading()
                    return
                finally:
                    if self._downloading:
                        self._set_downloading()

            self._run_setter(src, outputs, isVideo)

            try:
                shutil.copyfile(src, CACHE_ROOT / "current_wallpaper.png")
            except Exception:
                pass

            if isVideo:
                thumb = CACHE_ROOT / "thumbs" / ("000_" + src.name)
                wal_img = thumb if thumb.exists() else src
            else:
                wal_img = src
                self._maybe_thumb(src, safeName)
            self._run_wal(wal_img)

            reload_cmd = self.cfg.get("reload_cmd")
            if reload_cmd:
                subprocess.Popen(reload_cmd, shell=True,
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.wallpaperApplied.emit(safeName)
        except Exception as e:
            self._set_notice("No se pudo aplicar: %s" % e)

    def _maybe_thumb(self, src, safeName):
        thumb = CACHE_ROOT / "thumbs" / safeName
        try:
            if not thumb.exists():
                make_thumb(src, thumb)
        except Exception:
            pass

    def _map_url(self, name):
        try:
            mf = CACHE_ROOT / "search_map.txt"
            for line in mf.read_text(errors="ignore").splitlines():
                fname, _, url = line.partition("|")
                if fname.strip() == name.strip():
                    return url.strip()
        except Exception:
            pass
        return ""

    def _download_search_image(self, safeName, url):
        src = Path(self._wallpaper_dir) / safeName
        tmp = src.with_name(src.name + ".tmp")
        data = fetch(url, timeout=90)
        if data[:4] == b"RIFF" and data[8:12] == b"WEBP":
            im = Image.open(io.BytesIO(data)).convert("RGB")
            im.save(tmp, "JPEG", quality=92)
        else:
            tmp.write_bytes(data)
        os.replace(tmp, src)
        thumb = CACHE_ROOT / "thumbs" / safeName
        try:
            if not thumb.exists():
                make_thumb(src, thumb)
        except Exception:
            pass
        return src

    def _run_setter(self, img, outputs, isVideo):
        if isVideo:
            self._apply_video(img, outputs)
            return
        setter, mode = self._detect_setter()

        if mode == "custom":
            cmd = shlex.split(self.cfg.get("wpaper_cmd").replace("%s", shlex.quote(str(img))))
            subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            return

        if setter is None:
            raise RuntimeError("no se detectó ningún setter (awww/swww/swaybg/feh/nitrogen)")

        if mode == "sway":
            self._pkill("swaybg")
            cmd = [setter, "-i", str(img), "-m", "fill"]
        elif mode == "feh":
            self._pkill("feh")
            cmd = [setter, "--bg-fill", str(img)]
        elif mode == "nitrogen":
            self._pkill("nitrogen")
            cmd = [setter, "--set-scaled", str(img)]
        else:  # awww / swww
            if outputs and outputs != "all":
                cmd = self._awww_args(setter, img) + ["-o", outputs]
            else:
                cmd = self._awww_args(setter, img)

        subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def _awww_args(self, setter, img):
        t = TRANSITIONS[int(time.time() * 1000) % len(TRANSITIONS)]
        return [setter, "img", str(img), "--transition-type", t,
                "--transition-pos", "0.5,0.5", "--transition-fps", "144",
                "--transition-duration", "1"]

    def _apply_video(self, img, outputs):
        mpvpaper = shutil.which("mpvpaper")
        if not mpvpaper:
            raise RuntimeError("mpvpaper no está instalado")
        opts = "loop --no-audio --hwdec=auto --video-sync=display-resample " \
               "--interpolation --tscale=oversample"
        targets = ["*"] if (not outputs or outputs == "all") else outputs.split(",")
        for mon in targets:
            subprocess.Popen([mpvpaper, "-o", opts, mon, str(img)],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def _pkill(self, name):
        try:
            subprocess.run(["pkill", "-f", name], capture_output=True)
        except Exception:
            pass

    def _detect_setter(self):
        wanted = self.cfg.get("setter", "auto").lower()
        if self.cfg.get("wpaper_cmd"):
            return None, "custom"

        order = []
        if wanted and wanted != "auto":
            order.append(wanted)
        else:
            order.extend(["awww", "swww", "swaybg", "feh", "nitrogen"])

        for name in order:
            exe = shutil.which(name)
            if not exe:
                continue
            if name in ("awww", "swww"):
                if self._daemon_running() or self._try_init(exe):
                    return exe, "awww"
            elif name == "swaybg":
                return exe, "sway"
            elif name == "feh":
                return exe, "feh"
            elif name == "nitrogen":
                return exe, "nitrogen"
        return None, None

    def _daemon_running(self):
        try:
            r = subprocess.run(["pgrep", "-f", "awww-daemon|swww-daemon"],
                               capture_output=True, text=True)
            return r.returncode == 0
        except Exception:
            return False

    def _try_init(self, exe):
        try:
            r = subprocess.run([exe, "init"], capture_output=True, timeout=10)
            return r.returncode == 0
        except Exception:
            return False

    def _run_wal(self, img):
        wal = shutil.which("wal")
        if not wal:
            return
        user_cmd = self.cfg.get("wal_cmd")
        try:
            if user_cmd:
                cmd = shlex.split(user_cmd.replace("%s", shlex.quote(str(img))))
            else:
                cmd = [wal, "-i", str(img), "-q"]
            log = CACHE_ROOT / "wal.log"
            with open(log, "a") as f:
                f.write("\n[%s] %s\n" % (time.strftime("%H:%M:%S"), " ".join(cmd)))
                subprocess.Popen(cmd, stdout=f, stderr=f)
        except Exception:
            pass

    # ------------------------------------------------------------------ search
    @Slot(bool)
    def setSearchPaused(self, paused):
        if paused:
            self._pause_event.set()
        else:
            self._pause_event.clear()

    @Slot(str)
    def search(self, query):
        if not (query or "").strip():
            return
        self._pause_event.clear()
        self._stop_event.clear()
        self._search_thread = run_search_thread(
            query, str(CACHE_ROOT / "search_thumbs"),
            str(CACHE_ROOT / "search_map.txt"),
            self._pause_event, self._stop_event,
            lambda e: self._set_notice("Búsqueda: " + e))

    # ----------------------------------------------------------------- thumbs
    @Slot()
    def extractColors(self):
        if self._extracting:
            return
        self._extracting = True
        threading.Thread(target=self._extract_worker, daemon=True).start()

    def _extract_worker(self):
        try:
            work_extract(self._wallpaper_dir, CACHE_ROOT / "thumbs",
                         CACHE_ROOT / "colors_markers")
        finally:
            self._extracting = False

    @Slot()
    def shutdown(self):
        self._stop_event.set()
        self._set_notice("")
