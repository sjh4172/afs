# Python 3.10
import time, numpy as np, torch
from torch.optim import Adam
from .algo.ppo import MLPPolicy, PPOConfig, compute_gae, ppo_update
from .envs.dummy_env import DogfightDummyEnv

def train_dummy(total_iters=3):
    cfg = PPOConfig()
    env = DogfightDummyEnv(obs_dim=cfg.obs_dim)
    model = MLPPolicy(cfg.obs_dim, cfg.act_dim).to(cfg.device)
    optim = Adam(model.parameters(), lr=cfg.lr)

    for it in range(total_iters):
        obs_list, act_list, rew_list, done_list, logp_list, val_list = [], [], [], [], [], []
        obs, _ = env.reset()
        ep_len = 0

        for step in range(cfg.rollout_steps):
            x = torch.tensor(obs, dtype=torch.float32).unsqueeze(0)
            with torch.inference_mode():
                mu, std, v = model(x)
                dist = torch.distributions.Normal(mu, std)
                a = dist.sample()
                logp = dist.log_prob(a).sum(-1)
            act = a.squeeze(0).numpy()
            # throttle 범위 [0,1] 클램프
            act[3] = np.clip(act[3], 0, 1)

            next_obs, r, term, trunc, info = env.step(act.astype(np.float32))
            obs_list.append(obs); act_list.append(act); rew_list.append(r); done_list.append(term or trunc)
            logp_list.append(float(logp.item())); val_list.append(float(v.item()))
            obs = next_obs
            ep_len += 1
            if term or trunc:
                obs, _ = env.reset()
                ep_len = 0

        adv, ret = compute_gae(rew_list, val_list, done_list, cfg)
        buf = {"obs": obs_list, "act": act_list, "ret": ret, "adv": adv, "logp": logp_list}
        ppo_update(model, optim, cfg, buf)
        print(f"[iter {it+1}] rollout={cfg.rollout_steps}, adv_mean={adv.mean():.3f}")

    # 저장(추후 추론 서버에 장착 용이)
    torch.jit.script(model).save("models/pilot-dummy.pt")
    print("saved: models/pilot-dummy.pt")

if __name__ == "__main__":
    train_dummy()
