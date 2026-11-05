"""Image processing for tools/upscale: padding, alpha bleed, model inference, colour correction.

Images are float32 numpy arrays, H x W x C, values 0..1.
"""
import numpy as np
from PIL import Image

# ------------------------------------------------------------------------------------------------- basics --


def load(path):
    im = Image.open(path)
    im.load()
    if im.mode == "P":
        im = im.convert("RGBA" if "transparency" in im.info else "RGB")
    if im.mode not in ("RGB", "RGBA"):
        im = im.convert("RGBA" if "A" in im.getbands() else "RGB")
    return np.asarray(im, dtype=np.float32) / 255.0


def save(path, a, info=None):
    a = np.clip(np.rint(a * 255.0), 0, 255).astype(np.uint8)
    im = Image.fromarray(a, "RGBA" if a.shape[2] == 4 else "RGB")
    from PIL import PngImagePlugin
    meta = PngImagePlugin.PngInfo()
    for k, v in (info or {}).items():
        meta.add_text(k, v)
    im.save(path, pnginfo=meta, optimize=False, compress_level=6)


def resize(a, w, h, method=Image.LANCZOS):
    """Per channel in float mode, so values are not quantised between steps."""
    chans = [np.asarray(Image.fromarray(a[:, :, c], "F").resize((w, h), method)) for c in range(a.shape[2])]
    return np.stack(chans, axis=2)


# ---------------------------------------------------------------------------------------------- padding --


def pad(a, p, wrap):
    """Pads by p pixels: toroidally for tileable images (terrain), by edge replication otherwise."""
    return np.pad(a, ((p, p), (p, p), (0, 0)), mode="wrap" if wrap else "edge")


def crop(a, p, scale):
    q = p * scale
    return a[q: a.shape[0] - q, q: a.shape[1] - q]


# ------------------------------------------------------------------------------------------------- alpha --


def bleed(rgb, alpha, threshold=0.5, iterations=64):
    """Fills the colour of transparent pixels from their opaque neighbours (repeated 3x3 averaging of the known
    pixels), so the upscaler and later mipmaps/bilinear filtering never see the arbitrary colour hidden under
    alpha 0: no dark or coloured halos at cut-out edges."""
    known = alpha >= threshold
    if known.all() or not known.any():
        return rgb
    out = rgb.copy()
    out[~known] = 0
    k = known.astype(np.float32)
    for _ in range(iterations):
        if k.all():
            break
        s = np.zeros_like(out)
        n = np.zeros_like(k)
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                s += np.roll(np.roll(out * k[:, :, None], dy, 0), dx, 1)
                n += np.roll(np.roll(k, dy, 0), dx, 1)
        grow = (k == 0) & (n > 0)
        out[grow] = s[grow] / n[grow][:, None]
        k = np.where(grow, 1.0, k)
    rest = k == 0
    if rest.any():
        out[rest] = rgb[known].mean(axis=0)
    return out


# ---------------------------------------------------------------------------------------------- models --

_cache = {}


def device():
    import torch
    if torch.backends.mps.is_available():
        return torch.device("mps")
    if torch.cuda.is_available():
        return torch.device("cuda")
    return torch.device("cpu")


def load_model(path):
    if path not in _cache:
        import spandrel
        if not _cache.get("_extra"):
            try:
                import spandrel_extra_arches
                spandrel_extra_arches.install()   # registers once per process; a second call raises
            except ImportError:
                pass
            _cache["_extra"] = True
        m = spandrel.ModelLoader().load_from_file(path)
        if not isinstance(m, spandrel.ImageModelDescriptor):
            raise ValueError(f"{path}: not an image-to-image model")
        m.to(device()).eval()
        _cache[path] = m
    return _cache[path]


def run_model(model, rgb, tile=256, overlap=24):
    """Runs the model over an RGB image in overlapping tiles with feathered blending (bounded memory; DAT/HAT on a
    1024 x 768 background would not fit whole). Returns the image at model.scale."""
    import torch
    s = model.scale
    h, w, _ = rgb.shape
    out = np.zeros((h * s, w * s, 3), np.float32)
    wsum = np.zeros((h * s, w * s, 1), np.float32)
    step = tile - 2 * overlap
    ys = list(range(0, max(h - tile, 0) + 1, step)) or [0]
    xs = list(range(0, max(w - tile, 0) + 1, step)) or [0]
    if ys[-1] + tile < h:
        ys.append(h - tile)
    if xs[-1] + tile < w:
        xs.append(w - tile)
    dev = device()
    for y in ys:
        for x in xs:
            t = rgb[y: y + tile, x: x + tile]
            th, tw = t.shape[:2]
            # the architectures want sizes that are multiples of their window: reflect-pad to 16
            ph, pw = (-th) % 16, (-tw) % 16
            tp = np.pad(t, ((0, ph), (0, pw), (0, 0)), mode="reflect" if min(th, tw) > 1 else "edge")
            with torch.no_grad():
                inp = torch.from_numpy(np.ascontiguousarray(tp.transpose(2, 0, 1)))[None].to(dev)
                res = model(inp).clamp(0, 1)[0].float().cpu().numpy().transpose(1, 2, 0)
            res = res[: th * s, : tw * s]
            # feather weights: ramps over the overlap except at the image border
            wy = np.ones(th * s, np.float32)
            wx = np.ones(tw * s, np.float32)
            r = overlap * s
            if r:
                ramp = (np.arange(r, dtype=np.float32) + 0.5) / r
                if y > 0:
                    wy[:r] = np.minimum(wy[:r], ramp)
                if y + th < h:
                    wy[-r:] = np.minimum(wy[-r:], ramp[::-1])
                if x > 0:
                    wx[:r] = np.minimum(wx[:r], ramp)
                if x + tw < w:
                    wx[-r:] = np.minimum(wx[-r:], ramp[::-1])
            wgt = (wy[:, None] * wx[None, :])[:, :, None]
            out[y * s: (y + th) * s, x * s: (x + tw) * s] += res * wgt
            wsum[y * s: (y + th) * s, x * s: (x + tw) * s] += wgt
    return out / np.maximum(wsum, 1e-6)


def upscale_rgb(rgb, method, scale):
    """method: a model path, or 'lanczos' / 'bicubic' / 'nearest'. Returns the image at the model's own scale (4)
    for models, or at `scale` for the resamplers."""
    h, w, _ = rgb.shape
    if method in ("lanczos", "bicubic", "nearest"):
        m = {"lanczos": Image.LANCZOS, "bicubic": Image.BICUBIC, "nearest": Image.NEAREST}[method]
        return resize(rgb, w * scale, h * scale, m)
    return run_model(load_model(method), rgb)


# --------------------------------------------------------------------------------------- correction --


def blur(a, sigma):
    """Separable Gaussian, edges replicated."""
    r = max(1, int(3 * sigma + 0.5))
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / sigma) ** 2).astype(np.float32)
    k /= k.sum()
    p = np.pad(a, ((r, r), (r, r), (0, 0)), mode="edge")
    p = sum(k[i] * p[i: i + a.shape[0]] for i in range(2 * r + 1))          # rows: H x (W + 2r)
    return sum(k[i] * p[:, i: i + a.shape[1]] for i in range(2 * r + 1))   # columns: H x W


def back_project(up, orig, strength=1.0, sigma=1.0):
    """Restores the original's low frequencies: the upscaled image, box-downscaled to the original size, should
    match the original. The difference, blurred (sigma in source pixels) so only colour / brightness drift is
    corrected, is upsampled (bilinear: no overshoot, so no halos at hard edges) and added. Removes the drift the
    models introduce without touching the detail they add."""
    h, w, _ = orig.shape
    down = resize(up, w, h, Image.BOX)
    diff = orig - down
    if sigma:
        diff = blur(diff, sigma)
    return up + strength * resize(diff, up.shape[1], up.shape[0], Image.BILINEAR)
