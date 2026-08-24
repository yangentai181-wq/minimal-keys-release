#!/usr/bin/env python3
"""Validate the layer-7 trackball scroll wiring."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
OVERLAY = ROOT / "config/boards/shields/minimal-keys/minimal-keys_R.overlay"


def _node_body(source: str, node_name: str) -> str:
    match = re.search(rf"{re.escape(node_name)}\s*\{{", source)
    if not match:
        raise AssertionError(f"node {node_name!r} was not found")

    depth = 1
    i = match.end()
    while i < len(source) and depth:
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
        i += 1

    if depth:
        raise AssertionError(f"node {node_name!r} is not closed")
    return source[match.end() : i - 1]


class ScrollLayerConfigTest(unittest.TestCase):
    def setUp(self) -> None:
        self.overlay = OVERLAY.read_text()

    def test_layer_7_scroll_uses_listener_override_not_pmw_scroll_layers(self) -> None:
        self.assertIn("#include <input/processors.dtsi>", self.overlay)
        self.assertIn(
            "#include <input/processors/runtime-input-processor.dtsi>",
            self.overlay,
        )

        trackball = _node_body(self.overlay, "trackball: trackball@0")
        self.assertNotIn("scroll-layers", trackball)

        listener = _node_body(self.overlay, "trackball_listener")
        self.assertIn(
            "input-processors = <&zip_mouse_gesture &mouse_runtime_input_processor>;",
            listener,
        )

        scroll_layer = _node_body(listener, "scroll_layer")
        self.assertIn("layers = <7>;", scroll_layer)
        self.assertIn(
            "input-processors = <&zip_mouse_gesture &zip_xy_to_scroll_mapper "
            "&scroll_runtime_input_processor>;",
            scroll_layer,
        )

        scroll_processor = _node_body(self.overlay, "&scroll_runtime_input_processor")
        self.assertIn("scale-divisor = <16>;", scroll_processor)
        self.assertIn("y-invert;", scroll_processor)


if __name__ == "__main__":
    unittest.main()
