"""Genera los recursos binarios del mod: logo.png y resources/jump_cue.wav.

Uso:
    python tools/make_assets.py

Se puede borrar este archivo si no quieres regenerar los recursos.
"""

import math
import os
import struct
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def make_logo(path: str, size: int = 256) -> None:
    from PIL import Image, ImageDraw, ImageFilter

    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    margin = 12
    radius = 54
    draw.rounded_rectangle(
        [margin, margin, size - margin, size - margin],
        radius=radius,
        fill=(14, 22, 32, 255),
        outline=(38, 58, 74, 255),
        width=4,
    )

    # Anillo: pista apagada + ventana de salto encendida.
    ring = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    ring_draw = ImageDraw.Draw(ring)
    cx = cy = size / 2
    r = size * 0.31
    box = [cx - r, cy - r, cx + r, cy + r]
    ring_draw.ellipse(box, outline=(20, 44, 34, 255), width=11)
    ring_draw.arc(box, start=-108, end=126, fill=(52, 226, 118, 255), width=11)
    # Tapas redondeadas del arco.
    for angle in (-108, 126):
        rad = math.radians(angle)
        px = cx + r * math.cos(rad)
        py = cy + r * math.sin(rad)
        ring_draw.ellipse([px - 5.5, py - 5.5, px + 5.5, py + 5.5], fill=(52, 226, 118, 255))
    ring = ring.filter(ImageFilter.GaussianBlur(0.8))
    image.alpha_composite(ring)

    # Flecha de salto.
    arrow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    ad = ImageDraw.Draw(arrow)
    top = (cx, cy - 46)
    left = (cx - 34, cy + 6)
    right = (cx + 34, cy + 6)
    stem = [(cx - 15, cy + 4), (cx + 15, cy + 4), (cx + 15, cy + 44), (cx - 15, cy + 44)]
    ad.polygon([top, left, right], fill=(255, 255, 255, 255))
    ad.rectangle([cx - 13, cy + 4, cx + 13, cy + 46], fill=(255, 255, 255, 255))

    glow = arrow.filter(ImageFilter.GaussianBlur(10))
    glow.putalpha(glow.getchannel("A").point(lambda a: int(a * 0.55)))
    image.alpha_composite(glow)
    image.alpha_composite(arrow)

    image.save(path)


def make_cue_sound(path: str) -> None:
    sample_rate = 44100
    duration = 0.11
    total = int(sample_rate * duration)
    frames = bytearray()

    for i in range(total):
        t = i / sample_rate
        progress = i / total
        # Ataque muy rapido y caida exponencial: suena a "clic" corto.
        attack = min(1.0, progress / 0.04)
        decay = math.exp(-6.5 * progress)
        envelope = attack * decay

        # Dos armonicos para que se oiga bien sobre la musica.
        value = (
            0.70 * math.sin(2 * math.pi * 1046.5 * t)
            + 0.30 * math.sin(2 * math.pi * 2093.0 * t)
        )
        sample = int(max(-1.0, min(1.0, value * envelope * 0.85)) * 32767)
        frames += struct.pack("<h", sample)

    with wave.open(path, "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(sample_rate)
        handle.writeframes(bytes(frames))


def main() -> None:
    make_logo(os.path.join(ROOT, "logo.png"))
    resources = os.path.join(ROOT, "resources")
    os.makedirs(resources, exist_ok=True)
    make_cue_sound(os.path.join(resources, "jump_cue.wav"))
    print("logo.png y resources/jump_cue.wav generados.")


if __name__ == "__main__":
    main()
