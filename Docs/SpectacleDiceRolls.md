# Spectacle dice rolls

`ASpectacleDiceRollManager` and `USpectacleRollWidget` extend only the isolated
`/Game/Maps/Lvl_Game_AngledRoll` scene. The original dice Blueprints and the copied existing HUD Blueprint are not
modified. `ANaturalDiceGameMode` spawns the new manager, while `ASpectacleDiceHUD` layers native side panels over
the existing controls.

- **Вихрь: честно** holds all rigid bodies on moving physical orbits around the cursor. Dice collide in the cloud,
  retain their orbital velocity when released, and receive values only after settling.
- **Вихрь: прогноз** uses the same physical buildup. Values are selected only when the vortex releases; the existing
  airborne face-assist component then guides the landing.
- **Кубики-метеоры** places the dice at varied heights and drops them at short intervals so later dice can strike
  ones already on the board.
- **Переворот гравитации** raises the dice into a spring-held, tumbling 3D cloud and abruptly restores gravity.
- **Бросок пригоршней** forms a tight physical cluster under the cursor. After the activation click is released,
  hold LMB, make a throwing gesture, and release; cursor velocity becomes the shared launch velocity.
- **Из-за доски: хаос** automatically stages the dice beyond the camera-far edge and throws them inward. Their
  values remain unknown until every body settles.
- **Из-за доски: прогноз** uses the same incoming trajectory, selects values at launch, and applies the existing
  airborne physical face assist.
- **Из-за доски: по ЛКМ** waits behind the far edge until the activation click has been released. The next LMB
  click on the board becomes the shared target for an honest physical throw.

The three incoming controls live in the left panel; the five spectacle controls stay on the right and the existing
guided, natural, and gather controls stay at the bottom. The incoming modes temporarily open only the far invisible
boundary wall and close it as soon as all dice cross onto the board. This overlay and its manager are created only by
the game mode assigned to `/Game/Maps/Lvl_Game_AngledRoll`.

All honest variants keep `GeneratedNumber` at zero until their bodies stop. They reuse guided-roll impact sound and
camera shake, use a bouncy temporary physical material, add angular kicks on dice-to-dice contacts, and reuse the
inset invisible board boundaries so stopped dice remain inside the visible rim. Every borrowed body, component
setting, material, collision flag, and attachment is restored when the mode finishes or is cancelled.

Run `Daiso.Dice.Spectacle.Configuration` to verify that the roll-only map owns the native overlay and manager, all
eight controls are present, every mode can enter its intended phase, honest values remain unknown at launch, and
cancellation restores the six dice.
