import importlib.util
from pathlib import Path
import unittest

path = Path(__file__).parents[1] / "tools" / "replay_pickup_sphere.py"
spec = importlib.util.spec_from_file_location("sphere_replay", path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PickupSphereReplay(unittest.TestCase):
    def test_native_returns_last_plane_not_minimum(self):
        result = module.replay([0, 0, 0, 1], [[0, 0, 0, value] for value in (1, 4, 3, 2, 5, 6)])
        self.assertTrue(result["comparable"])
        self.assertEqual(result["expected_result"], 7)

    def test_boundary_and_adjacent_float_values(self):
        planes = [[0, 0, 0, 1] for _ in range(6)]
        planes[-1][-1] = -1
        result = module.replay([0, 0, 0, 1], planes)
        self.assertEqual(result["first_rejected_plane"], 5)
        self.assertTrue(result["boundary_equality"])
        planes[-1][-1] = -0.9999999403953552
        self.assertEqual(module.replay([0, 0, 0, 1], planes)["expected_result"], 2 ** -24)
        planes[-1][-1] = -1.0000001192092896
        self.assertEqual(module.replay([0, 0, 0, 1], planes)["first_rejected_plane"], 5)

    def test_cancellation_preserves_native_operation_order(self):
        planes = [[0, 0, 0, 1] for _ in range(5)] + [[-1e20, 1e20, 1, 0]]
        self.assertEqual(module.replay([1, 1, 1, 0], planes)["expected_result"], 1)

    def test_incomplete_and_unknown_numeric_modes_stay_unknown(self):
        planes = [[0, 0, 0, 1] for _ in range(6)]
        for sphere, values in (([0, 0, 0, 1], planes[:5]), ([0, 0, 0, -1], planes),
                               ([float("nan"), 0, 0, 1], planes), ([1e-40, 0, 0, 1], planes),
                               ([3e38, 0, 0, 1], [[3e38, 0, 0, 1]] * 6)):
            self.assertFalse(module.replay(sphere, values)["comparable"])


if __name__ == "__main__":
    unittest.main()
