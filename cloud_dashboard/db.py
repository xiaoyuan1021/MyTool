"""
SQLite 持久化层
"""

import json
import sqlite3
import threading
import time
from datetime import datetime, timedelta

from config import DB_PATH

import logging
_log = logging.getLogger("dashboard").info

_sqlite_lock = threading.Lock()


def get_db() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH, check_same_thread=False)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA synchronous=NORMAL")
    return conn


def init_db():
    conn = get_db()
    try:
        conn.executescript("""
            CREATE TABLE IF NOT EXISTS devices (
                device_id    TEXT PRIMARY KEY,
                status       TEXT DEFAULT 'online',
                first_seen   TEXT,
                last_heartbeat TEXT,
                last_ts      REAL,
                uptime       REAL DEFAULT 0,
                result_count INTEGER DEFAULT 0,
                last_result_json TEXT
            );
            CREATE TABLE IF NOT EXISTS results (
                id                INTEGER PRIMARY KEY AUTOINCREMENT,
                report_id         TEXT,
                device_id         TEXT,
                image_name        TEXT,
                roi_name          TEXT,
                passed            INTEGER,
                fail_reason       TEXT,
                total_items       INTEGER DEFAULT 0,
                passed_items      INTEGER DEFAULT 0,
                failed_items      INTEGER DEFAULT 0,
                failed_item_names TEXT,
                item_results_json TEXT,
                timestamp         INTEGER,
                date_time         TEXT,
                created_at        TEXT DEFAULT (datetime('now','localtime'))
            );
            CREATE TABLE IF NOT EXISTS heartbeats (
                id        INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT,
                status    TEXT,
                uptime    REAL,
                ts        INTEGER,
                created_at TEXT DEFAULT (datetime('now','localtime'))
            );
            CREATE INDEX IF NOT EXISTS idx_results_ts     ON results(timestamp);
            CREATE INDEX IF NOT EXISTS idx_results_device ON results(device_id);
            CREATE INDEX IF NOT EXISTS idx_results_pass   ON results(passed);
            CREATE INDEX IF NOT EXISTS idx_results_roi    ON results(roi_name);
            CREATE INDEX IF NOT EXISTS idx_hb_device      ON heartbeats(device_id);
            CREATE INDEX IF NOT EXISTS idx_hb_ts          ON heartbeats(ts);
        """)
        conn.commit()
    finally:
        conn.close()
    _log(f"[DB] SQLite 初始化完成: {DB_PATH}")


def db_upsert_device(device_id: str, data: dict):
    with _sqlite_lock:
        conn = get_db()
        try:
            conn.execute("""
                INSERT INTO devices(device_id,status,first_seen,last_heartbeat,last_ts,uptime,result_count,last_result_json)
                VALUES(?,?,?,?,?,?,?,?)
                ON CONFLICT(device_id) DO UPDATE SET
                    status=excluded.status, last_heartbeat=excluded.last_heartbeat,
                    last_ts=excluded.last_ts, uptime=excluded.uptime,
                    result_count=excluded.result_count, last_result_json=excluded.last_result_json
            """, (
                device_id,
                data.get("status", "online"),
                data.get("first_seen", datetime.now().strftime("%Y-%m-%d %H:%M:%S")),
                data.get("last_heartbeat", datetime.now().strftime("%Y-%m-%d %H:%M:%S")),
                data.get("last_ts", time.time()),
                data.get("uptime", 0),
                data.get("result_count", 0),
                json.dumps(data.get("last_result"), ensure_ascii=False) if data.get("last_result") else None,
            ))
            conn.commit()
        finally:
            conn.close()


def db_insert_result(record: dict):
    with _sqlite_lock:
        conn = get_db()
        try:
            conn.execute("""
                INSERT INTO results(report_id,device_id,image_name,roi_name,passed,fail_reason,
                                    total_items,passed_items,failed_items,failed_item_names,
                                    item_results_json,timestamp,date_time)
                VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)
            """, (
                record.get("reportId", ""),
                record.get("deviceId", ""),
                record.get("imageName", ""),
                record.get("roiName", ""),
                1 if record.get("passed") else 0,
                record.get("failReason", ""),
                record.get("totalItems", 0),
                record.get("passedItems", 0),
                record.get("failedItems", 0),
                record.get("failedItemNames", ""),
                json.dumps(record.get("itemResults", []), ensure_ascii=False),
                record.get("timestamp", int(time.time() * 1000)),
                record.get("dateTime", datetime.now().strftime("%Y-%m-%d %H:%M:%S")),
            ))
            conn.commit()
        finally:
            conn.close()


def db_insert_heartbeat(device_id: str, status: str, uptime: float, ts: int):
    with _sqlite_lock:
        conn = get_db()
        try:
            conn.execute(
                "INSERT INTO heartbeats(device_id,status,uptime,ts) VALUES(?,?,?,?)",
                (device_id, status, uptime, ts),
            )
            conn.commit()
        finally:
            conn.close()


def db_load_devices() -> dict:
    conn = get_db()
    try:
        rows = conn.execute("SELECT * FROM devices").fetchall()
        result = {}
        for r in rows:
            last_result = json.loads(r["last_result_json"]) if r["last_result_json"] else None
            result[r["device_id"]] = {
                "device_id": r["device_id"],
                "status": r["status"],
                "first_seen": r["first_seen"],
                "last_heartbeat": r["last_heartbeat"],
                "last_ts": r["last_ts"] or 0,
                "uptime": r["uptime"] or 0,
                "result_count": r["result_count"] or 0,
                "last_result": last_result,
            }
        return result
    finally:
        conn.close()


def db_load_recent_results(limit: int = 500) -> list:
    conn = get_db()
    try:
        rows = conn.execute(
            "SELECT * FROM results ORDER BY id DESC LIMIT ?", (limit,)
        ).fetchall()
        loaded = []
        for r in reversed(rows):
            try:
                item = {
                    "reportId": r["report_id"],
                    "deviceId": r["device_id"],
                    "imageName": r["image_name"] or "",
                    "roiName": r["roi_name"] or "",
                    "passed": bool(r["passed"]),
                    "failReason": r["fail_reason"] or "",
                    "totalItems": r["total_items"] or 0,
                    "passedItems": r["passed_items"] or 0,
                    "failedItems": r["failed_items"] or 0,
                    "failedItemNames": r["failed_item_names"] or "",
                    "timestamp": r["timestamp"],
                    "dateTime": r["date_time"] or "",
                }
                loaded.append(item)
            except (TypeError, KeyError):
                pass
        return loaded
    finally:
        conn.close()


def db_load_by_hour() -> dict:
    conn = get_db()
    try:
        rows = conn.execute("""
            SELECT substr(replace(date_time,'T',' '),1,13)||':00' AS hour,
                   COUNT(*) AS total,
                   SUM(passed) AS passed,
                   SUM(CASE WHEN passed=0 THEN 1 ELSE 0 END) AS failed
            FROM results
            GROUP BY hour
            ORDER BY hour
        """).fetchall()
        result = {}
        for r in rows:
            result[r["hour"]] = {"total": r["total"], "passed": r["passed"], "failed": r["failed"]}
        return result
    finally:
        conn.close()


def db_load_by_roi() -> dict:
    conn = get_db()
    try:
        rows = conn.execute("""
            SELECT roi_name,
                   COUNT(*) AS total,
                   SUM(passed) AS passed,
                   SUM(CASE WHEN passed=0 THEN 1 ELSE 0 END) AS failed
            FROM results
            GROUP BY roi_name
            ORDER BY total DESC
        """).fetchall()
        result = {}
        for r in rows:
            result[r["roi_name"]] = {"total": r["total"], "passed": r["passed"], "failed": r["failed"]}
        return result
    finally:
        conn.close()


def db_load_total_stats() -> dict:
    conn = get_db()
    try:
        row = conn.execute("""
            SELECT COUNT(*) AS total,
                   SUM(passed) AS passed,
                   SUM(CASE WHEN passed=0 THEN 1 ELSE 0 END) AS failed
            FROM results
        """).fetchone()
        return {"total": row["total"] or 0, "passed": row["passed"] or 0, "failed": row["failed"] or 0}
    finally:
        conn.close()


def db_load_today_stats() -> dict:
    today_str = datetime.now().strftime("%Y-%m-%d")
    conn = get_db()
    try:
        row = conn.execute("""
            SELECT COUNT(*) AS total,
                   SUM(passed) AS passed,
                   SUM(CASE WHEN passed=0 THEN 1 ELSE 0 END) AS failed
            FROM results WHERE date_time LIKE ?
        """, (f"{today_str}%",)).fetchone()
        return {"total": row["total"] or 0, "passed": row["passed"] or 0, "failed": row["failed"] or 0}
    finally:
        conn.close()


def db_query_history(device_id=None, roi_name=None, passed=None, days=None, page=1, page_size=50):
    conn = get_db()
    try:
        conditions, params = [], []
        if device_id:
            conditions.append("device_id=?")
            params.append(device_id)
        if roi_name:
            conditions.append("roi_name=?")
            params.append(roi_name)
        if passed is not None:
            conditions.append("passed=?")
            params.append(1 if passed else 0)
        if days:
            cutoff = (datetime.now() - timedelta(days=days)).strftime("%Y-%m-%d %H:%M:%S")
            conditions.append("date_time>=?")
            params.append(cutoff)
        where = ("WHERE " + " AND ".join(conditions)) if conditions else ""
        count_row = conn.execute(f"SELECT COUNT(*) AS cnt FROM results {where}", params).fetchone()
        total = count_row["cnt"] if count_row else 0
        offset = (page - 1) * page_size
        rows = conn.execute(
            f"SELECT * FROM results {where} ORDER BY id DESC LIMIT ? OFFSET ?",
            params + [page_size, offset],
        ).fetchall()
        items = []
        for r in rows:
            try:
                item = {
                    "reportId": r["report_id"],
                    "deviceId": r["device_id"],
                    "imageName": r["image_name"] or "",
                    "roiName": r["roi_name"] or "",
                    "passed": bool(r["passed"]),
                    "failReason": r["fail_reason"] or "",
                    "totalItems": r["total_items"] or 0,
                    "passedItems": r["passed_items"] or 0,
                    "failedItems": r["failed_items"] or 0,
                    "failedItemNames": r["failed_item_names"] or "",
                    "timestamp": r["timestamp"],
                    "dateTime": r["date_time"] or "",
                }
                items.append(item)
            except (TypeError, KeyError):
                pass
        return {"total": total, "page": page, "page_size": page_size, "items": items}
    finally:
        conn.close()
