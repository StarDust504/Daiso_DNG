import unreal


ASSET_PATH = "/Game/Materials/Dice/M_DiceSelectionHighlight"


# Создаёт или обновляет материал мягкого контура, используемый выбранными кубиками.
def create_selection_outline_material():
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        unreal.log("Dice selection highlight material already exists: " + ASSET_PATH)
        return

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_DiceSelectionHighlight",
        "/Game/Materials/Dice",
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if material is None:
        raise RuntimeError("Failed to create M_DiceSelectionHighlight")

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -700, -220
    )
    color.set_editor_property("parameter_name", "OutlineColor")
    color.set_editor_property("default_value", unreal.LinearColor(0.08, 1.0, 0.28, 1.0))

    emissive = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -420, -220
    )
    emissive.set_editor_property("const_b", 4.0)
    unreal.MaterialEditingLibrary.connect_material_expressions(color, "", emissive, "A")
    unreal.MaterialEditingLibrary.connect_material_property(
        emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )

    vertex_normal = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVertexNormalWS, -700, 100
    )
    outline_width = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -700, 210
    )
    outline_width.set_editor_property("parameter_name", "OutlineWidth")
    outline_width.set_editor_property("default_value", 0.8)
    world_offset = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -420, 120
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        vertex_normal, "", world_offset, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        outline_width, "", world_offset, "B"
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        world_offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET
    )

    two_sided_sign = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTwoSidedSign, -700, 410
    )
    invert_sign = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -500, 410
    )
    invert_sign.set_editor_property("const_b", -0.5)
    backface_mask = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, -300, 410
    )
    backface_mask.set_editor_property("const_b", 0.5)

    backface_strength = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -100, 410
    )
    backface_strength.set_editor_property("const_b", 0.7)
    front_opacity = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -300, 540
    )
    front_opacity.set_editor_property("parameter_name", "FrontOpacity")
    front_opacity.set_editor_property("default_value", 0.3)
    final_opacity = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, 100, 450
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        two_sided_sign, "", invert_sign, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        invert_sign, "", backface_mask, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        backface_mask, "", backface_strength, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        backface_strength, "", final_opacity, "A"
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        front_opacity, "", final_opacity, "B"
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        final_opacity, "", unreal.MaterialProperty.MP_OPACITY
    )

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError("Failed to save M_DiceSelectionHighlight")
    unreal.log("Dice selection outline material saved: " + ASSET_PATH)


create_selection_outline_material()
