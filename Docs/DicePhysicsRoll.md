# Dice Physics Roll

`UDicePhysicsRollComponent` adds a physical throw with a predetermined result to the existing dice Blueprints.
The numbered face is turned upward, which means the die actually lands on the opposite face.

## Blueprint hookup

The current `RollDice` event lives in `Content/Blueprints/Props/Dice/BP_Dice`, while
`BP_Dice_Basic` inherits from it. Connect the component in `BP_Dice` if every dice type should use the new roll.

1. Add the **Dice Physics Roll** component to `BP_Dice`.
2. Leave **Auto Find Dice Body** enabled when `Dice` is the only colliding Primitive Component. Otherwise set
   **Dice Body Reference** explicitly to the `Dice` Static Mesh Component.
3. In `RollDice`, keep the existing number generation and assignment to `GeneratedNumber`.
4. Replace the immediate `Set World Rotation` path with one of these calls:
   - **Roll To Value** (`Result = GeneratedNumber`) and calibrate **Face Local Normals** once; or
   - **Roll To Rotation** in each existing Switch Integer branch, passing the same `New Rotation` that branch
     previously sent to `Set World Rotation`. This preserves the mesh's current face mapping exactly.
5. Make sure the `Dice` mesh collision is enabled and uses a simple convex/box collision suitable for simulation.

`Roll To Rotation` is the safest first connection because the current Blueprint already contains the six correct
orientations for `SM_Dice_Basic`. After validating the local axes, the Switch can be removed and replaced by the
single `Roll To Value` call.

## Suggested starting values

| Group | Parameter | Default | What it changes |
|---|---|---:|---|
| Throw | Upward Speed | 420 | Height and hang time. The strongest contributor to a weighty arc. |
| Throw | Horizontal Speed | 95 | Maximum side speed. Board clustering may choose a lower value. |
| Throw | Spin Speed | 24 | Initial chaos and number of visible tumbles. |
| Throw | Air Linear Damping | 0.08 | Loss of translational energy before impact. |
| Throw | Air Angular Damping | 0.06 | Loss of spin before impact. |
| Landing Assist | Free Flight Time | 0.16 | Free tumbling before a stronger face-only aerial correction starts. |
| Landing Assist | Aerial Alignment Lead Time | 0.62 | Starts physical face alignment this many seconds before predicted board impact. |
| Landing Assist | Orientation Strength | 60 | How aggressively the requested face is found. |
| Landing Assist | Orientation Damping | 8 | Damps tilt while preserving visible yaw spin in the air. |
| Landing Assist | Max Angular Acceleration | 140 | Caps visible correction. Lower is subtler; higher is more reliable. |
| Landing Assist | Use Airborne Safety Alignment | true | Closes the small remaining face error immediately before impact, while the die is still airborne. |
| Landing Assist | Airborne Safety Alignment Time | 0.10 | Length of the final airborne face-capture window. |
| Landing Assist | Airborne Safety Tolerance | 0.75 degrees | Ignores tiny errors so a correct die is not touched unnecessarily. |
| Landing Assist | Airborne Yaw Spin Retention | 0.15 | Keeps a small amount of natural yaw without allowing another tumble. |
| Landing Assist | Minimum Airborne Clearance | 3 cm | The die must leave the board by this distance before a later contact can count as landing. |
| Correction Bounce | Retry Wrong Face With Bounce | true | Rejects a wrong landing and starts a small physical retry hop. |
| Correction Bounce | Correction Bounce Trigger Angle | 24 degrees | Minimum face error required to trigger a retry. |
| Correction Bounce | Correction Bounce Speed | 175 | Height of the retry hop. `140-190` stays compact. |
| Correction Bounce | Correction Bounce Horizontal Speed | 14 | Adds a subtle sideways variation to the retry. |
| Correction Bounce | Correction Bounce Angular Speed | 5 | Directed angular impulse toward the generated face. |
| Correction Bounce | Maximum Correction Bounces | 2 | Prevents an endless retry loop if the die is trapped. |
| Impact | Landing Linear Damping | 1.15 | How quickly sliding stops after the first real impact. |
| Impact | Landing Angular Damping | 8.0 | Stops physical spin quickly after the first real impact. |
| Impact | Minimum Impact Speed | 90 | Filters tiny contact sounds and false impact events. |
| Impact | Full Strength Impact Speed | 520 | Maps an impact to event `Strength = 1`. |
| Settle | Settle Linear Speed | 18 | Speed below which the body can be considered stable. |
| Settle | Settle Angular Speed | 0.65 | Spin below which the body can be considered stable. |
| Settle | Settle Angle Tolerance | 7 degrees | Allowed target error before final alignment. |
| Settle | Required Stable Time | 0.18 | Prevents finishing during a brief pause between bounces. |
| Settle | Maximum Roll Time | 3.25 | Guaranteed escape if the die gets wedged. |
| Settle | Final Alignment Time | 0.11 | Fallback used only if no genuine board/support contact was detected. |

The motion is split into three phases: free random tumbling, torque-based physical guidance, and a short airborne
face capture during the last `0.10 s`. A launch-frame contact is ignored until the body has actually cleared the
board. After the first genuine downward support contact, neither torque nor direct rotation is ever applied.

If that contact arrives with the wrong face more than `24 degrees` from upright, it is not accepted as the final
landing. The impact produces a low `175 cm/s` correction bounce and a directed angular kick. Face guidance then
runs during this new airborne arc. A correct contact is accepted normally, and no on-board rotation is used.

## Board bounds

The roll component automatically finds the current `BP_Board_C` and uses the largest Primitive Component as its
surface. It creates a shared invisible `Box Collision` floor plus four walls around the mesh bounds at runtime.
Multiple dice reuse the same five colliders. The simple floor prevents a simulated die from tunnelling through a
thin or complex `SM_Board` collision mesh.

- **Board Actor Override**: explicitly select the board instance when a level contains more than one board.
- **Board Actor Tag**: preferred automatic selection. Add actor tag `DiceBoard` to the intended `BP_Board` instance.
- **Create Board Boundary Walls**: enables physical containment and natural wall bounces.
- **Board Wall Height**: must be higher than the maximum height reached by the dice centre. Default `140 cm` is
  suitable for the default `Upward Speed 420`.
- **Board Wall Thickness**: raise this if very fast dice tunnel through a boundary.
- **Board Floor Thickness**: thickness of the invisible supporting floor. Start with `12 cm`.
- **Board Wall Inset**: positive values move the walls inward from the mesh edge.
- **Board Center Bias**: subtle inward steering applied everywhere on the board.
- **Board Edge Bias**: maximum inward steering near an edge.
- **Board Edge Bias Start**: normalized distance from the board centre at which stronger steering begins.
- **Cluster Throws On Board**: aims every die at a loose central landing area rather than using unrelated 360-degree directions.
- **Board Landing Cluster Radius**: size of that area. `0.25` is tight, `0.38` is moderately grouped, `0.60` is loose.

For a compact board start with `Board Wall Height 140`, `Board Wall Thickness 12`, `Board Center Bias 0.12`,
`Board Edge Bias 0.88`, and `Board Edge Bias Start 0.52`. To make wall hits more common, lower Edge Bias to `0.55`.

For a heavier die, try `Upward Speed 330`, `Spin Speed 13`, `Landing Linear Damping 1.8`, and
`Landing Angular Damping 6.0`. For a wilder Balatro-like roll, try `Upward Speed 500`,
`Horizontal Speed 220`, `Spin Speed 25`, and `Free Flight Time 0.36`.

## Physical Material

Create a Physical Material and assign it either to **Physical Material Override** on the roll component or directly
to the Static Mesh. These values have the largest effect on contact feel:

- **Restitution**: bounce. Start around `0.20-0.30`; above `0.45` feels rubbery.
- **Friction**: resistance to sliding. Start around `0.65-0.85` for a board-game die.
- **Static Friction**: resistance once almost stopped. Raise it if the die keeps creeping.
- **Density** changes mass, but the component uses velocity-change impulses, so the launch arc stays tunable and
  consistent. Mass still affects collision with other simulated objects.

## Juice event

Bind to **On Dice Impact** in Blueprint. Its `Strength` is normalized from 0 to 1 and is intended to drive:

- a short camera shake (roughly `0.05-0.12 s`),
- a low-frequency impact sound with pitch variation,
- a small dust/ring Niagara burst at `Location`,
- a controller rumble pulse,
- a very short global or local hit-stop only on strong impacts.

Keep the physics continuous; visual/audio feedback should make the landing feel stronger without changing the
generated result.
