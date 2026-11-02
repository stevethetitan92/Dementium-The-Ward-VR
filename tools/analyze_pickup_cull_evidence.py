"""Correlate saved pickup dumps and replay complete sphere evidence only."""
import argparse
import importlib.util
import json
import math
from pathlib import Path
import re
import struct

spec = importlib.util.spec_from_file_location("pickup_replay", Path(__file__).with_name("replay_pickup_sphere.py"))
replay_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(replay_module)


def analyze(lines):
    records = {}
    ambiguous = 0
    for line in lines:
        kind = line.split(" ", 1)[0].strip()
        if kind not in ("pickup_transition", "pickup_transition_tested_sphere", "pickup_transition_tested_plane"):
            continue
        fields = dict(re.findall(r"(\w+)=([^\s]+)", line))
        if not all(fields.get(name, "").isdigit() for name in ("dump", "record")):
            ambiguous += 1
            continue
        key = (fields["dump"], fields["record"])
        entry = records.setdefault(key, {"event": {}, "sphere": None, "planes": {}, "duplicates": 0,
                                         "invalid_details": 0, "identities": []})
        entry["identities"].append((fields.get("time"), fields.get("entity")))
        if kind == "pickup_transition":
            if entry["event"]:
                entry["duplicates"] += 1
            else:
                entry["event"] = fields
        elif kind == "pickup_transition_tested_sphere":
            values = fields.get("center", "").split(",") + [fields.get("radius", "")]
            if entry["sphere"] is not None:
                entry["duplicates"] += 1
            elif len(values) != 4 or not all(values):
                entry["invalid_details"] += 1
            else:
                entry["sphere"] = values
        else:
            index = fields.get("index", "")
            values = fields.get("values", "").split(",")
            if not index.isdigit() or int(index) >= 6 or len(values) != 4:
                entry["invalid_details"] += 1
            elif index in entry["planes"]:
                entry["duplicates"] += 1
            else:
                entry["planes"][index] = values
    output = []
    for (dump, record), entry in records.items():
        identities = entry.pop("identities")
        identity_consistent = bool(identities) and all(pair == identities[0] and all(pair) for pair in identities)
        complete = bool(entry["event"]) and entry["sphere"] is not None and set(entry["planes"]) == set(map(str, range(6)))
        usable = complete and identity_consistent and not entry["duplicates"] and not entry["invalid_details"] and entry["event"].get("tested") == "1"
        replay = replay_module.replay(entry["sphere"], [entry["planes"][str(i)] for i in range(6)]) if usable else {"comparable": False, "reason": "incomplete, ambiguous, duplicate or untested evidence"}
        matches = None
        if replay["comparable"]:
            try:
                native = float(entry["event"]["native"])
                if math.isfinite(native):
                    native = struct.unpack("<f", struct.pack("<f", native))[0]
                    matches = native == replay["expected_result"]
            except (KeyError, ValueError, OverflowError):
                pass
        output.append({"dump": dump, "record": record, **entry, "identity_consistent": identity_consistent,
                       "evidence_complete": complete, "arithmetic_replay": replay,
                       "result_matches_arithmetic_model": matches})
    return {"records": output, "uncorrelated_records": ambiguous,
            "limits": "Saved-value float32 arithmetic model only; actual MXCSR/native execution not verified. A mismatch is evidence to investigate, not a fault diagnosis. Final visibility also depends on sector/lifecycle/collection and is not predicted. No game access."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    with args.log.open(encoding="utf-8", errors="replace") as source:
        report = analyze(source)
    args.output.write_text(json.dumps(report, indent=2, allow_nan=False), encoding="utf-8")
    print(f"Saved {len(report['records'])} pickup records; no game access.")
