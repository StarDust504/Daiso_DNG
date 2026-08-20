import json
import os

import unreal


SOURCE_WIDGET = "/Game/Widgets/HUD/W_AngledRollOnly"
TARGET_WIDGET = "/Game/Widgets/HUD/W_NaturalRollOnly"
TARGET_MAP = "/Game/Maps/Lvl_Game_AngledRoll"
OUTPUT_PATH = os.path.join(
    unreal.Paths.project_saved_dir(), "Diagnostics", "NaturalRollModeValidation.json"
)


def object_path(value):
    return value.get_path_name() if value else None


def graph_titles(blueprint):
    titles = []
    for graph in unreal.BlueprintEditorLibrary.list_graphs(blueprint):
        editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
        if editor:
            for node in editor.list_all_nodes():
                titles.append(unreal.BlueprintEditorLibrary.get_node_title(node))
    return titles


def create_widget_copy():
    widget = None
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET_WIDGET):
        widget = unreal.EditorAssetLibrary.load_asset(TARGET_WIDGET)
    else:
        widget = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_WIDGET, TARGET_WIDGET)
    if widget is None:
        raise RuntimeError("Failed to duplicate W_AngledRollOnly")

    parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(widget)
    if object_path(parent) != "/Script/Daiso_DNG.NaturalRollWidget":
        unreal.BlueprintEditorLibrary.reparent_blueprint(widget, unreal.NaturalRollWidget)
    if not unreal.BlueprintEditorLibrary.compile_blueprint(widget):
        raise RuntimeError("W_NaturalRollOnly failed to compile")
    if not unreal.EditorAssetLibrary.save_loaded_asset(widget, only_if_is_dirty=False):
        raise RuntimeError("W_NaturalRollOnly failed to save")
    return widget


def configure_map():
    world = unreal.EditorLoadingAndSavingUtils.load_map(TARGET_MAP)
    if world is None:
        raise RuntimeError("Failed to load Lvl_Game_AngledRoll")
    game_mode = unreal.load_class(None, "/Script/Daiso_DNG.NaturalDiceGameMode")
    if game_mode is None:
        raise RuntimeError("NaturalDiceGameMode was not loaded")
    world.get_world_settings().set_editor_property("default_game_mode", game_mode)
    if not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True):
        raise RuntimeError("Failed to save Lvl_Game_AngledRoll")
    return world


def validate(widget, world):
    titles = graph_titles(widget)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    dice = [
        actor for actor in actors
        if actor.get_class().get_path_name().endswith("BP_Dice_Basic.BP_Dice_Basic_C")
    ]
    report = {
        "map": TARGET_MAP,
        "widget": TARGET_WIDGET,
        "widget_parent": object_path(
            unreal.BlueprintEditorLibrary.get_blueprint_parent_class(widget)
        ),
        "game_mode": object_path(
            world.get_world_settings().get_editor_property("default_game_mode")
        ),
        "original_roll_graph_preserved": (
            "On Click (GenerateBTN)" in titles and "RollDice" in titles
        ),
        "dice_count": len(dice),
        "dice_classes": sorted({actor.get_class().get_path_name() for actor in dice}),
        "errors": [],
    }
    if report["widget_parent"] != "/Script/Daiso_DNG.NaturalRollWidget":
        report["errors"].append("Unexpected widget parent")
    if report["game_mode"] != "/Script/Daiso_DNG.NaturalDiceGameMode":
        report["errors"].append("Unexpected game mode")
    if not report["original_roll_graph_preserved"]:
        report["errors"].append("Original guided-roll button graph was not preserved")
    if report["dice_count"] != 6:
        report["errors"].append("Expected the original six dice actors")

    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as output:
        json.dump(report, output, ensure_ascii=False, indent=2)
    unreal.log("NATURAL_ROLL_MODE_VALIDATION=" + OUTPUT_PATH)
    if report["errors"]:
        raise RuntimeError("; ".join(report["errors"]))


natural_widget = create_widget_copy()
natural_world = configure_map()
validate(natural_widget, natural_world)
