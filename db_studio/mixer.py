import numpy as np


def render_bus(indata, mix):
    """Render one virtual stereo AUX using the native engine's constant-power pan law."""
    audio = np.asarray(indata, dtype=np.float32)
    if audio.ndim == 1:
        audio = audio[:, None]
    if audio.ndim != 2:
        raise ValueError("Audio input must be a frames x channels matrix")

    # The product core supports at most 32 mono input channels.
    audio = audio[:, :32]
    if not np.isfinite(audio).all():
        audio = np.nan_to_num(audio, copy=True, nan=0.0, posinf=0.0, neginf=0.0)

    output = np.zeros((audio.shape[0], 2), dtype=np.float32)
    settings = mix.get("channels", {})
    valid_controls = [
        (int(channel) - 1, config)
        for channel, config in settings.items()
        if 1 <= int(channel) <= audio.shape[1]
    ]
    soloed = any(bool(config.get("solo", False)) for _, config in valid_controls)

    for index, config in valid_controls:
        if config.get("mute", False) or (soloed and not config.get("solo", False)):
            continue

        gain = float(np.clip(config.get("volume", 0.75), 0.0, 2.0))
        pan = float(np.clip(config.get("pan", 0.0), -1.0, 1.0))
        angle = (pan + 1.0) * np.pi / 4.0
        output[:, 0] += audio[:, index] * gain * np.cos(angle)
        output[:, 1] += audio[:, index] * gain * np.sin(angle)

    master = float(np.clip(mix.get("master", 0.75), 0.0, 1.5))
    output *= master
    np.clip(output, -0.98, 0.98, out=output)
    return output


def render_all(indata, mixes):
    if len(mixes) != 16:
        raise ValueError("The architecture requires exactly 16 mixes.")
    return [render_bus(indata, mix) for mix in mixes]
