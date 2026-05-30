"""
MQTT 回调与设备/结果处理
"""

import json
import queue
import time
import threading
from datetime import datetime

from config import MQTT_CONFIG, DEVICE_TIMEOUT_SECONDS
from db import db_upsert_device, db_insert_result, db_insert_heartbeat


class MQTTHandler:
    def __init__(self, app_state):
        self.s = app_state
        self._recent_results = set()
        self._recent_lock = threading.Lock()

    def on_connect(self, client, userdata, flags, reason_code, properties=None):
        if reason_code == 0:
            self.s["mqtt_connected"] = True
            print(f"[MQTT] 已连接 {MQTT_CONFIG['host']}:{MQTT_CONFIG['port']}")
            topics = [
                (MQTT_CONFIG["topics"]["results"], MQTT_CONFIG["qos"]),
                (MQTT_CONFIG["topics"]["heartbeat"], MQTT_CONFIG["qos"]),
            ]
            client.subscribe(topics)
            self._broadcast_sse("mqtt_status", {"connected": True})
        else:
            print(f"[MQTT] 连接失败, reason_code={reason_code}")

    def on_disconnect(self, client, userdata, reason_code, properties=None):
        self.s["mqtt_connected"] = False
        print(f"[MQTT] 断开连接 (reason_code={reason_code})")
        self._broadcast_sse("mqtt_status", {"connected": False})

    def on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode("utf-8"))
        except json.JSONDecodeError:
            return
        if msg.topic == MQTT_CONFIG["topics"]["heartbeat"]:
            self._handle_heartbeat(payload)
        elif msg.topic == MQTT_CONFIG["topics"]["results"]:
            self._handle_result(payload)

    def _handle_heartbeat(self, payload):
        device_id = payload.get("deviceId", "unknown")
        now = time.time()
        now_str_val = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        devices = self.s["devices"]

        with self.s["data_lock"]:
            if device_id not in devices:
                devices[device_id] = {
                    "device_id": device_id, "status": "online",
                    "first_seen": now_str_val, "last_heartbeat": now_str_val,
                    "last_ts": now, "uptime": payload.get("uptime", 0),
                    "result_count": 0, "last_result": None,
                }
            else:
                dev = devices[device_id]
                dev["status"] = "online"
                dev["last_heartbeat"] = now_str_val
                dev["last_ts"] = now
                dev["uptime"] = payload.get("uptime", dev["uptime"])

        db_upsert_device(device_id, devices[device_id])
        db_insert_heartbeat(device_id, "online", payload.get("uptime", 0),
                            payload.get("ts", int(now * 1000)))
        self._broadcast_sse("heartbeat", {"deviceId": device_id})

    def _handle_result(self, payload):
        report_id = payload.get("reportId", "")
        if report_id:
            with self._recent_lock:
                if report_id in self._recent_results:
                    return
                self._recent_results.add(report_id)
                if len(self._recent_results) > 500:
                    self._recent_results.clear()

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
            "timestamp": payload.get("timestamp", int(time.time() * 1000)),
            "dateTime": payload.get("dateTime", datetime.now().strftime("%Y-%m-%d %H:%M:%S")),
            "regionCount": len(payload.get("regions", [])),
            "barcodeCount": len(payload.get("barcodes", [])),
            "hasBarcode": len(payload.get("barcodes", [])) > 0,
        }

        self.s["results_deque"].append(record)

        with self.s["data_lock"]:
            devices = self.s["devices"]
            stats = self.s["stats"]

            if device_id not in devices:
                now_str_val = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                devices[device_id] = {
                    "device_id": device_id, "status": "online",
                    "first_seen": now_str_val, "last_heartbeat": now_str_val,
                    "last_ts": time.time(), "uptime": 0,
                    "result_count": 0, "last_result": None,
                }
                db_upsert_device(device_id, devices[device_id])

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

            if len(stats["by_hour"]) > 168:
                sorted_hours = sorted(stats["by_hour"].keys())
                for old_hour in sorted_hours[:-168]:
                    del stats["by_hour"][old_hour]

            dev = devices[device_id]
            dev["result_count"] = dev.get("result_count", 0) + 1
            dev["last_result"] = {
                "passed": passed, "roiName": roi_name,
                "dateTime": record["dateTime"],
            }

        db_upsert_device(device_id, devices[device_id])
        db_insert_result(record)
        self._broadcast_sse("result", record)

    def check_device_timeout(self):
        while True:
            time.sleep(15)
            now = time.time()
            now_str_val = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            devices = self.s["devices"]
            with self.s["data_lock"]:
                device_list = list(devices.items())
            for device_id, dev in device_list:
                if dev["status"] == "online" and (now - dev.get("last_ts", 0)) > DEVICE_TIMEOUT_SECONDS:
                    with self.s["data_lock"]:
                        dev["status"] = "offline"
                        dev["last_heartbeat"] = now_str_val
                    db_upsert_device(device_id, dev)
                    self._broadcast_sse("heartbeat", {"deviceId": device_id, "status": "offline"})

    def _broadcast_sse(self, event, data):
        with self.s["sse_lock"]:
            dead = []
            for q in list(self.s["sse_clients"]):
                try:
                    q.put_nowait((event, data))
                except (queue.Full, Exception):
                    dead.append(q)
            for q in dead:
                try:
                    self.s["sse_clients"].remove(q)
                except ValueError:
                    pass
