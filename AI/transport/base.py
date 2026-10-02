# Python 3.10
from abc import ABC, abstractmethod
from typing import Optional, Tuple

class Transport(ABC):
    """메시지 경계가 있는 이진 프레임 전송 추상화(로컬/원격 무관)"""

    @abstractmethod
    def open(self) -> None: ...

    @abstractmethod
    def close(self) -> None: ...

    @abstractmethod
    def recv_state(self, timeout_ms:int=20) -> Optional[Tuple[int,int,int,bytes]]:
        """State 수신: (proto_ver, msg_type, seq, payload_bytes) 또는 None(timeout)"""

    @abstractmethod
    def send_cmd(self, header_bytes:bytes, payload_bytes:bytes) -> None: ...
