#![recursion_limit = "256"]

use genie_dat::DatFile;
use serde_json::{json, Value};
use std::{env, fs::File, process};

fn sprite_id(id: Option<impl Into<u16>>) -> Option<u16> {
    id.map(Into::into)
}

fn effect_attribute_name(attribute: i16) -> Option<&'static str> {
    match attribute {
        0 => Some("hit_points"),
        1 => Some("line_of_sight"),
        5 => Some("movement_speed"),
        8 => Some("armor"),
        9 => Some("attack"),
        10 => Some("reload_time"),
        12 => Some("maximum_range"),
        13 => Some("work_rate"),
        22 => Some("blast_width"),
        23 => Some("search_radius"),
        _ => None,
    }
}

fn packed_attack(value: f32) -> Option<(i16, i16)> {
    let raw = value as i16 as u16;
    Some((
        (raw >> 8) as u8 as i8 as i16,
        (raw & 0xff) as u8 as i8 as i16,
    ))
}

fn main() {
    let path = env::args().nth(1).unwrap_or_else(|| {
        eprintln!("usage: aoe-dat-metadata EMPIRES2_X1_P1.DAT");
        process::exit(2);
    });
    let dat = DatFile::read_from(File::open(&path).unwrap_or_else(|error| {
        eprintln!("{path}: {error}");
        process::exit(2);
    }))
    .unwrap_or_else(|error| {
        eprintln!("cannot parse {path}: {error}");
        process::exit(2);
    });

    if dat.effects.len() != 514 || dat.civilizations.len() != 19 {
        eprintln!(
            "unexpected VER 5.7 profile: effects={}, civilizations={}",
            dat.effects.len(),
            dat.civilizations.len()
        );
        process::exit(1);
    }

    let effects: Vec<Value> = dat
        .effects
        .iter()
        .enumerate()
        .map(|(id, effect)| {
            let commands: Vec<Value> = effect
                .commands
                .iter()
                .map(|command| {
                    let packed_combat = (command.command_type == 4
                        && matches!(command.params.2, 8 | 9))
                    .then(|| packed_attack(command.params.3))
                    .flatten();
                    let attack = (command.params.2 == 9).then_some(packed_combat).flatten();
                    json!({
                        "type": command.command_type,
                        "a": command.params.0,
                        "b": command.params.1,
                        "c": command.params.2,
                        "d": command.params.3,
                        "attribute_name": effect_attribute_name(command.params.2),
                        "packed_class": packed_combat.map(|value| value.0),
                        "packed_amount": packed_combat.map(|value| value.1),
                        "packed_attack_class": attack.map(|value| value.0),
                        "packed_attack_amount": attack.map(|value| value.1)
                    })
                })
                .collect();
            json!({
                "id": id,
                "name": effect.name(),
                "command_count": commands.len(),
                "commands": commands
            })
        })
        .collect();
    let techs: Vec<Value> = dat
        .techs
        .iter()
        .enumerate()
        .map(|(id, tech)| {
            json!({
                "id": id,
                "name": tech.name(),
                "record": format!("{:?}", tech)
            })
        })
        .collect();

    let mut civilizations = Vec::new();
    for (civilization_id, civilization) in dat.civilizations.iter().enumerate() {
        let mut units = Vec::new();
        for unit_id in 0u16..10_000 {
            let Some(unit) = civilization.get_unit_type(unit_id) else {
                continue;
            };
            let standing = sprite_id(unit.static_.standing_sprite_1);
            let standing_2 = sprite_id(unit.static_.standing_sprite_2);
            let dying = sprite_id(unit.static_.dying_sprite);
            let walking = unit
                .moving
                .as_ref()
                .and_then(|moving| sprite_id(moving.move_sprite));
            let running = unit
                .moving
                .as_ref()
                .and_then(|moving| sprite_id(moving.run_sprite));
            let attack = unit
                .base_combat
                .as_ref()
                .and_then(|combat| sprite_id(combat.fight_sprite));
            let combat_stats = unit.base_combat.as_ref().map(|combat| {
                json!({
                    "armor": combat.base_armor,
                    "weapons": combat.weapons.iter().map(|weapon| json!({
                        "class": weapon.weapon_type,
                        "amount": weapon.value
                    })).collect::<Vec<_>>(),
                    "armors": combat.armors.iter().map(|armor| json!({
                        "class": armor.weapon_type,
                        "amount": armor.value
                    })).collect::<Vec<_>>(),
                    "range_max": combat.weapon_range_max,
                    "range_min": combat.weapon_range_min,
                    "area_effect_range": combat.area_effect_range,
                    "blast_level_offense": combat.blast_level_offense,
                    "reload_time": combat.attack_speed,
                    "missile_unit": combat.missile_id.map(Into::<u16>::into),
                    "accuracy": combat.base_hit_chance,
                    "break_off_combat": combat.break_off_combat,
                    "frame_delay": combat.frame_delay,
                    "weapon_offset": [
                        combat.weapon_offset.0,
                        combat.weapon_offset.1,
                        combat.weapon_offset.2
                    ],
                    "missed_missile_spread": combat.missed_missile_spread,
                    "displayed_attack": combat.displayed_attack,
                    "displayed_armor": combat.displayed_armor,
                    "displayed_range": combat.displayed_range,
                    "displayed_reload_time": combat.displayed_reload_time
                })
            });
            let missile_stats = unit.missile.as_ref().map(|missile| {
                json!({
                    "missile_type": missile.missile_type,
                    "targetting_type": missile.targetting_type,
                    "missile_hit_info": missile.missile_hit_info,
                    "missile_die_info": missile.missile_die_info,
                    "area_effect_specials": missile.area_effect_specials,
                    "ballistics_ratio": missile.ballistics_ratio
                })
            });
            let creation = unit.combat.as_ref().map(|combat| {
                json!({
                    "costs": combat.costs.iter().map(|cost| json!({
                        "resource": cost.attribute_type,
                        "amount": cost.amount,
                        "flag": cost.flag
                    })).collect::<Vec<_>>(),
                    "create_time": combat.create_time,
                    "create_at_unit": combat.create_at_building.map(Into::<u16>::into),
                    "create_button": combat.create_button,
                    "rear_attack_modifier": combat.rear_attack_modifier,
                    "flank_attack_modifier": combat.flank_attack_modifier,
                    "volley_fire_amount": combat.volley_fire_amount,
                    "max_attacks_in_volley": combat.max_attacks_in_volley,
                    "volley_spread": [combat.volley_spread.0, combat.volley_spread.1],
                    "volley_start_spread_adjustment": combat.volley_start_spread_adjustment,
                    "volley_missile_unit": combat.volley_missile.map(Into::<u16>::into),
                    "special_attack_graphic": sprite_id(combat.special_attack_sprite),
                    "special_attack_flag": combat.special_attack_flag
                })
            });
            let construction = unit
                .building
                .as_ref()
                .and_then(|building| sprite_id(building.construction_sprite));
            let building_stats = unit.building.as_ref().map(|building| {
                json!({
                    "garrison_type": building.garrison_type,
                    "garrison_heal_rate": building.garrison_heal_rate,
                    "garrison_repair_rate": building.garrison_repair_rate
                })
            });
            let action = unit.action.as_ref().map(|action| {
                let global_tasks = dat
                    .task_lists
                    .get(usize::from(unit.static_.copy_id))
                    .and_then(Option::as_ref);
                json!({
                    "default_task": action.default_task,
                    "search_radius": action.search_radius,
                    "work_rate": action.work_rate,
                    "command_sound": action.command_sound.map(Into::<u16>::into),
                    "move_sound": action.move_sound.map(Into::<u16>::into),
                    "tasks": format!("{:?}", action.tasks.as_ref().or(global_tasks)),
                    "task_list_source": if action.tasks.is_some() {
                        "unit"
                    } else if global_tasks.is_some() {
                        "global_copy_id"
                    } else {
                        "none"
                    }
                })
            });
            let moving_stats = unit.moving.as_ref().map(|moving| {
                json!({
                    "turn_speed": moving.turn_speed,
                    "size_class": moving.size_class,
                    "trailing_unit": moving.trailing_unit.map(Into::<u16>::into),
                    "trailing_options": moving.trailing_options,
                    "trailing_spacing": moving.trailing_spacing,
                    "move_algorithm": moving.move_algorithm,
                    "turn_radius": moving.turn_radius,
                    "turn_radius_speed": moving.turn_radius_speed,
                    "maximum_yaw_per_second_moving":
                        moving.maximum_yaw_per_second_moving,
                    "stationary_yaw_revolution_time":
                        moving.stationary_yaw_revolution_time,
                    "maximum_yaw_per_second_stationary":
                        moving.maximum_yaw_per_second_stationary
                })
            });

            units.push(json!({
                "id": unit_id,
                "base_class": format!("{:?}", unit.unit_base_class),
                "unit_class": unit.static_.unit_class,
                "language_dll_name": format!("{:?}", unit.static_.string_id),
                "language_dll_help": format!("{:?}", unit.static_.help_string_id),
                "button_icon": unit.static_.button_picture.map(|id| u32::try_from(id).unwrap()),
                "portrait_icon": unit.static_.portrait_picture.map(|id| u32::try_from(id).unwrap()),
                "enabled": unit.static_.enabled,
                "disabled": unit.static_.disabled,
                "hit_points": unit.static_.hp,
                "line_of_sight": unit.static_.los,
                "garrison_capacity": unit.static_.garrison_capacity,
                "area_effect_level": unit.static_.area_effect_level,
                "radius": [
                    unit.static_.radius.0,
                    unit.static_.radius.1,
                    unit.static_.radius.2
                ],
                "outline_radius": [
                    unit.static_.outline_radius.0,
                    unit.static_.outline_radius.1,
                    unit.static_.outline_radius.2
                ],
                "obstruction_type": unit.static_.obstruction_type,
                "selection_shape": unit.static_.selection_shape,
                "unit_group": unit.static_.unit_group,
                "speed": unit.animated.as_ref().map(|animated| animated.speed),
                "moving": moving_stats,
                "standing_graphic": standing,
                "standing_graphic_2": standing_2,
                "dying_graphic": dying,
                "walking_graphic": walking,
                "running_graphic": running,
                "attack_graphic": attack,
                "action": action,
                "combat": combat_stats,
                "missile": missile_stats,
                "creation": creation,
                "construction_graphic": construction,
                "building": building_stats,
                "track_as_resource": unit.static_.track_as_resource,
                "resource_group": unit.static_.resource_group,
                "attributes": unit.static_.attributes.iter().map(|attribute| json!({
                    "type": attribute.attribute_type,
                    "amount": attribute.amount,
                    "flag": attribute.flag
                })).collect::<Vec<_>>(),
                "copy_id": unit.static_.copy_id
                ,"terrain_restriction_id": unit.static_.terrain_restriction_id
                ,"attribute_max_amount": unit.static_.attribute_max_amount
                ,"attribute_rot": unit.static_.attribute_rot
                ,"train_sound": unit.static_.train_sound.map(Into::<u16>::into)
                ,"damage_sound": unit.static_.damage_sound.map(Into::<u16>::into)
                ,"selected_sound": unit.static_.selected_sound.map(Into::<u16>::into)
                ,"death_sound": unit.static_.death_sound.map(Into::<u16>::into)
                ,"damage_sprites": unit.static_.damage_sprites.iter().map(|damage| json!({
                    "graphic_id": Into::<u16>::into(damage.sprite),
                    "damage_percent": damage.damage_percent,
                    "flag": damage.flag
                })).collect::<Vec<_>>()
            }));
        }
        civilizations.push(json!({
            "id": civilization_id,
            "name": civilization.name(),
            "record": format!("{:?}", civilization),
            "units": units
        }));
    }
    let sounds: Vec<Value> = dat
        .sounds
        .iter()
        .map(|sound| {
            json!({
                "id": Into::<u16>::into(sound.id),
                "play_delay": sound.play_delay,
                "items": sound.items.iter().map(|item| json!({
                    "filename": item.filename.as_str(),
                    "resource_id": item.resource_id,
                    "probability": item.probability,
                    "civilization": item.civilization,
                    "icon_set": item.icon_set
                })).collect::<Vec<_>>()
            })
        })
        .collect();
    let sprite_sounds: Vec<Value> = dat
        .sprites
        .iter()
        .enumerate()
        .filter_map(|(id, sprite)| {
            let sprite = sprite.as_ref()?;
            if sprite.sound_id.is_none() && sprite.attack_sounds.is_empty() {
                return None;
            }
            Some(json!({
                "graphic_id": id,
                "sound_id": sprite.sound_id.map(Into::<u16>::into),
                "attack_sounds": sprite.attack_sounds.iter().map(|angle| {
                    angle.sound_props.iter().map(|sound| json!({
                        "delay": sound.sound_delay,
                        "sound_id": Into::<u16>::into(sound.sound_id)
                    })).collect::<Vec<_>>()
                }).collect::<Vec<_>>()
            }))
        })
        .collect();
    let graphics: Vec<Value> = dat
        .sprites
        .iter()
        .enumerate()
        .filter_map(|(id, sprite)| {
            let sprite = sprite.as_ref()?;
            Some(json!({
                "id": id,
                "name": sprite.name,
                "filename": sprite.filename,
                "slp_id": sprite.slp_id.map(|value| u32::try_from(value).unwrap()),
                "layer": sprite.layer,
                "palette": sprite.color_table,
                "frame_count": sprite.num_frames,
                "angle_count": sprite.num_angles,
                "frame_rate": sprite.frame_rate,
                "replay_delay": sprite.replay_delay,
                "sequence_type": sprite.sequence_type,
                "mirror_flag": sprite.mirror_flag,
                "deltas": sprite.deltas.iter().map(|delta| json!({
                    "graphic_id": delta.sprite_id.map(Into::<u16>::into),
                    "offset_x": delta.offset_x,
                    "offset_y": delta.offset_y,
                    "display_angle": delta.display_angle
                })).collect::<Vec<_>>()
            }))
        })
        .collect();
    let terrain_restrictions: Vec<Value> = dat
        .terrain_tables
        .iter()
        .enumerate()
        .map(|(id, restriction)| {
            json!({
                "id": id,
                "record": format!("{:?}", restriction)
            })
        })
        .collect();
    let terrains: Vec<Value> = dat
        .terrains
        .iter()
        .enumerate()
        .map(|(id, terrain)| {
            json!({
                "id": id,
                "record": format!("{:?}", terrain),
                "enabled": terrain.enabled,
                "slp_id": terrain.slp_id.map(|value| u32::try_from(value).unwrap()),
                "sound_id": terrain.sound_id.map(Into::<u16>::into),
                "passable_terrain_id": terrain.passable_terrain_id,
                "impassable_terrain_id": terrain.impassable_terrain_id,
                "terrain_objects": terrain.terrain_objects.iter().map(|object| json!({
                    "unit_id": Into::<u16>::into(object.object_id),
                    "density": object.density,
                    "placement_flag": object.placement_flag
                })).collect::<Vec<_>>()
            })
        })
        .collect();
    let terrain_borders: Vec<Value> = dat
        .terrain_borders
        .iter()
        .enumerate()
        .map(|(id, border)| {
            json!({
                "id": id,
                "record": format!("{:?}", border),
                "enabled": border.enabled,
                "slp_id": border.slp_id.map(|value| u32::try_from(value).unwrap()),
                "sound_id": border.sound_id.map(Into::<u16>::into),
                "color": [border.color.0, border.color.1, border.color.2],
                "underlay_terrain": border.underlay_terrain,
                "border_style": border.border_style,
                "frames": border.frames.iter().map(|row| {
                    row.iter().map(|frame| json!({
                        "num_frames": frame.num_frames,
                        "num_facets": frame.num_facets,
                        "frame_id": frame.frame_id
                    })).collect::<Vec<_>>()
                }).collect::<Vec<_>>()
            })
        })
        .collect();
    let player_colors: Vec<Value> = dat
        .color_tables
        .iter()
        .map(|color| {
            json!({
                "id": color.id,
                "base_palette_index": Into::<u8>::into(color.base),
                "unit_outline_palette_index": Into::<u8>::into(color.unit_outline_color),
                "unit_selection_palette_indices": [
                    Into::<u8>::into(color.unit_selection_colors.0),
                    Into::<u8>::into(color.unit_selection_colors.1)
                ],
                "minimap_palette_indices": [
                    Into::<u8>::into(color.minimap_colors.0),
                    Into::<u8>::into(color.minimap_colors.1),
                    Into::<u8>::into(color.minimap_colors.2)
                ],
                "statistics_text_color": color.statistics_text_color
            })
        })
        .collect();

    println!(
        "{}",
        serde_json::to_string_pretty(&json!({
            "format": "AOE_VER_5_7_METADATA_V1",
            "source": path,
            "effect_count": effects.len(),
            "tech_count": techs.len(),
            "civilization_count": civilizations.len(),
            "effects": effects,
            "techs": techs,
            "sounds": sounds,
            "graphics": graphics,
            "terrain_restrictions": terrain_restrictions,
            "terrains": terrains,
            "terrain_borders": terrain_borders,
            "player_colors": player_colors,
            "sprite_sounds": sprite_sounds,
            "civilizations": civilizations
        }))
        .unwrap()
    );
}

#[cfg(test)]
mod tests {
    use super::{effect_attribute_name, packed_attack};

    #[test]
    fn names_unique_tech_effect_attributes() {
        assert_eq!(effect_attribute_name(0), Some("hit_points"));
        assert_eq!(effect_attribute_name(1), Some("line_of_sight"));
        assert_eq!(effect_attribute_name(5), Some("movement_speed"));
        assert_eq!(effect_attribute_name(8), Some("armor"));
        assert_eq!(effect_attribute_name(9), Some("attack"));
        assert_eq!(effect_attribute_name(10), Some("reload_time"));
        assert_eq!(effect_attribute_name(12), Some("maximum_range"));
        assert_eq!(effect_attribute_name(13), Some("work_rate"));
        assert_eq!(effect_attribute_name(22), Some("blast_width"));
        assert_eq!(effect_attribute_name(23), Some("search_radius"));
        assert_eq!(effect_attribute_name(99), None);
    }

    #[test]
    fn decodes_genie_packed_attack_class_and_amount() {
        assert_eq!(packed_attack(770.0), Some((3, 2)));
        assert_eq!(packed_attack(769.0), Some((3, 1)));
        assert_eq!(packed_attack(262.0), Some((1, 6)));
        assert_eq!(packed_attack(-226.0), Some((-1, 30)));
    }
}
