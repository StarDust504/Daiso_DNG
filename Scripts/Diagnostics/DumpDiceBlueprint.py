import json
import os
import traceback

import unreal


OUTPUT_PATH = os.path.join(unreal.Paths.project_saved_dir(), "Diagnostics", "DiceBlueprintDump.json")


def safe_call(fn, default=None):
    try:
        return fn()
    except Exception as exc:
        return {"error": str(exc)} if default is None else default


def object_path(obj):
    if obj is None:
        return None
    return safe_call(lambda: obj.get_path_name(), str(obj))


def dump_pin(pin):
    pin_lib = unreal.BlueprintGraphPinLibrary
    linked = []
    for connected in safe_call(lambda: pin_lib.list_connected_pins(pin), []):
        owner = safe_call(lambda: pin_lib.get_owning_node(connected), None)
        linked.append(
            {
                "pin": str(safe_call(lambda: pin_lib.get_pin_name(connected), "")),
                "node_name": safe_call(lambda: owner.get_name(), "") if owner else "",
                "node_title": safe_call(lambda: unreal.BlueprintEditorLibrary.get_node_title(owner), "") if owner else "",
            }
        )
    return {
        "name": str(safe_call(lambda: pin_lib.get_pin_name(pin), "")),
        "direction": str(safe_call(lambda: pin_lib.get_pin_direction(pin), "")),
        "type": safe_call(lambda: pin_lib.get_pin_type_display_string(pin), ""),
        "value": safe_call(lambda: pin_lib.get_pin_value(pin), ""),
        "linked": linked,
    }


def dump_blueprint(asset_path):
    bp = unreal.load_asset(asset_path)
    result = {"asset": asset_path, "loaded": bp is not None, "graphs": [], "components": []}
    if bp is None:
        return result

    graphs = safe_call(lambda: unreal.BlueprintEditorLibrary.list_graphs(bp), [])
    for graph in graphs:
        graph_dump = {"name": graph.get_name(), "nodes": []}
        editor = safe_call(lambda: unreal.BlueprintGraphEditor.get_graph_editor(graph), None)
        nodes = safe_call(lambda: editor.list_all_nodes(), []) if editor else []
        for node in nodes:
            title = safe_call(lambda: unreal.BlueprintEditorLibrary.get_node_title(node), "")
            node_dump = {
                "name": node.get_name(),
                "class": object_path(node.get_class()),
                "title": title,
                "position": str(safe_call(lambda: unreal.BlueprintEditorLibrary.get_node_pos(node), "")),
                "pins": [],
            }
            for pin in safe_call(lambda: unreal.BlueprintEditorLibrary.list_all_pins(node), []):
                node_dump["pins"].append(dump_pin(pin))
            graph_dump["nodes"].append(node_dump)
        result["graphs"].append(graph_dump)

    generated_class = safe_call(lambda: bp.generated_class(), None)
    cdo = safe_call(lambda: unreal.get_default_object(generated_class), None) if generated_class else None
    components = safe_call(lambda: cdo.get_components_by_class(unreal.ActorComponent), []) if cdo else []
    for component in components:
        entry = {
            "name": component.get_name(),
            "class": object_path(component.get_class()),
        }
        if isinstance(component, unreal.SceneComponent):
            entry.update(
                {
                    "relative_location": str(safe_call(lambda: component.get_editor_property("relative_location"), "")),
                    "relative_rotation": str(safe_call(lambda: component.get_editor_property("relative_rotation"), "")),
                    "relative_scale": str(safe_call(lambda: component.get_editor_property("relative_scale3d"), "")),
                }
            )
        if isinstance(component, unreal.StaticMeshComponent):
            entry["static_mesh"] = object_path(safe_call(lambda: component.get_editor_property("static_mesh"), None))
            entry["collision_profile"] = str(safe_call(lambda: component.get_collision_profile_name(), ""))
        result["components"].append(entry)
    return result


def dump_static_mesh(asset_path):
    mesh = unreal.load_asset(asset_path)
    result = {"asset": asset_path, "loaded": mesh is not None}
    if mesh is None:
        return result
    result["bounds"] = str(safe_call(lambda: mesh.get_bounds(), ""))
    result["sockets"] = [str(name) for name in safe_call(lambda: mesh.get_socket_names(), [])]
    result["materials"] = [object_path(slot.material_interface) for slot in safe_call(lambda: mesh.get_editor_property("static_materials"), [])]
    import_data = safe_call(lambda: mesh.get_editor_property("asset_import_data"), None)
    result["source_file"] = safe_call(lambda: import_data.get_first_filename(), "") if import_data else ""
    return result


def main():
    report = {
        "bp_dice": dump_blueprint("/Game/Blueprints/Props/Dice/BP_Dice"),
        "bp_dice_basic": dump_blueprint("/Game/Blueprints/Props/Dice/Basic/BP_Dice_Basic"),
        "static_mesh": dump_static_mesh("/Game/Assets/StaticMeshes/SM_Dice_Basic"),
    }
    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as output:
        json.dump(report, output, ensure_ascii=False, indent=2, default=str)
    unreal.log("DICE_DIAGNOSTIC_OUTPUT=" + OUTPUT_PATH)


try:
    main()
except Exception:
    unreal.log_error(traceback.format_exc())
    raise
