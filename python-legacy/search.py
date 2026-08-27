import json
import re
import threading
import time
import urllib.parse
import urllib.request
from pathlib import Path

UA = {
    "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                  "(KHTML, like Gecko) Chrome/126.0 Safari/537.36",
}


def fetch(url, timeout=15):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.read()


def _vqd(html):
    for pat in (r'"vqd":"([^"]+)"', r"vqd=([\d-]+)"):
        m = re.search(pat, html)
        if m:
            return m.group(1)
    return None


def get_vqd(query):
    html = fetch("https://duckduckgo.com/?q=%s&iax=images&ia=images"
                 % urllib.parse.quote_plus(query)).decode("utf-8", errors="ignore")
    return _vqd(html)


def worker(query, out_dir, map_file, pause_event, stop_event, on_error):
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    map_file = Path(map_file)
    max_results = 24

    try:
        vqd = get_vqd(query)
        if not vqd:
            raise RuntimeError("no se obtuvo vqd de DuckDuckGo")

        api = ("https://duckduckgo.com/i.js?l=us-en&o=json&q=%s&vqd=%s&f=,,,&p=1"
               % (urllib.parse.quote_plus(query), urllib.parse.quote(vqd)))
        data = fetch(api).decode("utf-8", errors="ignore")
        results = json.loads(data).get("results", [])[:max_results]
        if not results:
            raise RuntimeError("sin resultados")

        map_file.write_text("")
        seen = set()
        for i, res in enumerate(results):
            while pause_event.is_set():
                if stop_event.is_set():
                    return
                time.sleep(0.2)
            if stop_event.is_set():
                return

            name = "ddg_%04d.jpg" % i
            path = out_dir / name
            if not path.exists():
                thumb_url = res.get("thumbnail") or ""
                if not thumb_url:
                    continue
                try:
                    path.write_bytes(fetch(thumb_url, timeout=12))
                except Exception:
                    continue
            if name in seen:
                continue
            seen.add(name)
            with open(map_file, "a") as f:
                f.write("%s|%s\n" % (name, res.get("image", "")))
    except Exception as e:
        on_error(str(e))


def run_search_thread(query, out_dir, map_file, pause_event, stop_event, on_error):
    t = threading.Thread(target=worker, args=(query, out_dir, map_file, pause_event,
                                              stop_event, on_error), daemon=True)
    t.start()
    return t
