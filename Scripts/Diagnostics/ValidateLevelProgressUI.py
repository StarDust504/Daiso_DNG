import json
import os

import unreal


OUTPUT_PATH = os.path.join(
    unreal.Paths.project_saved_dir(), "Diagnostics", "LevelProgressUIValidation.json"
)
BLUEPRINT_PATHS = [
    "/Game/Widgets/HUD/W_PlayerScreen",
    "/Game/Blueprints/Props/Dice/BP_Dice",
    "/Game/Player/BP_Player",
    "/Game/Player/HUD/HUD_Base",
]


# Проверяет таблицу целей и компиляцию Blueprint, от которых зависит игровой интерфейс.
def validate_level_progress_ui():
    report = {"goals": {}, "selection_light": {}, "blueprints": [], "errors": []}
    goals = unreal.EditorAssetLibrary.load_asset("/Game/Data/DT_LevelGoals")
    if goals is None:
        report["errors"].append("DT_LevelGoals was not found")
    else:
        row_names = [
            str(name)
            for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(goals)
        ]
        report["goals"] = {"row_count": len(row_names), "row_names": row_names}
        expected = [f"Level_{number:02d}" for number in range(1, 9)]
        if sorted(row_names) != expected:
            report["errors"].append("DT_LevelGoals does not contain Level_01..Level_08")

    for asset_path in BLUEPRINT_PATHS:
        blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
        entry = {"asset": asset_path, "loaded": blueprint is not None}
        if blueprint is None:
            report["errors"].append(f"{asset_path} was not found")
            report["blueprints"].append(entry)
            continue

        parent_class = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
        entry["parent_class"] = parent_class.get_path_name() if parent_class else None
        entry["compiled"] = bool(unreal.BlueprintEditorLibrary.compile_blueprint(blueprint))
        if not entry["compiled"]:
            report["errors"].append(f"{asset_path} failed to compile")

        if asset_path.endswith("BP_Dice"):
            generated_class = blueprint.generated_class()
            default_dice = unreal.get_default_object(generated_class)
            selection_light = default_dice.get_editor_property("selection_light")
            if selection_light is None:
                report["errors"].append("BP_Dice has no selection light")
            else:
                report["selection_light"] = {
                    "component": selection_light.get_path_name(),
                    "intensity": selection_light.get_editor_property("intensity"),
                    "attenuation_radius": selection_light.get_editor_property(
                        "attenuation_radius"
                    ),
                    "light_color": str(
                        selection_light.get_editor_property("light_color")
                    ),
                    "visible_by_default": bool(
                        selection_light.get_editor_property("visible")
                    ),
                }
        report["blueprints"].append(entry)

    player_screen = next(
        item for item in report["blueprints"] if item["asset"].endswith("W_PlayerScreen")
    )
    if player_screen.get("parent_class") != "/Script/Daiso_DNG.PlayerScreenWidget":
        report["errors"].append("W_PlayerScreen has an unexpected parent class")

    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as output:
        json.dump(report, output, ensure_ascii=False, indent=2)
    unreal.log("LEVEL_PROGRESS_UI_VALIDATION=" + OUTPUT_PATH)
    if report["errors"]:
        raise RuntimeError("; ".join(report["errors"]))


validate_level_progress_ui()
