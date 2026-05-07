"""
EdgeVision Cloud Dashboard
边云协同智能视觉检测云平台 - Flask 后端

订阅 MQTT 检测结果和心跳，提供 Web 可视化看板。
数据持久化到 SQLite，重启不丢历史数据。
"""

import json
import os
import queue
import socket
import sqlite3
import threading
import time
import uuid
from collections import deque, defaultdict
from datetime import datetime, timedelta

import paho.mqtt.client as mqtt
from flask import Flask, jsonify, render_template, request, Response, stream_with_context

# =========================================================================
# 配置
# =========================================================================

MQTT_CONFIG = {
    "host": os.environ.get("MQTT_BROKER_HOST", "broker.emqx.io"),
    "port": int(os.environ.get("MQTT_BROKER_PORT", "1883")),
    "keepalive": 60,
    "client_id": f"cloud-dashboard-{uuid.uuid4().hex[:8]}",
    "topics": {
        "results": "visiontool/results",
        "heartbeat": "visiontool/heartbeat",
        "commands": "visiontool/commands",
    },
    "qos": 1,
}

DEVICE_TIMEOUT_SECONDS = 75
DB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dashboard.db")

app = Flask(__name__)

# =========================================================================
# SQLite 持久化
# =========================================================================

_sqlite_lock = threading.Lock()


def get_db() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH, check_same_thread=False)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA synchronous=NORMAL")
    return conn


def init_db():
    conn = get_db()
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
            roi_name          TEXT,
            passed            INTEGER,
            fail_reason       TEXT,
            total_region_count INTEGER DEFAULT 0,
            region_count      INTEGER DEFAULT 0,
            barcode_count     INTEGER DEFAULT 0,
            has_barcode       INTEGER DEFAULT 0,
            timestamp         INTEGER,
            date_time         TEXT,
            raw_json          TEXT,
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
    conn.close()
    print(f"[DB] SQLite 初始化完成: {DB_PATH}")


# -------- 写操作 --------

def db_upsert_device(device_id: str, data: dict):
    with _sqlite_lock:
        conn = get_db()
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
        conn.close()


def db_insert_result(record: dict):
    with _sqlite_lock:
        conn = get_db()
        conn.execute("""
            INSERT INTO results(report_id,device_id,roi_name,passed,fail_reason,
                                total_region_count,region_count,barcode_count,has_barcode,
                                timestamp,date_time,raw_json)
            VALUES(?,?,?,?,?,?,?,?,?,?,?,?)
        """, (
            record.get("reportId", ""),
            record.get("deviceId", ""),
            record.get("roiName", ""),
            1 if record.get("passed") else 0,
            record.get("failReason", ""),
            record.get("totalRegionCount", 0),
            record.get("regionCount", 0),
            record.get("barcodeCount", 0),
            1 if record.get("hasBarcode") else 0,
            record.get("timestamp", int(time.time() * 1000)),
            record.get("dateTime", datetime.now().strftime("%Y-%m-%d %H:%M:%S")),
            json.dumps(record, ensure_ascii=False),
        ))
        conn.commit()
        conn.close()


def db_insert_heartbeat(device_id: str, status: str, uptime: float, ts: int):
    with _sqlite_lock:
        conn = get_db()
        conn.execute(
            "INSERT INTO heartbeats(device_id,status,uptime,ts) VALUES(?,?,?,?)",
            (device_id, status, uptime, ts),
        )
        conn.commit()
        conn.close()


# -------- 读操作 --------

def db_load_devices() -> dict:
    """从 SQLite 加载所有设备到内存"""
    conn = get_db()
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
    conn.close()
    return result


def db_load_recent_results(limit: int = 500) -> list:
    """从 SQLite 加载最近 N 条结果到内存缓存"""
    conn = get_db()
    rows = conn.execute(
        "SELECT raw_json FROM results ORDER BY id DESC LIMIT ?", (limit,)
    ).fetchall()
    conn.close()
    loaded = []
    for r in reversed(rows):
        try:
            loaded.append(json.loads(r["raw_json"]))
        except (json.JSONDecodeError, TypeError):
            pass
    return loaded


def db_load_by_hour() -> dict:
    """从 SQLite 聚合小时维度统计"""
    conn = get_db()
    rows = conn.execute("""
        SELECT substr(date_time,1,13)||':00' AS hour,
               COUNT(*) AS total,
               SUM(passed) AS passed,
               SUM(CASE WHEN passed=0 THEN 1 ELSE 0 END) AS failed
        FROM results
        GROUP BY hour
        ORDER BY hour
    """).fetchall()
    conn.close()
    result = {}
    for r in rows:
        result[r["hour"]] = {
            "total": r["total"],
            "passed": r["passed"],
            "failed": r["failed"],
        }
    return result


def db_load_by_roi() -> dict:
    """从 SQLite 聚合 ROI 维度统计"""
    conn = get_db()
    rows = conn.execute("""
        SELECT roi_name,
               COUNT(*) AS total,
               SUM(passed) AS passed,
               SUM(CASE WHEN passed=0 THEN 1 ELSE 0 END) AS failed
        FROM results
        GROUP BY roi_name
        ORDER BY total DESC
    """).fetchall()
    conn.close()
    result = {}
    for r in rows:
        result[r["roi_name"]] = {
            "total": r["total"],
            "passed": r["passed"],
            "failed": r["failed"],
        }
    return result


def db_query_history(device_id: str = None, roi_name: str = None,
                     passed: bool = None, days: int = None,
                     page: int = 1, page_size: int = 50) -> dict:
    """通用历史查询"""
    conn = get_db()
    conditions = []
    params = []

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

    # 总数
    count_row = conn.execute(f"SELECT COUNT(*) AS cnt FROM results {where}", params).fetchone()
    total = count_row["cnt"] if count_row else 0

    # 分页
    offset = (page - 1) * page_size
    rows = conn.execute(
        f"SELECT raw_json FROM results {where} ORDER BY id DESC LIMIT ? OFFSET ?",
        params + [page_size, offset],
    ).fetchall()
    conn.close()

    items = []
    for r in rows:
        try:
            items.append(json.loads(r["raw_json"]))
        except (json.JSONDecodeError, TypeError):
            pass

    return {"total": total, "page": page, "page_size": page_size, "items": items}


# =========================================================================
# 内存数据存储
# =========================================================================

devices: dict = {}
MAX_RESULTS = 2000
results_deque = deque(maxlen=MAX_RESULTS)

stats = {
    "total": 0,
    "passed": 0,
    "failed": 0,
    "by_roi": defaultdict(lambda: {"total": 0, "passed": 0, "failed": 0}),
    "by_hour": defaultdict(lambda: {"total": 0, "passed": 0, "failed": 0}),
}

# SSE 客户端
sse_clients: list[queue.Queue] = []
_sse_lock = threading.Lock()

# MQTT
mqtt_client: mqtt.Client | None = None
mqtt_connected = False


# =========================================================================
# 工具函数
# =========================================================================

def broadcast_sse(event: str, data: dict):
    with _sse_lock:
        dead = []
        for q in sse_clients:
            try:
                q.put_nowait((event, data))
            except queue.Full:
                dead.append(q)
        for q in dead:
            sse_clients.remove(q)


def now_ts() -> int:
    return int(time.time() * 1000)


def now_str() -> str:
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def get_lan_ip() -> str:
    """获取本机局域网 IP 地址"""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.settimeout(0.1)
        s.connect(("10.254.254.254", 1))  # 不需要真的连上
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        try:
            return socket.gethostbyname(socket.gethostname())
        except Exception:
            return "127.0.0.1"


# =========================================================================
# MQTT 回调
# =========================================================================

def on_connect(client, userdata, flags, reason_code, properties=None):
    global mqtt_connected
    if reason_code == 0:
        mqtt_connected = True
        print(f"[MQTT] 已连接 {MQTT_CONFIG['host']}:{MQTT_CONFIG['port']}")
        client.subscribe(MQTT_CONFIG["topics"]["results"], qos=MQTT_CONFIG["qos"])
        client.subscribe(MQTT_CONFIG["topics"]["heartbeat"], qos=MQTT_CONFIG["qos"])
        broadcast_sse("mqtt_status", {"connected": True})
    else:
        print(f"[MQTT] 连接失败, reason_code={reason_code}")


def on_disconnect(client, userdata, reason_code, properties=None):
    global mqtt_connected
    mqtt_connected = False
    print(f"[MQTT] 断开连接 (reason_code={reason_code})")
    broadcast_sse("mqtt_status", {"connected": False})


def on_message(client, userdata, msg):
    topic = msg.topic
    try:
        payload = json.loads(msg.payload.decode("utf-8"))
    except json.JSONDecodeError:
        return

    if topic == MQTT_CONFIG["topics"]["heartbeat"]:
        _handle_heartbeat(payload)
    elif topic == MQTT_CONFIG["topics"]["results"]:
        _handle_result(payload)


def _handle_heartbeat(payload: dict):
    device_id = payload.get("deviceId", "unknown")
    now = time.time()
    now_str_val = now_str()

    if device_id not in devices:
        devices[device_id] = {
            "device_id": device_id,
            "status": "online",
            "first_seen": now_str_val,
            "last_heartbeat": now_str_val,
            "last_ts": now,
            "uptime": payload.get("uptime", 0),
            "result_count": 0,
            "last_result": None,
        }
    else:
        dev = devices[device_id]
        dev["status"] = "online"
        dev["last_heartbeat"] = now_str_val
        dev["last_ts"] = now
        dev["uptime"] = payload.get("uptime", dev["uptime"])

    # 持久化
    db_upsert_device(device_id, devices[device_id])
    db_insert_heartbeat(device_id, "online", payload.get("uptime", 0), payload.get("ts", int(now * 1000)))

    broadcast_sse("heartbeat", {"deviceId": device_id})


def _handle_result(payload: dict):
    device_id = payload.get("deviceId", "unknown")
    result_obj = payload.get("result", {})
    passed = result_obj.get("passed", True)
    roi_name = payload.get("roiName", "全图")

    record = {
        "reportId": payload.get("reportId", ""),
        "deviceId": device_id,
        "roiName": roi_name,
        "passed": passed,
        "failReason": result_obj.get("failReason", ""),
        "totalRegionCount": result_obj.get("totalRegionCount", 0),
        "timestamp": payload.get("timestamp", now_ts()),
        "dateTime": payload.get("dateTime", now_str()),
        "regionCount": len(payload.get("regions", [])),
        "barcodeCount": len(payload.get("barcodes", [])),
        "hasBarcode": len(payload.get("barcodes", [])) > 0,
    }

    results_deque.append(record)

    # 更新统计
    stats["total"] += 1
    if passed:
        stats["passed"] += 1
    else:
        stats["failed"] += 1

    roi_stat = stats["by_roi"][roi_name]
    roi_stat["total"] += 1
    if passed:
        roi_stat["passed"] += 1
    else:
        roi_stat["failed"] += 1

    hour_key = datetime.now().strftime("%Y-%m-%d %H:00")
    hour_stat = stats["by_hour"][hour_key]
    hour_stat["total"] += 1
    if passed:
        hour_stat["passed"] += 1
    else:
        hour_stat["failed"] += 1

    # 更新设备
    if device_id in devices:
        dev = devices[device_id]
        dev["result_count"] = dev.get("result_count", 0) + 1
        dev["last_result"] = {
            "passed": passed,
            "roiName": roi_name,
            "dateTime": record["dateTime"],
        }
        db_upsert_device(device_id, dev)

    # 持久化结果
    db_insert_result(record)

    broadcast_sse("result", record)


def check_device_timeout():
    """后台线程：定期检查设备是否超时离线"""
    while True:
        time.sleep(15)
        now = time.time()
        now_str_val = now_str()
        for device_id, dev in list(devices.items()):
            if dev["status"] == "online" and (now - dev.get("last_ts", 0)) > DEVICE_TIMEOUT_SECONDS:
                dev["status"] = "offline"
                dev["last_heartbeat"] = now_str_val
                db_upsert_device(device_id, dev)
                broadcast_sse("heartbeat", {"deviceId": device_id, "status": "offline"})


def init_mqtt():
    global mqtt_client
    cfg = MQTT_CONFIG
    mqtt_client = mqtt.Client(
        client_id=cfg["client_id"],
        protocol=mqtt.MQTTv311,
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
    )
    mqtt_client.on_connect = on_connect
    mqtt_client.on_disconnect = on_disconnect
    mqtt_client.on_message = on_message
    mqtt_client.reconnect_delay_set(min_delay=1, max_delay=30)

    try:
        mqtt_client.connect(cfg["host"], cfg["port"], cfg["keepalive"])
        mqtt_client.loop_start()
        print(f"[MQTT] 正在连接 {cfg['host']}:{cfg['port']}...")
    except Exception as e:
        print(f"[MQTT] 连接失败: {e}")


# =========================================================================
# Flask 路由
# =========================================================================

@app.route("/")
def index():
    return render_template("index.html")


# -------- API: 状态 ----------

@app.route("/api/status")
def api_status():
    return jsonify({
        "mqtt_connected": mqtt_connected,
        "device_count": len(devices),
        "online_count": sum(1 for d in devices.values() if d["status"] == "online"),
        "total_results": stats["total"],
    })


@app.route("/api/host-info")
def api_host_info():
    return jsonify({
        "lanIp": get_lan_ip(),
        "port": int(os.environ.get("PORT", 5000)),
    })


@app.route("/api/stats")
def api_stats():
    pass_rate = round(stats["passed"] / stats["total"] * 100, 1) if stats["total"] else 0
    return jsonify({
        "total": stats["total"],
        "passed": stats["passed"],
        "failed": stats["failed"],
        "passRate": pass_rate,
        "deviceCount": len(devices),
        "onlineCount": sum(1 for d in devices.values() if d["status"] == "online"),
    })


@app.route("/api/devices")
def api_devices():
    return jsonify(list(devices.values()))


@app.route("/api/results")
def api_results():
    limit = request.args.get("limit", 50, type=int)
    all_results = list(results_deque)
    return jsonify(all_results[-limit:])


@app.route("/api/results/recent")
def api_results_recent():
    limit = request.args.get("limit", 50, type=int)
    all_results = list(results_deque)
    recent = all_results[-limit:]
    return jsonify([
        {"index": i, "passed": r["passed"], "roiName": r["roiName"],
         "dateTime": r["dateTime"], "deviceId": r["deviceId"]}
        for i, r in enumerate(recent)
    ])


@app.route("/api/stats/by-roi")
def api_stats_by_roi():
    """ROI 统计：优先内存，回退到 SQLite"""
    if stats["by_roi"]:
        return jsonify(dict(stats["by_roi"]))
    return jsonify(db_load_by_roi())


@app.route("/api/stats/by-hour")
def api_stats_by_hour():
    if stats["by_hour"]:
        return jsonify(dict(stats["by_hour"]))
    return jsonify(db_load_by_hour())


# -------- API: 历史查询 (SQLite) ----------

@app.route("/api/results/history")
def api_results_history():
    """分页历史查询，支持 device/roi/passed/days 过滤"""
    device_id = request.args.get("device", None)
    roi_name = request.args.get("roi", None)
    passed_str = request.args.get("passed", None)
    days = request.args.get("days", None, type=int)
    page = request.args.get("page", 1, type=int)
    page_size = request.args.get("page_size", 50, type=int)
    page_size = min(page_size, 200)

    passed = None
    if passed_str is not None:
        passed = passed_str.lower() in ("1", "true", "pass")

    result = db_query_history(device_id, roi_name, passed, days, page, page_size)
    return jsonify(result)


@app.route("/api/results/summary")
def api_results_summary():
    """时间段聚合统计"""
    days = request.args.get("days", 7, type=int)
    return jsonify({
        "total": stats["total"],
        "passed": stats["passed"],
        "failed": stats["failed"],
        "passRate": round(stats["passed"] / stats["total"] * 100, 1) if stats["total"] else 0,
        "deviceCount": len(devices),
        "onlineCount": sum(1 for d in devices.values() if d["status"] == "online"),
        "period": f"{days}天",
    })


# -------- SSE 实时推送 ----------

@app.route("/stream")
def stream():
    def event_stream():
        q = queue.Queue(maxsize=128)
        with _sse_lock:
            sse_clients.append(q)
        try:
            yield f"event: connected\ndata: {json.dumps({'ok': True})}\n\n"
            while True:
                event, data = q.get()
                yield f"event: {event}\ndata: {json.dumps(data)}\n\n"
        except GeneratorExit:
            pass
        finally:
            with _sse_lock:
                if q in sse_clients:
                    sse_clients.remove(q)

    return Response(
        stream_with_context(event_stream()),
        mimetype="text/event-stream",
        headers={
            "Cache-Control": "no-cache",
            "X-Accel-Buffering": "no",
        },
    )


# -------- API: 远程指令 ----------

@app.route("/api/command", methods=["POST"])
def api_send_command():
    if not mqtt_client or not mqtt_connected:
        return jsonify({"error": "MQTT 未连接"}), 503

    body = request.get_json(force=True)
    cmd = body.get("cmd", "")
    allowed = {"capture", "start_detection", "stop_detection", "ping"}
    if cmd not in allowed:
        return jsonify({"error": f"无效指令: {cmd}，允许: {allowed}"}), 400

    payload = json.dumps({"cmd": cmd, "ts": now_ts()})
    mqtt_client.publish(MQTT_CONFIG["topics"]["commands"], payload, qos=1)
    return jsonify({"ok": True, "cmd": cmd})


# -------- API: 模拟数据 (仅在 ENABLE_SIMULATE=1 时可用) ----------

@app.route("/api/simulate", methods=["POST"])
def api_simulate():
    if not os.environ.get("ENABLE_SIMULATE"):
        return jsonify({"error": "模拟数据接口已禁用，启动时设置 ENABLE_SIMULATE=1 可启用"}), 403

    import random

    body = request.get_json(force=True) or {}
    count = min(body.get("count", 1), 20)

    device_ids = ["edge-device-01", "edge-device-02"]
    roi_names = ["PCB-焊点检测", "元件定位", "条码识别", "外观瑕疵"]
    fail_reasons = ["区域面积超出阈值", "圆度不达标", "条码无法解码", "模板匹配失败"]

    for _ in range(count):
        device_id = random.choice(device_ids)
        passed = random.random() < 0.88
        roi_name = random.choice(roi_names)

        payload = {
            "reportId": uuid.uuid4().hex[:16],
            "deviceId": device_id,
            "imageId": f"sim_{int(time.time())}",
            "roiName": roi_name,
            "roiId": f"roi_{random.randint(1, 5)}",
            "timestamp": now_ts(),
            "dateTime": now_str(),
            "result": {
                "passed": passed,
                "failReason": "" if passed else random.choice(fail_reasons),
                "totalRegionCount": random.randint(0, 10),
                "originalRegionCount": random.randint(0, 15),
            },
            "regions": [],
            "barcodes": [{"type": "QRCode", "data": f"DEMO-{uuid.uuid4().hex[:8].upper()}", "quality": 1.0}]
            if random.random() < 0.3 else [],
        }

        _handle_heartbeat({
            "deviceId": device_id, "status": "online",
            "ts": now_ts(), "uptime": random.randint(60, 36000),
        })
        _handle_result(payload)
        time.sleep(0.05)

    return jsonify({"ok": True, "simulated": count})


# =========================================================================
# 启动
# =========================================================================

if __name__ == "__main__":
    print("=" * 50)
    print("EdgeVision Cloud Dashboard")
    print("边云协同智能视觉检测云平台")
    print("=" * 50)

    # 初始化 SQLite + 加载历史数据
    init_db()

    loaded_devices = db_load_devices()
    devices.update(loaded_devices)

    loaded_results = db_load_recent_results(500)
    for r in loaded_results:
        results_deque.append(r)
        stats["total"] += 1
        if r.get("passed"):
            stats["passed"] += 1
        else:
            stats["failed"] += 1

    # 重建内存统计
    stats["by_roi"].clear()
    stats["by_hour"].clear()
    for r in results_deque:
        roi = r.get("roiName", "全图")
        stats["by_roi"][roi]["total"] += 1
        if r.get("passed"):
            stats["by_roi"][roi]["passed"] += 1
        else:
            stats["by_roi"][roi]["failed"] += 1

        dt = r.get("dateTime", "")
        hour_key = dt[:13] + ":00" if len(dt) >= 13 else datetime.now().strftime("%Y-%m-%d %H:00")
        stats["by_hour"][hour_key]["total"] += 1
        if r.get("passed"):
            stats["by_hour"][hour_key]["passed"] += 1
        else:
            stats["by_hour"][hour_key]["failed"] += 1

    print(f"[DB] 已加载 {len(loaded_devices)} 台设备, {len(loaded_results)} 条结果")

    # 初始化 MQTT
    init_mqtt()

    # 设备超时检测线程
    threading.Thread(target=check_device_timeout, daemon=True).start()

    # 启动 HTTP 服务
    port = int(os.environ.get("PORT", 5000))
    try:
        from waitress import serve
        print(f"[Server] 启动 waitress 服务器 http://0.0.0.0:{port}")
        serve(app, host="0.0.0.0", port=port, threads=8)
    except ImportError:
        print(f"[Server] waitress 未安装，使用 Flask 开发服务器 http://127.0.0.1:{port}")
        app.run(host="0.0.0.0", port=port, debug=False, threaded=True)
