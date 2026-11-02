"""Arithmetic replay of saved pickup sphere/plane values; no game access."""
import argparse
import json
import math
import struct
from pathlib import Path

MIN_NORMAL = 2 ** -126


def rounded(value):
    if not math.isfinite(value) or (value and abs(value) < MIN_NORMAL):
        raise ValueError("nonfinite or subnormal arithmetic; saved MXCSR unknown")
    result = struct.unpack("<f", struct.pack("<f", value))[0]
    if not math.isfinite(result) or (result and abs(result) < MIN_NORMAL):
        raise ValueError("nonfinite or subnormal float32 result")
    return result


def replay(sphere, planes):
    try:
        if len(sphere) != 4 or len(planes) != 6 or any(len(plane) != 4 for plane in planes):
            raise ValueError("complete sphere and six planes required")
        x, y, z, radius = [rounded(float(value)) for value in sphere]
        if radius < 0:
            raise ValueError("negative radius is not suitable diagnostic geometry")
        planes = [[rounded(float(value)) for value in plane] for plane in planes]
        distances = []
        for index, (px, py, pz, constant) in enumerate(planes):
            # Pinned7B826..7B83F: scalar SSE operations, not a fused dot product.
            distance = rounded(rounded(rounded(y * py) + rounded(x * px)) + rounded(z * pz))
            distance = rounded(distance + constant)
            distances.append(distance)
            if distance <= -radius:
                return {"comparable": True, "expected_result": 0.0, "first_rejected_plane": index,
                        "boundary_equality": distance == -radius, "tested_distances": distances}
        return {"comparable": True, "expected_result": rounded(distances[-1] + radius),
                "first_rejected_plane": None, "boundary_equality": False, "tested_distances": distances}
    except (ValueError, OverflowError, TypeError) as error:
        return {"comparable": False, "reason": str(error)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path, help="JSON with sphere[4] and planes[6][4]")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    report = replay(capture.get("sphere", []), capture.get("planes", []))
    report["limits"] = "Standard float32 round-to-nearest arithmetic model from pinned native instructions; saved MXCSR and native call execution are not verified. Not final pickup visibility, a fault diagnosis, or live acceptance."
    args.output.write_text(json.dumps(report, indent=2, allow_nan=False), encoding="utf-8")
    print("Saved arithmetic replay; comparable=" + str(report["comparable"]) + "; no game access.")


if __name__ == "__main__":
    main()
