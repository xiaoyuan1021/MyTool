FROM python:3.11-slim

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY app.py config.py db.py mqtt_handler.py ./
COPY templates/ templates/
COPY static/ static/

# 数据目录（SQLite 存储）
RUN mkdir -p /app/data

EXPOSE 5000

CMD ["python", "app.py"]
