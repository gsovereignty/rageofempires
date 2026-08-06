#!/usr/bin/env python3
"""Join represented DAT animation records to exact live SLP frame counts."""

import argparse
import importlib.util
import json
import struct
from collections import Counter
from pathlib import Path


ROLES = (
    "standing_graphic", "standing_graphic_2", "walking_graphic",
    "running_graphic", "attack_graphic", "dying_graphic",
    "construction_graphic",
)

CPP_ROLES = {
    "standing_graphic": "standing",
    "standing_graphic_2": "alternate_standing",
    "walking_graphic": "walking",
    "running_graphic": "running",
    "attack_graphic": "attack",
    "dying_graphic": "dying",
    "construction_graphic": "construction",
}

# The original task-bearing Builder and Repairer master objects supply work
# graphics for the native Villager's build/repair action states. These are
# task fields, not direct graphic roles on Villager object 83.
TASK_ROLE_SOURCES = (
    {
        "target_category": "unit", "target_name": "villager",
        "role": "construction", "source_dat_id": 118,
        "action_type": 101, "graphic_field": "work_graphic",
    },
    {
        "target_category": "unit", "target_name": "villager",
        "role": "repair", "source_dat_id": 156,
        "action_type": 106, "graphic_field": "work_graphic",
    },
)


def mapping_module(path):
    spec = importlib.util.spec_from_file_location("civ_matrix", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def drs_slp_counts(path):
    data = Path(path).read_bytes()
    if len(data) < 64 or data[40:42] != b"1.":
        raise ValueError("invalid DRS")
    result = {}
    table_count = struct.unpack_from("<i", data, 56)[0]
    for table in range(table_count):
        offset = 64 + table * 12
        extension = data[offset:offset + 4][::-1].decode(
            "ascii", errors="strict"
        ).strip("\0 ").lower()
        info, count = struct.unpack_from("<ii", data, offset + 4)
        for entry in range(count):
            entry_offset = info + entry * 12
            if entry_offset < 0 or entry_offset + 12 > len(data):
                raise ValueError("DRS entry outside archive")
            resource_id, payload, size = struct.unpack_from(
                "<iii", data, entry_offset
            )
            if payload < 0 or size < 0 or payload + size > len(data):
                raise ValueError("DRS payload outside archive")
            if extension == "slp":
                if size < 8:
                    raise ValueError("short SLP")
                result[resource_id] = struct.unpack_from(
                    "<I", data, payload + 4
                )[0]
    return result


def graphic_record(graphic_id, graphics, slps):
    if graphic_id is None:
        return {
            "graphic_id": None,
            "classification": "absent",
        }
    graphic = graphics.get(graphic_id)
    if graphic is None:
        return {
            "graphic_id": graphic_id,
            "classification": "ambiguous",
            "reason": "referenced DAT graphic record absent",
        }
    slp_id = graphic["slp_id"]
    if slp_id is None:
        return {
            "graphic_id": graphic_id,
            "classification": "absent",
            "reason": "DAT graphic has no SLP resource ID",
            "field_classification": "exact_dat",
        }
    physical = slps.get(slp_id)
    frames = graphic["frame_count"]
    angles = graphic["angle_count"]
    full_count = frames * angles
    mirrored_count = frames * (angles // 2 + 1)
    count_matches = (
        physical == full_count or
        (graphic["mirror_flag"] != 0 and physical == mirrored_count)
    )
    classification = (
        "exact" if physical is not None and count_matches
        else "ambiguous"
    )
    reason = None
    if physical is None:
        reason = "SLP absent from supplied graphics archive"
    elif not count_matches:
        reason = (
            "physical SLP count differs from both full and mirrored "
            "DAT frame layouts"
        )
    return {
        "graphic_id": graphic_id,
        "classification": classification,
        "reason": reason,
        "name": graphic["name"],
        "filename": graphic["filename"],
        "slp_id": slp_id,
        "dat_frames_per_angle": graphic["frame_count"],
        "dat_angle_count": graphic["angle_count"],
        "physical_slp_frame_count": physical,
        "physical_stored_angle_count": (
            physical // frames
            if physical is not None and frames > 0 and physical % frames == 0
            else None
        ),
        "dat_frame_duration": graphic["frame_rate"],
        "dat_replay_delay": graphic["replay_delay"],
        "dat_sequence_type": graphic["sequence_type"],
        "dat_mirroring_mode": graphic["mirror_flag"],
        "dat_layer": graphic["layer"],
        "deltas": graphic["deltas"],
        "field_classification": "exact_dat",
        "duration_unit": "seconds_per_frame",
        "duration_unit_classification": "exact_openage_dat_schema",
    }


def represented_record(category, name, dat_id, unit, graphics, slps):
    roles = {
        role: graphic_record(unit.get(role), graphics, slps)
        for role in ROLES
    }
    attack = roles["attack_graphic"]
    timing_class = (
        "exact" if unit.get("attack_graphic") is not None else "absent"
    )
    return {
        "category": category,
        "name": name,
        "dat_id": dat_id,
        "animations": roles,
        "action_timing": {
            "attack_graphic_id": unit.get("attack_graphic"),
            "attack_frame_delay": unit["combat"]["frame_delay"],
            "reload_time": unit["combat"]["reload_time"],
            "classification": timing_class,
            "synchronization_contract": (
                "DAT attack frame delay and reload time are exact fields; "
                "executable-to-render-tick scheduling is unproved"
            ),
            "runtime_tick_mapping_classification": "ambiguous",
        },
    }


def task_role_records(content_catalog, graphics, slps, mapping):
    records = []
    variants = content_catalog["object_variants"]
    for source in TASK_ROLE_SOURCES:
        values = {
            task[source["graphic_field"]]
            for variant in variants
            if variant["id"] == source["source_dat_id"]
            for task in variant["action"]["tasks"]
            if task["action_type"] == source["action_type"] and
               task[source["graphic_field"]] is not None
        }
        if len(values) != 1:
            raise ValueError(
                "task graphic identity is not unique for "
                f"DAT object {source['source_dat_id']} role {source['role']}"
            )
        graphic_id = values.pop()
        animation = graphic_record(graphic_id, graphics, slps)
        if animation["classification"] != "exact":
            raise ValueError(
                "task role lacks exact live layout for "
                f"DAT object {source['source_dat_id']} role {source['role']}"
            )
        records.append({
            **source,
            "target_dat_id": mapping.UNIT[source["target_name"]][0],
            "animation": animation,
        })
    return records


def make_catalog(metadata, graphics_drs, mapping_path, content_catalog):
    mapping = mapping_module(mapping_path)
    units = {
        item["id"]: item for item in metadata["civilizations"][0]["units"]
    }
    graphics = {item["id"]: item for item in metadata["graphics"]}
    slps = drs_slp_counts(graphics_drs)
    records = []
    for name, (dat_id, _gate) in mapping.UNIT.items():
        records.append(represented_record(
            "unit", name, dat_id, units[dat_id], graphics, slps
        ))
    for name, (dat_id, _gate) in mapping.BUILDING.items():
        records.append(represented_record(
            "building", name, dat_id, units[dat_id], graphics, slps
        ))

    classifications = Counter(
        animation["classification"]
        for record in records
        for animation in record["animations"].values()
    )
    action_classifications = Counter(
        record["action_timing"]["classification"] for record in records
    )
    task_roles = task_role_records(
        content_catalog, graphics, slps, mapping
    )
    return {
        "schema": "aoe-animation-evidence-v1",
        "source": metadata["source"],
        "scope": {"units": len(mapping.UNIT), "buildings": len(mapping.BUILDING)},
        "summary": {
            "represented_record_count": len(records),
            "role_record_count": len(records) * len(ROLES),
            "animation_classifications": dict(sorted(classifications.items())),
            "action_timing_classifications": dict(
                sorted(action_classifications.items())
            ),
        },
        "evidence_contract": {
            "dat_fields": "exact",
            "physical_slp_frame_count": "exact archive header",
            "frame_duration_unit": "exact openage DAT schema",
            "runtime_tick_mapping": "ambiguous",
            "executable_action_synchronization": "absent",
            "note": (
                "No cadence is derived when DAT/SLP identity disagrees or "
                "runtime scheduling is not proved"
            ),
        },
        "runtime_exact_roles": {
            f"{record['category']}:{record['name']}": [
                role for role, animation in record["animations"].items()
                if animation["classification"] == "exact"
            ]
            for record in records
        },
        "runtime_exact_task_roles": task_roles,
        "records": records,
    }


def cpp_number(value):
    if isinstance(value, float):
        return format(value, ".17g")
    return str(value)


def write_runtime_catalog(catalog, destination, mapping_path):
    mapping = mapping_module(mapping_path)
    unit_ids = dict(mapping.UNIT)
    building_ids = dict(mapping.BUILDING)
    lines = [
        "// Generated by tools/dat_metadata/generate_animation_evidence.py.",
        "// Exact means DAT role identity plus a matching live SLP layout.",
        "constexpr ExactRoleBinding generated_exact_role_bindings[] = {",
    ]
    for record in catalog["records"]:
        category = record["category"]
        for role, animation in record["animations"].items():
            if animation["classification"] != "exact":
                continue
            fields = (
                animation["graphic_id"], animation["slp_id"],
                animation["dat_frames_per_angle"],
                animation["dat_angle_count"],
                animation["physical_slp_frame_count"],
                animation["dat_frame_duration"],
                animation["dat_replay_delay"],
                animation["dat_mirroring_mode"],
                animation["dat_sequence_type"],
                animation["dat_layer"],
            )
            layout = (
                "mirrored"
                if animation["physical_stored_angle_count"] !=
                   animation["dat_angle_count"]
                else "full"
            )
            lines.append(
                "    {ObjectCategory::%s, %d, Role::%s, "
                "{%s, Layout::%s}}," % (
                    category, record["dat_id"], CPP_ROLES[role],
                    ", ".join(cpp_number(value) for value in fields),
                    layout,
                )
            )
    for task_role in catalog["runtime_exact_task_roles"]:
        animation = task_role["animation"]
        fields = (
            animation["graphic_id"], animation["slp_id"],
            animation["dat_frames_per_angle"],
            animation["dat_angle_count"],
            animation["physical_slp_frame_count"],
            animation["dat_frame_duration"],
            animation["dat_replay_delay"],
            animation["dat_mirroring_mode"],
            animation["dat_sequence_type"],
            animation["dat_layer"],
        )
        layout = (
            "mirrored"
            if animation["physical_stored_angle_count"] !=
               animation["dat_angle_count"]
            else "full"
        )
        lines.append(
            "    {ObjectCategory::%s, %d, Role::%s, "
            "{%s, Layout::%s}}," % (
                task_role["target_category"],
                task_role["target_dat_id"], task_role["role"],
                ", ".join(cpp_number(value) for value in fields),
                layout,
            )
        )
    lines.extend([
        "};",
        "",
        "constexpr UnitDatId generated_unit_dat_ids[] = {",
    ])
    for name, (dat_id, _gate) in unit_ids.items():
        lines.append(f"    {{UnitKind::{name}, {dat_id}}},")
    lines.extend([
        "};",
        "",
        "constexpr BuildingDatId generated_building_dat_ids[] = {",
    ])
    for name, (dat_id, _gate) in building_ids.items():
        lines.append(f"    {{BuildingKind::{name}, {dat_id}}},")
    lines.extend(["};", ""])
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("\n".join(lines))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata")
    parser.add_argument("graphics_drs")
    parser.add_argument(
        "--mapping", default="tools/dat_metadata/generate_civ_matrix.py"
    )
    parser.add_argument(
        "--output", default="generated/animation_evidence.json"
    )
    parser.add_argument(
        "--runtime-output",
        default="generated/animation_binding_catalog.inc",
    )
    parser.add_argument(
        "--content-catalog", default="generated/content_catalog.json"
    )
    args = parser.parse_args()
    metadata = json.loads(Path(args.metadata).read_text())
    content_catalog = json.loads(Path(args.content_catalog).read_text())
    result = make_catalog(
        metadata, Path(args.graphics_drs), Path(args.mapping), content_catalog
    )
    Path(args.output).write_text(json.dumps(result, indent=2) + "\n")
    write_runtime_catalog(
        result, Path(args.runtime_output), Path(args.mapping)
    )


if __name__ == "__main__":
    main()
