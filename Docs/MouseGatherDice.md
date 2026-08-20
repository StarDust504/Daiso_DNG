# Mouse-gather dice modes

`AMouseGatherDiceManager` adds two interaction variants to `Lvl_Game_AngledRoll` without changing either dice
Blueprint. Press **Собрать: честно** or **Собрать: прогноз**, then sweep the cursor over dice. Each touched die stops
simulating its previous roll and rises into a compact, physics-simulated 3D cluster under the cursor. Soft springs hold the dice together while collisions,
inertia, gentle floating and slow tumbling keep the group feeling alive. The activation click is explicitly ignored;
a later left click releases the gathered dice.

- **Собрать: честно** clears `GeneratedNumber`, applies unguided spin and collision impulses, and reads each physical
  top face only after the group settles.
- **Собрать: прогноз** chooses each value at release, then temporarily configures the existing physical roll
  component for a zero-upward-speed fall. Dice still collide and tumble, while airborne assistance guides the chosen
  face before impact.

Both variants use the inset visible-rim boundaries and a moderately bouncy temporary physical material. Component,
material, collision, gravity, damping, and original board-boundary settings are restored when the drop completes.
The HUD uses Game-and-UI input so UMG buttons, cursor projection, and the world-space release click work together.
