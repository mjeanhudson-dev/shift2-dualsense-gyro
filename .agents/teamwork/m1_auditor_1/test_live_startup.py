import subprocess
import time
import urllib.request
import urllib.error
import json
import os

gitea_bin = r'C:\Users\Zachariah\Documents\antigravity\local-forge\bin\gitea.exe'
work_dir = r'C:\Users\Zachariah\Documents\antigravity\local-forge'
config_path = r'C:\Users\Zachariah\Documents\antigravity\local-forge\custom\conf\app.ini'

token_file = r'C:\Users\Zachariah\Documents\antigravity\local-forge\forge_token.txt'
with open(token_file, 'r', encoding='ascii') as f:
    admin_token = f.read().strip()

fa_token_file = r'C:\Users\Zachariah\Documents\antigravity\local-forge\forgeadmin_token.txt'
with open(fa_token_file, 'r', encoding='ascii') as f:
    fa_token = f.read().strip()

print("Testing live Gitea server startup...")
print(f"Admin Token: {admin_token}")
print(f"ForgeAdmin Token: {fa_token}")

cmd = [gitea_bin, 'web', '--config', config_path, '--work-path', work_dir]

proc = subprocess.Popen(cmd, cwd=work_dir, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
print(f"Started Gitea PID: {proc.pid}")

started = False
for i in range(25):
    time.sleep(1)
    if proc.poll() is not None:
        out, err = proc.communicate()
        print(f"Process terminated prematurely with returncode {proc.returncode}")
        print("STDOUT:", out.decode('utf-8', errors='replace'))
        print("STDERR:", err.decode('utf-8', errors='replace'))
        break
    try:
        req = urllib.request.Request('http://127.0.0.1:3000/api/v1/version')
        with urllib.request.urlopen(req, timeout=2) as resp:
            if resp.status == 200:
                body = json.loads(resp.read().decode())
                print("Server is listening on http://127.0.0.1:3000!")
                print("Version response:", body)
                started = True
                break
    except Exception as e:
        print(f"Waiting for server... ({e})")

if started:
    # Test GET /api/v1/user with admin_token
    try:
        req_admin = urllib.request.Request('http://127.0.0.1:3000/api/v1/user')
        req_admin.add_header('Authorization', f'token {admin_token}')
        with urllib.request.urlopen(req_admin, timeout=2) as resp:
            data = json.loads(resp.read().decode())
            print(f"GET /api/v1/user (admin token) -> Status {resp.status}")
            print(f"User login: {data.get('login')}, email: {data.get('email')}, is_admin: {data.get('is_admin')}")
    except Exception as e:
        print(f"Failed admin token check: {e}")

    # Test GET /api/v1/user with fa_token
    try:
        req_fa = urllib.request.Request('http://127.0.0.1:3000/api/v1/user')
        req_fa.add_header('Authorization', f'token {fa_token}')
        with urllib.request.urlopen(req_fa, timeout=2) as resp:
            data = json.loads(resp.read().decode())
            print(f"GET /api/v1/user (forgeadmin token) -> Status {resp.status}")
            print(f"User login: {data.get('login')}, email: {data.get('email')}, is_admin: {data.get('is_admin')}")
    except Exception as e:
        print(f"Failed forgeadmin token check: {e}")

# Clean up process
print("Stopping test Gitea process...")
proc.terminate()
try:
    proc.wait(timeout=5)
except subprocess.TimeoutExpired:
    proc.kill()
    proc.wait()

print("Gitea process stopped cleanly.")
