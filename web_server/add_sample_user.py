#!/usr/bin/env python3
"""
Add sample users to the attendance system database
Run this script to quickly populate the database with test users
"""

import requests
import sys

# Server configuration
SERVER_URL = "http://192.168.50.1:8063"

# Sample users (fingerprint IDs 1-20)
SAMPLE_USERS = [
    {"fingerprint_id": 1, "name": "NAME_1", "employee_id": "ID_1", "department": "HW specialist"},
    {"fingerprint_id": 2, "name": "NAME_2", "employee_id": "ID_2", "department": "SW specialist"},
]

def add_users():
    """Add sample users to the database"""
    print("=" * 60)
    print("Adding Sample Users to Attendance System")
    print("=" * 60)
    print(f"Server: {SERVER_URL}")
    print(f"Users to add: {len(SAMPLE_USERS)}")
    print("=" * 60)
    
    success_count = 0
    fail_count = 0
    
    for user in SAMPLE_USERS:
        try:
            response = requests.post(
                f"{SERVER_URL}/users",
                json=user,
                timeout=5
            )
            
            if response.status_code == 200:
                print(f"✓ Added: FP_ID={user['fingerprint_id']:2d} - {user['name']}")
                success_count += 1
            else:
                print(f"✗ Failed: FP_ID={user['fingerprint_id']:2d} - {user['name']} ({response.status_code})")
                fail_count += 1
                
        except requests.exceptions.ConnectionError:
            print(f"✗ ERROR: Cannot connect to server at {SERVER_URL}")
            print("  Make sure the server is running and accessible")
            sys.exit(1)
        except Exception as e:
            print(f"✗ Failed: FP_ID={user['fingerprint_id']:2d} - {user['name']} ({e})")
            fail_count += 1
    
    print("=" * 60)
    print(f"Summary: {success_count} added, {fail_count} failed")
    print("=" * 60)
    
    if success_count > 0:
        print("\n✓ Users added successfully!")
        print(f"  View dashboard: {SERVER_URL}/")
        print(f"  View users API: {SERVER_URL}/users")


if __name__ == "__main__":
    # Check if custom server URL provided
    if len(sys.argv) > 1:
        SERVER_URL = sys.argv[1]
        print(f"Using custom server URL: {SERVER_URL}")
    
    add_users()
