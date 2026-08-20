import json
import os

import unreal


SOURCE_MAP = "/Game/Maps/Lvl_Game_AngledRoll"
TARGET_MAP = "/Game/Maps/Lvl_Game_TrajectoryThrows"
OUTPUT_PATH = os.path.join(
    unreal.Paths.project_saved_dir(), "Diagnostics", "TrajectoryDiceSceneValidation.json"
)


def object_path(value):
    return value.get_path_name() if value else None


def find_game_camera(actors):
    cameras = [
        actor
        for actor in actors
        if isinstance(actor, unreal.CameraActor)
        and actor.get_actor_label() == "GameCamera"
    ]
    if len(cameras) != 1:
        raise RuntimeError(f"Expected one GameCamera, found {len(cameras)}")
    return cameras[0]


def capture_solo_camera():
    world = unreal.EditorLoadingAndSavingUtils.load_map(SOURCE_MAP)
    if world is None:
        raise RuntimeError("Failed to load the solo roll map")
    actors = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    ).get_all_level_actors()
    camera = find_game_camera(actors)
    location = camera.get_actor_location()
    rotation = camera.get_actor_rotation()
    fov = camera.get_editor_property("camera_component").get_editor_property(
        "field_of_view"
    )
    return (
        unreal.Vector(location.x, location.y, location.z),
        unreal.Rotator(rotation.roll, rotation.pitch, rotation.yaw),
        fov,
    )


def duplicate_scene_if_needed():
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET_MAP):
        return
    duplicated = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP, TARGET_MAP)
    if duplicated is None:
        raise RuntimeError("Failed to duplicate the solo roll map")
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        duplicated, only_if_is_dirty=False
    ):
        raise RuntimeError("Failed to save the duplicated trajectory map")
    duplicated = None
    unreal.SystemLibrary.collect_garbage()


def configure_scene(camera_location, camera_rotation, camera_fov):
    world = unreal.EditorLoadingAndSavingUtils.load_map(TARGET_MAP)
    if world is None:
        raise RuntimeError("Failed to load the trajectory map")
    game_mode = unreal.load_class(
        None, "/Script/Daiso_DNG.TrajectoryDiceGameMode"
    )
    if game_mode is None:
        raise RuntimeError("TrajectoryDiceGameMode was not loaded")
    world.get_world_settings().set_editor_property("default_game_mode", game_mode)

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors()
    camera = find_game_camera(actors)
    camera.set_actor_location(camera_location, False, False)
    camera.set_actor_rotation(camera_rotation, False)
    camera.set_editor_property("auto_activate_for_player", unreal.AutoReceiveInput.PLAYER0)
    camera.get_editor_property("camera_component").set_editor_property(
        "field_of_view", camera_fov
    )

    # The new HUD owns scoring output. Remove the old diagnostic collector so it
    # cannot mix partial manual throws across batches or print duplicate scores.
    for actor in list(actors):
        actor_class_path = actor.get_class().get_path_name()
        if (
            "DiceScoreCollector" in actor_class_path
            or "TurnManager" in actor_class_path
            or "DiceEnemyAI" in actor_class_path
        ):
            actor_subsystem.destroy_actor(actor)

    if not unreal.EditorAssetLibrary.save_loaded_asset(world, only_if_is_dirty=False):
        raise RuntimeError("Failed to save Lvl_Game_TrajectoryThrows")
    return world


def validate_scene(world, expected_location, expected_rotation, expected_fov):
    actors = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    ).get_all_level_actors()
    camera = find_game_camera(actors)
    actual_location = camera.get_actor_location()
    actual_rotation = camera.get_actor_rotation()
    actual_fov = camera.get_editor_property("camera_component").get_editor_property(
        "field_of_view"
    )
    dice = [
        actor
        for actor in actors
        if actor.get_class()
        .get_path_name()
        .endswith("BP_Dice_Basic.BP_Dice_Basic_C")
    ]
    collectors = [
        actor
        for actor in actors
        if "DiceScoreCollector" in actor.get_class().get_path_name()
    ]
    turn_managers = [
        actor
        for actor in actors
        if "TurnManager" in actor.get_class().get_path_name()
    ]
    enemy_dice = [
        actor
        for actor in actors
        if "DiceEnemyAI" in actor.get_class().get_path_name()
    ]
    report = {
        "map": TARGET_MAP,
        "source_camera_map": SOURCE_MAP,
        "game_mode": object_path(
            world.get_world_settings().get_editor_property("default_game_mode")
        ),
        "dice_count": len(dice),
        "camera_location": str(actual_location),
        "camera_rotation": str(actual_rotation),
        "camera_fov": actual_fov,
        "legacy_score_collectors": len(collectors),
        "legacy_turn_managers": len(turn_managers),
        "legacy_enemy_dice": len(enemy_dice),
        "errors": [],
    }
    if report["game_mode"] != "/Script/Daiso_DNG.TrajectoryDiceGameMode":
        report["errors"].append("Unexpected GameMode")
    if len(dice) != 6:
        report["errors"].append("Expected the original six solo-roll dice")
    if unreal.MathLibrary.vector_distance(actual_location, expected_location) > 0.01:
        report["errors"].append("Camera location differs from the solo-roll scene")
    if max(
        abs(actual_rotation.pitch - expected_rotation.pitch),
        abs(actual_rotation.yaw - expected_rotation.yaw),
        abs(actual_rotation.roll - expected_rotation.roll),
    ) > 0.01:
        report["errors"].append("Camera rotation differs from the solo-roll scene")
    if abs(actual_fov - expected_fov) > 0.01:
        report["errors"].append("Camera FOV differs from the solo-roll scene")
    if collectors:
        report["errors"].append("Legacy score collector was not removed")
    if turn_managers:
        report["errors"].append("Legacy turn manager was not removed")
    if enemy_dice:
        report["errors"].append("Legacy enemy dice was not removed")

    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as output:
        json.dump(report, output, ensure_ascii=False, indent=2)
    unreal.log("TRAJECTORY_DICE_SCENE_VALIDATION=" + OUTPUT_PATH)
    if report["errors"]:
        raise RuntimeError("; ".join(report["errors"]))


solo_location, solo_rotation, solo_fov = capture_solo_camera()
duplicate_scene_if_needed()
trajectory_world = configure_scene(solo_location, solo_rotation, solo_fov)
validate_scene(trajectory_world, solo_location, solo_rotation, solo_fov)
