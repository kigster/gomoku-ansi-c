
import json
import time
import subprocess
import sys
import os
import http.client
import urllib.parse
from urllib.parse import urlparse

PORT = 8999
SERVER_URL = f"http://localhost:{PORT}/gomoku/play"

def start_server():
    print(f"Starting server on port {PORT}...")
    server_process = subprocess.Popen(
        ["./gomoku-httpd", "-b", str(PORT)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    time.sleep(1)  # Wait for server to start
    # Check if process is still running
    if server_process.poll() is not None:
        stdout, stderr = server_process.communicate()
        print(f"Server failed to start. Stdout: {stdout}\nStderr: {stderr}")
        sys.exit(1)
    return server_process

def stop_server(process):
    print("Stopping server...")
    process.terminate()
    process.wait()

def send_request(url, data, timeout=None):
    parsed = urlparse(url)
    conn = http.client.HTTPConnection(parsed.hostname, parsed.port, timeout=timeout)
    headers = {'Content-type': 'application/json'}
    json_data = json.dumps(data)
    
    try:
        conn.request("POST", parsed.path, json_data, headers)
        response = conn.getresponse()
        resp_data = response.read().decode()
        return response.status, resp_data
    except Exception as e:
        raise e
    finally:
        conn.close()

def verify_timeout():
    # Construct a valid board state with no winner
    moves = []
    # Place stones in a way that doesn't connect 5
    # X at (i, i), O at (i, i+1)
    for i in range(5, 15):
        moves.append({"X": [i, i]})
        moves.append({"O": [i, i+1]})
    
    # That gives 10 pairs = 20 moves. Next is X.
    # Wait, we want O to move?
    # CLI default is Human vs AI (X vs O).
    # If we want O (AI) to move, we send moves ending with X.
    # The loop above adds X then O. So last move is O. Next is X.
    # If we want next to be O (AI), we need last move to be X.
    # Let's add one more X.
    moves.append({"X": [15, 15]})
    
    # Now it is O's turn.
                 
    payload_base = {
        "board_size": 19,
        "X": {"player": "human", "time_ms": 0},
        "O": {"player": "AI", "depth": 10}, # Max depth
        "moves": moves
    }

    # Test 1: Explicit small timeout
    print("\n--- Test 1: Explicit Timeout (1s) ---")
    payload_timeout = payload_base.copy()
    payload_timeout["timeout"] = 1 # 1 second
    
    start_time = time.time()
    try:
        status, resp_data = send_request(SERVER_URL, payload_timeout)
        duration = time.time() - start_time
        print(f"Request duration: {duration:.2f}s")
        
        if status == 200:
            if duration < 1.5: 
                print("PASS: Returned quickly.")
            else:
                print("FAIL: Took too long.")
        else:
            print(f"FAIL: Server returned {status}")
            print(resp_data)

    except Exception as e:
        print(f"FAIL: Request failed: {e}")

    # Test 2: No Timeout
    print("\n--- Test 2: No Timeout (Default) ---")
    payload_no_timeout = payload_base.copy()
    if "timeout" in payload_no_timeout:
        del payload_no_timeout["timeout"]
    
    start_time = time.time()
    try:
        print("Sending request with NO timeout...")
        # We assume with depth 10 (actually 12) it will take time.
        status, resp_data = send_request(SERVER_URL, payload_no_timeout, timeout=10)
        duration = time.time() - start_time
        print(f"Request duration: {duration:.2f}s")
        
        if status == 200:
            if duration > 1.1:
                print("PASS: Calculation took > 1s")
            else:
                 print(f"INCONCLUSIVE: Too fast ({duration}s).")
                 # print(f"Response: {resp_data}")
        else:
             print(f"FAIL: Server returned {status}")
             print(resp_data)

    except Exception as e:
        print(f"FAIL: Request failed: {e}")

if __name__ == "__main__":
    if not os.path.exists("./gomoku-httpd"):
        print("Error: gomoku-httpd binary not found.")
        sys.exit(1)
        
    server_process = start_server()
    try:
        verify_timeout()
    finally:
        stop_server(server_process)
