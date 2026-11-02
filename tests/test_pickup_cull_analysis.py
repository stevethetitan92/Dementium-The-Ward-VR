import importlib.util
from pathlib import Path
import unittest

path = Path(__file__).parents[1] / "tools" / "analyze_pickup_cull_evidence.py"
spec = importlib.util.spec_from_file_location("pickup_analysis", path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def record(dump=1, index=0, native="2", last="1"):
    tag = f"dump={dump} record={index} time=100 entity=ABCD"
    return [f"pickup_transition {tag} tested=1 native={native} visible=0 sector=stamp_differs",
            f"pickup_transition_tested_sphere {tag} center=0,0,0 radius=1"] + [
        f"pickup_transition_tested_plane {tag} index={i} values=0,0,0,{last if i==5 else '1'}" for i in range(6)]


class PickupCullAnalysis(unittest.TestCase):
    def test_same_time_entity_stays_separate_by_dump_record(self):
        report = module.analyze(record() + record(index=1, native="0", last="-1") + record(dump=2))
        self.assertEqual(len(report["records"]), 3)
        self.assertTrue(all(item["result_matches_arithmetic_model"] for item in report["records"]))
        self.assertIn("Final visibility", report["limits"])

    def test_missing_duplicate_and_conflicting_details_stay_unknown(self):
        for lines in (record()[:-1], record() + record()[-1:], record()[:-1] + [record()[-1].replace("entity=ABCD", "entity=DCBA")]):
            item = module.analyze(lines)["records"][0]
            self.assertFalse(item["arithmetic_replay"]["comparable"])
            self.assertIsNone(item["result_matches_arithmetic_model"])

    def test_mismatch_and_tiny_positive_are_not_rounded_to_rejection(self):
        mismatch = module.analyze(record(native="1"))["records"][0]
        self.assertFalse(mismatch["result_matches_arithmetic_model"])
        tiny = module.analyze(record(native="5.96046448e-08", last="-0.9999999403953552"))["records"][0]
        self.assertTrue(tiny["result_matches_arithmetic_model"])
        self.assertGreater(tiny["arithmetic_replay"]["expected_result"], 0)

    def test_older_records_not_guessed(self):
        self.assertEqual(module.analyze(["pickup_transition time=100 entity=ABCD native=1"])["uncorrelated_records"], 1)


if __name__ == "__main__":
    unittest.main()
