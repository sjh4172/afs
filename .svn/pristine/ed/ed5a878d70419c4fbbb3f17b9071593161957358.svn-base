# Python 3.10
import gymnasium as gym
import numpy as np

class DogfightDummyEnv(gym.Env):
    metadata = {"render_modes": []}

    def __init__(self, obs_dim: int = 24, seed: int | None = None):
        super().__init__()
        self.obs_dim = obs_dim
        self.rng = np.random.default_rng(seed)
        # 관측: [-1, 1] 범위 벡터
        self.observation_space = gym.spaces.Box(low=-1.0, high=1.0, shape=(obs_dim,), dtype=np.float32)
        # 행동: pitch/roll/rudder ∈ [-1,1], throttle ∈ [0,1]
        self.action_space = gym.spaces.Box(
            low=np.array([-1, -1, -1, 0], dtype=np.float32),
            high=np.array([ 1,  1,  1, 1], dtype=np.float32),
            dtype=np.float32
        )
        self.max_steps = 300
        self.step_count = 0

    def seed(self, seed=None):
        if seed is not None:
            self.rng = np.random.default_rng(seed)

    def reset(self, *, seed: int | None = None, options=None):
        if seed is not None:
            self.seed(seed)
        self.step_count = 0
        # bearing/elevation을 마지막 2칸에 배치(초기엔 랜덤)
        obs = self.rng.uniform(-0.5, 0.5, size=(self.obs_dim,), dtype=np.float32)
        self.state = obs
        return obs, {}

    def step(self, action: np.ndarray):
        self.step_count += 1

        # 단순 동역학 흉내: bearing/elevation(마지막 2칸)이 action에 의해 0으로 수렴하는 형태
        bearing_idx = self.obs_dim - 2
        elevation_idx = self.obs_dim - 1

        sp, sr, rd, th = action.astype(np.float32)
        # 가짜 업데이트: 롤(sr), 피치(sp)가 bearing/elevation 감소에 도움을 준다고 가정
        self.state[bearing_idx]   = np.clip(self.state[bearing_idx]   - 0.1*sr, -1, 1)
        self.state[elevation_idx] = np.clip(self.state[elevation_idx] - 0.1*sp, -1, 1)

        # 보상: bearing/elevation을 0으로 붙일수록 보상 +, 큰 제어에는 소정 페널티
        aim = - (abs(self.state[bearing_idx]) + abs(self.state[elevation_idx]))
        ctrl_penalty = - 0.01 * float(np.square(action).sum())
        reward = float(aim + ctrl_penalty)

        # 종료 조건: 충분히 0에 가까워지면 종료
        terminated = (abs(self.state[bearing_idx]) < 0.02 and abs(self.state[elevation_idx]) < 0.02)
        truncated = (self.step_count >= self.max_steps)

        info = {"aim": aim, "ctrl_penalty": ctrl_penalty}
        return self.state.astype(np.float32), reward, terminated, truncated, info

# 간단한 수동 테스트
if __name__ == "__main__":
    env = DogfightDummyEnv()
    obs, _ = env.reset()
    for _ in range(5):
        action = env.action_space.sample()
        obs, r, term, trunc, info = env.step(action)
        print(r, term, trunc)
