import os

import unreal


ASSET_PATH = "/Game/Data/DT_DiceScoringRules"
CSV_PATH = os.path.abspath(
    os.path.join(unreal.Paths.project_dir(), "SourceData", "DiceScoringRules.csv")
)


def create_or_update_data_table():
    data_table = (
        unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
        if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH)
        else None
    )
    if data_table is None:
        factory = unreal.DataTableFactory()
        row_struct = unreal.load_object(None, "/Script/Daiso_DNG.DiceScoringRule")
        if row_struct is None:
            raise RuntimeError("DiceScoringRule C++ struct was not loaded")
        factory.set_editor_property("struct", row_struct)
        data_table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "DT_DiceScoringRules", "/Game/Data", unreal.DataTable, factory
        )
    if data_table is None:
        raise RuntimeError("Failed to create DT_DiceScoringRules")
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(data_table, CSV_PATH):
        raise RuntimeError(f"Failed to import scoring rules from {CSV_PATH}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(data_table, only_if_is_dirty=False):
        raise RuntimeError("Failed to save DT_DiceScoringRules")
    unreal.log(f"Dice scoring Data Table saved: {ASSET_PATH}")


create_or_update_data_table()
