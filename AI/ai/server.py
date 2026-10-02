# Python 3.10
import time, json
from pathlib import Path
import yaml
from transport.udp import UdpTransport
from transport.base import Transport
from iface.packets import *
from .policy import DummyPolicy

LOG_DIR = Path("server_logs"); LOG_DIR.mkdir(exist_ok=True)

def now_usec() -> int: return int(time.time()*1e6)

def build_transport(cfg:dict) -> Transport:
    tcfg = cfg["transport"]
    ttype = tcfg["type"].lower()
    if ttype == "udp":
        return UdpTransport(
            state_bind_host=tcfg.get("state_bind_host","0.0.0.0"),
            state_port=tcfg.get("state_port",50000),
            cmd_host=tcfg.get("cmd_host","127.0.0.1"),
            cmd_port=tcfg.get("cmd_port",50001),
        )
    # elif ttype == "tcp": from transport.tcp import TcpTransport; return TcpTransport(...)
    # elif ttype == "pipe": from transport.pipes import PipeTransport; return PipeTransport(...)
    else:
        raise ValueError(f"unknown transport: {ttype}")

def main():
    # 1) 설정 로드
    cfg = yaml.safe_load(open(Path(__file__).with_name("config.yaml"), "r", encoding="utf-8"))
    transport = build_transport(cfg)
    policy = DummyPolicy()

    # 2) 로깅
    logf = open(LOG_DIR / f"ai_{int(time.time())}.jsonl", "a", encoding="utf-8")
    seq_out = 0
    last_seq_in = -1

    # 3) 시작
    transport.open()
    print("[AI] transport opened:", cfg["transport"])

    try:
        while True:
            rec = transport.recv_state(timeout_ms=cfg.get("recv_timeout_ms",20))
            if rec is None: 
                continue
            _, _, _, raw = rec
            # 헤더/페이로드 분리
            if len(raw) < HDR_SIZE + STATE_SIZE: 
                continue
            proto, mtype, seq_in, t_usec = unpack_header(raw)
            if proto != PROTO_VER or mtype != MSG_STATE:
                continue
            if seq_in <= last_seq_in:
                continue
            last_seq_in = seq_in
            obs = unpack_state(raw[HDR_SIZE:HDR_SIZE+STATE_SIZE])

            # 정책 실행
            t0 = time.perf_counter()
            sp, sr, rd, th, mode = policy(obs)
            latency_ms = (time.perf_counter() - t0)*1000.0

            # 송신
            seq_out += 1
            hdr = pack_header(MSG_CMD, seq_out, now_usec())
            cmd = pack_cmd(sp, sr, rd, th, mode)
            transport.send_cmd(hdr, cmd)

            # 로그
            logf.write(json.dumps({
                "seq_in": int(seq_in), "t_usec_in": int(t_usec),
                "cmd": [sp, sr, rd, th, mode],
                "latency_ms": round(latency_ms,3)
            })+"\n"); logf.flush()
    finally:
        transport.close()
        logf.close()

# This is the main entry point for the AI server.
if __name__ == "__main__":
    main()