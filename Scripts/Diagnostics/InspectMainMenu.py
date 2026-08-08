import json
import os

import unreal


OUTPUT_PATH = os.path.join(
    unreal.Paths.project_saved_dir(), "Diagnostics", "MainMenuInspection.json"
)
ASSETS = [
    "/Game/Widgets/MainMenu/W_MainMenu",
    "/Game/Player/HUD/HUD_MainMenu",
    "/Game/Player/GameMode/GM_MainMenu",
]


def safe_call(callback, default=None):
    try:
        return callback()
    except Exception:
        return default


# Собирает структуру готового главного меню и связи его Blueprint-графов.
def inspect_main_menu():
    report = {
        "assets": [],
        "widget_tree": [],
        "graph_nodes": [],
        "map": {},
        "errors": [],
        "python_api": {},
    }
    for asset_path in ASSETS:
        blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
        entry = {"asset": asset_path, "loaded": blueprint is not None}
        if blueprint is not None:
            parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
            entry["parent"] = parent.get_path_name() if parent else None
            entry["compiled"] = bool(
                unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            )
            for graph in unreal.BlueprintEditorLibrary.list_graphs(blueprint):
                editor = safe_call(
                    lambda graph=graph: unreal.BlueprintGraphEditor.get_graph_editor(
                        graph
                    )
                )
                if editor is None:
                    continue
                for node in safe_call(lambda: editor.list_all_nodes(), []):
                    node_entry = {
                        "asset": asset_path,
                        "graph": graph.get_name(),
                        "title": safe_call(
                            lambda node=node: unreal.BlueprintEditorLibrary.get_node_title(
                                node
                            ),
                            "",
                        ),
                        "pins": [],
                    }
                    for pin in safe_call(
                        lambda node=node: unreal.BlueprintEditorLibrary.list_all_pins(
                            node
                        ),
                        [],
                    ):
                        node_entry["pins"].append(
                            {
                                "name": str(
                                    safe_call(
                                        lambda pin=pin: unreal.BlueprintGraphPinLibrary.get_pin_name(
                                            pin
                                        ),
                                        "",
                                    )
                                ),
                                "value": safe_call(
                                    lambda pin=pin: unreal.BlueprintGraphPinLibrary.get_pin_value(
                                        pin
                                    ),
                                    "",
                                ),
                            }
                        )
                    report["graph_nodes"].append(node_entry)
        report["assets"].append(entry)

    menu = unreal.EditorAssetLibrary.load_asset(ASSETS[0])
    report["python_api"] = {
        "unreal_symbols": [
            name for name in dir(unreal) if "widgetblueprint" in name.lower()
        ],
        "menu_members": [
            name
            for name in dir(menu)
            if "widget" in name.lower() or "tree" in name.lower()
        ],
    }
    widget_tree = safe_call(lambda: menu.get_editor_property("widget_tree"))
    for widget in safe_call(lambda: widget_tree.get_all_widgets(), []):
        entry = {
            "name": widget.get_name(),
            "class": widget.get_class().get_path_name(),
        }
        text = safe_call(lambda widget=widget: widget.get_editor_property("text"))
        if text is not None:
            entry["text"] = str(text)
        report["widget_tree"].append(entry)

    world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/Lvl_MainMenu")
    world_settings = world.get_world_settings() if world else None
    game_mode = (
        world_settings.get_editor_property("default_game_mode")
        if world_settings
        else None
    )
    report["map"] = {
        "loaded": world is not None,
        "default_game_mode": game_mode.get_path_name() if game_mode else None,
    }

    if any(not entry.get("compiled", False) for entry in report["assets"]):
        report["errors"].append("One or more main menu Blueprints failed to compile")
    if report["map"].get("default_game_mode") != (
        "/Game/Player/GameMode/GM_MainMenu.GM_MainMenu_C"
    ):
        report["errors"].append("Lvl_MainMenu does not use GM_MainMenu")
    node_titles = [entry["title"] for entry in report["graph_nodes"]]
    if "Open Level (by Name)" not in node_titles:
        report["errors"].append("W_MainMenu has no Open Level node")
    pin_values = [
        str(pin["value"])
        for entry in report["graph_nodes"]
        for pin in entry["pins"]
    ]
    if "Lvl_Game" not in pin_values:
        report["errors"].append("The menu start button does not target Lvl_Game")
    for required_node in ("Create Widget", "AddToViewport", "Set Input Mode UI Only"):
        if required_node not in node_titles:
            report["errors"].append(f"HUD_MainMenu is missing {required_node}")

    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as output:
        json.dump(report, output, ensure_ascii=False, indent=2)
    unreal.log("MAIN_MENU_INSPECTION=" + OUTPUT_PATH)
    if report["errors"]:
        raise RuntimeError("; ".join(report["errors"]))


inspect_main_menu()
