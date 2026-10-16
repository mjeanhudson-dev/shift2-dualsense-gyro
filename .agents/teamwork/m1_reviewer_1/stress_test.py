import os
import sqlite3
import subprocess

print("=== 1. ENVIRONMENT & USER CHECK ===")
print("USER:", os.environ.get("USERNAME"))
print("USERPROFILE:", os.environ.get("USERPROFILE"))

print("\n=== 2. GIT SYSTEM DEPENDENCY CHECK ===")
try:
    res = subprocess.run(["git", "--version"], capture_output=True, text=True, check=True)
    print("git --version:", res.stdout.strip())
except Exception as e:
    print("git error:", e)

print("\n=== 3. LOG FILE INSPECTION ===")
log_path = r"C:\Users\Zachariah\Documents\antigravity\local-forge\log\gitea.log"
if os.path.exists(log_path):
    size = os.path.getsize(log_path)
    print(f"gitea.log exists, size: {size} bytes")
    with open(log_path, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()
        print(f"Total lines in log: {len(lines)}")
        print("Last 10 lines of log:")
        for line in lines[-10:]:
            print("  ", line.strip())
else:
    print("gitea.log does not exist yet (or logged to console only).")

print("\n=== 4. DATABASE TOKEN SCOPES & EXPIRATION ===")
db_path = r"C:\Users\Zachariah\Documents\antigravity\local-forge\data\gitea.db"
conn = sqlite3.connect(db_path)
cur = conn.cursor()
cur.execute("PRAGMA table_info(access_token)")
cols = [c[1] for c in cur.fetchall()]
print("access_token columns:", cols)

cur.execute("SELECT id, uid, name, token_last_eight, created_unix, updated_unix FROM access_token")
for row in cur.fetchall():
    print("Token detail:", row)

print("\n=== 5. CHECK WAL & SHM FILES ===")
wal_path = db_path + "-wal"
shm_path = db_path + "-shm"
print(f"WAL file exists: {os.path.exists(wal_path)} (size: {os.path.getsize(wal_path) if os.path.exists(wal_path) else 0})")
print(f"SHM file exists: {os.path.exists(shm_path)} (size: {os.path.getsize(shm_path) if os.path.exists(shm_path) else 0})")

conn.close()
