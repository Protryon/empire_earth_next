"""The game's settings file, config.yaml (include/platform/PlatConfig.h): a fresh file from a run's pins.

A pin is "block.path.key=value", e.g. "audio.music.enabled=false" or "network.pt_rate.min=40"; the keys are the
ones EEPreferences.cpp's table writes (docs/DATA_FORMATS.md "config.yaml"). Integers and true/false are written as
they are, any other value as a double-quoted string."""
import json
import re

PLAIN = re.compile(r"^(-?\d+|true|false)$")


def config_text(pins):
    """config.yaml text holding the pins (in their order; blocks grouped)."""
    tree = {}
    for item in pins:
        path, sep, value = item.partition("=")
        keys = [k.strip() for k in path.strip().split(".")]
        if not sep or not all(keys) or any(" " in k for k in keys):
            raise ValueError(f"config pin {item!r}: expected block.key=value (config.yaml keys, no spaces)")
        node = tree
        for k in keys[:-1]:
            node = node.setdefault(k, {})
            if not isinstance(node, dict):
                raise ValueError(f"config pin {item!r}: {k} is a value, not a block")
        value = value.strip()
        node[keys[-1]] = value if PLAIN.match(value) else json.dumps(value)
    lines = []

    def emit(node, indent):
        for k, v in node.items():
            if isinstance(v, dict):
                lines.append(f"{' ' * indent}{k}:")
                emit(v, indent + 2)
            else:
                lines.append(f"{' ' * indent}{k}: {v}")
    emit(tree, 0)
    return "\n".join(lines) + "\n"
