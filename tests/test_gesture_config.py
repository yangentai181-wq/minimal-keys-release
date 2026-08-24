#!/usr/bin/env python3
"""Validate trackball gesture shortcuts are wired to reserved key bindings."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
KEYMAP = ROOT / "config/minimal-keys.keymap"
OVERLAY = ROOT / "config/boards/shields/minimal-keys/minimal-keys_R.overlay"
MANIFEST = ROOT / "config/west.yml"
SOURCE = ROOT / "config/src/behavior_gesture_slot.c"
MODULE = ROOT / "config/zephyr/module.yml"
MODULE_CMAKE = ROOT / "config/CMakeLists.txt"
MODULE_SOURCE = ROOT / "config/src/behavior_gesture_slot.c"
MODULE_BINDING = ROOT / "config/dts/bindings/behaviors/minimal-keys,behavior-gesture-slot.yaml"
GESTURE_REVISION = "62f3c9d8ca6763e160b73efc46108c61dd0243a0"


def _node_body(source: str, node_name: str) -> str:
    match = re.search(rf"{re.escape(node_name)}\s*\{{", source)
    if not match:
        raise AssertionError(f"node {node_name!r} was not found")
    depth = 1
    index = match.end()
    while index < len(source) and depth:
        if source[index] == "{": depth += 1
        elif source[index] == "}": depth -= 1
        index += 1
    if depth: raise AssertionError(f"node {node_name!r} is not closed")
    return source[match.end() : index - 1]


def project_property(manifest: str, project_name: str, property_name: str) -> str:
    project = re.search(rf"^    - name: {re.escape(project_name)}$([\s\S]*?)(?=^    - name:|^  self:)", manifest, re.MULTILINE)
    if not project: raise AssertionError(f"project {project_name!r} was not found")
    value = re.search(rf"^      {re.escape(property_name)}: [\"']?([^\"'\s]+)[\"']?$", project.group(1), re.MULTILINE)
    if not value: raise AssertionError(f"project {project_name!r} has no {property_name}")
    return value.group(1)


class GestureConfigTest(unittest.TestCase):
    def setUp(self) -> None:
        self.keymap = KEYMAP.read_text()
        self.overlay = OVERLAY.read_text()
        self.manifest = MANIFEST.read_text()

    def test_gesture_module_is_pinned_to_the_approved_fork_commit(self) -> None:
        self.assertEqual(project_property(self.manifest, "zmk-mouse-gesture", "remote"), "kot149")
        self.assertEqual(project_property(self.manifest, "zmk-mouse-gesture", "revision"), GESTURE_REVISION)
        self.assertRegex(GESTURE_REVISION, r"^[0-9a-f]{40}$")

    def test_manifest_config_root_registers_the_gesture_module_for_build_and_dts(self) -> None:
        """The manifest self path is config, so its module must expose both assets."""
        module = MODULE.read_text()
        self.assertIn("cmake: .", module)
        self.assertIn("dts_root: .", module)
        self.assertIn("src/behavior_gesture_slot.c", MODULE_CMAKE.read_text())
        self.assertTrue(MODULE_SOURCE.is_file())
        self.assertTrue(MODULE_BINDING.is_file())

    def test_gesture_adapter_is_compiled_only_when_the_gesture_feature_is_enabled(self) -> None:
        cmake = MODULE_CMAKE.read_text()
        self.assertIn("target_sources_ifdef(CONFIG_ZMK_MOUSE_GESTURE app PRIVATE", cmake)
        self.assertIn("src/behavior_gesture_slot.c", cmake)

    def test_gesture_layer_is_reserved_at_index_nine(self) -> None:
        layer_names = re.findall(r"^ {8}([A-Za-z_][A-Za-z0-9_]*)\s*\{", _node_body(self.keymap, "keymap"), re.MULTILINE)
        self.assertEqual(layer_names.index("gesture_layer"), 9)
        bindings = re.search(r"bindings\s*=\s*<(.*?)>;", _node_body(self.keymap, "gesture_layer"), re.DOTALL)
        self.assertIsNotNone(bindings)
        gesture_bindings = re.findall(r"&(?:none|kp(?:\s+[^&\s]+)*)", bindings.group(1))
        self.assertEqual(len(gesture_bindings), 43)
        self.assertEqual(gesture_bindings[7], "&kp LC(DOWN)")
        self.assertEqual(gesture_bindings[18], "&kp LC(RIGHT)")
        self.assertEqual(gesture_bindings[20], "&kp LC(LEFT)")
        self.assertEqual(gesture_bindings[31], "&kp LC(UP)")
        self.assertTrue(all(binding == "&none" for index, binding in enumerate(gesture_bindings) if index not in {7, 18, 20, 31}))

    def test_i_and_o_toggle_the_gesture_processor(self) -> None:
        gesture_combo = _node_body(_node_body(self.keymap, "combos"), "gesture_toggle")
        self.assertIn("key-positions = <7 8>;", gesture_combo)
        self.assertIn("bindings = <&mouse_gesture_toggle>;", gesture_combo)

    def test_gesture_processor_routes_each_direction_to_the_reserved_slot(self) -> None:
        self.assertIn("#include <mouse-gesture.dtsi>", self.overlay)
        self.assertIn("#include <dt-bindings/zmk/mouse-gesture.h>", self.overlay)
        gesture = _node_body(self.overlay, "&zip_mouse_gesture")
        self.assertIn("suppress-movement;", gesture)
        self.assertIn("enable-eager-mode;", gesture)
        self.assertIn("partial-gesture-timeout-ms = <400>;", gesture)
        for direction, pattern, slot in (("up", "GESTURE_UP", 7), ("down", "GESTURE_DOWN", 31), ("left", "GESTURE_LEFT", 18), ("right", "GESTURE_RIGHT", 20)):
            direction_node = _node_body(gesture, direction)
            self.assertIn(f"pattern = <{pattern}>;", direction_node)
            self.assertIn(f"bindings = <&gesture_slot {slot}>;", direction_node)
        listener = _node_body(self.overlay, "trackball_listener")
        self.assertIn("input-processors = <&zip_mouse_gesture &mouse_runtime_input_processor>;", listener)
        self.assertIn("input-processors = <&zip_mouse_gesture &zip_xy_to_scroll_mapper &scroll_runtime_input_processor>;", _node_body(listener, "scroll_layer"))

    def test_gesture_slot_adapter_has_the_required_runtime_guards(self) -> None:
        source = SOURCE.read_text()
        self.assertRegex(source, r"#define\s+GESTURE_LAYER_INDEX\s+9")
        self.assertRegex(source, r"position\s*==\s*7.*position\s*==\s*18.*position\s*==\s*20.*position\s*==\s*31")
        self.assertIn("zmk_keymap_layer_index_to_id(GESTURE_LAYER_INDEX)", source)
        self.assertIn("ZMK_KEYMAP_LAYER_ID_INVAL", source)
        self.assertIn("if (!target)", source)
        self.assertIn("strcmp(target->behavior_dev, binding->behavior_dev) == 0", source)
        self.assertRegex(source, r"zmk_behavior_invoke_binding\(target,\s*event,\s*pressed\)")
        self.assertRegex(source, r"gesture_slot_pressed[\s\S]*invoke_slot\([^;]+,\s*true\)")
        self.assertRegex(source, r"gesture_slot_released[\s\S]*invoke_slot\([^;]+,\s*false\)")


if __name__ == "__main__": unittest.main()
