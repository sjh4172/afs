# udp_sender.py  (Python 3.10)
import argparse, json, math, socket, time

def mono():
    return time.perf_counter()

def normalize(v):
    x, y, z = v
    n = (x*x + y*y + z*z) ** 0.5
    if n == 0.0: return (0.0, 0.0, 0.0)
    return (x/n, y/n, z/n)

def cross(a, b):
    ax, ay, az = a; bx, by, bz = b
    return (ay*bz - az*by, az*bx - ax*bz, ax*by - ay*bx)

def quat_from_axes(fwd, up):
    """
    fwd: 정규화된 진행방향(Forward, +Z)
    up : 월드업(Y+) 근사. fwd와 평행이면 자동 보정
    Unity 좌표(Y up, Z forward) 기준의 회전행렬 → 쿼터니언(x,y,z,w)
    """
    # 직교기저 구성 (Right, Up2, Fwd)
    # Right = Up x Fwd
    right = cross(up, fwd)
    rn = (right[0]**2 + right[1]**2 + right[2]**2) ** 0.5
    if rn < 1e-6:
        # fwd가 up과 거의 평행하면 임의의 보조업 사용
        up = (0.0, 0.0, 1.0)  # Z를 임시 업으로
        right = cross(up, fwd)
        rn = (right[0]**2 + right[1]**2 + right[2]**2) ** 0.5
        if rn < 1e-6:
            # 여전히 불안정하면 단위 회전 반환
            return (0.0, 0.0, 0.0, 1.0)
    right = (right[0]/rn, right[1]/rn, right[2]/rn)
    up2 = cross(fwd, right)  # 재직교

    # Unity의 행렬(열벡터 기준): [Right, Up, Forward]
    m00, m01, m02 = right
    m10, m11, m12 = up2
    m20, m21, m22 = fwd

    # 행렬→쿼터니언 (x,y,z,w)
    trace = m00 + m11 + m22
    if trace > 0.0:
        s = math.sqrt(trace + 1.0) * 2.0
        qw = 0.25 * s
        qx = (m21 - m12) / s
        qy = (m02 - m20) / s
        qz = (m10 - m01) / s
    elif (m00 > m11) and (m00 > m22):
        s = math.sqrt(1.0 + m00 - m11 - m22) * 2.0
        qw = (m21 - m12) / s
        qx = 0.25 * s
        qy = (m01 + m10) / s
        qz = (m02 + m20) / s
    elif m11 > m22:
        s = math.sqrt(1.0 + m11 - m00 - m22) * 2.0
        qw = (m02 - m20) / s
        qx = (m01 + m10) / s
        qy = 0.25 * s
        qz = (m12 + m21) / s
    else:
        s = math.sqrt(1.0 + m22 - m00 - m11) * 2.0
        qw = (m10 - m01) / s
        qx = (m02 + m20) / s
        qy = (m12 + m21) / s
        qz = 0.25 * s
    return (qx, qy, qz, qw)

def main(ip: str, port: int, hz: float, obj_id: str):
    addr = (ip, port)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    period = 1.0 / hz
    next_tick = mono() + period
    t0 = mono()

    sent_count = 0
    last_log = time.time()

    # 초기 위치(원 궤적; XZ에서 반지름 2m, Y=0.5)
    def pos_at(t):
        theta = t * 2.0 * math.pi * 0.25  # 0.25 Hz 회전
        return (20.0*math.cos(theta), 5, 20.0*math.sin(theta))

    prev_t = mono()
    prev_p = pos_at(prev_t - t0)
    last_q = (0.0, 0.0, 0.0, 1.0)

    print(f"[INFO] UDP send → {ip}:{port}, {hz:.2f} Hz, id='{obj_id}' (heading enabled)")

    while True:
        # 타이밍 정렬
        while True:
            remain = next_tick - mono()
            if remain <= 0: break
            if remain > 0.0005:
                time.sleep(remain * 0.8)

        t_send = mono()
        p = pos_at(t_send - t0)

        # 속도 벡터로 진행방향 계산
        dt = max(t_send - prev_t, 1e-6)
        vel = ((p[0]-prev_p[0])/dt, (p[1]-prev_p[1])/dt, (p[2]-prev_p[2])/dt)
        fwd = normalize(vel)

        if fwd == (0.0, 0.0, 0.0):
            qx, qy, qz, qw = last_q  # 정지 시 이전 헤딩 유지
        else:
            # 월드 업(Y+) 기준으로 앞을 진행방향에 맞춤
            qx, qy, qz, qw = quat_from_axes(fwd, (0.0, 1.0, 0.0))
            last_q = (qx, qy, qz, qw)

        msg = {
            "id": obj_id,
            "t": time.time(),            # epoch seconds
            "x": p[0], "y": p[1], "z": p[2],
            "qx": qx, "qy": qy, "qz": qz, "qw": qw
        }
        sock.sendto(json.dumps(msg).encode("utf-8"), addr)

        sent_count += 1
        now_epoch = time.time()
        if now_epoch - last_log >= 1.0:
            print(f"[UDP] Sent {sent_count} packets in last second")
            sent_count = 0
            last_log = now_epoch

        prev_t, prev_p = t_send, p
        next_tick += period
        lag = mono() - next_tick
        if lag > period:
            skipped = int(lag / period) + 1
            next_tick += skipped * period

if __name__ == "__main__":
    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("--ip",   default="127.0.0.1")
    p.add_argument("--port", type=int, default=5008)
    p.add_argument("--hz",   type=float, default=32.0)
    p.add_argument("--id",   default="drone1")
    a = p.parse_args()
    main(a.ip, a.port, a.hz, a.id)
