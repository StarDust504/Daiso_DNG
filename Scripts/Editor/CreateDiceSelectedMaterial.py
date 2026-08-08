import unreal


SOURCE_PATH = "/Game/Blueprints/Props/Dice/Basic/Material/M_Dice_Basic"
ASSET_PATH = "/Game/Materials/Dice/M_DiceSelected"
INSTANCE_PATH = "/Game/Materials/Dice/MI_DiceSelected"


# Создаёт emissive-копию штатного материала, сохраняя текстуры и точки кубика.
def create_dice_selected_material():
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        material = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    else:
        material = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_PATH, ASSET_PATH)
        if material is None:
            raise RuntimeError("Failed to duplicate M_Dice_Basic")

        color = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionVectorParameter, -360, -120
        )
        color.set_editor_property("parameter_name", "SelectionColor")
        color.set_editor_property(
            "default_value", unreal.LinearColor(0.02, 1.0, 0.12, 1.0)
        )

        intensity = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionScalarParameter, -360, 20
        )
        intensity.set_editor_property("parameter_name", "SelectionIntensity")
        intensity.set_editor_property("default_value", 8.0)

        emissive = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionMultiply, -80, -80
        )
        unreal.MaterialEditingLibrary.connect_material_expressions(
            color, "", emissive, "A"
        )
        unreal.MaterialEditingLibrary.connect_material_expressions(
            intensity, "", emissive, "B"
        )
        unreal.MaterialEditingLibrary.connect_material_property(
            emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
        )

        unreal.MaterialEditingLibrary.layout_material_expressions(material)
        unreal.MaterialEditingLibrary.recompile_material(material)
        if not unreal.EditorAssetLibrary.save_loaded_asset(
            material, only_if_is_dirty=False
        ):
            raise RuntimeError("Failed to save M_DiceSelected")

    if unreal.EditorAssetLibrary.does_asset_exist(INSTANCE_PATH):
        material_instance = unreal.EditorAssetLibrary.load_asset(INSTANCE_PATH)
    else:
        material_instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MI_DiceSelected",
            "/Game/Materials/Dice",
            unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew(),
        )
    if material_instance is None:
        raise RuntimeError("Failed to create MI_DiceSelected")

    unreal.MaterialEditingLibrary.set_material_instance_parent(
        material_instance, material
    )
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        material_instance,
        "SelectionColor",
        unreal.LinearColor(0.16, 0.36, 0.12, 1.0),
    )
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        material_instance, "SelectionIntensity", 1.35
    )
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        material_instance, only_if_is_dirty=False
    ):
        raise RuntimeError("Failed to save MI_DiceSelected")
    unreal.log("Soft dice selection material saved: " + INSTANCE_PATH)


create_dice_selected_material()
