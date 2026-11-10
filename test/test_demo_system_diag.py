import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import demo
from demo_config import MODELS, config_for


class SystemDiagModelTest(unittest.TestCase):
    def test_model_is_explicit_for_system_diag(self):
        with self.assertRaisesRegex(SystemExit, "modelo físico"):
            demo.resolve_model(None, "SystemDiag")
        self.assertEqual(demo.resolve_model("express_legacy", "SystemDiag"), "express_legacy")
        self.assertEqual(demo.resolve_model(None, "AFM07"), "dron_legacy")

    def test_all_supported_models_have_a_diagnostic_environment(self):
        self.assertEqual(set(config_for("SystemDiag")["models"]), set(MODELS))


if __name__ == "__main__":
    unittest.main()
