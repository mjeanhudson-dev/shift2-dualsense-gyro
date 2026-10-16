import sqlite3
import hashlib
import json

db_path = r"C:\Users\Zachariah\Documents\antigravity\local-forge\data\gitea.db"
conn = sqlite3.connect(db_path)
cur = conn.cursor()

print("=== PRAGMA INTEGRITY CHECK ===")
cur.execute("PRAGMA integrity_check")
print("integrity_check:", cur.fetchall())

print("\n=== PRAGMA JOURNAL MODE ===")
cur.execute("PRAGMA journal_mode")
print("journal_mode:", cur.fetchall())

print("\n=== TOTAL TABLES ===")
cur.execute("SELECT count(*) FROM sqlite_master WHERE type='table'")
table_count = cur.fetchone()[0]
print(f"Table count: {table_count}")

cur.execute("SELECT count(*) FROM sqlite_master WHERE type='index'")
index_count = cur.fetchone()[0]
print(f"Index count: {index_count}")

print("\n=== VERSION TABLE ===")
cur.execute("SELECT * FROM version")
print("version:", cur.fetchall())

print("\n=== USER TABLE COLUMNS & RECORDS ===")
cur.execute("PRAGMA table_info(user)")
cols = [c[1] for c in cur.fetchall()]
print(f"User columns count: {len(cols)}")
cur.execute("SELECT id, name, full_name, email, is_active, is_admin, must_change_password, passwd_hash_algo FROM user")
users = cur.fetchall()
for u in users:
    print("User:", u)

print("\n=== ACCESS_TOKEN TABLE COLUMNS & RECORDS ===")
cur.execute("PRAGMA table_info(access_token)")
token_cols = [c[1] for c in cur.fetchall()]
print(f"Token columns count: {len(token_cols)}")
cur.execute("SELECT id, uid, name, token_last_eight, created_unix, updated_unix FROM access_token")
tokens = cur.fetchall()
for t in tokens:
    print("Token record:", t)

# Verify token last eight against actual token files
with open(r"C:\Users\Zachariah\Documents\antigravity\local-forge\forge_token.txt") as f:
    admin_token = f.read().strip()
with open(r"C:\Users\Zachariah\Documents\antigravity\local-forge\forgeadmin_token.txt") as f:
    forgeadmin_token = f.read().strip()

print(f"\nadmin token file last 8: '{admin_token[-8:]}'")
print(f"forgeadmin token file last 8: '{forgeadmin_token[-8:]}'")

cur.execute("SELECT uid, name, token_last_eight, token_hash FROM access_token")
db_tokens = cur.fetchall()
for uid, name, last8, thash in db_tokens:
    expected_last8 = admin_token[-8:] if uid == 2 else forgeadmin_token[-8:]
    match = (last8 == expected_last8)
    print(f"UID={uid} ({name}): db_last_8='{last8}', file_last_8='{expected_last8}', match={match}")

conn.close()
