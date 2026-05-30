"""
EdgeVision Cloud Dashboard - Flask 入口
"""

import json
import os
import queue
import socket
import sys
import threading
import time

# ===== 日志同时输出到文件和控制台 =====
import logging
_log_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dashboard.log")
_log_handler = logging.FileHandler(_log_file, encoding="utf-8")
_log_handler.setFormatter(logging.Formatter("[%(asctime)s] %(message)s", datefmt="%H:%M:%S"))
_console_handler = logging.StreamHandler(sys.stdout)
_console_handler.setFormatter(logging.Formatter("%(message)s"))
_logging = logging.getLogger("dashboard")
_logging.setLevel(logging.DEBUG)
_logging.addHandler(_log_handler)
_logging.addHandler(_console_handler)
_log = _logging.info
from collections import deque, defaultdict
from datetime import datetime

import paho.mqtt.client as mqtt
from flask import Flask, jsonify, render_template, request, Response, stream_with_context

from config import MQTT_CONFIG, MAX_RESULTS
from db import (
    init_db, db_load_devices, db_load_recent_results,
    db_load_by_roi, db_load_by_hour, db_load_total_stats,
    db_load_today_stats, db_query_history,
)
from mqtt_handler import MQTTHandler

app = Flask(__name__)

# ========== 共享状态 ==========
app_state = {
    "devices": {},
    "results_deque": deque(maxlen=MAX_RESULTS),
    "stats": {
        "total": 0, "passed": 0, "failed": 0,
        "today": 0, "today_passed": 0, "today_failed": 0,
        "by_roi": defaultdict(lambda: {"total": 0, "passed": 0, "failed": 0}),
        "by_hour": defaultdict(lambda: {"total": 0, "passed": 0, "failed": 0}),
    },
    "mqtt_connected": False,
    "sse_clients": [],
    "data_lock": threading.Lock(),
    "sse_lock": threading.Lock(),
}

handler = MQTTHandler(app_state)


# ========== 工具函数 ==========

def now_ts():
    return int(time.time() * 1000)


def get_lan_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.settimeout(0.1)
        s.connect(("10.254.254.254", 1))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        try:
            return socket.gethostbyname(socket.gethostname())
        except Exception:
            return "127.0.0.1"


def init_mqtt():
    cfg = MQTT_CONFIG
    client = mqtt.Client(
        client_id=cfg["client_id"],
        protocol=mqtt.MQTTv311,
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
    )
    client.on_connect = handler.on_connect
    client.on_disconnect = handler.on_disconnect
    client.on_message = handler.on_message
    client.reconnect_delay_set(min_delay=1, max_delay=30)
    app_state["mqtt_client"] = client
    try:
        client.connect(cfg["host"], cfg["port"], cfg["keepalive"])
        client.loop_start()
        _log(f"[MQTT] 正在连接 {cfg['host']}:{cfg['port']}...")
    except Exception as e:
        _log(f"[MQTT] 连接失败: {e}")


# ========== 路由 ==========

@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/status")
def api_status():
    with app_state["data_lock"]:
        return jsonify({
            "mqtt_connected": app_state["mqtt_connected"],
            "device_count": len(app_state["devices"]),
            "online_count": sum(1 for d in app_state["devices"].values() if d["status"] == "online"),
            "total_results": app_state["stats"]["total"],
        })


@app.route("/api/host-info")
def api_host_info():
    return jsonify({"lanIp": get_lan_ip(), "port": int(os.environ.get("PORT", 5000))})


@app.route("/api/stats")
def api_stats():
    with app_state["data_lock"]:
        total = app_state["stats"]["total"]
        passed = app_state["stats"]["passed"]
        failed = app_state["stats"]["failed"]
        device_count = len(app_state["devices"])
        online_count = sum(1 for d in app_state["devices"].values() if d["status"] == "online")
    pass_rate = round(passed / total * 100, 1) if total else 0
    _log(f"[DEBUG] GET /api/stats → total={total}, passed={passed}, failed={failed}, passRate={pass_rate}")
    return jsonify({
        "total": total,
        "passed": passed,
        "failed": failed,
        "passRate": pass_rate,
        "deviceCount": device_count,
        "onlineCount": online_count,
    })


@app.route("/api/stats/today")
def api_stats_today():
    with app_state["data_lock"]:
        today_total = app_state["stats"]["today"]
        today_passed = app_state["stats"]["today_passed"]
        today_failed = app_state["stats"]["today_failed"]
        device_count = len(app_state["devices"])
        online_count = sum(1 for d in app_state["devices"].values() if d["status"] == "online")
    pass_rate = round(today_passed / today_total * 100, 1) if today_total else 0
    return jsonify({
        "total": today_total, "passed": today_passed,
        "failed": today_failed, "passRate": pass_rate,
        "deviceCount": device_count, "onlineCount": online_count,
    })


@app.route("/api/devices")
def api_devices():
    with app_state["data_lock"]:
        return jsonify(list(app_state["devices"].values()))


@app.route("/api/results")
def api_results():
    limit = request.args.get("limit", 50, type=int)
    return jsonify(list(app_state["results_deque"])[-limit:])


@app.route("/api/results/recent")
def api_results_recent():
    limit = request.args.get("limit", 50, type=int)
    return jsonify(list(app_state["results_deque"])[-limit:])


@app.route("/api/stats/by-roi")
def api_stats_by_roi():
    return jsonify(db_load_by_roi())


@app.route("/api/stats/by-hour")
def api_stats_by_hour():
    with app_state["data_lock"]:
        has_hour = bool(app_state["stats"]["by_hour"])
    if has_hour:
        with app_state["data_lock"]:
            return jsonify(dict(app_state["stats"]["by_hour"]))
    return jsonify(db_load_by_hour())


@app.route("/api/results/history")
def api_results_history():
    device_id = request.args.get("device")
    roi_name = request.args.get("roi")
    passed_str = request.args.get("passed")
    days = request.args.get("days", None, type=int)
    page = request.args.get("page", 1, type=int)
    page_size = min(request.args.get("page_size", 50, type=int), 200)
    passed = None
    if passed_str is not None:
        passed = passed_str.lower() in ("1", "true", "pass")
    return jsonify(db_query_history(device_id, roi_name, passed, days, page, page_size))


@app.route("/api/results/summary")
def api_results_summary():
    days = request.args.get("days", 7, type=int)
    total_stats = db_load_total_stats()
    pass_rate = round(total_stats["passed"] / total_stats["total"] * 100, 1) if total_stats["total"] else 0
    with app_state["data_lock"]:
        device_count = len(app_state["devices"])
        online_count = sum(1 for d in app_state["devices"].values() if d["status"] == "online")
    return jsonify({
        "total": total_stats["total"], "passed": total_stats["passed"],
        "failed": total_stats["failed"], "passRate": pass_rate,
        "deviceCount": device_count, "onlineCount": online_count,
        "period": f"{days}天" if days else "全部",
    })


@app.route("/stream")
def stream():
    def event_stream():
        q = queue.Queue(maxsize=128)
        with app_state["sse_lock"]:
            app_state["sse_clients"].append(q)
        try:
            yield "event: connected\ndata: " + json.dumps({"ok": True}) + "\n\n"
            yield "event: mqtt_status\ndata: " + json.dumps({"connected": app_state["mqtt_connected"]}) + "\n\n"
            while True:
                try:
                    event, data = q.get(timeout=30)
                    yield f"event: {event}\ndata: {json.dumps(data)}\n\n"
                except queue.Empty:
                    yield ": keepalive\n\n"
        except GeneratorExit:
            pass
        finally:
            with app_state["sse_lock"]:
                if q in app_state["sse_clients"]:
                    app_state["sse_clients"].remove(q)

    return Response(
        stream_with_context(event_stream()),
        mimetype="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"},
    )


@app.route("/api/command", methods=["POST"])
def api_send_command():
    if not app_state.get("mqtt_client") or not app_state["mqtt_connected"]:
        return jsonify({"error": "MQTT 未连接"}), 503
    body = request.get_json(force=True)
    cmd = body.get("cmd", "")
    allowed = {"capture", "start_detection", "stop_detection", "ping"}
    if cmd not in allowed:
        return jsonify({"error": f"无效指令: {cmd}，允许: {allowed}"}), 400
    payload = json.dumps({"cmd": cmd, "ts": now_ts()})
    app_state["mqtt_client"].publish(MQTT_CONFIG["topics"]["commands"], payload, qos=1)
    return jsonify({"ok": True, "cmd": cmd})


@app.route("/api/simulate", methods=["POST"])
def api_simulate():
    import random, uuid
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
            "reportId": uuid.uuid4().hex[:16], "deviceId": device_id,
            "roiName": roi_name, "timestamp": now_ts(),
            "dateTime": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "result": {"passed": passed, "failReason": "" if passed else random.choice(fail_reasons),
                       "totalRegionCount": random.randint(0, 10)},
            "regions": [],
            "barcodes": [{"type": "QRCode", "data": f"DEMO-{uuid.uuid4().hex[:8].upper()}"}]
            if random.random() < 0.3 else [],
        }
        handler._handle_heartbeat({"deviceId": device_id, "status": "online",
                                   "ts": now_ts(), "uptime": random.randint(60, 36000)})
        handler._handle_result(payload)
        time.sleep(0.05)
    return jsonify({"ok": True, "count": count})


# ========== 调试 API ==========

@app.route("/api/debug/diag")
def api_debug_diag():
    total_db = db_load_total_stats()
    today_db = db_load_today_stats()
    with app_state["data_lock"]:
        return jsonify({
            "memory_stats": {"total": app_state["stats"]["total"], "passed": app_state["stats"]["passed"],
                             "failed": app_state["stats"]["failed"]},
            "db_total": total_db, "db_today": today_db,
            "api_stats_now": {"total": total_db["total"], "passed": total_db["passed"], "failed": total_db["failed"]},
            "devices_in_mem": len(app_state["devices"]),
            "deque_len": len(app_state["results_deque"]),
            "by_roi_count": len(app_state["stats"]["by_roi"]),
            "by_hour_count": len(app_state["stats"]["by_hour"]),
            "db_path": os.path.abspath(os.path.join(os.path.dirname(__file__), "dashboard.db")),
        })


@app.route("/api/debug/update-stats", methods=["POST"])
def api_debug_update_stats():
    try:
        body = request.get_json(force=True)
        total = body.get("total", 0)
        passed = body.get("passed", 0)
        failed = body.get("failed", 0)

        with app_state["data_lock"]:
            old = dict(app_state["stats"])
            app_state["stats"]["total"] = total
            app_state["stats"]["passed"] = passed
            app_state["stats"]["failed"] = failed
            today_stats = db_load_today_stats()
            app_state["stats"]["today"] = today_stats["total"]
            app_state["stats"]["today_passed"] = today_stats["passed"]
            app_state["stats"]["today_failed"] = today_stats["failed"]
        _log(f"[DEBUG] update-stats 收到 total={total}, passed={passed}, failed={failed}")
        _log(f"[DEBUG] update-stats 之前 stats: total={old['total']}, passed={old['passed']}, failed={old['failed']}, today={old['today']}")
        _log(f"[DEBUG] update-stats 之后 stats: total={app_state['stats']['total']}, passed={app_state['stats']['passed']}, failed={app_state['stats']['failed']}, today={app_state['stats']['today']}")

        app_state["results_deque"].clear()
        loaded = db_load_recent_results(500)
        for r in loaded:
            app_state["results_deque"].append(r)

        return jsonify({"ok": True})
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/debug/update-roi", methods=["POST"])
def api_debug_update_roi():
    try:
        body = request.get_json(force=True)
        old_name = body.get("oldName")
        new_name = body.get("newName", old_name) or old_name
        total = body.get("total", 0)
        passed = body.get("passed", 0)
        if not old_name:
            return jsonify({"error": "请指定要更新的ROI"}), 400
        with app_state["data_lock"]:
            old_data = app_state["stats"]["by_roi"].get(new_name, {"total": 0, "passed": 0, "failed": 0})
            if old_name in app_state["stats"]["by_roi"] and old_name != new_name:
                del app_state["stats"]["by_roi"][old_name]
            app_state["stats"]["by_roi"][new_name] = {"total": total, "passed": passed, "failed": total - passed}
            delta_total = total - old_data["total"]
            delta_passed = passed - old_data["passed"]
            delta_failed = (total - passed) - old_data["failed"]
            app_state["stats"]["total"] += delta_total
            app_state["stats"]["passed"] += delta_passed
            app_state["stats"]["failed"] += delta_failed
        import sqlite3
        from config import DB_PATH
        conn = sqlite3.connect(DB_PATH)
        try:
            if old_name != new_name:
                conn.execute("UPDATE results SET roi_name=? WHERE roi_name=?", (new_name, old_name))
            old_records = conn.execute("SELECT id, passed FROM results WHERE roi_name=?", (new_name,)).fetchall()
            if len(old_records) != total:
                conn.execute("DELETE FROM results WHERE roi_name=?", (new_name,))
                for i in range(total):
                    p = i < passed
                    conn.execute(
                        "INSERT INTO results(report_id,device_id,roi_name,passed,fail_reason,"
                        "total_region_count,region_count,barcode_count,has_barcode,"
                        "timestamp,date_time,raw_json) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)",
                        (f"debug-roi-{new_name}-{i}", "__debug__", new_name, 1 if p else 0, "",
                         0, 0, 0, 0, int(time.time() * 1000),
                         datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                         json.dumps({"debug": True, "passed": p}, ensure_ascii=False)),
                    )
            else:
                for idx, row in enumerate(old_records):
                    new_passed = 1 if idx < passed else 0
                    if row["passed"] != new_passed:
                        conn.execute("UPDATE results SET passed=? WHERE id=?", (new_passed, row["id"]))
            conn.commit()
        finally:
            conn.close()
        return jsonify({"ok": True})
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/debug/add-roi", methods=["POST"])
def api_debug_add_roi():
    try:
        body = request.get_json(force=True)
        name = body.get("name")
        total = body.get("total", 0)
        passed = body.get("passed", 0)
        if not name:
            return jsonify({"error": "ROI名称不能为空"}), 400
        with app_state["data_lock"]:
            app_state["stats"]["by_roi"][name] = {"total": total, "passed": passed, "failed": total - passed}
            app_state["stats"]["total"] += total
            app_state["stats"]["passed"] += passed
            app_state["stats"]["failed"] += (total - passed)
        import sqlite3, uuid as _uuid
        from config import DB_PATH
        conn = sqlite3.connect(DB_PATH)
        try:
            for i in range(total):
                p = i < passed
                conn.execute(
                    "INSERT INTO results(report_id,device_id,roi_name,passed,fail_reason,"
                    "total_region_count,region_count,barcode_count,has_barcode,"
                    "timestamp,date_time,raw_json) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)",
                    (f"debug-roi-{name}-{_uuid.uuid4().hex[:8]}", "__debug__", name, 1 if p else 0, "",
                     0, 0, 0, 0, int(time.time() * 1000),
                     datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                     json.dumps({"debug": True, "passed": p}, ensure_ascii=False)),
                )
            conn.commit()
        finally:
            conn.close()
        return jsonify({"ok": True})
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/debug/delete-roi", methods=["POST"])
def api_debug_delete_roi():
    try:
        body = request.get_json(force=True)
        name = body.get("name")
        removed = {"total": 0, "passed": 0, "failed": 0}
        with app_state["data_lock"]:
            if name in app_state["stats"]["by_roi"]:
                removed = dict(app_state["stats"]["by_roi"][name])
                del app_state["stats"]["by_roi"][name]
                app_state["stats"]["total"] -= removed["total"]
                app_state["stats"]["passed"] -= removed["passed"]
                app_state["stats"]["failed"] -= removed["failed"]
                app_state["stats"]["total"] = max(0, app_state["stats"]["total"])
                app_state["stats"]["passed"] = max(0, app_state["stats"]["passed"])
                app_state["stats"]["failed"] = max(0, app_state["stats"]["failed"])
        import sqlite3
        from config import DB_PATH
        conn = sqlite3.connect(DB_PATH)
        try:
            conn.execute("DELETE FROM results WHERE roi_name=?", (name,))
            conn.commit()
        finally:
            conn.close()
        return jsonify({"ok": True, "removed": removed})
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/debug/update-device", methods=["POST"])
def api_debug_update_device():
    try:
        body = request.get_json(force=True)
        device_id = body.get("deviceId")
        with app_state["data_lock"]:
            devices = app_state["devices"]
            now_str_val = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            if device_id in devices:
                devices[device_id].update({
                    "status": body.get("status", "online"),
                    "result_count": body.get("resultCount", 0),
                    "uptime": body.get("uptime", 0),
                    "last_heartbeat": now_str_val,
                    "last_ts": time.time(),
                })
            else:
                devices[device_id] = {
                    "device_id": device_id, "status": body.get("status", "online"),
                    "first_seen": now_str_val,
                    "last_heartbeat": now_str_val,
                    "last_ts": time.time(), "uptime": body.get("uptime", 0),
                    "result_count": body.get("resultCount", 0), "last_result": None,
                }
        from db import db_upsert_device
        db_upsert_device(device_id, app_state["devices"][device_id])
        handler._broadcast_sse("heartbeat", {"deviceId": device_id})
        return jsonify({"ok": True})
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/debug/add-device", methods=["POST"])
def api_debug_add_device():
    try:
        body = request.get_json(force=True)
        device_id = body.get("deviceId", "").strip()
        if not device_id:
            return jsonify({"error": "设备ID不能为空"}), 400
        with app_state["data_lock"]:
            app_state["devices"][device_id] = {
                "device_id": device_id, "status": body.get("status", "online"),
                "first_seen": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                "last_heartbeat": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                "last_ts": time.time(), "uptime": body.get("uptime", 0),
                "result_count": body.get("resultCount", 0), "last_result": None,
            }
        from db import db_upsert_device
        db_upsert_device(device_id, app_state["devices"][device_id])
        handler._broadcast_sse("heartbeat", {"deviceId": device_id})
        return jsonify({"ok": True})
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/debug/clear-data", methods=["POST"])
def api_debug_clear_data():
    try:
        with app_state["data_lock"]:
            app_state["devices"].clear()
            app_state["results_deque"].clear()
            app_state["stats"]["total"] = 0
            app_state["stats"]["passed"] = 0
            app_state["stats"]["failed"] = 0
            app_state["stats"]["today"] = 0
            app_state["stats"]["today_passed"] = 0
            app_state["stats"]["today_failed"] = 0
            app_state["stats"]["by_roi"].clear()
            app_state["stats"]["by_hour"].clear()
        import sqlite3
        from config import DB_PATH
        conn = sqlite3.connect(DB_PATH)
        try:
            conn.execute("DELETE FROM devices")
            conn.execute("DELETE FROM results")
            conn.execute("DELETE FROM heartbeats")
            conn.commit()
        finally:
            conn.close()
        handler._broadcast_sse("heartbeat", {"deviceId": "all", "status": "cleared"})
        return jsonify({"ok": True})
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/debug/export")
def api_debug_export():
    try:
        with app_state["data_lock"]:
            data = {
                "stats": {
                    "total": app_state["stats"]["total"],
                    "passed": app_state["stats"]["passed"],
                    "failed": app_state["stats"]["failed"],
                    "by_roi": dict(app_state["stats"]["by_roi"]),
                    "by_hour": dict(app_state["stats"]["by_hour"]),
                },
                "devices": list(app_state["devices"].values()),
                "results": list(app_state["results_deque"])[-100:],
            }
        return jsonify(data)
    except Exception as e:
        return jsonify({"error": str(e)}), 500


# ========== 启动 ==========

if __name__ == "__main__":
    _log("=" * 50)
    _log("EdgeVision Cloud Dashboard")
    _log("边云协同智能视觉检测云平台")
    _log("=" * 50)

    init_db()

    today_str = datetime.now().strftime("%Y-%m-%d")

    try:
        loaded_devices = db_load_devices()
        app_state["devices"].update(loaded_devices)
        _log(f"[DB] 已加载 {len(loaded_devices)} 台设备")
    except Exception as e:
        _log(f"[DB] 加载设备失败: {e}")

    try:
        total_stats = db_load_total_stats()
        today_stats = db_load_today_stats()
        app_state["stats"]["total"] = total_stats["total"]
        app_state["stats"]["passed"] = total_stats["passed"]
        app_state["stats"]["failed"] = total_stats["failed"]
        app_state["stats"]["today"] = today_stats["total"]
        app_state["stats"]["today_passed"] = today_stats["passed"]
        app_state["stats"]["today_failed"] = today_stats["failed"]
        _log(f"[DB] 全量统计: total={total_stats['total']}, passed={total_stats['passed']}, failed={total_stats['failed']}")
        _log(f"[DB] 今日统计: total={today_stats['total']}, passed={today_stats['passed']}, failed={today_stats['failed']}")
    except Exception as e:
        _log(f"[DB] 加载统计失败: {e}")

    try:
        app_state["stats"]["by_roi"].clear()
        app_state["stats"]["by_hour"].clear()
        for roi_name, roi_data in db_load_by_roi().items():
            app_state["stats"]["by_roi"][roi_name] = roi_data
        for hour_key, hour_data in db_load_by_hour().items():
            app_state["stats"]["by_hour"][hour_key] = hour_data
        _log(f"[DB] ROI 统计: {len(app_state['stats']['by_roi'])} 个, 小时统计: {len(app_state['stats']['by_hour'])} 个")
    except Exception as e:
        _log(f"[DB] 加载维度统计失败: {e}")

    try:
        loaded_results = db_load_recent_results(500)
        for r in loaded_results:
            app_state["results_deque"].append(r)
        _log(f"[DB] 已加载 {len(loaded_results)} 条最近结果到缓存")
    except Exception as e:
        _log(f"[DB] 加载结果失败: {e}")

    init_mqtt()

    threading.Thread(target=handler.check_device_timeout, daemon=True).start()

    port = int(os.environ.get("PORT", 5000))
    try:
        from waitress import serve
        _log(f"[Server] 启动 waitress 服务器 http://0.0.0.0:{port}")
        serve(app, host="0.0.0.0", port=port, threads=8)
    except ImportError:
        _log(f"[Server] waitress 未安装，使用 Flask 开发服务器 http://127.0.0.1:{port}")
        app.run(host="0.0.0.0", port=port, debug=False, threaded=True)
