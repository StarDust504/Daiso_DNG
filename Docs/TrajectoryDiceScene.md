# Trajectory dice game scene

`/Game/Maps/Lvl_Game_TrajectoryThrows` is an independent playable scene copied from the solo-roll map. Its
`GameCamera` location, rotation, FOV, board, lighting, and six dice are kept identical to that source scene.

The bottom HUD deliberately exposes only two roll actions:

- **1 Ручной сбор + бросок** — sweep over any desired physical dice, then hold LMB, move the gathered cluster
  along the intended trajectory, and release. Cursor velocity becomes the shared launch velocity.
- **2 Автосбор + бросок** — reuses the existing physical handful mode: all unselected dice gather automatically;
  hold LMB, perform the throw gesture, and release.

Both actions are honest physical rolls. Their values stay unknown until the bodies settle, then feed the same
scoring rules and run progression. The result strip automatically selects the maximum scoring subset; individual
results can be toggled either in the lower strip or by clicking the matching physical die on the board. Selected
dice are highlighted, locked in place, and excluded from both manual and automatic rerolls until deselected.

Finishing a valid round opens `URunStoreWidget`, a native full-screen store with rarity-coloured offer cards,
descriptions, price, current/max stacks, purchase availability, and continue/restart flow. The same fallback store
is also spawned by `W_PlayerScreen`, so the existing main scene no longer depends on unimplemented Blueprint store
events.

After compiling C++, run `Scripts/Editor/CreateTrajectoryDiceScene.py` through Unreal's Python plugin to create or
refresh the map. Run `Daiso.Dice.TrajectoryScene.Configuration` for the full runtime integration check.
