import shutil
import subprocess
from pathlib import Path

from PIL import Image

VIDEO_EXT = {".mp4", ".mkv", ".mov", ".webm", ".m4v"}
IMAGE_EXT = {".jpg", ".jpeg", ".png", ".webp", ".gif"}


def make_thumb(src, dst):
    src = Path(src)
    dst = Path(dst)
    try:
        im = Image.open(src)
        if getattr(im, "n_frames", 1) > 1:
            im.seek(0)
        im = im.convert("RGB")
        im.thumbnail((10000, 420), Image.LANCZOS)
        im.save(dst, "JPEG", quality=82)
        return True
    except Exception:
        return False


def dominant_hex(img_or_path):
    try:
        im = Image.open(img_or_path) if not hasattr(img_or_path, "convert") else img_or_path
        if getattr(im, "n_frames", 1) > 1:
            im.seek(0)
        im = im.convert("RGB").resize((1, 1), Image.LANCZOS)
        hsv = im.convert("HSV")
        h, s, v = hsv.getpixel((0, 0))
        s = min(255, int(s * 2))
        im2 = Image.new("HSV", (1, 1))
        im2.putpixel((0, 0), (h, s, v))
        r, g, b = im2.convert("RGB").getpixel((0, 0))
        return "%02X%02X%02X" % (r, g, b)
    except Exception:
        return ""


def _video_thumb(src, dst):
    dst = Path(dst)
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        return False
    try:
        subprocess.run(
            [ffmpeg, "-y", "-ss", "0", "-i", str(src), "-frames:v", "1",
             "-vf", "scale=420:-2", "-f", "image2", str(dst)],
            capture_output=True, timeout=20)
        return dst.exists()
    except Exception:
        return False


def source_entry(f):
    entry = Path(f)
    if not entry.is_file():
        return None
    n = entry.name
    if n.startswith("."):
        return None
    ext = entry.suffix.lower()
    is_video = ext in VIDEO_EXT or n.startswith("000_")
    if not is_video and ext not in IMAGE_EXT:
        return None
    thumb_name = ("000_" + n) if is_video else n
    return entry, thumb_name, is_video


def work_extract(src_dir, thumbs_dir, markers_dir):
    src_dir = Path(src_dir)
    thumbs_dir = Path(thumbs_dir)
    markers_dir = Path(markers_dir)
    thumbs_dir.mkdir(parents=True, exist_ok=True)
    markers_dir.mkdir(parents=True, exist_ok=True)

    done = 0
    try:
        entries = sorted(src_dir.iterdir())
    except Exception:
        return 0

    for f in entries:
        try:
            data = source_entry(f)
            if data is None:
                continue
            entry, thumb_name, is_video = data
            thumb_path = thumbs_dir / thumb_name
            marker_exists = len(list(markers_dir.glob(thumb_name + "_HEX_*"))) > 0

            if not thumb_path.exists():
                if is_video:
                    _video_thumb(entry, thumb_path)
                else:
                    make_thumb(entry, thumb_path)
            if not thumb_path.exists():
                continue
            if marker_exists:
                continue
            hexc = dominant_hex(thumb_path)
            if hexc:
                (markers_dir / (thumb_name + "_HEX_" + hexc)).touch()
                done += 1
        except Exception:
            continue
    return done
