#!/usr/bin/env python3
"""
Verify MySQL connectivity for PhenoLabHT.

Usage:
  python scripts/verify_mysql_connection.py
"""

import sys
from datetime import datetime


CONFIG = {
    "host": "192.168.10.170",
    "port": 3306,
    "user": "root",
    "password": "Zq17122320_",
    "database": "PhenoLabHT",
    "connect_timeout": 5,
}


def try_mysql_connector():
    import mysql.connector  # type: ignore

    conn = mysql.connector.connect(
        host=CONFIG["host"],
        port=CONFIG["port"],
        user=CONFIG["user"],
        password=CONFIG["password"],
        database=CONFIG["database"],
        connection_timeout=CONFIG["connect_timeout"],
    )
    return conn, "mysql-connector-python"


def try_pymysql():
    import pymysql  # type: ignore

    conn = pymysql.connect(
        host=CONFIG["host"],
        port=CONFIG["port"],
        user=CONFIG["user"],
        password=CONFIG["password"],
        database=CONFIG["database"],
        connect_timeout=CONFIG["connect_timeout"],
        charset="utf8mb4",
        autocommit=True,
    )
    return conn, "PyMySQL"


def main():
    print(f"[{datetime.now().isoformat(sep=' ', timespec='seconds')}] Start MySQL connectivity check")
    print(
        f"Target: {CONFIG['host']}:{CONFIG['port']} | DB: {CONFIG['database']} | User: {CONFIG['user']}"
    )

    conn = None
    driver = None
    errors = []

    for fn in (try_mysql_connector, try_pymysql):
        try:
            conn, driver = fn()
            break
        except Exception as exc:  # noqa: BLE001
            errors.append(f"{fn.__name__}: {exc}")

    if conn is None:
        print("FAILED: No supported MySQL Python driver available or connection failed.")
        for e in errors:
            print(" -", e)
        print("Tip: pip install mysql-connector-python  (or pip install pymysql)")
        return 2

    try:
        cur = conn.cursor()
        cur.execute("SELECT VERSION()")
        version = cur.fetchone()
        print(f"SUCCESS: Connected using {driver}")
        print("MySQL VERSION():", version[0] if version else "<unknown>")
        cur.close()
        return 0
    except Exception as exc:  # noqa: BLE001
        print("FAILED: Connected but query test failed:", exc)
        return 3
    finally:
        try:
            conn.close()
        except Exception:
            pass


if __name__ == "__main__":
    sys.exit(main())

