import sqlite3
import os
import hashlib
import binascii

db_path = r'C:\Users\Zachariah\Documents\antigravity\local-forge\data\gitea.db'
size = os.path.getsize(db_path)
print(f"DB File Size: {size} bytes")

with open(db_path, 'rb') as f:
    header = f.read(16)
print(f"Magic Header: {header}")

conn = sqlite3.connect(db_path)
cur = conn.cursor()

# Integrity check
cur.execute("PRAGMA integrity_check;")
print(f"Integrity Check: {cur.fetchall()}")

# Journal mode
cur.execute("PRAGMA journal_mode;")
print(f"Journal Mode: {cur.fetchall()}")

# Table count
cur.execute("SELECT count(*) FROM sqlite_master WHERE type='table';")
print(f"Table Count: {cur.fetchone()[0]}")

# Indexes count
cur.execute("SELECT count(*) FROM sqlite_master WHERE type='index';")
print(f"Index Count: {cur.fetchone()[0]}")

# Users
print("\n--- USERS ---")
cur.execute("SELECT id, lower_name, name, full_name, email, is_admin, is_active, must_change_password, created_unix FROM user;")
users = cur.fetchall()
for row in users:
    print(row)

# Access tokens
print("\n--- ACCESS TOKENS ---")
cur.execute("SELECT id, uid, name, token_hash, token_salt, token_last_eight, created_unix, updated_unix FROM access_token;")
tokens = cur.fetchall()
for row in tokens:
    print(row)

# Token verification
print("\n--- TOKEN FILE VERIFICATION ---")
token_file = r'C:\Users\Zachariah\Documents\antigravity\local-forge\forge_token.txt'
if os.path.exists(token_file):
    with open(token_file, 'rb') as tf:
        raw_admin_token = tf.read()
    print(f"forge_token.txt bytes: {len(raw_admin_token)} -> {raw_admin_token.decode('ascii')}")
    token_str = raw_admin_token.decode('ascii').strip()
    last_eight = token_str[-8:]
    print(f"Admin token last 8: {last_eight}")
    
forgeadmin_token_file = r'C:\Users\Zachariah\Documents\antigravity\local-forge\forgeadmin_token.txt'
if os.path.exists(forgeadmin_token_file):
    with open(forgeadmin_token_file, 'rb') as tf:
        raw_fa_token = tf.read()
    print(f"forgeadmin_token.txt bytes: {len(raw_fa_token)} -> {raw_fa_token.decode('ascii')}")
    fa_token_str = raw_fa_token.decode('ascii').strip()
    fa_last_eight = fa_token_str[-8:]
    print(f"ForgeAdmin token last 8: {fa_last_eight}")

# Check matching last_eight in DB
for row in tokens:
    tid, uid, name, thash, tsalt, tlast8, ctime, utime = row
    print(f"Token row in DB: id={tid}, uid={uid}, name={name}, token_last_eight={tlast8}")
    if tlast8 == last_eight:
        print(f"  -> Matches forge_token.txt! UID={uid}")
    if tlast8 == fa_last_eight:
        print(f"  -> Matches forgeadmin_token.txt! UID={uid}")

# Check version table
print("\n--- VERSION TABLE ---")
cur.execute("SELECT * FROM version;")
print(cur.fetchall())
