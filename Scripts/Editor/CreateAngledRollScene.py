import json
import os

import unreal


SOURCE_WIDGET = "/Game/Widgets/HUD/W_PlayerScreen"
TARGET_WIDGET = "/Game/Widgets/HUD/W_AngledRollOnly"
SOURCE_MAP = "/Game/Maps/Lvl_Game"
TARGET_MAP = "/Game/Maps/Lvl_Game_AngledRoll"
OUTPUT_PATH = os.path.join(
    unreal.Paths.project_saved_dir(), "Diagnostics", "AngledRollSceneValidation.json"
)


def safe_call(callback, default=None):
    """Возвращает default для необязательных editor-свойств вместо остановки создания сцены."""
    try:
        return callback()
    except Exception:
        return default


def object_path(value):
    """Преобразует UObject/UClass в стабильный путь для проверочного отчёта."""
    return safe_call(lambda: value.get_path_name(), str(value)) if value else None


def create_roll_only_widget():
    """Дублирует рабочую кнопку броска и заменяет только её нативную основу на RollOnlyWidget."""
    widget = unreal.EditorAssetLibrary.load_asset(TARGET_WIDGET)
    if widget is None:
        widget = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_WIDGET, TARGET_WIDGET)
    if widget is None:
        raise RuntimeError("Failed to duplicate W_PlayerScreen")

    parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(widget)
    if object_path(parent) != "/Script/Daiso_DNG.RollOnlyWidget":
        unreal.BlueprintEditorLibrary.reparent_blueprint(widget, unreal.RollOnlyWidget)
    if not unreal.BlueprintEditorLibrary.compile_blueprint(widget):
        raise RuntimeError("W_AngledRollOnly failed to compile")
    if not unreal.EditorAssetLibrary.save_loaded_asset(widget, only_if_is_dirty=False):
        raise RuntimeError("W_AngledRollOnly failed to save")
    return widget


def collect_graph_titles(blueprint):
    """Возвращает названия узлов для проверки сохранённого OnClicked → RollDice графа."""
    titles = []
    for graph in unreal.BlueprintEditorLibrary.list_graphs(blueprint):
        editor = safe_call(lambda graph=graph: unreal.BlueprintGraphEditor.get_graph_editor(graph))
        for node in safe_call(lambda: editor.list_all_nodes(), []) if editor else []:
            titles.append(
                safe_call(lambda node=node: unreal.BlueprintEditorLibrary.get_node_title(node), "")
            )
    return titles


def create_angled_map():
    """Копирует всю сборку Lvl_Game и меняет только GameMode и ракурс GameCamera."""
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_MAP):
        duplicated = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP, TARGET_MAP)
        if duplicated is None:
            raise RuntimeError("Failed to duplicate Lvl_Game")

        # duplicate_asset возвращает UWorld. Перед load_map сохраняем его и
        # отпускаем Python-ссылку, иначе UE 5.8 не может выгрузить старый World.
        if not unreal.EditorAssetLibrary.save_loaded_asset(duplicated, only_if_is_dirty=False):
            raise RuntimeError("Failed to save duplicated Lvl_Game_AngledRoll")
        duplicated = None
        unreal.SystemLibrary.collect_garbage()

    world = unreal.EditorLoadingAndSavingUtils.load_map(TARGET_MAP)
    if world is None:
        raise RuntimeError("Failed to load Lvl_Game_AngledRoll")

    game_mode_class = unreal.load_class(None, "/Script/Daiso_DNG.AngledDiceGameMode")
    if game_mode_class is None:
        raise RuntimeError("AngledDiceGameMode class was not loaded")
    world.get_world_settings().set_editor_property("default_game_mode", game_mode_class)

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors()
    cameras = [
        actor for actor in actors
        if isinstance(actor, unreal.CameraActor) and actor.get_actor_label() == "GameCamera"
    ]
    if len(cameras) != 1:
        raise RuntimeError(f"Expected one GameCamera, found {len(cameras)}")

    camera = cameras[0]
    # Камера стоит по центру ближнего бортика, а не на диагонали от угла.
    camera_location = unreal.Vector(0.0, -360.0, 285.0)
    board_target = unreal.Vector(0.0, 0.0, 125.0)
    camera_rotation = unreal.MathLibrary.find_look_at_rotation(camera_location, board_target)
    camera.set_actor_location(camera_location, False, False)
    camera.set_actor_rotation(camera_rotation, False)
    camera.set_editor_property("auto_activate_for_player", unreal.AutoReceiveInput.PLAYER0)
    camera_component = camera.get_editor_property("camera_component")
    camera_component.set_editor_property("field_of_view", 58.0)

    if not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True):
        raise RuntimeError("Failed to save Lvl_Game_AngledRoll")
    return world, actors, camera, camera_component


def validate_scene(widget, world, actors, camera, camera_component):
    """Проверяет сохранённые связи, шесть костей, доску, GameMode и не-вертикальную камеру."""
    graph_titles = collect_graph_titles(widget)
    dice_count = len(
        [
            actor for actor in actors
            if actor.get_class().get_path_name().endswith("BP_Dice_Basic.BP_Dice_Basic_C")
        ]
    )
    board_count = len(
        [actor for actor in actors if actor.get_actor_label() == "BP_Board"]
    )
    game_mode = world.get_world_settings().get_editor_property("default_game_mode")
    report = {
        "map": TARGET_MAP,
        "widget": TARGET_WIDGET,
        "widget_parent": object_path(
            unreal.BlueprintEditorLibrary.get_blueprint_parent_class(widget)
        ),
        "has_generate_click": "On Click (GenerateBTN)" in graph_titles,
        "has_roll_dice_call": "RollDice" in graph_titles,
        "dice_count": dice_count,
        "board_count": board_count,
        "game_mode": object_path(game_mode),
        "camera_location": str(camera.get_actor_location()),
        "camera_rotation": str(camera.get_actor_rotation()),
        "camera_fov": camera_component.get_editor_property("field_of_view"),
        "errors": [],
    }
    if report["widget_parent"] != "/Script/Daiso_DNG.RollOnlyWidget":
        report["errors"].append("Unexpected widget parent")
    if not report["has_generate_click"] or not report["has_roll_dice_call"]:
        report["errors"].append("Roll button graph was not preserved")
    if dice_count != 6 or board_count != 1:
        report["errors"].append("The copied board assembly is incomplete")
    if report["game_mode"] != "/Script/Daiso_DNG.AngledDiceGameMode":
        report["errors"].append("Unexpected GameMode")
    if abs(camera.get_actor_rotation().pitch + 90.0) < 1.0:
        report["errors"].append("Camera is still top-down")

    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as output:
        json.dump(report, output, ensure_ascii=False, indent=2)
    unreal.log("ANGLED_ROLL_SCENE_VALIDATION=" + OUTPUT_PATH)
    if report["errors"]:
        raise RuntimeError("; ".join(report["errors"]))


roll_widget = create_roll_only_widget()
scene_world, scene_actors, game_camera, game_camera_component = create_angled_map()
validate_scene(roll_widget, scene_world, scene_actors, game_camera, game_camera_component)
