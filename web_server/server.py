    #!/usr/bin/env python3
"""
ESP32-S3 Attendance System Server
Receives attendance records from ESP32 and stores them in SQLite database
"""

from flask import Flask, request, jsonify, render_template_string
from datetime import datetime
import sqlite3
import json
import logging
from pathlib import Path

# Configuration
DATABASE_FILE = 'attendance.db'
SERVER_HOST = '0.0.0.0'  # Listen on all interfaces
SERVER_PORT = 8063
LOG_FILE = 'attendance_server.log'

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(LOG_FILE),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

app = Flask(__name__)

# ==================== Database Functions ====================

def init_database():
    """Initialize SQLite database with required tables"""
    conn = sqlite3.connect(DATABASE_FILE)
    cursor = conn.cursor()
    
    # Create attendance records table
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS attendance (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            fingerprint_id INTEGER NOT NULL,
            timestamp TEXT NOT NULL,
            login_method TEXT NOT NULL,
            device_ip TEXT,
            received_at TEXT NOT NULL,
            UNIQUE(fingerprint_id, timestamp)
        )
    ''')
    
    # Create users table (for mapping fingerprint IDs to names)
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS users (
            fingerprint_id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            employee_id TEXT,
            department TEXT,
            created_at TEXT NOT NULL
        )
    ''')
    
    # Create index for faster queries
    cursor.execute('''
        CREATE INDEX IF NOT EXISTS idx_timestamp 
        ON attendance(timestamp)
    ''')
    
    cursor.execute('''
        CREATE INDEX IF NOT EXISTS idx_fingerprint_id 
        ON attendance(fingerprint_id)
    ''')
    
    conn.commit()
    conn.close()
    logger.info("Database initialized successfully")


def insert_attendance(fingerprint_id, timestamp, login_method, device_ip):
    """Insert attendance record into database"""
    conn = sqlite3.connect(DATABASE_FILE)
    cursor = conn.cursor()
    
    received_at = datetime.now().isoformat()
    
    try:
        cursor.execute('''
            INSERT INTO attendance (fingerprint_id, timestamp, login_method, device_ip, received_at)
            VALUES (?, ?, ?, ?, ?)
        ''', (fingerprint_id, timestamp, login_method, device_ip, received_at))
        
        conn.commit()
        record_id = cursor.lastrowid
        logger.info(f"Attendance recorded: ID={record_id}, FP_ID={fingerprint_id}, Time={timestamp}")
        return record_id
        
    except sqlite3.IntegrityError:
        logger.warning(f"Duplicate attendance record: FP_ID={fingerprint_id}, Time={timestamp}")
        return None
    except Exception as e:
        logger.error(f"Database error: {e}")
        return None
    finally:
        conn.close()


def get_attendance_records(limit=100, fingerprint_id=None, date=None):
    """Retrieve attendance records from database"""
    conn = sqlite3.connect(DATABASE_FILE)
    conn.row_factory = sqlite3.Row
    cursor = conn.cursor()
    
    query = '''
        SELECT a.*, u.name, u.employee_id, u.department
        FROM attendance a
        LEFT JOIN users u ON a.fingerprint_id = u.fingerprint_id
        WHERE 1=1
    '''
    params = []
    
    if fingerprint_id:
        query += ' AND a.fingerprint_id = ?'
        params.append(fingerprint_id)
    
    if date:
        query += ' AND DATE(a.timestamp) = ?'
        params.append(date)
    
    query += ' ORDER BY a.timestamp DESC LIMIT ?'
    params.append(limit)
    
    cursor.execute(query, params)
    records = [dict(row) for row in cursor.fetchall()]
    conn.close()
    
    return records


def add_user(fingerprint_id, name, employee_id=None, department=None):
    """Add or update user information"""
    conn = sqlite3.connect(DATABASE_FILE)
    cursor = conn.cursor()
    
    created_at = datetime.now().isoformat()
    
    try:
        cursor.execute('''
            INSERT OR REPLACE INTO users (fingerprint_id, name, employee_id, department, created_at)
            VALUES (?, ?, ?, ?, ?)
        ''', (fingerprint_id, name, employee_id, department, created_at))
        
        conn.commit()
        logger.info(f"User added/updated: FP_ID={fingerprint_id}, Name={name}")
        return True
        
    except Exception as e:
        logger.error(f"Error adding user: {e}")
        return False
    finally:
        conn.close()


def get_users():
    """Get all registered users"""
    conn = sqlite3.connect(DATABASE_FILE)
    conn.row_factory = sqlite3.Row
    cursor = conn.cursor()
    
    cursor.execute('SELECT * FROM users ORDER BY name')
    users = [dict(row) for row in cursor.fetchall()]
    conn.close()
    
    return users


# ==================== API Endpoints ====================

@app.route('/attendance', methods=['POST'])
def receive_attendance():
    """
    Receive attendance data from ESP32
    Expected JSON format:
    {
        "fingerprint_id": 5,
        "timestamp": "2025-12-18T14:30:00+02:00",
        "login_method": "fingerprint"
    }
    """
    try:
        # Get JSON data
        data = request.get_json()
        
        if not data:
            logger.warning("Received empty request")
            return jsonify({'error': 'No data received'}), 400
        
        # Validate required fields
        required_fields = ['fingerprint_id', 'timestamp', 'login_method']
        missing_fields = [field for field in required_fields if field not in data]
        
        if missing_fields:
            logger.warning(f"Missing fields: {missing_fields}")
            return jsonify({'error': f'Missing fields: {missing_fields}'}), 400
        
        # Extract data
        fingerprint_id = data['fingerprint_id']
        timestamp = data['timestamp']
        login_method = data['login_method']
        device_ip = request.remote_addr
        
        # Validate fingerprint_id
        if not isinstance(fingerprint_id, int) or fingerprint_id < 1 or fingerprint_id > 20:
            logger.warning(f"Invalid fingerprint_id: {fingerprint_id}")
            return jsonify({'error': 'Invalid fingerprint_id (must be 1-20)'}), 400
        
        # Insert into database
        record_id = insert_attendance(fingerprint_id, timestamp, login_method, device_ip)
        
        if record_id:
            response = {
                'status': 'success',
                'message': 'Attendance recorded',
                'record_id': record_id,
                'fingerprint_id': fingerprint_id,
                'timestamp': timestamp
            }
            logger.info(f"Attendance success: {response}")
            return jsonify(response), 200
        else:
            response = {
                'status': 'duplicate',
                'message': 'Duplicate attendance record (already exists)',
                'fingerprint_id': fingerprint_id,
                'timestamp': timestamp
            }
            logger.info(f"Duplicate attendance: {response}")
            return jsonify(response), 200  # Return 200 even for duplicates
        
    except Exception as e:
        logger.error(f"Error processing attendance: {e}", exc_info=True)
        return jsonify({'error': 'Internal server error'}), 500


@app.route('/attendance', methods=['GET'])
def list_attendance():
    """Get attendance records with optional filters"""
    try:
        # Get query parameters
        limit = request.args.get('limit', 100, type=int)
        fingerprint_id = request.args.get('fingerprint_id', type=int)
        date = request.args.get('date')  # Format: YYYY-MM-DD
        
        # Retrieve records
        records = get_attendance_records(limit=limit, fingerprint_id=fingerprint_id, date=date)
        
        return jsonify({
            'status': 'success',
            'count': len(records),
            'records': records
        }), 200
        
    except Exception as e:
        logger.error(f"Error retrieving attendance: {e}", exc_info=True)
        return jsonify({'error': 'Internal server error'}), 500


@app.route('/users', methods=['GET'])
def list_users():
    """Get all registered users"""
    try:
        users = get_users()
        return jsonify({
            'status': 'success',
            'count': len(users),
            'users': users
        }), 200
        
    except Exception as e:
        logger.error(f"Error retrieving users: {e}", exc_info=True)
        return jsonify({'error': 'Internal server error'}), 500


@app.route('/users', methods=['POST'])
def add_user_endpoint():
    """
    Add or update user information
    Expected JSON format:
    {
        "fingerprint_id": 5,
        "name": "John Doe",
        "employee_id": "EMP001",
        "department": "Engineering"
    }
    """
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({'error': 'No data received'}), 400
        
        # Validate required fields
        if 'fingerprint_id' not in data or 'name' not in data:
            return jsonify({'error': 'Missing required fields: fingerprint_id, name'}), 400
        
        fingerprint_id = data['fingerprint_id']
        name = data['name']
        employee_id = data.get('employee_id')
        department = data.get('department')
        
        # Validate fingerprint_id
        if not isinstance(fingerprint_id, int) or fingerprint_id < 1 or fingerprint_id > 20:
            return jsonify({'error': 'Invalid fingerprint_id (must be 1-20)'}), 400
        
        # Add user
        success = add_user(fingerprint_id, name, employee_id, department)
        
        if success:
            return jsonify({
                'status': 'success',
                'message': 'User added/updated',
                'fingerprint_id': fingerprint_id,
                'name': name
            }), 200
        else:
            return jsonify({'error': 'Failed to add user'}), 500
        
    except Exception as e:
        logger.error(f"Error adding user: {e}", exc_info=True)
        return jsonify({'error': 'Internal server error'}), 500


@app.route('/stats', methods=['GET'])
def get_stats():
    """Get attendance statistics"""
    try:
        conn = sqlite3.connect(DATABASE_FILE)
        cursor = conn.cursor()
        
        # Total records
        cursor.execute('SELECT COUNT(*) FROM attendance')
        total_records = cursor.fetchone()[0]
        
        # Records today
        cursor.execute('SELECT COUNT(*) FROM attendance WHERE DATE(timestamp) = DATE("now")')
        today_records = cursor.fetchone()[0]
        
        # Registered users
        cursor.execute('SELECT COUNT(*) FROM users')
        total_users = cursor.fetchone()[0]
        
        # Most recent attendance
        cursor.execute('''
            SELECT a.*, u.name 
            FROM attendance a
            LEFT JOIN users u ON a.fingerprint_id = u.fingerprint_id
            ORDER BY a.timestamp DESC LIMIT 5
        ''')
        recent = [dict(zip([col[0] for col in cursor.description], row)) for row in cursor.fetchall()]
        
        conn.close()
        
        return jsonify({
            'status': 'success',
            'stats': {
                'total_records': total_records,
                'today_records': today_records,
                'total_users': total_users,
                'recent_attendance': recent
            }
        }), 200
        
    except Exception as e:
        logger.error(f"Error getting stats: {e}", exc_info=True)
        return jsonify({'error': 'Internal server error'}), 500


@app.route('/health', methods=['GET'])
def health_check():
    """Health check endpoint"""
    return jsonify({
        'status': 'healthy',
        'timestamp': datetime.now().isoformat(),
        'database': 'connected' if Path(DATABASE_FILE).exists() else 'not found'
    }), 200


@app.route('/', methods=['GET'])
def dashboard():
    """Simple web dashboard"""
    html = '''
    <!DOCTYPE html>
    <html>
    <head>
        <title>Attendance System Dashboard</title>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <style>
            * { margin: 0; padding: 0; box-sizing: border-box; }
            body {
                font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
                background: #f5f5f5;
                padding: 20px;
            }
            .container { max-width: 1200px; margin: 0 auto; }
            h1 { color: #333; margin-bottom: 30px; }
            .stats {
                display: grid;
                grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
                gap: 20px;
                margin-bottom: 30px;
            }
            .stat-card {
                background: white;
                padding: 20px;
                border-radius: 8px;
                box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            }
            .stat-value { font-size: 32px; font-weight: bold; color: #007bff; }
            .stat-label { color: #666; margin-top: 5px; }
            .section {
                background: white;
                padding: 20px;
                border-radius: 8px;
                box-shadow: 0 2px 4px rgba(0,0,0,0.1);
                margin-bottom: 20px;
            }
            h2 { color: #333; margin-bottom: 15px; font-size: 20px; }
            table {
                width: 100%;
                border-collapse: collapse;
            }
            th, td {
                padding: 12px;
                text-align: left;
                border-bottom: 1px solid #ddd;
            }
            th {
                background: #f8f9fa;
                font-weight: 600;
                color: #333;
            }
            tr:hover { background: #f8f9fa; }
            .status-success { color: #28a745; font-weight: bold; }
            .refresh-btn {
                background: #007bff;
                color: white;
                border: none;
                padding: 10px 20px;
                border-radius: 5px;
                cursor: pointer;
                font-size: 14px;
            }
            .refresh-btn:hover { background: #0056b3; }
            .timestamp { color: #666; font-size: 0.9em; }
        </style>
    </head>
    <body>
        <div class="container">
            <h1>📊 ESP32 Attendance System Dashboard</h1>
            
            <div class="stats" id="stats">
                <div class="stat-card">
                    <div class="stat-value" id="total-records">-</div>
                    <div class="stat-label">Total Records</div>
                </div>
                <div class="stat-card">
                    <div class="stat-value" id="today-records">-</div>
                    <div class="stat-label">Today's Records</div>
                </div>
                <div class="stat-card">
                    <div class="stat-value" id="total-users">-</div>
                    <div class="stat-label">Registered Users</div>
                </div>
            </div>
            
            <div class="section">
                <h2>Recent Attendance <button class="refresh-btn" onclick="loadData()">🔄 Refresh</button></h2>
                <table>
                    <thead>
                        <tr>
                            <th>ID</th>
                            <th>Fingerprint ID</th>
                            <th>Name</th>
                            <th>Timestamp</th>
                            <th>Method</th>
                            <th>Device IP</th>
                        </tr>
                    </thead>
                    <tbody id="attendance-table">
                        <tr><td colspan="6" style="text-align:center">Loading...</td></tr>
                    </tbody>
                </table>
            </div>
            
            <div class="section">
                <h2>Registered Users</h2>
                <table>
                    <thead>
                        <tr>
                            <th>Fingerprint ID</th>
                            <th>Name</th>
                            <th>Employee ID</th>
                            <th>Department</th>
                        </tr>
                    </thead>
                    <tbody id="users-table">
                        <tr><td colspan="4" style="text-align:center">Loading...</td></tr>
                    </tbody>
                </table>
            </div>
        </div>
        
        <script>
            async function loadData() {
                try {
                    // Load stats
                    const statsRes = await fetch('/stats');
                    const statsData = await statsRes.json();
                    
                    document.getElementById('total-records').textContent = statsData.stats.total_records;
                    document.getElementById('today-records').textContent = statsData.stats.today_records;
                    document.getElementById('total-users').textContent = statsData.stats.total_users;
                    
                    // Load attendance
                    const attendanceRes = await fetch('/attendance?limit=50');
                    const attendanceData = await attendanceRes.json();
                    
                    const attendanceTable = document.getElementById('attendance-table');
                    if (attendanceData.records.length === 0) {
                        attendanceTable.innerHTML = '<tr><td colspan="6" style="text-align:center">No records found</td></tr>';
                    } else {
                        attendanceTable.innerHTML = attendanceData.records.map(r => `
                            <tr>
                                <td>${r.id}</td>
                                <td>${r.fingerprint_id}</td>
                                <td>${r.name || 'Unknown'}</td>
                                <td class="timestamp">${new Date(r.timestamp).toLocaleString()}</td>
                                <td><span class="status-success">${r.login_method}</span></td>
                                <td>${r.device_ip || '-'}</td>
                            </tr>
                        `).join('');
                    }
                    
                    // Load users
                    const usersRes = await fetch('/users');
                    const usersData = await usersRes.json();
                    
                    const usersTable = document.getElementById('users-table');
                    if (usersData.users.length === 0) {
                        usersTable.innerHTML = '<tr><td colspan="4" style="text-align:center">No users registered</td></tr>';
                    } else {
                        usersTable.innerHTML = usersData.users.map(u => `
                            <tr>
                                <td>${u.fingerprint_id}</td>
                                <td>${u.name}</td>
                                <td>${u.employee_id || '-'}</td>
                                <td>${u.department || '-'}</td>
                            </tr>
                        `).join('');
                    }
                } catch (error) {
                    console.error('Error loading data:', error);
                }
            }
            
            // Load data on page load
            loadData();
            
            // Auto-refresh every 30 seconds
            setInterval(loadData, 30000);
        </script>
    </body>
    </html>
    '''
    return render_template_string(html)


# ==================== Main ====================

if __name__ == '__main__':
    print("=" * 60)
    print("ESP32-S3 Attendance System Server")
    print("=" * 60)
    print(f"Database: {DATABASE_FILE}")
    print(f"Server: http://{SERVER_HOST}:{SERVER_PORT}")
    print(f"Log file: {LOG_FILE}")
    print("=" * 60)
    
    # Initialize database
    init_database()
    
    # Start server
    logger.info("Starting Flask server...")
    app.run(host=SERVER_HOST, port=SERVER_PORT, debug=False)
