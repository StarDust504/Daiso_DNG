# Natural dice roll

`ANaturalDiceRollManager` is the independent, unguided alternative to the existing predetermined physical roll.
It never calls `BP_Dice.RollDice` and writes `GeneratedNumber = 0` at launch. Once all six rigid bodies have made a
support contact and remained stable, it reads the upward face directly from each mesh transform and only then writes
the results. No torque, rotation interpolation, corrective bounce, or face target is used.

The orange **Бросить с хаосом** button is added by `UNaturalRollWidget`. The original blue button and its Blueprint
`On Click (GenerateBTN) -> RollDice` graph remain intact, so the angled roll map now demonstrates both behaviours.

Extra chaos comes from higher random spin, a shared loose convergence point, full dice-to-dice blocking, CCD, and a
small angular kick on actual dice collisions. The existing invisible board floor and wall convention is reused so
both roll modes remain contained.

The natural mode temporarily assigns a `0.34` restitution physical material, producing a short first rebound instead
of a dead impact. While that mode is active, shared boundary faces are inset `6.5 cm` from the mesh's outer bounds,
matching the playable side of the visible wooden rim. The material, body settings, and original guided-mode boundary
area are restored after the batch finishes.

## Asset setup

After compiling C++, run `Scripts/Editor/CreateNaturalRollMode.py` in Unreal Editor. It creates
`/Game/Widgets/HUD/W_NaturalRollOnly` as a copy of the existing roll-only widget, reparents only the copy, and switches
`/Game/Maps/Lvl_Game_AngledRoll` to `ANaturalDiceGameMode`. The original dice Blueprints are not modified or copied
because this mode operates on their existing static mesh components at runtime.

## Verification

Run the `Daiso.Dice.NaturalRoll.FaceDetection` automation test. It checks the landing rotations already calibrated in
`BP_Dice` for all six faces. Runtime logs explicitly report that results are unknown at launch and print each value
only after the physical batch has settled.

Natural impacts now reuse the sound, pitch/volume scaling, cooldown and camera-shake settings from each die's
`DicePhysicsRollComponent`. This applies both to the direct honest roll and to the honest mouse-gather drop.
