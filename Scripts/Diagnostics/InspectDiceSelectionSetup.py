import json
import os

import unreal


OUTPUT_PATH = os.path.join(
    unreal.Paths.project_saved_dir(), "Diagnostics", "DiceSelectionSetup.json"
)


# Сохраняет фактические компоненты CDO кубика, чтобы проверить получателя подсветки.
def inspect_dice_selection_setup():
    report = {"errors": [], "components": []}
    blueprint = unreal.EditorAssetLibrary.load_asset(
        "/Game/Blueprints/Props/Dice/BP_Dice"
    )
    if blueprint is None:
        raise RuntimeError("BP_Dice was not found")

    generated_class = blueprint.generated_class()
    default_dice = unreal.get_default_object(generated_class)
    native_mesh = default_dice.get_editor_property("smc_dice")
    selection_light = default_dice.get_editor_property("selection_light")
    report["native_mesh_property"] = (
        native_mesh.get_path_name() if native_mesh else None
    )

    components = default_dice.get_components_by_class(unreal.StaticMeshComponent)
    for component in components:
        static_mesh = component.get_editor_property("static_mesh")
        overlay = component.get_editor_property("overlay_material")
        report["components"].append(
            {
                "name": component.get_name(),
                "path": component.get_path_name(),
                "static_mesh": static_mesh.get_path_name() if static_mesh else None,
                "overlay_material": overlay.get_path_name() if overlay else None,
                "visible": bool(component.get_editor_property("visible")),
                "hidden_in_game": bool(
                    component.get_editor_property("hidden_in_game")
                ),
                "materials": [
                    material.get_path_name() if material else None
                    for material in component.get_materials()
                ],
            }
        )

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    spawned_dice = actor_subsystem.spawn_actor_from_class(
        generated_class, unreal.Vector(0.0, 0.0, -100000.0)
    )
    if spawned_dice is None:
        report["errors"].append("Failed to spawn BP_Dice for material switch test")
    else:
        try:
            spawned_mesh = spawned_dice.get_editor_property("smc_dice")
            spawned_light = spawned_dice.get_editor_property("selection_light")
            material_before = spawned_mesh.get_material(0)
            spawned_dice.set_is_active(True)
            material_selected = spawned_mesh.get_material(0)
            selected_light_visible = bool(
                spawned_light.get_editor_property("visible")
            )
            spawned_dice.set_is_active(False)
            material_restored = spawned_mesh.get_material(0)
            deselected_light_visible = bool(
                spawned_light.get_editor_property("visible")
            )
            report["selection_light_test"] = {
                "before": material_before.get_path_name() if material_before else None,
                "during_selection": material_selected.get_path_name()
                if material_selected
                else None,
                "after": material_restored.get_path_name()
                if material_restored
                else None,
                "material_unchanged": material_before
                == material_selected
                == material_restored,
                "selected_light_visible": selected_light_visible,
                "deselected_light_visible": deselected_light_visible,
            }
            if material_before != material_selected or material_before != material_restored:
                report["errors"].append("Dice material changed during selection")
            if not selected_light_visible or deselected_light_visible:
                report["errors"].append("Selection light visibility is incorrect")
        finally:
            actor_subsystem.destroy_actor(spawned_dice)

    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as output:
        json.dump(report, output, ensure_ascii=False, indent=2)
    unreal.log("DICE_SELECTION_SETUP=" + OUTPUT_PATH)
    if report["errors"]:
        raise RuntimeError("; ".join(report["errors"]))


inspect_dice_selection_setup()
