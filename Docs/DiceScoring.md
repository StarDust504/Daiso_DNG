# Dice scoring prototype

`UDiceScoringLibrary::CalculateDiceRollScore` is the Blueprint-facing entry point for scoring one roll of exactly six ordinary dice.

## Blueprint setup

1. Use `/Game/Data/DT_DiceScoringRules` as the rules table.
2. In Blueprint, call **Calculate Dice Roll Score** with six face values and that table.
3. Break `DiceRollScoreResult` to read `Total Score`, `Combinations`, `Unscored Dice Values`, `Is Bust`, and input validity.

`Is Bust` is true only for a valid six-dice roll whose maximum score is zero. Invalid input instead returns `Is Valid = false` and an `Error Message`.

## Automatic level connection

`/Game/Blueprints/Testing/BP_DiceScoreCollector` is already placed in `Lvl_Game`. On Begin Play it finds exactly six actors containing `DicePhysicsRollComponent`, subscribes to every `OnDiceRollFinished`, and scores after all six results arrive. The result is stored in `LastScoreResult`, broadcast through `OnRollScored`, written to the runtime log, and printed on screen for eight seconds.

No changes to `BP_Dice` or `BP_Dice_Basic` are required. If another level later contains more dice, fill the collector's `DiceActors` array explicitly with the six dice that belong to this roll.

## Existing dice selection

The existing `BP_Dice` click chain remains unchanged: clicking toggles `bIsActive` and calls `AddComboToTempArray` or `RemoveComboFromTempArray` on `GameManagerSubsystem`. Those functions now evaluate the selected subset through `CalculateSelectedDiceScore` and `DT_DiceScoringRules` instead of looking up a concatenated row name in the old combo table.

Every selection change prints the selected faces, maximum score, validity, and any unscored selected dice. A selection is valid only when every selected die is consumed by a scoring combination. Blueprint can also read `GetSelectedDiceValues`, `GetSelectedDiceScore`, and `IsCurrentDiceSelectionValid`, or subscribe to `OnDiceSelectionChanged`.

## Algorithm

The scorer builds every rule-matching subset of the six physical dice (at most 63 subsets). Dynamic programming then evaluates all non-overlapping partitions and selects the one with the highest total score. A die therefore cannot be counted by two combinations. Rule priority is used only to make equal-score results deterministic.

The implementation contains no Kingdom Come point values. Scores, ranges, faces, scaling, and straight bounds all come from the supplied Data Table. The source CSV is `SourceData/DiceScoringRules.csv`; after editing it, run `Scripts/Editor/CreateDiceScoringDataTable.py` through Unreal's Python plugin to refresh the asset without recompiling C++.

## Automated tests

In Unreal Editor, run tests matching `Daiso.Dice.Scoring`. They cover the supplied examples, a full straight, six-of-a-kind scaling, a bust, wrong dice count, invalid face values, and a missing table.
