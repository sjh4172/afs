# Python 3.10
import socket, time
from typing import Optional, Tuple
from .base import Transport

class UdpTransport(Transport):
    def __init__(self, state_bind_host:str, state_port:int, cmd_host:str, cmd_port:int):
        self.state_addr = (state_bind_host, state_port)
        self.cmd_addr   = (cmd_host, cmd_port)
        self.state_sock: Optional[socket.socket] = None
        self.cmd_sock:   Optional[socket.socket] = None

    def open(self) -> None:
        self.state_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.state_sock.bind(self.state_addr)
        self.state_sock.setblocking(False)
        self.cmd_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def close(self) -> None:
        try:
            if self.state_sock: self.state_sock.close()
            if self.cmd_sock:   self.cmd_sock.close()
        finally:
            self.state_sock = None
            self.cmd_sock = None

    def recv_state(self, timeout_ms:int=20) -> Optional[Tuple[int,int,int,bytes]]:
        assert self.state_sock is not None
        end = time.perf_counter() + (timeout_ms/1000.0)
        last = None
        while time.perf_counter() < end:
            try:
                data, _ = self.state_sock.recvfrom(4096)
                last = data  # 최신 우선(몰릴 경우 최신 것만 채택)
                # 루프 계속 돌아 더 최신이 있으면 대체
            except BlockingIOError:
                if last: break
                time.sleep(0.0005)
        if not last: return None
        return (0,0,0,last)  # 헤더 파싱은 상위에서 수행

    def send_cmd(self, header_bytes:bytes, payload_bytes:bytes) -> None:
        assert self.cmd_sock is not None
        self.cmd_sock.sendto(header_bytes + payload_bytes, self.cmd_addr)
