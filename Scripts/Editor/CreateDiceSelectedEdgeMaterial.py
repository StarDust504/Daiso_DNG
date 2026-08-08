import unreal


SOURCE_PATH = "/Game/Blueprints/Props/Dice/Basic/Material/M_Dice_Basic"
MATERIAL_PATH = "/Game/Materials/Dice/M_DiceSelectedEdge"
INSTANCE_PATH = "/Game/Materials/Dice/MI_DiceSelectedEdge"


# Создаёт копию штатного материала с Fresnel-свечением только по краям кубика.
def create_dice_selected_edge_material():
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PATH):
        material = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    else:
        material = unreal.EditorAssetLibrary.duplicate_asset(
            SOURCE_PATH, MATERIAL_PATH
        )
        if material is None:
            raise RuntimeError("Failed to duplicate M_Dice_Basic")

        edge_color = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionVectorParameter, -620, -180
        )
        edge_color.set_editor_property("parameter_name", "EdgeColor")
        edge_color.set_editor_property(
            "default_value", unreal.LinearColor(0.05, 0.45, 0.08, 1.0)
        )

        edge_intensity = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionScalarParameter, -620, -40
        )
        edge_intensity.set_editor_property("parameter_name", "EdgeIntensity")
        edge_intensity.set_editor_property("default_value", 2.25)

        colored_glow = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionMultiply, -340, -140
        )
        unreal.MaterialEditingLibrary.connect_material_expressions(
            edge_color, "", colored_glow, "A"
        )
        unreal.MaterialEditingLibrary.connect_material_expressions(
            edge_intensity, "", colored_glow, "B"
        )

        fresnel = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionFresnel, -340, 100
        )
        edge_falloff = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionScalarParameter, -620, 100
        )
        edge_falloff.set_editor_property("parameter_name", "EdgeFalloff")
        edge_falloff.set_editor_property("default_value", 4.5)
        edge_base = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionScalarParameter, -620, 220
        )
        edge_base.set_editor_property("parameter_name", "EdgeBase")
        edge_base.set_editor_property("default_value", 0.0)
        unreal.MaterialEditingLibrary.connect_material_expressions(
            edge_falloff, "", fresnel, "ExponentIn"
        )
        unreal.MaterialEditingLibrary.connect_material_expressions(
            edge_base, "", fresnel, "BaseReflectFractionIn"
        )

        edge_emissive = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionMultiply, -40, -80
        )
        unreal.MaterialEditingLibrary.connect_material_expressions(
            colored_glow, "", edge_emissive, "A"
        )
        unreal.MaterialEditingLibrary.connect_material_expressions(
            fresnel, "", edge_emissive, "B"
        )
        unreal.MaterialEditingLibrary.connect_material_property(
            edge_emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
        )

        unreal.MaterialEditingLibrary.layout_material_expressions(material)
        unreal.MaterialEditingLibrary.recompile_material(material)
        if not unreal.EditorAssetLibrary.save_loaded_asset(
            material, only_if_is_dirty=False
        ):
            raise RuntimeError("Failed to save M_DiceSelectedEdge")

    if unreal.EditorAssetLibrary.does_asset_exist(INSTANCE_PATH):
        material_instance = unreal.EditorAssetLibrary.load_asset(INSTANCE_PATH)
    else:
        material_instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MI_DiceSelectedEdge",
            "/Game/Materials/Dice",
            unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew(),
        )
    if material_instance is None:
        raise RuntimeError("Failed to create MI_DiceSelectedEdge")

    unreal.MaterialEditingLibrary.set_material_instance_parent(
        material_instance, material
    )
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        material_instance, "EdgeColor", unreal.LinearColor(0.05, 0.42, 0.08, 1.0)
    )
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        material_instance, "EdgeIntensity", 2.0
    )
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        material_instance, "EdgeFalloff", 4.5
    )
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        material_instance, "EdgeBase", 0.0
    )
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        material_instance, only_if_is_dirty=False
    ):
        raise RuntimeError("Failed to save MI_DiceSelectedEdge")
    unreal.log("Dice edge selection material saved: " + INSTANCE_PATH)


create_dice_selected_edge_material()
