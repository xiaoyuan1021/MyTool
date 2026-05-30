import os
import uuid

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
MAX_RESULTS = 2000
MAX_HOURLY_ENTRIES = 168  # 7 days * 24 hours
