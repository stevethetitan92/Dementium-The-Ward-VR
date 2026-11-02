"""Read saved scene evidence only. Never opens a process or changes the game."""
import argparse
import json
import re
import math
from pathlib import Path

FIELDS = re.compile(r"(\w+)=([^\s]+)")


def pointer_value(value):
    if not re.fullmatch(r"(?:0x)?[0-9a-f]+", value.lower()):
        return None
    return int(value, 16)


def analyze(lines):
    groups = {}
    uncorrelated = 0
    for line in lines:
        kind = line.split(" ", 1)[0].strip()
        if not kind.startswith("scene_evidence_") or kind == "scene_evidence_requested":
            continue
        fields = dict(FIELDS.findall(line))
        if not all(name in fields for name in ("ticket", "frame", "thread")):
            uncorrelated += 1
            continue
        key = tuple(fields[name] for name in ("ticket", "frame", "thread"))
        group = groups.setdefault(key, {"profile": {}, "resource": {}, "sampler": {}, "first_draw_textures": {},
                                        "profile_rows": {}, "invalid_profile_rows": 0, "duplicate_profile_rows": 0})
        if kind == "scene_evidence_cpu_profile":
            group["profile"] = fields
        elif kind == "scene_evidence_profile_resource":
            group["resource"] = fields
        elif kind == "scene_evidence_native_sampler":
            group["sampler"] = fields
        elif kind == "scene_evidence_cpu_profile_values":
            tokens = line.rsplit("row=", 1)[-1].split()
            try:
                row = int(tokens[0])
                if not 0 <= row < 64 or len(tokens) != 5:
                    raise ValueError("invalid row")
                for value in tokens[1:]:
                    float(value)  # Preserve raw strings, including nonfinite evidence, in strict JSON.
                if str(row) in group["profile_rows"]:
                    group["duplicate_profile_rows"] += 1
                else:
                    group["profile_rows"][str(row)] = tokens[1:]
            except (ValueError, IndexError):
                group["invalid_profile_rows"] += 1
        elif kind == "scene_evidence_texture" and fields.get("sample") == "1" and fields.get("slot", "").isdigit() and int(fields["slot"]) < 8:
            group["first_draw_textures"][fields["slot"]] = fields
    result = []
    for key, group in groups.items():
        resource = group["resource"]
        pointer = pointer_value(resource.get("resource_pointer", ""))
        known = resource.get("resource_known") == "1" and bool(pointer)
        matches = []
        complete = len(group["profile_rows"]) == 64 and not group["invalid_profile_rows"] and not group["duplicate_profile_rows"]
        values = [float(value) for row in group["profile_rows"].values() for value in row]
        finite = complete and all(math.isfinite(value) for value in values)
        if known:
            for slot, texture in group["first_draw_textures"].items():
                candidate = pointer_value(texture.get("resource", ""))
                if candidate == pointer:
                    matches.append(slot)
        result.append({"ticket": key[0], "frame": key[1], "thread": key[2], **group,
                       "pointer_comparison_available": known and bool(group["first_draw_textures"]),
                       "profile_values_complete": complete, "profile_values_finite": finite,
                       "profile_value_range": [min(values), max(values)] if finite else None,
                       "first_draw_matching_slots": sorted(matches, key=int)})
    return {"groups": result, "uncorrelated_subrecords": uncorrelated,
            "limits": "Raw pointer-value comparison for first draw only; not canonical COM identity, GPU contents, lifetime, a fault diagnosis, or live acceptance. Older records lacking frame/thread are never merged."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    with args.log.open(encoding="utf-8", errors="replace") as source:
        report = analyze(source)
    args.output.write_text(json.dumps(report, indent=2, allow_nan=False), encoding="utf-8")
    print(f"Saved {len(report['groups'])} correlated groups; {report['uncorrelated_subrecords']} older/uncorrelated subrecords retained as a count. No game access.")


if __name__ == "__main__":
    main()
