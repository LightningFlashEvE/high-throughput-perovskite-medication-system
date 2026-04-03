"""
检查 PhenoLabHT 数据库连接和 live_code 表状态
运行方式: python check_db.py
"""
import pyodbc

HOST = "192.168.10.170"
PORT = 3306
DATABASE = "PhenoLabHT"
USER = "root"
PASSWORD = "root"  # 如果密码不对请修改

DSN = (
    f"DRIVER={{MySQL ODBC 9.6 Unicode Driver}};"
    f"SERVER={HOST};PORT={PORT};DATABASE={DATABASE};"
    f"UID={USER};PWD={PASSWORD};CHARSET=utf8;"
)

print("=" * 50)
print("连接信息:")
print(f"  HOST     : {HOST}:{PORT}")
print(f"  DATABASE : {DATABASE}")
print("=" * 50)

try:
    conn = pyodbc.connect(DSN, timeout=5)
    print("[OK] 连接成功\n")
except Exception as e:
    print(f"[FAIL] 连接失败: {e}")
    exit(1)

cursor = conn.cursor()

# 1. 列出所有表
print("--- 当前数据库所有表 ---")
cursor.execute("SHOW TABLES")
tables = [row[0] for row in cursor.fetchall()]
for t in tables:
    print(f"  {t}")
print()

# 2. 检查 live_code 是否存在
if "live_code" in tables:
    print("[OK] live_code 表存在\n")

    # 查看结构
    print("--- live_code 表结构 ---")
    cursor.execute("DESCRIBE live_code")
    for row in cursor.fetchall():
        print(f"  {row}")
    print()

    # 查看内容
    print("--- live_code 现有数据（前20行）---")
    cursor.execute("SELECT * FROM live_code LIMIT 20")
    rows = cursor.fetchall()
    if rows:
        for row in rows:
            print(f"  {row}")
    else:
        print("  （表为空）")
    print()

    # 测试写入
    print("--- 测试写入 ---")
    try:
        cursor.execute("INSERT INTO live_code (drug_name) VALUES ('__test__')")
        conn.commit()
        print("[OK] 写入成功")
        cursor.execute("DELETE FROM live_code WHERE drug_name = '__test__'")
        conn.commit()
        print("[OK] 清理成功")
    except Exception as e:
        print(f"[FAIL] 写入失败: {e}")
else:
    print("[FAIL] live_code 表不存在！\n")
    print("--- 是否在其他数据库中？---")
    try:
        cursor.execute("SELECT table_schema, table_name FROM information_schema.tables WHERE table_name = 'live_code'")
        results = cursor.fetchall()
        if results:
            for row in results:
                print(f"  找到: 数据库={row[0]}, 表={row[1]}")
        else:
            print("  所有数据库中均未找到 live_code 表")
    except Exception as e:
        print(f"  查询 information_schema 失败: {e}")

cursor.close()
conn.close()
print("\n完成。")
