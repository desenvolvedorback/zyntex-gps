# app.py
from flask import Flask, request, jsonify, redirect
from Crypto.Cipher import AES
from Crypto.Util.Padding import unpad
import base64
import sqlite3
from datetime import datetime
from flask_cors import CORS

AES_KEY = b'16chaveaesexemplo'  # MESMA chave do Arduino (16 bytes)
DB_PATH = 'track.db'

app = Flask(__name__)
CORS(app)

def init_db():
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('''
        CREATE TABLE IF NOT EXISTS locations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device TEXT,
            lat REAL,
            lon REAL,
            alt REAL,
            hdop REAL,
            time TEXT,
            pressure REAL,
            temp REAL,
            received_at TEXT
        )
    ''')
    conn.commit()
    conn.close()

@app.route('/public/receive', methods=['POST'])
def receive():
    data = request.get_json(force=True)
    if not data or 'payload' not in data or 'iv' not in data:
        return jsonify({'error':'invalid payload'}), 400

    try:
        payload_b64 = data['payload']
        iv_b64 = data['iv']
        cipher = base64.b64decode(payload_b64)
        iv = base64.b64decode(iv_b64)
        cipher_obj = AES.new(AES_KEY, AES.MODE_CBC, iv)
        plain = unpad(cipher_obj.decrypt(cipher), AES.block_size)
        plain_text = plain.decode('utf-8')
    except Exception as e:
        return jsonify({'error':'decrypt_failed', 'msg':str(e)}), 400

    # plain_text is JSON string
    import json
    try:
        j = json.loads(plain_text)
    except Exception as e:
        return jsonify({'error':'json_parse', 'msg':str(e), 'plain':plain_text}), 400

    # save to sqlite
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('''
        INSERT INTO locations (device, lat, lon, alt, hdop, time, pressure, temp, received_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    ''', (
        j.get('device'),
        j.get('lat'),
        j.get('lon'),
        j.get('alt'),
        j.get('hdop'),
        j.get('time'),
        j.get('pressure'),
        j.get('temp'),
        datetime.utcnow().isoformat() + 'Z'
    ))
    conn.commit()
    conn.close()

    return jsonify({'status':'ok'})

@app.route('/last/<device>', methods=['GET'])
def last(device):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('SELECT device,lat,lon,alt,hdop,time,pressure,temp,received_at FROM locations WHERE device=? ORDER BY id DESC LIMIT 1', (device,))
    row = c.fetchone()
    conn.close()
    if not row:
        return jsonify({'error':'not_found'}), 404
    keys = ['device','lat','lon','alt','hdop','time','pressure','temp','received_at']
    return jsonify(dict(zip(keys, row)))

@app.route('/map/<device>', methods=['GET'])
def map_redirect(device):
    # redireciona para google maps com última posição
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('SELECT lat,lon FROM locations WHERE device=? ORDER BY id DESC LIMIT 1', (device,))
    row = c.fetchone()
    conn.close()
    if not row:
        return jsonify({'error':'not_found'}), 404
    lat, lon = row
    url = f"https://www.google.com/maps/search/?api=1&query={lat},{lon}"
    return redirect(url)

if __name__ == '__main__':
    init_db()
    app.run(host='0.0.0.0', port=5000)
