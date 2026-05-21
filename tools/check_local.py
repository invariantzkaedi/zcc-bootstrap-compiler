import sqlite3, shutil, os
path = r'C:\Users\zkaed\AppData\Local\Microsoft\Edge\User Data\Default\History'
temp_path = r'temp_history.sqlite'
shutil.copy2(path, temp_path)
conn = sqlite3.connect(temp_path)
cur = conn.cursor()
query = """
SELECT datetime(v.visit_time / 1000000 - 11644473600, 'unixepoch', 'localtime'), u.url 
FROM visits v 
JOIN urls u ON v.url = u.id 
WHERE u.url LIKE '%localhost%' OR u.url LIKE '%127.0.0.1%' OR u.url LIKE '%8000%'
ORDER BY v.visit_time DESC LIMIT 5;
"""
cur.execute(query)
rows = cur.fetchall()
for r in rows:
    print(r)
conn.close()
os.remove(temp_path)
