import json
import os

import unreal


OUTPUT_PATH = os.path.join(unreal.Paths.project_saved_dir(), "Diagnostics", "DiceLevelDump.json")


unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/Lvl_Game")
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report = []
for actor in actor_subsystem.get_all_level_actors():
    components = actor.get_components_by_class(unreal.DicePhysicsRollComponent)
    class_name = actor.get_class().get_name().lower()
    if components or "board" in class_name or "scorecollector" in class_name:
        report.append(
            {
                "actor": actor.get_actor_label(),
                "class": actor.get_class().get_path_name(),
                "location": str(actor.get_actor_location()),
                "dice_components": [component.get_name() for component in components],
            }
        )

os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
with open(OUTPUT_PATH, "w", encoding="utf-8") as output:
    json.dump(report, output, ensure_ascii=False, indent=2)
unreal.log("DICE_LEVEL_DIAGNOSTIC_OUTPUT=" + OUTPUT_PATH)
