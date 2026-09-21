#!/usr/bin/env python3
"""Fırat Kebap menü + admin. Çalıştır: python server.py"""

from __future__ import annotations

import base64
import json
import mimetypes
import os
import re
import secrets
import threading
import time
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import unquote, urlparse

ROOT = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(ROOT, "data")
UPLOADS = os.path.join(ROOT, "uploads")
MENU_PATH = os.path.join(DATA, "menu.json")
CONFIG_PATH = os.path.join(DATA, "config.json")
SESSIONS_PATH = os.path.join(DATA, "sessions.json")
PORT = int(os.environ.get("PORT", "4173"))

os.makedirs(UPLOADS, exist_ok=True)
os.makedirs(DATA, exist_ok=True)

LOCK = threading.Lock()
FAILS: dict[str, list[float]] = {}

BLOCKED = {"server.py", "start.bat"}
BLOCKED_DIRS = {"data"}


def read_json(path, default):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except OSError:
        return default


def write_json(path, payload):
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(payload, f, ensure_ascii=False, indent=2)
    os.replace(tmp, path)


def pin() -> str:
    cfg = read_json(CONFIG_PATH, {"pin": "1234"})
    return str(cfg.get("pin") or "1234")


def sessions() -> dict:
    return read_json(SESSIONS_PATH, {})


def save_sessions(data: dict) -> None:
    now = time.time()
    clean = {k: v for k, v in data.items() if v > now}
    write_json(SESSIONS_PATH, clean)


def clip(value, n: int) -> str:
    return str(value or "")[:n]


def sanitize_menu(menu: dict) -> dict:
    sections = []
    for section in menu.get("sections") or []:
        items = []
        for item in section.get("items") or []:
            image = clip(item.get("image"), 200)
            if image and not image.startswith("/uploads/"):
                image = ""
            try:
                price = round(float(item.get("price") or 0), 2)
            except (TypeError, ValueError):
                price = 0
            items.append(
                {
                    "id": clip(item.get("id"), 64) or secrets.token_hex(4),
                    "name": clip(item.get("name"), 120),
                    "desc": clip(item.get("desc"), 240),
                    "price": max(0, price),
                    "image": image,
                }
            )
        sections.append(
            {
                "id": clip(section.get("id"), 64) or secrets.token_hex(4),
                "title": clip(section.get("title"), 80),
                "items": items,
            }
        )
    return {
        "name": clip(menu.get("name"), 80),
        "subtitle": clip(menu.get("subtitle"), 120),
        "kicker": clip(menu.get("kicker"), 80),
        "sections": sections,
    }


def valid_token(token: str | None) -> bool:
    if not token:
        return False
    with LOCK:
        data = sessions()
        exp = data.get(token)
        if not exp or exp < time.time():
            return False
        return True


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=ROOT, **kwargs)

    def log_message(self, fmt, *args):
        print("[%s] %s" % (self.log_date_time_string(), fmt % args))

    def _json(self, code, payload):
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _body(self, limit=4_000_000):
        length = int(self.headers.get("Content-Length") or 0)
        if length > limit:
            return None
        raw = self.rfile.read(length) if length else b""
        try:
            return json.loads(raw.decode("utf-8") or "{}")
        except json.JSONDecodeError:
            return None

    def _auth(self) -> str | None:
        header = self.headers.get("Authorization") or ""
        if header.lower().startswith("bearer "):
            return header.split(" ", 1)[1].strip()
        return None

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/api/menu":
            with LOCK:
                menu = read_json(MENU_PATH, {"sections": []})
            self._json(200, menu)
            return
        if parsed.path == "/api/me":
            ok = valid_token(self._auth())
            self._json(200 if ok else 401, {"ok": ok})
            return
        super().do_GET()

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path == "/api/login":
            self._login()
            return
        if parsed.path == "/api/upload":
            self._upload()
            return
        self._json(404, {"error": "not_found"})

    def do_PUT(self):
        parsed = urlparse(self.path)
        if parsed.path == "/api/menu":
            self._save_menu()
            return
        self._json(404, {"error": "not_found"})

    def _login(self):
        ip = self.client_address[0]
        now = time.time()
        recent = [t for t in FAILS.get(ip, []) if now - t < 600]
        if len(recent) >= 8:
            self._json(429, {"error": "too_many"})
            return
        data = self._body(limit=2000)
        if data is None:
            self._json(400, {"error": "bad_json"})
            return
        given = str(data.get("pin") or "")
        if given != pin():
            recent.append(now)
            FAILS[ip] = recent
            self._json(401, {"error": "bad_pin"})
            return
        FAILS[ip] = []
        token = secrets.token_urlsafe(24)
        with LOCK:
            data_s = sessions()
            data_s[token] = now + 60 * 60 * 24 * 7
            save_sessions(data_s)
        self._json(200, {"token": token})

    def _save_menu(self):
        if not valid_token(self._auth()):
            self._json(401, {"error": "auth"})
            return
        data = self._body()
        if not isinstance(data, dict) or "sections" not in data:
            self._json(400, {"error": "bad_menu"})
            return
        with LOCK:
            write_json(MENU_PATH, sanitize_menu(data))
        self._json(200, {"ok": True})

    def _upload(self):
        if not valid_token(self._auth()):
            self._json(401, {"error": "auth"})
            return
        data = self._body()
        if not isinstance(data, dict):
            self._json(400, {"error": "bad_json"})
            return
        raw = str(data.get("data") or "")
        match = re.match(r"data:(image/(jpeg|png|webp));base64,(.+)$", raw, re.I | re.S)
        if not match:
            self._json(400, {"error": "bad_image"})
            return
        ext = match.group(2).lower().replace("jpeg", "jpg")
        try:
            blob = base64.b64decode(match.group(3))
        except Exception:
            self._json(400, {"error": "bad_image"})
            return
        if len(blob) > 2_500_000:
            self._json(400, {"error": "too_big"})
            return
        name = secrets.token_hex(8) + "." + ext
        path = os.path.join(UPLOADS, name)
        with open(path, "wb") as f:
            f.write(blob)
        self._json(200, {"url": "/uploads/" + name})

    def translate_path(self, path):
        path = unquote(urlparse(path).path)
        rel = path.lstrip("/").replace("\\", "/")
        first = rel.split("/", 1)[0].lower()
        base = os.path.basename(rel).lower()
        if first in BLOCKED_DIRS or base in BLOCKED:
            return os.path.join(ROOT, "__blocked__")
        return super().translate_path(path)

    def guess_type(self, path):
        ctype, _ = mimetypes.guess_type(path)
        return ctype or "application/octet-stream"

    def end_headers(self):
        if self.path.startswith("/api/"):
            pass
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def main():
    os.chdir(ROOT)
    httpd = ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    print(f"Menü:  http://127.0.0.1:{PORT}")
    print(f"Admin: http://127.0.0.1:{PORT}/admin.html")
    httpd.serve_forever()


if __name__ == "__main__":
    main()
