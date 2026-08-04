#!/usr/bin/env python3
"""Validate the release boundary for the internal trackball precision mode."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
KEYMAP = ROOT / "config/minimal-keys.keymap"
OVERLAY = ROOT / "config/boards/shields/minimal-keys/minimal-keys_R.overlay"
CONF = ROOT / "config/boards/shields/minimal-keys/minimal-keys_R.conf"
MANIFEST = ROOT / "config/west.yml"
PMW_MODULE_REVISION = "c1ce02ff949bcd0cc4c53b05ffea2db30be9fa12"


def _node_body(source: str, node_name: str) -> str:
    match = re.search(rf"{re.escape(node_name)}\s*\{{", source)
    if not match:
        raise AssertionError(f"node {node_name!r} was not found")

    depth = 1
    index = match.end()
    while index < len(source) and depth:
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
        index += 1

    if depth:
        raise AssertionError(f"node {node_name!r} is not closed")
    return source[match.end() : index - 1]


def _project_revision(manifest: str, project_name: str) -> str:
    project = re.search(
        rf"^    - name: {re.escape(project_name)}$([\s\S]*?)(?=^    - name:|^  self:)",
        manifest,
        re.MULTILINE,
    )
    if not project:
        raise AssertionError(f"project {project_name!r} was not found")

    revision = re.search(r"^      revision: [\"']?([^\"'\s]+)[\"']?$", project.group(1), re.MULTILINE)
    if not revision:
        raise AssertionError(f"project {project_name!r} has no revision")
    return revision.group(1)


class PrecisionLayerConfigTest(unittest.TestCase):
    def setUp(self) -> None:
        self.keymap = KEYMAP.read_text()
        self.overlay = OVERLAY.read_text()
        self.conf = CONF.read_text()
        self.manifest = MANIFEST.read_text()

    def test_internal_precision_layer_is_all_transparent_and_has_43_positions(self) -> None:
        """Breaks if L8 becomes user-editable or loses a physical key position."""
        precision_layer = _node_body(self.keymap, "precision_layer")
        bindings = re.search(r"bindings\s*=\s*<(.*?)>;", precision_layer, re.DOTALL)
        self.assertIsNotNone(bindings, "precision_layer must define bindings")
        self.assertEqual(re.findall(r"&\w+", bindings.group(1)), ["&trans"] * 43)

    def test_precision_layer_is_explicitly_index_8(self) -> None:
        """Breaks if the internal layer is moved away from the PMW snipe index."""
        keymap = _node_body(self.keymap, "keymap")
        layer_names = re.findall(r"^ {8}([A-Za-z_][A-Za-z0-9_]*)\s*\{", keymap, re.MULTILINE)
        self.assertEqual(layer_names.index("precision_layer"), 8)

    def test_trackball_switches_to_internal_precision_layer_8(self) -> None:
        """Breaks if PMW snipe mode activates a layer other than the reserved L8."""
        trackball = _node_body(self.overlay, "trackball: trackball@0")
        self.assertIn("snipe-layers = <8>;", trackball)

    def test_trackball_precision_defaults_and_studio_settings_are_enabled(self) -> None:
        """Breaks if firmware loses the calibrated normal/snipe defaults or RPC controls."""
        expected_defaults = {
            "CONFIG_PMW3610_CPI": "800",
            "CONFIG_PMW3610_SNIPE_CPI": "200",
            "CONFIG_PMW3610_X_SCALE": "100",
            "CONFIG_PMW3610_Y_SCALE": "100",
            "CONFIG_PMW3610_STUDIO_RPC": "y",
            "CONFIG_PMW3610_TRACKBALL_SETTINGS": "y",
        }
        actual = dict(re.findall(r"^(CONFIG_PMW3610_[A-Z0-9_]+)=(.+)$", self.conf, re.MULTILINE))
        for setting, value in expected_defaults.items():
            self.assertEqual(actual.get(setting), value, setting)

    def test_pmw_module_is_pinned_to_the_precision_settings_commit(self) -> None:
        """Breaks if the manifest reverts to the old driver or a moving revision."""
        revision = _project_revision(self.manifest, "pmw3610-driver-minimal")
        self.assertEqual(revision, PMW_MODULE_REVISION)
        self.assertRegex(revision, r"^[0-9a-f]{40}$")
        self.assertNotEqual(revision, "ed93886")


if __name__ == "__main__":
    unittest.main()
