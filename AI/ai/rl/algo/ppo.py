# Python 3.10
import math, numpy as np, torch
import torch.nn as nn
from dataclasses import dataclass

@dataclass
class PPOConfig:
    obs_dim: int = 24
    act_dim: int = 4
    gamma: float = 0.995
    lam: float = 0.95
    lr: float = 3e-4
    clip: float = 0.2
    ent_coef: float = 0.01
    vf_coef: float = 0.5
    train_epochs: int = 5
    batch_size: int = 256
    rollout_steps: int = 4096
    max_grad_norm: float = 0.5
    device: str = "cpu"

class MLPPolicy(nn.Module):
    def __init__(self, obs_dim, act_dim):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(obs_dim, 256), nn.Tanh(),
            nn.Linear(256, 256), nn.Tanh()
        )
        self.mu = nn.Linear(256, act_dim)
        self.log_std = nn.Parameter(torch.zeros(act_dim))
        self.v  = nn.Linear(256, 1)

    def forward(self, x):
        h = self.net(x)
        mu = self.mu(h)
        v  = self.v(h)
        std = torch.exp(self.log_std)
        return mu, std, v

def ppo_update(model, optim, cfg: PPOConfig, buf):
    obs = torch.tensor(np.array(buf["obs"]), dtype=torch.float32, device=cfg.device)
    act = torch.tensor(np.array(buf["act"]), dtype=torch.float32, device=cfg.device)
    ret = torch.tensor(np.array(buf["ret"]), dtype=torch.float32, device=cfg.device)
    adv = torch.tensor(np.array(buf["adv"]), dtype=torch.float32, device=cfg.device)
    logp_old = torch.tensor(np.array(buf["logp"]), dtype=torch.float32, device=cfg.device)

    n = obs.shape[0]
    idx = np.arange(n)
    for _ in range(cfg.train_epochs):
        np.random.shuffle(idx)
        for s in range(0, n, cfg.batch_size):
            j = idx[s:s+cfg.batch_size]
            mu, std, v = model(obs[j])
            dist = torch.distributions.Normal(mu, std)
            logp = dist.log_prob(act[j]).sum(-1)
            ratio = torch.exp(logp - logp_old[j])
            surr1 = ratio * adv[j]
            surr2 = torch.clamp(ratio, 1-cfg.clip, 1+cfg.clip) * adv[j]
            pi_loss = -torch.min(surr1, surr2).mean() - cfg.ent_coef * dist.entropy().sum(-1).mean()
            vf_loss = ((ret[j] - v.squeeze(-1))**2).mean() * cfg.vf_coef
            loss = pi_loss + vf_loss
            optim.zero_grad()
            loss.backward()
            nn.utils.clip_grad_norm_(model.parameters(), cfg.max_grad_norm)
            optim.step()

def compute_gae(rewards, values, dones, cfg: PPOConfig):
    adv, gae = [], 0.0
    next_value = 0.0
    for t in reversed(range(len(rewards))):
        delta = rewards[t] + cfg.gamma * (0 if dones[t] else next_value) - values[t]
        gae = delta + cfg.gamma * cfg.lam * (0 if dones[t] else 1) * gae
        adv.append(gae)
        next_value = values[t]
    adv = adv[::-1]
    ret = [a + v for a, v in zip(adv, values)]
    adv = np.array(adv, dtype=np.float32)
    ret = np.array(ret, dtype=np.float32)
    adv = (adv - adv.mean()) / (adv.std() + 1e-8)
    return adv, ret
