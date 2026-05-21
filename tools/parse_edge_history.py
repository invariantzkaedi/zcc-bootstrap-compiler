import sqlite3
import shutil
import os
import sys

source_db = r"C:\Users\zkaed\AppData\Local\Microsoft\Edge\User Data\Default\History"
temp_db = r"G:\zccMAIN\zcc\tools\temp_history_dump.sqlite"

if not os.path.exists(source_db):
    print("[-] Error: Edge History database not found at:", source_db)
    sys.exit(1)

try:
    # Copying to avoid "database is locked" errors if Edge is running
    shutil.copy2(source_db, temp_db)
except Exception as e:
    print(f"[-] Error copying history file: {e}")
    sys.exit(1)

try:
    conn = sqlite3.connect(temp_db)
    cursor = conn.cursor()

    query = """
    SELECT 
        datetime(v.visit_time / 1000000 - 11644473600, 'unixepoch', 'localtime') as local_time,
        u.url,
        u.title
    FROM visits v
    JOIN urls u ON v.url = u.id
    WHERE u.url LIKE '%hf.space%' 
       OR u.url LIKE '%zkaedi.ai%' 
       OR u.url LIKE '%huggingface%'
    ORDER BY v.visit_time DESC
    LIMIT 100;
    """
    cursor.execute(query)
    rows = cursor.fetchall()
    conn.close()
    
    # Cleanup temp copy
    os.remove(temp_db)

    print("[+] Blockpath Edge History Audit:")
    print("-" * 80)
    if not rows:
        print("    No blockpath.com history found in the database.")
    else:
        for r in rows:
            print(f"Time  : {r[0]}")
            print(f"Title : {r[2]}")
            print(f"URL   : {r[1]}")
            print("-" * 80)

except Exception as e:
    print(f"[-] Error parsing SQLite Database: {e}")
    sys.exit(1)
