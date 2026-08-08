import unreal


ASSET_PATH = "/Game/Widgets/HUD/W_PlayerScreen"


def configure_player_screen():
    blueprint = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    if blueprint is None:
        raise RuntimeError("W_PlayerScreen was not found")

    parent_class = unreal.PlayerScreenWidget
    current_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
    current_parent_path = current_parent.get_path_name() if current_parent else ""
    if current_parent_path != "/Script/Daiso_DNG.PlayerScreenWidget":
        unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, parent_class)

    if not unreal.BlueprintEditorLibrary.compile_blueprint(blueprint):
        raise RuntimeError("W_PlayerScreen failed to compile after reparenting")
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        raise RuntimeError("W_PlayerScreen could not be saved")
    unreal.log("W_PlayerScreen configured with native level UI")


configure_player_screen()
