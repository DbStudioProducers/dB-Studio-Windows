import numpy as np

def render_bus(indata, mix):
    x = np.asarray(indata, dtype=np.float32)
    if x.ndim == 1:
        x = x[:, None]
    out = np.zeros((x.shape[0], 2), dtype=np.float32)
    settings = mix.get("channels", {})
    soloed = any(bool(v.get("solo", False)) for v in settings.values())
    for key, cfg in settings.items():
        idx = int(key) - 1
        if idx < 0 or idx >= min(x.shape[1], 32):
            continue
        if cfg.get("mute", False) or (soloed and not cfg.get("solo", False)):
            continue
        gain = float(np.clip(cfg.get("volume", 0.75), 0.0, 2.0))
        pan = float(np.clip(cfg.get("pan", 0.0), -1.0, 1.0))
        angle = (pan + 1.0) * np.pi / 4.0
        out[:, 0] += x[:, idx] * gain * np.cos(angle) * np.sqrt(2.0)
        out[:, 1] += x[:, idx] * gain * np.sin(angle) * np.sqrt(2.0)
    out *= float(np.clip(mix.get("master", 0.75), 0.0, 1.5))
    np.clip(out, -0.98, 0.98, out=out)
    return out

def render_all(indata, mixes):
    if len(mixes) != 16:
        raise ValueError("A arquitetura exige exatamente 16 mixes.")
    return [render_bus(indata, m) for m in mixes]
