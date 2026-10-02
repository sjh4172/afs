# Python 3.10 / struct로 고정 길이 패킹
import struct
from dataclasses import dataclass
from typing import Tuple

# Little-Endian 지정: '<'
HDR_FMT = "<HHIQ"     # proto_ver (H), msg_type(H), seq(I), t_usec(Q)
# State (예시 24개의 float: 24 * 4 = 96바이트)  필드 수는 계획에 맞게 조정
STATE_FMT = "<" + "f"*24
CMD_FMT   = "<" + "f"*4 + "B"   # 4 floats + 1 byte

HDR_SIZE = struct.calcsize(HDR_FMT)
STATE_SIZE = struct.calcsize(STATE_FMT)
CMD_SIZE   = struct.calcsize(CMD_FMT)

PROTO_VER = 1
MSG_STATE = 1
MSG_CMD   = 2

def pack_header(msg_type:int, seq:int, t_usec:int) -> bytes:
    return struct.pack(HDR_FMT, PROTO_VER, msg_type, seq, t_usec)

def unpack_header(buf:bytes) -> Tuple[int,int,int,int]:
    # returns (proto_ver, msg_type, seq, t_usec)
    return struct.unpack(HDR_FMT, buf[:HDR_SIZE])

def pack_state(payload:list[float]) -> bytes:
    assert len(payload) == 24, "STATE fields must be 24 floats"
    return struct.pack(STATE_FMT, *payload)

def unpack_state(buf:bytes) -> Tuple[float,...]:
    return struct.unpack(STATE_FMT, buf[:STATE_SIZE])

def pack_cmd(stick_pitch, stick_roll, rudder, throttle, cmd_mode:int) -> bytes:
    return struct.pack(CMD_FMT, float(stick_pitch), float(stick_roll),
                       float(rudder), float(throttle), int(cmd_mode))
