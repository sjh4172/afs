# Python 3.10
from typing import Tuple

class DummyPolicy:
    """아주 단순한 규칙 기반 더미 정책 (S1 점검용)"""
    def __call__(self, obs: tuple[float,...]) -> Tuple[float,float,float,float,int]:
        # 예시: obs[-6:]이 [bearing, elevation, range, closure, AOT, aspect]
        bearing, elevation, rng, closure, aot, aspect = obs[-6:]
        sp = max(-0.2, min(0.2, -0.5*elevation))
        sr = max(-0.3, min(0.3, -0.8*bearing))
        rd = 0.0
        th = 0.7 if abs(bearing)<0.2 and abs(elevation)<0.2 else 0.6
        return sp, sr, rd, th, 0  # cmd_mode=0
