import importlib.util
from pathlib import Path
import unittest
import json

path = Path(__file__).parents[1] / "tools" / "analyze_item2_scene_evidence.py"
spec = importlib.util.spec_from_file_location("scene_analysis", path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class SceneEvidenceAnalysis(unittest.TestCase):
    def test_threads_and_frames_stay_separate(self):
        lines = []
        for frame, thread, pointer in ((10, 1, "0000ABCD"), (10, 2, "0000DCBA"), (11, 1, "1234")):
            tag = f"ticket=1 frame={frame} thread={thread}"
            lines += [f"scene_evidence_profile_resource {tag} resource_known=1 resource_pointer={pointer}",
                      f"scene_evidence_texture {tag} sample=1 slot=7 resource=0x{pointer.lower()}",
                      f"scene_evidence_texture {tag} sample=2 slot=7 resource=9999"]
        report = module.analyze(lines)
        self.assertEqual(len(report["groups"]), 3)
        for group in report["groups"]:
            self.assertTrue(group["pointer_comparison_available"])
            self.assertEqual(group["first_draw_matching_slots"], ["7"])

    def test_missing_unknown_and_old_records_do_not_claim_matches(self):
        report = module.analyze([
            "scene_evidence_requested ticket=1 frame=10 max_draws_per_thread=12 source_frames=1",
            "scene_evidence_texture ticket=1 sample=1 slot=7 resource=ABCD",
            "scene_evidence_profile_resource ticket=1 frame=10 thread=1 resource_known=0 resource_pointer=ABCD",
            "scene_evidence_texture ticket=1 frame=10 thread=1 sample=1 slot=7 resource=ABCD",
            "scene_evidence_profile_resource ticket=2 frame=10 thread=1 resource_known=1 resource_pointer=00000000",
            "scene_evidence_profile_resource ticket=3 frame=10 thread=1 resource_known=1 resource_pointer=unknown",
            "scene_evidence_texture ticket=3 frame=10 thread=1 sample=1 slot=x resource=unknown",
        ])
        self.assertEqual(report["uncorrelated_subrecords"], 1)
        self.assertTrue(all(not group["pointer_comparison_available"] for group in report["groups"]))
        self.assertTrue(all(not group["first_draw_matching_slots"] for group in report["groups"]))

    def test_mismatch_is_evidence_only(self):
        report = module.analyze([
            "scene_evidence_profile_resource ticket=3 frame=20 thread=4 resource_known=1 resource_pointer=ABCD",
            "scene_evidence_texture ticket=3 frame=20 thread=4 sample=1 slot=7 resource=DCBA",
        ])
        self.assertTrue(report["groups"][0]["pointer_comparison_available"])
        self.assertEqual(report["groups"][0]["first_draw_matching_slots"], [])
        self.assertIn("not canonical COM identity", report["limits"])

    def test_profile_rows_require_complete_unique_finite_evidence(self):
        rows = [f"scene_evidence_cpu_profile_values ticket=1 frame=2 thread=3 row={row} .95 .96 .98 1" for row in range(64)]
        group = module.analyze(rows)["groups"][0]
        self.assertTrue(group["profile_values_complete"] and group["profile_values_finite"])
        self.assertEqual(group["profile_value_range"], [.95, 1])
        self.assertFalse(module.analyze(rows[:-1])["groups"][0]["profile_values_complete"])
        self.assertFalse(module.analyze(rows + rows[:1])["groups"][0]["profile_values_complete"])
        rows[-1] = "scene_evidence_cpu_profile_values ticket=1 frame=2 thread=3 row=63 nan .96 .98 1"
        report = module.analyze(rows)
        self.assertTrue(report["groups"][0]["profile_values_complete"])
        self.assertFalse(report["groups"][0]["profile_values_finite"])
        self.assertIsNone(report["groups"][0]["profile_value_range"])
        json.dumps(report, allow_nan=False)
        rows[-1] = "scene_evidence_cpu_profile_values ticket=1 frame=2 thread=3 row=63 clipped"
        self.assertEqual(module.analyze(rows)["groups"][0]["invalid_profile_rows"], 1)


if __name__ == "__main__":
    unittest.main()
