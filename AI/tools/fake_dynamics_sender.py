# Python 3.10 — UDP로 상태 프레임 발사(50Hz)
import socket, time
from iface.packets import *

STATE_ADDR = ("127.0.0.1", 50000)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
seq = 0

bearing, elev, rng, closure, aot, aspect = 0.5, 0.1, 3000.0, -50.0, 0.5, 0.0

while True:
    seq += 1
    payload = [
        0.0,0.0,1000.0,    # px,py,pz
        0.0,0.0,0.0,1.0,   # qx,qy,qz,qw
        200.0,0.0,0.0,     # vx,vy,vz
        0.0,0.0,0.0,       # p,q,r
        230.0, 0.02, 0.01, # V_tas, alpha, beta
        1.0,               # nz
        0.5,               # reserved
        bearing, elev, rng, closure, aot, aspect  # 6개
    ]
    pkt = pack_header(MSG_STATE, seq, int(time.time()*1e6)) + pack_state(payload)
    sock.sendto(pkt, STATE_ADDR)

    # 약간씩 목표에 접근하는 형태로 값 변화
    bearing *= 0.99; elev *= 0.995
    rng = max(100.0, rng + closure*0.05)
    time.sleep(0.02)  # 50 Hz
    if seq % 100 == 0:
        print(f"Sent packet {seq} with bearing={bearing}, elev={elev}, rng={rng}, closure={closure}, aot={aot}, aspect={aspect}")