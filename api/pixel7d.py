#!/usr/bin/env python3
"""pixel7d: the test door of the Pixel 7 on chromium-vm.

Listens on the Tailscale address (default 100.102.86.30:8897), every request carries X-Mantra-Token.
One device, one lock: requests that touch the phone run one at a time.

  GET  /health                         booted?, model, Android version, installed test apps
  GET  /api                            this list as JSON
  POST /install        body=apk        adb install -r -g (grants runtime permissions)
  POST /uninstall?package=P
  POST /launch?package=P[&activity=A]  start the app (launcher intent unless an activity is named)
  POST /stop?package=P
  POST /clear?package=P                pm clear
  GET  /screenshot                     image/png
  GET  /logcat?lines=N[&package=P]     text
  POST /tap?x=&y=   /swipe?x1=&y1=&x2=&y2=&ms=   /text?s=   /key?code=
  POST /shell          body=command    adb shell output (exit code in X-Exit-Code)
  POST /test           body=apk ?package=P&seconds=S[&activity=A]
                       install, clear the log, launch, wait S s, then JSON: crashed, anr, screenshot (base64 png),
                       logcat of the app, the app's pid, time to first frame if Android logged one
  POST /instrument?runner=pkg/runner[&args=...]   am instrument -w -r (upload the test apk with /install first)
  POST /reset                          factory reset (wipes /data, reboots the container)
"""
import base64, fcntl, json, os, re, subprocess, threading, time
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

SERIAL = os.environ.get("PIXEL7_SERIAL", "127.0.0.1:5555")
HOST = os.environ.get("PIXEL7_HOST", "100.102.86.30")
PORT = int(os.environ.get("PIXEL7_PORT", "8897"))
TOKEN_FILE = os.path.expanduser(os.environ.get("PIXEL7_TOKEN_FILE", "~/.config/pixel7/token"))
WORK = os.path.expanduser("~/.cache/pixel7")
RUN_SH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "pixel7", "run.sh")
os.makedirs(WORK, exist_ok=True)


class DeviceLock:
    """One user of the phone at a time, across threads and across processes (pixel7d and pixel7-jobs)."""
    def __init__(self):
        self.t = threading.Lock()

    def __enter__(self):
        self.t.acquire()
        self.f = open(os.path.join(WORK, "device.lock"), "w")
        fcntl.flock(self.f, fcntl.LOCK_EX)

    def __exit__(self, *a):
        fcntl.flock(self.f, fcntl.LOCK_UN)
        self.f.close()
        self.t.release()


LOCK = DeviceLock()


def token():
    with open(TOKEN_FILE) as f:
        return f.read().strip()


def adb(*args, timeout=120, data=None):
    subprocess.run(["adb", "connect", SERIAL], capture_output=True, timeout=15)
    p = subprocess.run(["adb", "-s", SERIAL, *args], capture_output=True, timeout=timeout, input=data)
    return p.returncode, p.stdout, p.stderr


def sh(cmd, timeout=120):
    rc, out, err = adb("shell", cmd, timeout=timeout)
    return rc, (out + err).decode("utf-8", "replace")


def prop(name):
    return sh(f"getprop {name}", 15)[1].strip()


def booted():
    try:
        return prop("sys.boot_completed") == "1"
    except Exception:
        return False


def screenshot():
    rc, out, err = adb("exec-out", "screencap", "-p", timeout=30)
    if rc or not out.startswith(b"\x89PNG"):
        raise RuntimeError("screencap failed: " + err.decode("utf-8", "replace"))
    return out


def install(apk_bytes):
    path = os.path.join(WORK, f"upload-{int(time.time()*1000)}.apk")
    with open(path, "wb") as f:
        f.write(apk_bytes)
    try:
        rc, out, err = adb("install", "-r", "-g", "-t", path, timeout=300)
        text = (out + err).decode("utf-8", "replace").strip()
        pkg = apk_package(path)
        return rc == 0 and "Success" in text, text, pkg
    finally:
        os.remove(path)


def apk_package(path):
    for tool in ("aapt2", "aapt"):
        try:
            p = subprocess.run([tool, "dump", "badging", path], capture_output=True, text=True, timeout=30)
            m = re.search(r"package: name='([^']+)'", p.stdout)
            if m:
                return m.group(1)
        except FileNotFoundError:
            continue
    return None


def launch(pkg, activity=None):
    if activity:
        comp = activity if "/" in activity else f"{pkg}/{activity}"
        return sh(f"am start -W -n {comp}")
    out = sh(f"cmd package resolve-activity --brief -a android.intent.action.MAIN -c android.intent.category.LAUNCHER {pkg}", 20)[1]
    comp = next((l.strip() for l in out.splitlines() if l.strip().startswith(pkg + "/")), None)
    if comp:
        return sh(f"am start -W -n {comp}")
    return sh(f"monkey -p {pkg} -c android.intent.category.LAUNCHER 1")


def logcat(lines=500, pkg=None):
    pid = sh(f"pidof {pkg}", 10)[1].strip() if pkg else ""
    args = ["logcat", "-d", "-v", "time", "-t", str(lines)]
    if pid:
        args += ["--pid", pid.split()[0]]
    rc, out, err = adb(*args, timeout=60)
    return out.decode("utf-8", "replace")


def test_app(apk_bytes, pkg, seconds, activity=None):
    t0 = time.time()
    ok, text, found = install(apk_bytes)
    pkg = pkg or found
    res = {"installed": ok, "install_output": text, "package": pkg}
    if not ok or not pkg:
        res["error"] = "install failed" if not ok else "package name unknown: pass ?package="
        return res
    adb("logcat", "-c", timeout=20)
    res["launch_output"] = launch(pkg, activity)[1].strip()
    time.sleep(max(1, min(int(seconds), 300)))
    full = adb("logcat", "-d", "-v", "time", timeout=60)[1].decode("utf-8", "replace")
    pid = sh(f"pidof {pkg}", 10)[1].strip()
    res["running"] = bool(pid)
    res["pid"] = pid
    res["crashed"] = bool(re.search(rf"FATAL EXCEPTION|Process: {re.escape(pkg)}|Force finishing activity {re.escape(pkg)}", full))
    res["anr"] = f"ANR in {pkg}" in full
    m = re.search(rf"Displayed {re.escape(pkg)}/\S+: \+(\S+)", full)
    res["first_frame"] = m.group(1) if m else None
    res["logcat"] = "\n".join(l for l in full.splitlines()
                              if pkg in l or (pid and f"({pid.split()[0]:>5})" in l) or "AndroidRuntime" in l)[-200000:]
    res["focus"] = sh("dumpsys window | grep -m1 mCurrentFocus", 15)[1].strip()
    res["screenshot_png_base64"] = base64.b64encode(screenshot()).decode()
    res["seconds_total"] = round(time.time() - t0, 1)
    return res


def reset():
    p = subprocess.run(["sudo", "env", "PIXEL7_FRESH=1", RUN_SH], capture_output=True, text=True, timeout=400)
    return p.returncode == 0, (p.stdout + p.stderr).strip()


def health():
    b = booted()
    h = {"device": "pixel7", "booted": b, "serial": SERIAL}
    if b:
        h.update(model=prop("ro.product.model"), device_code=prop("ro.product.device"),
                 android=prop("ro.build.version.release"), sdk=prop("ro.build.version.sdk"),
                 fingerprint=prop("ro.build.fingerprint"), abi=prop("ro.product.cpu.abi"),
                 size=sh("wm size", 10)[1].strip().split(": ")[-1], density=sh("wm density", 10)[1].strip().split(": ")[-1],
                 user_apps=[l.replace("package:", "") for l in sh("pm list packages -3", 20)[1].split()])
    return h


class H(BaseHTTPRequestHandler):
    server_version = "pixel7d/1"

    def log_message(self, fmt, *a):
        pass

    def send(self, code, body, ctype="application/json", headers=None):
        if not isinstance(body, (bytes, bytearray)):
            body = (json.dumps(body, indent=1) if ctype == "application/json" else str(body)).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        for k, v in (headers or {}).items():
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def body(self):
        n = int(self.headers.get("Content-Length") or 0)
        if n > 1024 * 1024 * 1024:
            raise ValueError("body over 1 GB")
        return self.rfile.read(n) if n else b""

    def route(self, method):
        if self.headers.get("X-Mantra-Token", "") != token():
            return self.send(401, {"error": "X-Mantra-Token missing or wrong"})
        u = urlparse(self.path)
        q = {k: v[-1] for k, v in parse_qs(u.query).items()}
        path = u.path.rstrip("/") or "/"
        if method == "GET" and path == "/api":
            return self.send(200, {"doc": __doc__})
        with LOCK:
            if method == "GET" and path == "/health":
                return self.send(200, health())
            if not booted() and path != "/reset":
                return self.send(503, {"error": "pixel7 is not booted; POST /reset or run pixel7/run.sh"})
            if method == "GET" and path == "/screenshot":
                return self.send(200, screenshot(), "image/png")
            if method == "GET" and path == "/logcat":
                return self.send(200, logcat(int(q.get("lines", 500)), q.get("package")), "text/plain; charset=utf-8")
            if method != "POST":
                return self.send(404, {"error": "unknown route", "see": "/api"})
            if path == "/install":
                ok, text, pkg = install(self.body())
                return self.send(200 if ok else 400, {"installed": ok, "package": pkg, "output": text})
            if path == "/test":
                return self.send(200, test_app(self.body(), q.get("package"), q.get("seconds", 10), q.get("activity")))
            pkg = q.get("package", "")
            if pkg and not re.fullmatch(r"[A-Za-z0-9_.]+", pkg):
                return self.send(400, {"error": "bad package name"})
            simple = {
                "/uninstall": lambda: sh(f"pm uninstall {pkg}"),
                "/launch": lambda: launch(pkg, q.get("activity")),
                "/stop": lambda: sh(f"am force-stop {pkg}"),
                "/clear": lambda: sh(f"pm clear {pkg}"),
                "/tap": lambda: sh(f"input tap {int(q['x'])} {int(q['y'])}"),
                "/swipe": lambda: sh(f"input swipe {int(q['x1'])} {int(q['y1'])} {int(q['x2'])} {int(q['y2'])} {int(q.get('ms', 300))}"),
                "/key": lambda: sh(f"input keyevent {re.sub(r'[^A-Za-z0-9_]', '', q['code'])}"),
                "/text": lambda: sh("input text " + "'" + q["s"].replace(" ", "%s").replace("'", "") + "'"),
                "/instrument": lambda: sh(f"am instrument -w -r {q.get('args', '')} {q['runner']}", 1800),
            }
            if path in simple:
                rc, out = simple[path]()
                return self.send(200, {"rc": rc, "output": out})
            if path == "/shell":
                rc, out = sh(self.body().decode(), 600)
                return self.send(200, out, "text/plain; charset=utf-8", {"X-Exit-Code": str(rc)})
            if path == "/reset":
                ok, out = reset()
                return self.send(200 if ok else 500, {"reset": ok, "output": out})
            return self.send(404, {"error": "unknown route", "see": "/api"})

    def do_GET(self):
        self.safe("GET")

    def do_POST(self):
        self.safe("POST")

    def safe(self, m):
        try:
            self.route(m)
        except Exception as e:
            try:
                self.send(500, {"error": f"{type(e).__name__}: {e}"})
            except Exception:
                pass


if __name__ == "__main__":
    ThreadingHTTPServer((HOST, PORT), H).serve_forever()
