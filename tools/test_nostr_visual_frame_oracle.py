#!/usr/bin/env python3

import unittest

from nostr_visual_frame_oracle import (
    FrameOracleError,
    evaluate_layer,
    expected_frame,
    logical_direction,
)


class FrameSelectionOracleTests(unittest.TestCase):
    def test_all_eight_direction_displacements(self):
        vectors = (
            ((1, 1), 0), ((0, 1), 1), ((-1, 1), 2), ((-1, 0), 3),
            ((-1, -1), 4), ((0, -1), 5), ((1, -1), 6), ((1, 0), 7),
        )
        for vector, expected in vectors:
            with self.subTest(vector=vector):
                self.assertEqual(logical_direction((0, 0), vector, 8), expected)

    def test_all_sixteen_direction_slots_full_layout(self):
        for direction in range(16):
            for action_frame in (0, 2, 4):
                selected = expected_frame(
                    logical_direction_value=direction,
                    action_frame=action_frame,
                    frames_per_direction=5,
                    direction_count=16,
                    mirroring_mode=0,
                    physical_frame_count=80,
                )
                self.assertEqual(selected.stored_direction, direction)
                self.assertEqual(selected.physical_frame,
                                 direction * 5 + action_frame)
                self.assertFalse(selected.flip_horizontal)

    def test_all_eight_direction_slots_mirrored_layout(self):
        expected = (
            (2, True), (1, True), (0, False), (1, False),
            (2, False), (3, False), (4, False), (3, True),
        )
        for direction, (stored, flip) in enumerate(expected):
            for action_frame in (0, 2, 4):
                selected = expected_frame(
                    logical_direction_value=direction,
                    action_frame=action_frame,
                    frames_per_direction=5,
                    direction_count=8,
                    mirroring_mode=6,
                    physical_frame_count=25,
                )
                self.assertEqual(selected.stored_direction, stored)
                self.assertEqual(selected.physical_frame,
                                 stored * 5 + action_frame)
                self.assertEqual(selected.flip_horizontal, flip)

    def test_all_sixteen_direction_slots_mirrored_layout(self):
        expected = (
            (4, True), (3, True), (2, True), (1, True),
            (0, False), (1, False), (2, False), (3, False),
            (4, False), (5, False), (6, False), (7, False),
            (8, False), (7, True), (6, True), (5, True),
        )
        for direction, (stored, flip) in enumerate(expected):
            for action_frame in (0, 2, 4):
                selected = expected_frame(
                    logical_direction_value=direction,
                    action_frame=action_frame,
                    frames_per_direction=5,
                    direction_count=16,
                    mirroring_mode=12,
                    physical_frame_count=45,
                )
                self.assertEqual(selected.stored_direction, stored)
                self.assertEqual(selected.physical_frame,
                                 stored * 5 + action_frame)
                self.assertEqual(selected.flip_horizontal, flip)

    def test_two_direction_layout_uses_one_stored_block(self):
        left = expected_frame(logical_direction_value=0, action_frame=2,
                              frames_per_direction=4, direction_count=2,
                              mirroring_mode=1, physical_frame_count=4)
        right = expected_frame(logical_direction_value=1, action_frame=2,
                               frames_per_direction=4, direction_count=2,
                               mirroring_mode=1, physical_frame_count=4)
        self.assertEqual(left.physical_frame, right.physical_frame)
        self.assertFalse(left.flip_horizontal)
        self.assertTrue(right.flip_horizontal)

    def test_invalid_contract_inputs_fail_closed(self):
        valid = dict(logical_direction_value=0, action_frame=0,
                     frames_per_direction=5, direction_count=8,
                     mirroring_mode=6, physical_frame_count=25)
        mutations = (
            {"direction_count": 0}, {"frames_per_direction": 0},
            {"physical_frame_count": 0}, {"mirroring_mode": 8},
            {"mirroring_mode": 1},
            {"action_frame": 5}, {"logical_direction_value": 8},
            {"physical_frame_count": 24},
        )
        for mutation in mutations:
            with self.subTest(mutation=mutation), self.assertRaises(FrameOracleError):
                expected_frame(**(valid | mutation))

    def test_frame_block_flip_and_direction_mutations_fail(self):
        base = dict(previous=(10, 10), current=(11, 11), direction_count=8,
                    frames_per_direction=5, physical_frame_count=25,
                    mirroring_mode=6, action_frame=2)
        correct = evaluate_layer(**base, actual_frame=12,
                                 actual_flip_horizontal=True)
        self.assertEqual(correct["verdict"], "PASS")
        for mutation in (
            {"actual_frame": 17, "actual_flip_horizontal": True},
            {"actual_frame": 12, "actual_flip_horizontal": False},
        ):
            self.assertEqual(evaluate_layer(**base, **mutation)["verdict"],
                             "FAIL")
        self.assertEqual(evaluate_layer(
            **base, actual_frame=12, actual_flip_horizontal=True,
            actual_stored_direction=3,
        )["verdict"], "FAIL")
        for offset in range(1, 8):
            wrong = expected_frame(
                logical_direction_value=offset, action_frame=2,
                frames_per_direction=5, direction_count=8,
                mirroring_mode=6, physical_frame_count=25,
            )
            if (wrong.physical_frame, wrong.flip_horizontal) != (12, True):
                self.assertEqual(evaluate_layer(
                    **base, actual_frame=wrong.physical_frame,
                    actual_flip_horizontal=wrong.flip_horizontal,
                )["verdict"], "FAIL")


if __name__ == "__main__":
    unittest.main()
