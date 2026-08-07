import unreal


ASSET_PATH = "/Game/Blueprints/Testing/BP_DiceScoreCollector"
MAP_PATH = "/Game/Maps/Lvl_Game"


def load_or_create_blueprint():
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        return unreal.EditorAssetLibrary.load_asset(ASSET_PATH)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.DiceRollScoreCollector)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "BP_DiceScoreCollector", "/Game/Blueprints/Testing", unreal.Blueprint, factory
    )
    if blueprint is None:
        raise RuntimeError("Failed to create BP_DiceScoreCollector")
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    return blueprint


def place_collector(blueprint):
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    collector_class = blueprint.generated_class()
    collectors = [
        actor for actor in actor_subsystem.get_all_level_actors()
        if actor.get_class() == collector_class
    ]
    if not collectors:
        collector = actor_subsystem.spawn_actor_from_class(
            collector_class, unreal.Vector(0.0, 0.0, 180.0)
        )
        if collector is None:
            raise RuntimeError("Failed to place BP_DiceScoreCollector in Lvl_Game")
        collector.set_actor_label("BP_DiceScoreCollector")
    if not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True):
        raise RuntimeError("Failed to save BP_DiceScoreCollector or Lvl_Game")


place_collector(load_or_create_blueprint())
unreal.log("Dice score collector created and placed in Lvl_Game")
