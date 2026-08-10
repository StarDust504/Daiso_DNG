import os

import unreal


TABLES = (
    (
        "/Game/Data/DT_RunStages",
        "DT_RunStages",
        "RunStages.csv",
        "/Script/Daiso_DNG.RunStageRow",
    ),
    (
        "/Game/Data/DT_Boosts",
        "DT_Boosts",
        "Boosts.csv",
        "/Script/Daiso_DNG.BoostRow",
    ),
)


def create_or_update_data_table(asset_path, asset_name, csv_name, row_struct_path):
    """Создаёт либо обновляет Data Table из UTF-8 CSV, сохраняя штатный Unreal asset."""
    data_table = (
        unreal.EditorAssetLibrary.load_asset(asset_path)
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path)
        else None
    )
    if data_table is None:
        factory = unreal.DataTableFactory()
        row_struct = unreal.load_object(None, row_struct_path)
        if row_struct is None:
            raise RuntimeError(f"C++ row struct was not loaded: {row_struct_path}")
        factory.set_editor_property("struct", row_struct)
        data_table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name, "/Game/Data", unreal.DataTable, factory
        )
    if data_table is None:
        raise RuntimeError(f"Failed to create {asset_path}")

    csv_path = os.path.abspath(
        os.path.join(unreal.Paths.project_dir(), "SourceData", csv_name)
    )
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(
        data_table, csv_path
    ):
        raise RuntimeError(f"Failed to import {csv_path}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        data_table, only_if_is_dirty=False
    ):
        raise RuntimeError(f"Failed to save {asset_path}")
    unreal.log(f"Run progression Data Table saved: {asset_path}")


for table in TABLES:
    create_or_update_data_table(*table)
