import json
from pathlib import Path
import sys
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

import production_qualification as qa


def valid_sample(state="SOAK", elapsed_ms=0, flow=5.0):
    return {
        "_kind": "sample",
        "state": state,
        "uptimeMs": elapsed_ms,
        "elapsedMs": elapsed_ms,
        "rtcUnix": 1_800_000_000 + elapsed_ms // 1000,
        "rtcValid": True,
        "rtcMonotonic": True,
        "flow": flow,
        "flowValid": True,
        "flowFresh": True,
        "flowAgeMs": 0,
        "bmeValid": True,
        "ntcValid": True,
        "ntcC": 25.0,
        "pwm": [0, 0] if state == "SOAK" else [100, 0],
        "capturedVolumeL": flow * elapsed_ms / 60000.0,
        "integratedVolumeL": flow * elapsed_ms / 60000.0,
        "rs485": {
            "successes": 200,
            "failures": 0,
            "timeouts": 0,
            "crc": 0,
            "incomplete": 0,
            "malformed": 0,
            "busBusy": 0,
            "lateBytes": 0,
            "unexpected": 0,
            "exceptions": 0,
            "deadlineMisses": 0,
            "maxSuccessGapMs": 1005,
            "minRestMs": 1000,
        },
        "i2c": {
            "failures": 0,
            "nacks": 0,
            "timeouts": 0,
            "shortReads": 0,
            "recoveries": 0,
        },
    }


def ready_result():
    return {
        "_kind": "result",
        "result": "READY_FOR_LONG_MEASUREMENTS",
        "reason": "all_acceptance_gates_passed",
        "volumeDifferenceL": 0.05,
        "pwmFinalZero": True,
        "captureFile": "log_2026_09_01T12_00_00.csv",
        "csvFinalized": True,
        "masterIndexUpdated": True,
        "csvReopenable": True,
        "csvCadenceOk": True,
    }


class ProtocolTests(unittest.TestCase):
    def test_parses_all_structured_prefixes_and_ignores_logs(self):
        for prefix, kind in qa.PREFIXES.items():
            parsed = qa.parse_protocol_line(prefix + ' {"state":"SOAK"}')
            self.assertEqual(parsed["_kind"], kind)
            self.assertEqual(parsed["state"], "SOAK")
        self.assertIsNone(qa.parse_protocol_line("PID flujo: medido 5.0"))

    def test_parses_structured_message_after_boot_null_bytes(self):
        parsed = qa.parse_protocol_line('\x00\x00EOLO_QA_EVENT {"state":"IDLE"}')
        self.assertEqual(parsed["_kind"], "event")
        self.assertEqual(parsed["state"], "IDLE")

    def test_status_message_can_confirm_idle_boot_state(self):
        parsed = qa.parse_protocol_line('EOLO_QA_EVENT {"event":"STATUS","state":"IDLE"}')
        self.assertEqual(parsed["event"], "STATUS")
        self.assertEqual(parsed["state"], "IDLE")

    def test_rejects_malformed_structured_json(self):
        with self.assertRaises(qa.QualificationError):
            qa.parse_protocol_line("EOLO_QA_EVENT {no-json}")
        with self.assertRaises(qa.QualificationError):
            qa.parse_protocol_line("EOLO_QA_EVENT []")


class MarginSelectionTests(unittest.TestCase):
    def test_selects_two_approved_steps_slower_than_fastest(self):
        results = [
            {"gapMs": 1000, "approved": True},
            {"gapMs": 800, "approved": True},
            {"gapMs": 600, "approved": True},
            {"gapMs": 400, "approved": False},
        ]
        self.assertEqual(qa.select_margin_gap(results), 1000)

    def test_blocks_when_only_1000_ms_is_approved(self):
        self.assertIsNone(qa.select_margin_gap([
            {"gapMs": 1000, "approved": True},
            {"gapMs": 800, "approved": False},
        ]))

    def test_all_approved_selects_600_ms(self):
        results = [{"gapMs": gap, "approved": True} for gap in qa.SWEEP_GAPS_MS]
        self.assertEqual(qa.select_margin_gap(results), 600)


class SoakAcceptanceTests(unittest.TestCase):
    def make_samples(self):
        return [valid_sample(elapsed_ms=index * 1000) for index in range(120)]

    def test_accepts_complete_clean_soak(self):
        result = qa.evaluate_soak_samples(self.make_samples(), 1000)
        self.assertTrue(result["approved"], result["reason"])

    def test_every_soak_failure_field_blocks(self):
        mutations = {
            "afm": lambda s: s.update(flowFresh=False),
            "bme": lambda s: s.update(bmeValid=False),
            "rtc": lambda s: s.update(rtcMonotonic=False),
            "ntc_invalid": lambda s: s.update(ntcValid=False),
            "ntc_hot": lambda s: s.update(ntcC=70.0),
            "pwm": lambda s: s.update(pwm=[1, 0]),
            "rs485": lambda s: s["rs485"].update(crc=1),
            "i2c": lambda s: s["i2c"].update(timeouts=1),
            "max_gap": lambda s: s["rs485"].update(maxSuccessGapMs=1501),
            "min_rest": lambda s: s["rs485"].update(minRestMs=994),
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name):
                samples = self.make_samples()
                mutate(samples[-1])
                self.assertFalse(qa.evaluate_soak_samples(samples, 1000)["approved"])

    def test_incomplete_soak_blocks(self):
        self.assertFalse(qa.evaluate_soak_samples(self.make_samples()[:109], 1000)["approved"])


class LongAcceptanceTests(unittest.TestCase):
    def make_samples(self):
        return [valid_sample("CAPTURE", index * 1000, 5.0) for index in range(600)]

    def test_accepts_complete_long_capture(self):
        result = qa.evaluate_long_capture(self.make_samples(), ready_result())
        self.assertTrue(result["approved"], result["reason"])

    def test_all_finalization_failures_block(self):
        fields = (
            "pwmFinalZero",
            "csvFinalized",
            "masterIndexUpdated",
            "csvReopenable",
            "csvCadenceOk",
        )
        for field in fields:
            with self.subTest(field=field):
                result = ready_result()
                result[field] = False
                self.assertFalse(qa.evaluate_long_capture(self.make_samples(), result)["approved"])
        result = ready_result()
        result["captureFile"] = ""
        self.assertFalse(qa.evaluate_long_capture(self.make_samples(), result)["approved"])

    def test_quality_and_transport_failures_block(self):
        cases = []
        not_ready = ready_result()
        not_ready["result"] = "NOT_READY"
        cases.append((self.make_samples(), not_ready, False))
        cases.append((self.make_samples(), ready_result(), True))
        cases.append((self.make_samples()[:579], ready_result(), False))

        late_band = self.make_samples()
        for index in range(121):
            late_band[index]["flow"] = 4.0
        cases.append((late_band, ready_result(), False))

        low_ratio = self.make_samples()
        for index in range(0, 600, 10):
            low_ratio[index]["flow"] = 4.0
        cases.append((low_ratio, ready_result(), False))

        consecutive = self.make_samples()
        for index in range(100, 111):
            consecutive[index]["flow"] = 4.0
        cases.append((consecutive, ready_result(), False))

        transport = self.make_samples()
        transport[-1]["rs485"]["exceptions"] = 1
        cases.append((transport, ready_result(), False))

        excess_failures = self.make_samples()
        excess_failures[-1]["rs485"]["failures"] = 6
        cases.append((excess_failures, ready_result(), False))

        for index, (samples, result, reset_seen) in enumerate(cases):
            with self.subTest(case=index):
                self.assertFalse(
                    qa.evaluate_long_capture(samples, result, reset_seen)["approved"]
                )

        result = ready_result()
        result["volumeDifferenceL"] = 0.101
        self.assertFalse(qa.evaluate_long_capture(self.make_samples(), result)["approved"])


class ArtifactTests(unittest.TestCase):
    def test_detects_incomplete_artifacts(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "metadata.json").write_text("{}", encoding="utf-8")
            complete, missing = qa.validate_artifact_bundle(root)
            self.assertFalse(complete)
            self.assertIn("events.jsonl", missing)

    def test_accepts_complete_artifact_contract(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "metadata.json").write_text(json.dumps({
                "gitSha": "abc", "binarySha256": "def", "mac": "00:11:22:33:44:55",
                "environment": qa.ENVIRONMENT, "gapMs": 800,
            }), encoding="utf-8")
            (root / "events.jsonl").write_text("{}\n", encoding="utf-8")
            (root / "samples.csv").write_text("state\nCAPTURE\n", encoding="utf-8")
            (root / "result.json").write_text(json.dumps({
                "result": "READY_FOR_LONG_MEASUREMENTS"
            }), encoding="utf-8")
            self.assertEqual(qa.validate_artifact_bundle(root), (True, []))


if __name__ == "__main__":
    unittest.main()
