#!/usr/bin/env python3
"""pixel7-jobs: the Pixel 7's door for sessions that are not on the tailnet (cloud sessions).

The private repository markoboskoauroville/vm-android-jobs is the mailbox. A session pushes to main:

    jobs/<id>/job.json     {"apk": "app.apk" | "apk_url": "https://...",  "package": "com.x" (optional),
                            "activity": ".Main" (optional), "seconds": 10,
                            "steps": [{"tap": [540, 1200]}, {"wait": 2}, {"text": "hello"}, {"key": "BACK"},
                                      {"swipe": [540, 1800, 540, 600, 300]}, {"shot": "after-tap"},
                                      {"shell": "dumpsys battery"}]}
    jobs/<id>/app.apk      (when "apk" names a file in the job folder)

Within about 20 seconds this runner pulls, runs it on the Pixel 7, and pushes:

    results/<id>/result.json   installed, crashed, anr, running, first_frame, focus, steps output, timings
    results/<id>/screen.png    after `seconds`; plus <name>.png for every {"shot": name}
    results/<id>/logcat.txt    the app's log

and deletes jobs/<id>/ (the APK leaves git history small only if pushed with the job; prefer apk_url for big APKs).
"""
import base64, json, os, shutil, subprocess, sys, time, traceback, urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pixel7d as P  # noqa: E402

REPO = os.path.expanduser(os.environ.get("PIXEL7_JOBS", "~/vm-android-jobs"))
EVERY = int(os.environ.get("PIXEL7_JOBS_EVERY", "20"))


def git(*a, check=True):
    return subprocess.run(["git", "-C", REPO, *a], capture_output=True, text=True, check=check, timeout=300)


def run_job(jid, jdir, out):
    job = json.load(open(os.path.join(jdir, "job.json")))
    if job.get("apk_url"):
        req = urllib.request.Request(job["apk_url"], headers={"User-Agent": "pixel7-jobs/1"})
        apk = urllib.request.urlopen(req, timeout=600).read()
    else:
        apk = open(os.path.join(jdir, job.get("apk", "app.apk")), "rb").read()
    with P.LOCK:
        if not P.booted():
            ok, txt = P.reset()
            if not ok:
                raise RuntimeError("pixel7 not booted and reset failed: " + txt)
        res = P.test_app(apk, job.get("package"), job.get("seconds", 10), job.get("activity"))
        shot = res.pop("screenshot_png_base64", None)
        if shot:
            open(os.path.join(out, "screen.png"), "wb").write(base64.b64decode(shot))
        steps = []
        if res.get("installed") and res.get("package"):
            for st in job.get("steps", []):
                k, v = next(iter(st.items()))
                if k == "tap":
                    r = P.sh(f"input tap {int(v[0])} {int(v[1])}")
                elif k == "swipe":
                    r = P.sh("input swipe " + " ".join(str(int(x)) for x in v))
                elif k == "wait":
                    time.sleep(min(float(v), 120)); r = (0, "")
                elif k == "text":
                    r = P.sh("input text '" + str(v).replace(" ", "%s").replace("'", "") + "'")
                elif k == "key":
                    r = P.sh("input keyevent " + "".join(c for c in str(v) if c.isalnum() or c == "_"))
                elif k == "shell":
                    r = P.sh(str(v), 300)
                elif k == "shot":
                    name = "".join(c for c in str(v) if c.isalnum() or c in "-_") or f"shot{len(steps)}"
                    open(os.path.join(out, name + ".png"), "wb").write(P.screenshot()); r = (0, name + ".png")
                else:
                    r = (1, f"unknown step {k}")
                steps.append({"step": st, "rc": r[0], "output": r[1][-20000:]})
            if steps:
                pid = P.sh(f"pidof {res['package']}", 10)[1].strip()
                res["running_after_steps"] = bool(pid)
                res["logcat_after_steps"] = P.logcat(2000, res["package"]) if pid else ""
        res["steps"] = steps
    open(os.path.join(out, "logcat.txt"), "w").write(res.pop("logcat", "") + "\n" + res.pop("logcat_after_steps", ""))
    res["device"] = {k: P.prop(k) for k in ("ro.product.model", "ro.product.device", "ro.build.version.release", "ro.build.fingerprint")}
    return res


def once():
    git("pull", "-q", "--rebase", "origin", "main")
    jobs = os.path.join(REPO, "jobs")
    if not os.path.isdir(jobs):
        return
    for jid in sorted(os.listdir(jobs)):
        jdir = os.path.join(jobs, jid)
        if not os.path.isfile(os.path.join(jdir, "job.json")):
            continue
        out = os.path.join(REPO, "results", jid)
        shutil.rmtree(out, ignore_errors=True)
        os.makedirs(out)
        t0 = time.time()
        try:
            res = run_job(jid, jdir, out)
        except Exception as e:
            res = {"error": f"{type(e).__name__}: {e}", "trace": traceback.format_exc()[-4000:]}
        res.update(job=jid, finished=time.strftime("%Y-%m-%d %H:%M:%S %Z"), seconds=round(time.time() - t0, 1))
        json.dump(res, open(os.path.join(out, "result.json"), "w"), indent=1)
        shutil.rmtree(jdir)
        git("add", "-A")
        verdict = "error" if "error" in res else ("CRASHED" if res.get("crashed") else "ok")
        git("commit", "-q", "-m", f"result {jid}: {verdict}")
        for _ in range(3):
            if git("push", "-q", "origin", "main", check=False).returncode == 0:
                break
            git("pull", "-q", "--rebase", "origin", "main", check=False)


if __name__ == "__main__":
    while True:
        try:
            once()
        except Exception:
            traceback.print_exc()
        time.sleep(EVERY)
