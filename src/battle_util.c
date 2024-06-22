#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_arena.h"
#include "battle_pyramid.h"
#include "battle_util.h"
#include "pokemon.h"
#include "international_string_util.h"
#include "item.h"
#include "util.h"
#include "battle_scripts.h"
#include "random.h"
#include "text.h"
#include "safari_zone.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "trig.h"
#include "window.h"
#include "battle_message.h"
#include "battle_ai_script_commands.h"
#include "battle_controllers.h"
#include "event_data.h"
#include "link.h"
#include "field_weather.h"
#include "battle_setup.h"
#include "pokedex.h"
#include "battle_interface.h"
#include "constants/abilities.h"
#include "constants/battle_anim.h"
#include "constants/battle_move_effects.h"
#include "constants/battle_script_commands.h"
#include "constants/battle_string_ids.h"
#include "constants/hold_effects.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/songs.h"
#include "constants/species.h"
#include "constants/weather.h"
#include "constants/trainers.h"

/*
NOTE: The data and functions in this file up until (but not including) sSoundMovesTable
are actually part of battle_main.c. They needed to be moved to this file in order to
match the ROM; this is also why sSoundMovesTable's declaration is in the middle of
functions instead of at the top of the file with the other declarations.
*/

extern const u8 *const gBattleScriptsForMoveEffects[];
extern const u8 *const gBattlescriptsForBallThrow[];
extern const u8 *const gBattlescriptsForRunningByItem[];
extern const u8 *const gBattlescriptsForUsingItem[];
extern const u8 *const gBattlescriptsForSafariActions[];

static const u8 sPkblToEscapeFactor[][3] = {
    {
        [B_MSG_MON_CURIOUS]    = 0,
        [B_MSG_MON_ENTHRALLED] = 0,
        [B_MSG_MON_IGNORED]    = 0
    },{
        [B_MSG_MON_CURIOUS]    = 3,
        [B_MSG_MON_ENTHRALLED] = 5,
        [B_MSG_MON_IGNORED]    = 0
    },{
        [B_MSG_MON_CURIOUS]    = 2,
        [B_MSG_MON_ENTHRALLED] = 3,
        [B_MSG_MON_IGNORED]    = 0
    },{
        [B_MSG_MON_CURIOUS]    = 1,
        [B_MSG_MON_ENTHRALLED] = 2,
        [B_MSG_MON_IGNORED]    = 0
    },{
        [B_MSG_MON_CURIOUS]    = 1,
        [B_MSG_MON_ENTHRALLED] = 1,
        [B_MSG_MON_IGNORED]    = 0
    }
};
static const u8 sGoNearCounterToCatchFactor[] = {4, 3, 2, 1};
static const u8 sGoNearCounterToEscapeFactor[] = {4, 4, 4, 4};

void HandleAction_UseMove(void)
{
    u8 side;
    u8 var = 4;

    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];

    if (*(&gBattleStruct->absentBattlerFlags) & gBitTable[gBattlerAttacker])
    {
        gCurrentActionFuncId = B_ACTION_FINISHED;
        return;
    }

    gCritMultiplier = 1;
    gBattleScripting.dmgMultiplier = 1;
    gBattleStruct->atkCancellerTracker = 0;
    gMoveResultFlags = 0;
    gMultiHitCounter = 0;
    gBattleCommunication[MISS_TYPE] = 0;
    gCurrMovePos = gChosenMovePos = *(gBattleStruct->chosenMovePositions + gBattlerAttacker);

    // choose move
    if (gProtectStructs[gBattlerAttacker].noValidMoves)
    {
        gProtectStructs[gBattlerAttacker].noValidMoves = FALSE;
        gCurrentMove = gChosenMove = MOVE_STRUGGLE;
        gHitMarker |= HITMARKER_NO_PPDEDUCT;
        *(gBattleStruct->moveTarget + gBattlerAttacker) = GetMoveTarget(MOVE_STRUGGLE, NO_TARGET_OVERRIDE);
    }
    else if (gBattleMons[gBattlerAttacker].status2 & STATUS2_MULTIPLETURNS || gBattleMons[gBattlerAttacker].status2 & STATUS2_RECHARGE)
    {
        gCurrentMove = gChosenMove = gLockedMoves[gBattlerAttacker];
    }
    // encore forces you to use the same move
    else if (gDisableStructs[gBattlerAttacker].encoredMove != MOVE_NONE
             && gDisableStructs[gBattlerAttacker].encoredMove == gBattleMons[gBattlerAttacker].moves[gDisableStructs[gBattlerAttacker].encoredMovePos])
    {
        gCurrentMove = gChosenMove = gDisableStructs[gBattlerAttacker].encoredMove;
        gCurrMovePos = gChosenMovePos = gDisableStructs[gBattlerAttacker].encoredMovePos;
        *(gBattleStruct->moveTarget + gBattlerAttacker) = GetMoveTarget(gCurrentMove, NO_TARGET_OVERRIDE);
    }
    // check if the encored move wasn't overwritten
    else if (gDisableStructs[gBattlerAttacker].encoredMove != MOVE_NONE
             && gDisableStructs[gBattlerAttacker].encoredMove != gBattleMons[gBattlerAttacker].moves[gDisableStructs[gBattlerAttacker].encoredMovePos])
    {
        gCurrMovePos = gChosenMovePos = gDisableStructs[gBattlerAttacker].encoredMovePos;
        gCurrentMove = gChosenMove = gBattleMons[gBattlerAttacker].moves[gCurrMovePos];
        gDisableStructs[gBattlerAttacker].encoredMove = MOVE_NONE;
        gDisableStructs[gBattlerAttacker].encoredMovePos = 0;
        gDisableStructs[gBattlerAttacker].encoreTimer = 0;
        *(gBattleStruct->moveTarget + gBattlerAttacker) = GetMoveTarget(gCurrentMove, NO_TARGET_OVERRIDE);
    }
    else if (gBattleMons[gBattlerAttacker].moves[gCurrMovePos] != gChosenMoveByBattler[gBattlerAttacker])
    {
        gCurrentMove = gChosenMove = gBattleMons[gBattlerAttacker].moves[gCurrMovePos];
        *(gBattleStruct->moveTarget + gBattlerAttacker) = GetMoveTarget(gCurrentMove, NO_TARGET_OVERRIDE);
    }
    else
    {
        gCurrentMove = gChosenMove = gBattleMons[gBattlerAttacker].moves[gCurrMovePos];
    }

    if (gBattleMons[gBattlerAttacker].hp != 0)
    {
        if (GetBattlerSide(gBattlerAttacker) == B_SIDE_PLAYER)
            gBattleResults.lastUsedMovePlayer = gCurrentMove;
        else
            gBattleResults.lastUsedMoveOpponent = gCurrentMove;
    }

    // choose target
    side = BATTLE_OPPOSITE(GetBattlerSide(gBattlerAttacker));
    if (gSideTimers[side].followmeTimer != 0
        && gBattleMoves[gCurrentMove].target == MOVE_TARGET_SELECTED
        && GetBattlerSide(gBattlerAttacker) != GetBattlerSide(gSideTimers[side].followmeTarget)
        && gBattleMons[gSideTimers[side].followmeTarget].hp != 0)
    {
        gBattlerTarget = gSideTimers[side].followmeTarget;
    }
    else if ((gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
             && gSideTimers[side].followmeTimer == 0
             && (gBattleMoves[gCurrentMove].power != 0
                 || gBattleMoves[gCurrentMove].target != MOVE_TARGET_USER)
             && gBattleMons[*(gBattleStruct->moveTarget + gBattlerAttacker)].ability != ABILITY_LIGHTNING_ROD
             && gBattleMoves[gCurrentMove].type == TYPE_ELECTRIC)
    {
        side = GetBattlerSide(gBattlerAttacker);
        for (gActiveBattler = 0; gActiveBattler < gBattlersCount; gActiveBattler++)
        {
            if (side != GetBattlerSide(gActiveBattler)
                && *(gBattleStruct->moveTarget + gBattlerAttacker) != gActiveBattler
                && gBattleMons[gActiveBattler].ability == ABILITY_LIGHTNING_ROD
                && GetBattlerTurnOrderNum(gActiveBattler) < var)
            {
                var = GetBattlerTurnOrderNum(gActiveBattler);
            }
        }
        if (var == 4)
        {
            if (gBattleMoves[gChosenMove].target & MOVE_TARGET_RANDOM)
            {
                if (GetBattlerSide(gBattlerAttacker) == B_SIDE_PLAYER)
                {
                    if (Random() & 1)
                        gBattlerTarget = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
                    else
                        gBattlerTarget = GetBattlerAtPosition(B_POSITION_OPPONENT_RIGHT);
                }
                else
                {
                    if (Random() & 1)
                        gBattlerTarget = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
                    else
                        gBattlerTarget = GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT);
                }
            }
            else
            {
                gBattlerTarget = *(gBattleStruct->moveTarget + gBattlerAttacker);
            }

            if (gAbsentBattlerFlags & gBitTable[gBattlerTarget])
            {
                if (GetBattlerSide(gBattlerAttacker) != GetBattlerSide(gBattlerTarget))
                {
                    gBattlerTarget = GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gBattlerTarget)));
                }
                else
                {
                    gBattlerTarget = GetBattlerAtPosition(BATTLE_OPPOSITE(GetBattlerPosition(gBattlerAttacker)));
                    if (gAbsentBattlerFlags & gBitTable[gBattlerTarget])
                        gBattlerTarget = GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gBattlerTarget)));
                }
            }
        }
        else
        {
            gActiveBattler = gBattlerByTurnOrder[var];
            RecordAbilityBattle(gActiveBattler, gBattleMons[gActiveBattler].ability);
            gSpecialStatuses[gActiveBattler].lightningRodRedirected = 1;
            gBattlerTarget = gActiveBattler;
        }
    }
    else if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE
             && gBattleMoves[gChosenMove].target & MOVE_TARGET_RANDOM)
    {
        if (GetBattlerSide(gBattlerAttacker) == B_SIDE_PLAYER)
        {
            if (Random() & 1)
                gBattlerTarget = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
            else
                gBattlerTarget = GetBattlerAtPosition(B_POSITION_OPPONENT_RIGHT);
        }
        else
        {
            if (Random() & 1)
                gBattlerTarget = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
            else
                gBattlerTarget = GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT);
        }

        if (gAbsentBattlerFlags & gBitTable[gBattlerTarget]
            && GetBattlerSide(gBattlerAttacker) != GetBattlerSide(gBattlerTarget))
        {
            gBattlerTarget = GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gBattlerTarget)));
        }
    }
    else
    {
        gBattlerTarget = *(gBattleStruct->moveTarget + gBattlerAttacker);
        if (gAbsentBattlerFlags & gBitTable[gBattlerTarget])
        {
            if (GetBattlerSide(gBattlerAttacker) != GetBattlerSide(gBattlerTarget))
            {
                gBattlerTarget = GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gBattlerTarget)));
            }
            else
            {
                gBattlerTarget = GetBattlerAtPosition(BATTLE_OPPOSITE(GetBattlerPosition(gBattlerAttacker)));
                if (gAbsentBattlerFlags & gBitTable[gBattlerTarget])
                    gBattlerTarget = GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gBattlerTarget)));
            }
        }
    }

    if (gBattleTypeFlags & BATTLE_TYPE_PALACE && gProtectStructs[gBattlerAttacker].palaceUnableToUseMove)
    {
        // Battle Palace, select battle script for failure to use move
        if (gBattleMons[gBattlerAttacker].hp == 0)
        {
            gCurrentActionFuncId = B_ACTION_FINISHED;
            return;
        }
        else if (gPalaceSelectionBattleScripts[gBattlerAttacker] != NULL)
        {
            gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_INCAPABLE_OF_POWER;
            gBattlescriptCurrInstr = gPalaceSelectionBattleScripts[gBattlerAttacker];
            gPalaceSelectionBattleScripts[gBattlerAttacker] = NULL;
        }
        else
        {
            gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_INCAPABLE_OF_POWER;
            gBattlescriptCurrInstr = BattleScript_MoveUsedLoafingAround;
        }
    }
    else
    {
        gBattlescriptCurrInstr = gBattleScriptsForMoveEffects[gBattleMoves[gCurrentMove].effect];
    }

    if (gBattleTypeFlags & BATTLE_TYPE_ARENA)
        BattleArena_AddMindPoints(gBattlerAttacker);

    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
}

void HandleAction_Switch(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;
    gActionSelectionCursor[gBattlerAttacker] = 0;
    gMoveSelectionCursor[gBattlerAttacker] = 0;

    PREPARE_MON_NICK_BUFFER(gBattleTextBuff1, gBattlerAttacker, *(gBattleStruct->battlerPartyIndexes + gBattlerAttacker))

    gBattleScripting.battler = gBattlerAttacker;
    gBattlescriptCurrInstr = BattleScript_ActionSwitch;
    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;

    if (gBattleResults.playerSwitchesCounter < 255)
        gBattleResults.playerSwitchesCounter++;
}

void HandleAction_UseItem(void)
{
    gBattlerAttacker = gBattlerTarget = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;
    ClearFuryCutterDestinyBondGrudge(gBattlerAttacker);
    gLastUsedItem = gBattleBufferB[gBattlerAttacker][1] | (gBattleBufferB[gBattlerAttacker][2] << 8);

    if (gLastUsedItem <= LAST_BALL) // is ball
    {
        gBattlescriptCurrInstr = gBattlescriptsForBallThrow[gLastUsedItem];
    }
    else if (gLastUsedItem == ITEM_POKE_DOLL || gLastUsedItem == ITEM_FLUFFY_TAIL)
    {
        gBattlescriptCurrInstr = gBattlescriptsForRunningByItem[0]; // BattleScript_RunByUsingItem
    }
    else if (GetBattlerSide(gBattlerAttacker) == B_SIDE_PLAYER)
    {
        gBattlescriptCurrInstr = gBattlescriptsForUsingItem[0]; // BattleScript_PlayerUsesItem
    }
    else
    {
        gBattleScripting.battler = gBattlerAttacker;

        switch (*(gBattleStruct->AI_itemType + (gBattlerAttacker >> 1)))
        {
        case AI_ITEM_FULL_RESTORE:
        case AI_ITEM_HEAL_HP:
            break;
        case AI_ITEM_CURE_CONDITION:
            gBattleCommunication[MULTISTRING_CHOOSER] = AI_HEAL_CONFUSION;
            if (*(gBattleStruct->AI_itemFlags + gBattlerAttacker / 2) & (1 << AI_HEAL_CONFUSION))
            {
                if (*(gBattleStruct->AI_itemFlags + gBattlerAttacker / 2) & ((1 << AI_HEAL_PARALYSIS)
                                                                           | (1 << AI_HEAL_FREEZE)
                                                                           | (1 << AI_HEAL_BURN)
                                                                           | (1 << AI_HEAL_POISON)
                                                                           | (1 << AI_HEAL_SLEEP)))
                    gBattleCommunication[MULTISTRING_CHOOSER] = AI_HEAL_SLEEP;
            }
            else
            {
                // Check for other statuses, stopping at first (shouldn't be more than one)
                while (!(*(gBattleStruct->AI_itemFlags + gBattlerAttacker / 2) & 1))
                {
                    *(gBattleStruct->AI_itemFlags + gBattlerAttacker / 2) >>= 1;
                    gBattleCommunication[MULTISTRING_CHOOSER]++;
                    // MULTISTRING_CHOOSER will be either AI_HEAL_PARALYSIS, AI_HEAL_FREEZE,
                    // AI_HEAL_BURN, AI_HEAL_POISON, or AI_HEAL_SLEEP
                }
            }
            break;
        case AI_ITEM_X_STAT:
            gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_STAT_ROSE_ITEM;
            if (*(gBattleStruct->AI_itemFlags + (gBattlerAttacker >> 1)) & (1 << AI_DIRE_HIT))
            {
                gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_USED_DIRE_HIT;
            }
            else
            {
                PREPARE_STAT_BUFFER(gBattleTextBuff1, STAT_ATK)
                PREPARE_STRING_BUFFER(gBattleTextBuff2, CHAR_X)

                while (!((*(gBattleStruct->AI_itemFlags + (gBattlerAttacker >> 1))) & 1))
                {
                    *(gBattleStruct->AI_itemFlags + gBattlerAttacker / 2) >>= 1;
                    gBattleTextBuff1[2]++;
                }

                gBattleScripting.animArg1 = gBattleTextBuff1[2] + STAT_ANIM_PLUS1;
                gBattleScripting.animArg2 = 0;
            }
            break;
        case AI_ITEM_GUARD_SPEC:
            // It seems probable that at some point there was a special message for
            // an AI trainer using Guard Spec in a double battle.
            // There isn't now however, and the assignment to 2 below goes out of
            // bounds for gMistUsedStringIds and instead prints "{mon} is getting pumped"
            // from the next table, gFocusEnergyUsedStringIds.
            // In any case this isn't an issue in the retail version, as no trainers
            // are ever given any Guard Spec to use.
#ifndef UBFIX
            if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
                gBattleCommunication[MULTISTRING_CHOOSER] = 2;
            else
#endif
                gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_SET_MIST;
            break;
        }

        gBattlescriptCurrInstr = gBattlescriptsForUsingItem[*(gBattleStruct->AI_itemType + gBattlerAttacker / 2)];
    }
    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
}

bool8 TryRunFromBattle(u8 battler)
{
    bool8 effect = FALSE;
    u8 holdEffect;
    u8 pyramidMultiplier;
    u8 speedVar;

    if (gBattleMons[battler].item == ITEM_ENIGMA_BERRY)
        holdEffect = gEnigmaBerries[battler].holdEffect;
    else
        holdEffect = ItemId_GetHoldEffect(gBattleMons[battler].item);

    gPotentialItemEffectBattler = battler;

    if (holdEffect == HOLD_EFFECT_CAN_ALWAYS_RUN)
    {
        gLastUsedItem = gBattleMons[battler].item;
        gProtectStructs[battler].fleeType = FLEE_ITEM;
        effect++;
    }
    else if (gBattleMons[battler].ability == ABILITY_RUN_AWAY)
    {
        if (InBattlePyramid())
        {
            gBattleStruct->runTries++;
            pyramidMultiplier = GetPyramidRunMultiplier();
            speedVar = (gBattleMons[battler].speed * pyramidMultiplier) / (gBattleMons[BATTLE_OPPOSITE(battler)].speed) + (gBattleStruct->runTries * 30);
            if (speedVar > (Random() & 0xFF))
            {
                gLastUsedAbility = ABILITY_RUN_AWAY;
                gProtectStructs[battler].fleeType = FLEE_ABILITY;
                effect++;
            }
        }
        else
        {
            gLastUsedAbility = ABILITY_RUN_AWAY;
            gProtectStructs[battler].fleeType = FLEE_ABILITY;
            effect++;
        }
    }
    else if (gBattleTypeFlags & (BATTLE_TYPE_FRONTIER | BATTLE_TYPE_TRAINER_HILL) && gBattleTypeFlags & BATTLE_TYPE_TRAINER)
    {
        effect++;
    }
    else
    {
        if (!(gBattleTypeFlags & BATTLE_TYPE_DOUBLE))
        {
            if (InBattlePyramid())
            {
                pyramidMultiplier = GetPyramidRunMultiplier();
                speedVar = (gBattleMons[battler].speed * pyramidMultiplier) / (gBattleMons[BATTLE_OPPOSITE(battler)].speed) + (gBattleStruct->runTries * 30);
                if (speedVar > (Random() & 0xFF))
                    effect++;
            }
            else if (gBattleMons[battler].speed < gBattleMons[BATTLE_OPPOSITE(battler)].speed)
            {
                speedVar = (gBattleMons[battler].speed * 128) / (gBattleMons[BATTLE_OPPOSITE(battler)].speed) + (gBattleStruct->runTries * 30);
                if (speedVar > (Random() & 0xFF))
                    effect++;
            }
            else // same speed or faster
            {
                effect++;
            }
        }

        gBattleStruct->runTries++;
    }

    if (effect != 0)
    {
        gCurrentTurnActionNumber = gBattlersCount;
        gBattleOutcome = B_OUTCOME_RAN;
    }

    return effect;
}

void HandleAction_Run(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];

    if (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED_LINK))
    {
        gCurrentTurnActionNumber = gBattlersCount;

        for (gActiveBattler = 0; gActiveBattler < gBattlersCount; gActiveBattler++)
        {
            if (GetBattlerSide(gActiveBattler) == B_SIDE_PLAYER)
            {
                if (gChosenActionByBattler[gActiveBattler] == B_ACTION_RUN)
                    gBattleOutcome |= B_OUTCOME_LOST;
            }
            else
            {
                if (gChosenActionByBattler[gActiveBattler] == B_ACTION_RUN)
                    gBattleOutcome |= B_OUTCOME_WON;
            }
        }

        gBattleOutcome |= B_OUTCOME_LINK_BATTLE_RAN;
        gSaveBlock2Ptr->frontier.disableRecordBattle = TRUE;
    }
    else
    {
        if (GetBattlerSide(gBattlerAttacker) == B_SIDE_PLAYER)
        {
            if (!TryRunFromBattle(gBattlerAttacker)) // failed to run away
            {
                ClearFuryCutterDestinyBondGrudge(gBattlerAttacker);
                gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_CANT_ESCAPE_2;
                gBattlescriptCurrInstr = BattleScript_PrintFailedToRunString;
                gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
            }
        }
        else
        {
            if (gBattleMons[gBattlerAttacker].status2 & (STATUS2_WRAPPED | STATUS2_ESCAPE_PREVENTION))
            {
                gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_ATTACKER_CANT_ESCAPE;
                gBattlescriptCurrInstr = BattleScript_PrintFailedToRunString;
                gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
            }
            else
            {
                gCurrentTurnActionNumber = gBattlersCount;
                gBattleOutcome = B_OUTCOME_MON_FLED;
            }
        }
    }
}

void HandleAction_WatchesCarefully(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;
    gBattlescriptCurrInstr = gBattlescriptsForSafariActions[0];
    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
}

void HandleAction_SafariZoneBallThrow(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;
    gNumSafariBalls--;
    gLastUsedItem = ITEM_SAFARI_BALL;
    gBattlescriptCurrInstr = gBattlescriptsForBallThrow[ITEM_SAFARI_BALL];
    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
}

void HandleAction_ThrowPokeblock(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;
    gBattleCommunication[MULTISTRING_CHOOSER] = gBattleBufferB[gBattlerAttacker][1] - 1;
    gLastUsedItem = gBattleBufferB[gBattlerAttacker][2];

    if (gBattleResults.pokeblockThrows < 255)
        gBattleResults.pokeblockThrows++;
    if (gBattleStruct->safariPkblThrowCounter < 3)
        gBattleStruct->safariPkblThrowCounter++;
    if (gBattleStruct->safariEscapeFactor > 1)
    {
        // BUG: safariEscapeFactor can become 0 below. This causes the pokeblock throw glitch.
        #ifdef BUGFIX
        if (gBattleStruct->safariEscapeFactor <= sPkblToEscapeFactor[gBattleStruct->safariPkblThrowCounter][gBattleCommunication[MULTISTRING_CHOOSER]])
        #else
        if (gBattleStruct->safariEscapeFactor < sPkblToEscapeFactor[gBattleStruct->safariPkblThrowCounter][gBattleCommunication[MULTISTRING_CHOOSER]])
        #endif
            gBattleStruct->safariEscapeFactor = 1;
        else
            gBattleStruct->safariEscapeFactor -= sPkblToEscapeFactor[gBattleStruct->safariPkblThrowCounter][gBattleCommunication[MULTISTRING_CHOOSER]];
    }

    gBattlescriptCurrInstr = gBattlescriptsForSafariActions[2];
    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
}

void HandleAction_GoNear(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;

    gBattleStruct->safariCatchFactor += sGoNearCounterToCatchFactor[gBattleStruct->safariGoNearCounter];
    if (gBattleStruct->safariCatchFactor > 20)
        gBattleStruct->safariCatchFactor = 20;

    gBattleStruct->safariEscapeFactor += sGoNearCounterToEscapeFactor[gBattleStruct->safariGoNearCounter];
    if (gBattleStruct->safariEscapeFactor > 20)
        gBattleStruct->safariEscapeFactor = 20;

    if (gBattleStruct->safariGoNearCounter < 3)
    {
        gBattleStruct->safariGoNearCounter++;
        gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_CREPT_CLOSER;
    }
    else
    {
        gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_CANT_GET_CLOSER;
    }
    gBattlescriptCurrInstr = gBattlescriptsForSafariActions[1];
    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
}

void HandleAction_SafariZoneRun(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    PlaySE(SE_FLEE);
    gCurrentTurnActionNumber = gBattlersCount;
    gBattleOutcome = B_OUTCOME_RAN;
}

void HandleAction_WallyBallThrow(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;

    PREPARE_MON_NICK_BUFFER(gBattleTextBuff1, gBattlerAttacker, gBattlerPartyIndexes[gBattlerAttacker])

    gBattlescriptCurrInstr = gBattlescriptsForSafariActions[3];
    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
    gActionsByTurnOrder[1] = B_ACTION_FINISHED;
}

void HandleAction_TryFinish(void)
{
    if (!HandleFaintedMonActions())
    {
        gBattleStruct->faintedActionsState = 0;
        gCurrentActionFuncId = B_ACTION_FINISHED;
    }
}

void HandleAction_NothingIsFainted(void)
{
    gCurrentTurnActionNumber++;
    gCurrentActionFuncId = gActionsByTurnOrder[gCurrentTurnActionNumber];
    gHitMarker &= ~(HITMARKER_DESTINYBOND | HITMARKER_IGNORE_SUBSTITUTE | HITMARKER_ATTACKSTRING_PRINTED
                    | HITMARKER_NO_PPDEDUCT | HITMARKER_STATUS_ABILITY_EFFECT | HITMARKER_IGNORE_ON_AIR
                    | HITMARKER_IGNORE_UNDERGROUND | HITMARKER_IGNORE_UNDERWATER | HITMARKER_PASSIVE_DAMAGE
                    | HITMARKER_OBEYS | HITMARKER_WAKE_UP_CLEAR | HITMARKER_SYNCHRONISE_EFFECT
                    | HITMARKER_CHARGING | HITMARKER_NEVER_SET);
}

void HandleAction_ActionFinished(void)
{
    *(gBattleStruct->monToSwitchIntoId + gBattlerByTurnOrder[gCurrentTurnActionNumber]) = PARTY_SIZE;
    gCurrentTurnActionNumber++;
    gCurrentActionFuncId = gActionsByTurnOrder[gCurrentTurnActionNumber];
    SpecialStatusesClear();
    gHitMarker &= ~(HITMARKER_DESTINYBOND | HITMARKER_IGNORE_SUBSTITUTE | HITMARKER_ATTACKSTRING_PRINTED
                    | HITMARKER_NO_PPDEDUCT | HITMARKER_STATUS_ABILITY_EFFECT | HITMARKER_IGNORE_ON_AIR
                    | HITMARKER_IGNORE_UNDERGROUND | HITMARKER_IGNORE_UNDERWATER | HITMARKER_PASSIVE_DAMAGE
                    | HITMARKER_OBEYS | HITMARKER_WAKE_UP_CLEAR | HITMARKER_SYNCHRONISE_EFFECT
                    | HITMARKER_CHARGING | HITMARKER_NEVER_SET);

    gCurrentMove = 0;
    gBattleMoveDamage = 0;
    gMoveResultFlags = 0;
    gBattleScripting.animTurn = 0;
    gBattleScripting.animTargetsHit = 0;
    gLastLandedMoves[gBattlerAttacker] = 0;
    gLastHitByType[gBattlerAttacker] = 0;
    gBattleStruct->dynamicMoveType = 0;
    gDynamicBasePower = 0;
    gBattleScripting.moveendState = 0;
    gBattleCommunication[3] = 0;
    gBattleCommunication[4] = 0;
    gBattleScripting.multihitMoveEffect = 0;
    gBattleResources->battleScriptsStack->size = 0;
}

#define SOUND_MOVES_END 0xFFFF

static const u16 sSoundMovesTable[] =
{
    MOVE_GROWL, MOVE_ROAR, MOVE_SING, MOVE_SUPERSONIC, MOVE_SCREECH, MOVE_SNORE,
    MOVE_UPROAR, MOVE_METAL_SOUND, MOVE_GRASS_WHISTLE, MOVE_HYPER_VOICE, SOUND_MOVES_END
};

u8 GetBattlerForBattleScript(u8 caseId)
{
    u8 ret = 0;
    switch (caseId)
    {
    case BS_TARGET:
        ret = gBattlerTarget;
        break;
    case BS_ATTACKER:
        ret = gBattlerAttacker;
        break;
    case BS_EFFECT_BATTLER:
        ret = gEffectBattler;
        break;
    case BS_BATTLER_0:
        ret = 0;
        break;
    case BS_SCRIPTING:
        ret = gBattleScripting.battler;
        break;
    case BS_FAINTED:
        ret = gBattlerFainted;
        break;
    case BS_FAINTED_LINK_MULTIPLE_1:
        ret = gBattlerFainted;
        break;
    case BS_ATTACKER_WITH_PARTNER:
    case BS_FAINTED_LINK_MULTIPLE_2:
    case BS_ATTACKER_SIDE:
    case BS_NOT_ATTACKER_SIDE:
    case BS_PLAYER1:
        ret = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
        break;
    case BS_OPPONENT1:
        ret = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
        break;
    case BS_PLAYER2:
        ret = GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT);
        break;
    case BS_OPPONENT2:
        ret = GetBattlerAtPosition(B_POSITION_OPPONENT_RIGHT);
        break;
    }
    return ret;
}

void PressurePPLose(u8 target, u8 attacker, u16 move)
{
    int moveIndex;

    if (gBattleMons[target].ability != ABILITY_PRESSURE)
        return;

    for (moveIndex = 0; moveIndex < MAX_MON_MOVES; moveIndex++)
    {
        if (gBattleMons[attacker].moves[moveIndex] == move)
            break;
    }

    if (moveIndex == MAX_MON_MOVES)
        return;

    if (gBattleMons[attacker].pp[moveIndex] != 0)
        gBattleMons[attacker].pp[moveIndex]--;

    if (MOVE_IS_PERMANENT(attacker, moveIndex))
    {
        gActiveBattler = attacker;
        BtlController_EmitSetMonData(BUFFER_A, REQUEST_PPMOVE1_BATTLE + moveIndex, 0, 1, &gBattleMons[gActiveBattler].pp[moveIndex]);
        MarkBattlerForControllerExec(gActiveBattler);
    }
}

void PressurePPLoseOnUsingImprison(u8 attacker)
{
    int i, j;
    int imprisonPos = MAX_MON_MOVES;
    u8 atkSide = GetBattlerSide(attacker);

    for (i = 0; i < gBattlersCount; i++)
    {
        if (atkSide != GetBattlerSide(i) && gBattleMons[i].ability == ABILITY_PRESSURE)
        {
            for (j = 0; j < MAX_MON_MOVES; j++)
            {
                if (gBattleMons[attacker].moves[j] == MOVE_IMPRISON)
                    break;
            }
            if (j != MAX_MON_MOVES)
            {
                imprisonPos = j;
                if (gBattleMons[attacker].pp[j] != 0)
                    gBattleMons[attacker].pp[j]--;
            }
        }
    }

    if (imprisonPos != MAX_MON_MOVES && MOVE_IS_PERMANENT(attacker, imprisonPos))
    {
        gActiveBattler = attacker;
        BtlController_EmitSetMonData(BUFFER_A, REQUEST_PPMOVE1_BATTLE + imprisonPos, 0, 1, &gBattleMons[gActiveBattler].pp[imprisonPos]);
        MarkBattlerForControllerExec(gActiveBattler);
    }
}

void PressurePPLoseOnUsingPerishSong(u8 attacker)
{
    int i, j;
    int perishSongPos = MAX_MON_MOVES;

    for (i = 0; i < gBattlersCount; i++)
    {
        if (gBattleMons[i].ability == ABILITY_PRESSURE && i != attacker)
        {
            for (j = 0; j < MAX_MON_MOVES; j++)
            {
                if (gBattleMons[attacker].moves[j] == MOVE_PERISH_SONG)
                    break;
            }
            if (j != MAX_MON_MOVES)
            {
                perishSongPos = j;
                if (gBattleMons[attacker].pp[j] != 0)
                    gBattleMons[attacker].pp[j]--;
            }
        }
    }

    if (perishSongPos != MAX_MON_MOVES && MOVE_IS_PERMANENT(attacker, perishSongPos))
    {
        gActiveBattler = attacker;
        BtlController_EmitSetMonData(BUFFER_A, REQUEST_PPMOVE1_BATTLE + perishSongPos, 0, 1, &gBattleMons[gActiveBattler].pp[perishSongPos]);
        MarkBattlerForControllerExec(gActiveBattler);
    }
}

static void UNUSED MarkAllBattlersForControllerExec(void)
{
    int i;

    if (gBattleTypeFlags & BATTLE_TYPE_LINK)
    {
        for (i = 0; i < gBattlersCount; i++)
            gBattleControllerExecFlags |= gBitTable[i] << (32 - MAX_BATTLERS_COUNT);
    }
    else
    {
        for (i = 0; i < gBattlersCount; i++)
            gBattleControllerExecFlags |= gBitTable[i];
    }
}

void MarkBattlerForControllerExec(u8 battlerId)
{
    if (gBattleTypeFlags & BATTLE_TYPE_LINK)
        gBattleControllerExecFlags |= gBitTable[battlerId] << (32 - MAX_BATTLERS_COUNT);
    else
        gBattleControllerExecFlags |= gBitTable[battlerId];
}

void MarkBattlerReceivedLinkData(u8 battlerId)
{
    s32 i;

    for (i = 0; i < GetLinkPlayerCount(); i++)
        gBattleControllerExecFlags |= gBitTable[battlerId] << (i << 2);

    gBattleControllerExecFlags &= ~((1 << 28) << battlerId);
}

void CancelMultiTurnMoves(u8 battler)
{
    gBattleMons[battler].status2 &= ~STATUS2_MULTIPLETURNS;
    gBattleMons[battler].status2 &= ~STATUS2_LOCK_CONFUSE;
    gBattleMons[battler].status2 &= ~STATUS2_UPROAR;
    gBattleMons[battler].status2 &= ~STATUS2_BIDE;

    gStatuses3[battler] &= ~STATUS3_SEMI_INVULNERABLE;

    gDisableStructs[battler].rolloutTimer = 0;
    gDisableStructs[battler].furyCutterCounter = 0;
}

bool8 WasUnableToUseMove(u8 battler)
{
    if (gProtectStructs[battler].prlzImmobility
        || gProtectStructs[battler].targetNotAffected
        || gProtectStructs[battler].usedImprisonedMove
        || gProtectStructs[battler].loveImmobility
        || gProtectStructs[battler].usedDisabledMove
        || gProtectStructs[battler].usedTauntedMove
        || gProtectStructs[battler].flag2Unknown
        || gProtectStructs[battler].flinchImmobility
        || gProtectStructs[battler].confusionSelfDmg)
        return TRUE;
    else
        return FALSE;
}

void PrepareStringBattle(u16 stringId, u8 battler)
{
    gActiveBattler = battler;
    BtlController_EmitPrintString(BUFFER_A, stringId);
    MarkBattlerForControllerExec(gActiveBattler);
}

void ResetSentPokesToOpponentValue(void)
{
    s32 i;
    u32 bits = 0;

    gSentPokesToOpponent[0] = 0;
    gSentPokesToOpponent[1] = 0;

    for (i = 0; i < gBattlersCount; i += 2)
        bits |= gBitTable[gBattlerPartyIndexes[i]];

    for (i = 1; i < gBattlersCount; i += 2)
        gSentPokesToOpponent[(i & BIT_FLANK) >> 1] = bits;
}

void OpponentSwitchInResetSentPokesToOpponentValue(u8 battler)
{
    s32 i = 0;
    u32 bits = 0;

    if (GetBattlerSide(battler) == B_SIDE_OPPONENT)
    {
        u8 flank = ((battler & BIT_FLANK) >> 1);
        gSentPokesToOpponent[flank] = 0;

        for (i = 0; i < gBattlersCount; i += 2)
        {
            if (!(gAbsentBattlerFlags & gBitTable[i]))
                bits |= gBitTable[gBattlerPartyIndexes[i]];
        }
        gSentPokesToOpponent[flank] = bits;
    }
}

void UpdateSentPokesToOpponentValue(u8 battler)
{
    if (GetBattlerSide(battler) == B_SIDE_OPPONENT)
    {
        OpponentSwitchInResetSentPokesToOpponentValue(battler);
    }
    else
    {
        s32 i;
        for (i = 1; i < gBattlersCount; i++)
            gSentPokesToOpponent[(i & BIT_FLANK) >> 1] |= gBitTable[gBattlerPartyIndexes[battler]];
    }
}

void BattleScriptPush(const u8 *bsPtr)
{
    gBattleResources->battleScriptsStack->ptr[gBattleResources->battleScriptsStack->size++] = bsPtr;
}

void BattleScriptPushCursor(void)
{
    gBattleResources->battleScriptsStack->ptr[gBattleResources->battleScriptsStack->size++] = gBattlescriptCurrInstr;
}

void BattleScriptPop(void)
{
    gBattlescriptCurrInstr = gBattleResources->battleScriptsStack->ptr[--gBattleResources->battleScriptsStack->size];
}

u8 TrySetCantSelectMoveBattleScript(void)
{
    u8 limitations = 0;
    u16 move = gBattleMons[gActiveBattler].moves[gBattleBufferB[gActiveBattler][2]];
    u8 holdEffect;
    u16 *choicedMove = &gBattleStruct->choicedMove[gActiveBattler];

    if (gDisableStructs[gActiveBattler].disabledMove == move && move != MOVE_NONE)
    {
        gBattleScripting.battler = gActiveBattler;
        gCurrentMove = move;
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gPalaceSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingDisabledMoveInPalace;
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = TRUE;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingDisabledMove;
            limitations = 1;
        }
    }

    if (move == gLastMoves[gActiveBattler] && move != MOVE_STRUGGLE && (gBattleMons[gActiveBattler].status2 & STATUS2_TORMENT))
    {
        CancelMultiTurnMoves(gActiveBattler);
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gPalaceSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingTormentedMoveInPalace;
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = TRUE;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingTormentedMove;
            limitations++;
        }
    }

    if (gDisableStructs[gActiveBattler].tauntTimer != 0 && gBattleMoves[move].power == 0)
    {
        gCurrentMove = move;
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gPalaceSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingNotAllowedMoveTauntInPalace;
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = TRUE;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingNotAllowedMoveTaunt;
            limitations++;
        }
    }

    if (GetImprisonedMovesCount(gActiveBattler, move))
    {
        gCurrentMove = move;
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gPalaceSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingImprisonedMoveInPalace;
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = TRUE;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingImprisonedMove;
            limitations++;
        }
    }

    if (gBattleMons[gActiveBattler].item == ITEM_ENIGMA_BERRY)
        holdEffect = gEnigmaBerries[gActiveBattler].holdEffect;
    else
        holdEffect = ItemId_GetHoldEffect(gBattleMons[gActiveBattler].item);

    gPotentialItemEffectBattler = gActiveBattler;

    if (holdEffect == HOLD_EFFECT_CHOICE_BAND && *choicedMove != MOVE_NONE && *choicedMove != MOVE_UNAVAILABLE && *choicedMove != move)
    {
        gCurrentMove = *choicedMove;
        gLastUsedItem = gBattleMons[gActiveBattler].item;
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = TRUE;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingNotAllowedMoveChoiceItem;
            limitations++;
        }
    }

    if (gBattleMons[gActiveBattler].pp[gBattleBufferB[gActiveBattler][2]] == 0)
    {
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = TRUE;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingMoveWithNoPP;
            limitations++;
        }
    }

    return limitations;
}

u8 CheckMoveLimitations(u8 battlerId, u8 unusableMoves, u8 check)
{
    u8 holdEffect;
    u16 *choicedMove = &gBattleStruct->choicedMove[battlerId];
    s32 i;

    if (gBattleMons[battlerId].item == ITEM_ENIGMA_BERRY)
        holdEffect = gEnigmaBerries[battlerId].holdEffect;
    else
        holdEffect = ItemId_GetHoldEffect(gBattleMons[battlerId].item);

    gPotentialItemEffectBattler = battlerId;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        // No move
        if (gBattleMons[battlerId].moves[i] == MOVE_NONE && check & MOVE_LIMITATION_ZEROMOVE)
            unusableMoves |= gBitTable[i];
        // No PP
        if (gBattleMons[battlerId].pp[i] == 0 && check & MOVE_LIMITATION_PP)
            unusableMoves |= gBitTable[i];
        // Disable
        if (gBattleMons[battlerId].moves[i] == gDisableStructs[battlerId].disabledMove && check & MOVE_LIMITATION_DISABLED)
            unusableMoves |= gBitTable[i];
        // Torment
        if (gBattleMons[battlerId].moves[i] == gLastMoves[battlerId] && check & MOVE_LIMITATION_TORMENTED && gBattleMons[battlerId].status2 & STATUS2_TORMENT)
            unusableMoves |= gBitTable[i];
        // Taunt
        if (gDisableStructs[battlerId].tauntTimer && check & MOVE_LIMITATION_TAUNT && gBattleMoves[gBattleMons[battlerId].moves[i]].power == 0)
            unusableMoves |= gBitTable[i];
        // Imprison
        if (GetImprisonedMovesCount(battlerId, gBattleMons[battlerId].moves[i]) && check & MOVE_LIMITATION_IMPRISON)
            unusableMoves |= gBitTable[i];
        // Encore
        if (gDisableStructs[battlerId].encoreTimer && gDisableStructs[battlerId].encoredMove != gBattleMons[battlerId].moves[i])
            unusableMoves |= gBitTable[i];
        // Choice Band
        if (holdEffect == HOLD_EFFECT_CHOICE_BAND && *choicedMove != MOVE_NONE && *choicedMove != MOVE_UNAVAILABLE && *choicedMove != gBattleMons[battlerId].moves[i])
            unusableMoves |= gBitTable[i];
    }
    return unusableMoves;
}

bool8 AreAllMovesUnusable(void)
{
    u8 unusable = CheckMoveLimitations(gActiveBattler, 0, MOVE_LIMITATIONS_ALL);

    if (unusable == ALL_MOVES_MASK) // All moves are unusable.
    {
        gProtectStructs[gActiveBattler].noValidMoves = TRUE;
        gSelectionBattleScripts[gActiveBattler] = BattleScript_NoMovesLeft;
    }
    else
    {
        gProtectStructs[gActiveBattler].noValidMoves = FALSE;
    }

    return (unusable == ALL_MOVES_MASK);
}

u8 GetImprisonedMovesCount(u8 battlerId, u16 move)
{
    s32 i;
    u8 imprisonedMoves = 0;
    u8 battlerSide = GetBattlerSide(battlerId);

    for (i = 0; i < gBattlersCount; i++)
    {
        if (battlerSide != GetBattlerSide(i) && gStatuses3[i] & STATUS3_IMPRISONED_OTHERS)
        {
            s32 j;
            for (j = 0; j < MAX_MON_MOVES; j++)
            {
                if (move == gBattleMons[i].moves[j])
                    break;
            }
            if (j < MAX_MON_MOVES)
                imprisonedMoves++;
        }
    }

    return imprisonedMoves;
}

enum
{
    ENDTURN_ORDER,
    ENDTURN_REFLECT,
    ENDTURN_LIGHT_SCREEN,
    ENDTURN_MIST,
    ENDTURN_SAFEGUARD,
    ENDTURN_WISH,
    ENDTURN_RAIN,
    ENDTURN_SANDSTORM,
    ENDTURN_SUN,
    ENDTURN_HAIL,
    ENDTURN_FIELD_COUNT,
};

u8 DoFieldEndTurnEffects(void)
{
    u8 effect = 0;
    s32 i;

    for (gBattlerAttacker = 0; gBattlerAttacker < gBattlersCount && gAbsentBattlerFlags & gBitTable[gBattlerAttacker]; gBattlerAttacker++)
    {
    }
    for (gBattlerTarget = 0; gBattlerTarget < gBattlersCount && gAbsentBattlerFlags & gBitTable[gBattlerTarget]; gBattlerTarget++)
    {
    }

    do
    {
        u8 side;

        switch (gBattleStruct->turnCountersTracker)
        {
        case ENDTURN_ORDER:
            for (i = 0; i < gBattlersCount; i++)
            {
                gBattlerByTurnOrder[i] = i;
            }
            for (i = 0; i < gBattlersCount - 1; i++)
            {
                s32 j;
                for (j = i + 1; j < gBattlersCount; j++)
                {
                    if (GetWhoStrikesFirst(gBattlerByTurnOrder[i], gBattlerByTurnOrder[j], FALSE))
                        SwapTurnOrder(i, j);
                }
            }

            // It's stupid, but won't match without it
            {
                u8 *var = &gBattleStruct->turnCountersTracker;
                (*var)++;
                gBattleStruct->turnSideTracker = 0;
            }
            // fall through
        case ENDTURN_REFLECT:
            while (gBattleStruct->turnSideTracker < 2)
            {
                side = gBattleStruct->turnSideTracker;
                gActiveBattler = gBattlerAttacker = gSideTimers[side].reflectBattlerId;
                if (gSideStatuses[side] & SIDE_STATUS_REFLECT)
                {
                    if (--gSideTimers[side].reflectTimer == 0)
                    {
                        gSideStatuses[side] &= ~SIDE_STATUS_REFLECT;
                        BattleScriptExecute(BattleScript_SideStatusWoreOff);
                        PREPARE_MOVE_BUFFER(gBattleTextBuff1, MOVE_REFLECT);
                        effect++;
                    }
                }
                gBattleStruct->turnSideTracker++;
                if (effect != 0)
                    break;
            }
            if (effect == 0)
            {
                gBattleStruct->turnCountersTracker++;
                gBattleStruct->turnSideTracker = 0;
            }
            break;
        case ENDTURN_LIGHT_SCREEN:
            while (gBattleStruct->turnSideTracker < 2)
            {
                side = gBattleStruct->turnSideTracker;
                gActiveBattler = gBattlerAttacker = gSideTimers[side].lightscreenBattlerId;
                if (gSideStatuses[side] & SIDE_STATUS_LIGHTSCREEN)
                {
                    if (--gSideTimers[side].lightscreenTimer == 0)
                    {
                        gSideStatuses[side] &= ~SIDE_STATUS_LIGHTSCREEN;
                        BattleScriptExecute(BattleScript_SideStatusWoreOff);
                        gBattleCommunication[MULTISTRING_CHOOSER] = side;
                        PREPARE_MOVE_BUFFER(gBattleTextBuff1, MOVE_LIGHT_SCREEN);
                        effect++;
                    }
                }
                gBattleStruct->turnSideTracker++;
                if (effect != 0)
                    break;
            }
            if (effect == 0)
            {
                gBattleStruct->turnCountersTracker++;
                gBattleStruct->turnSideTracker = 0;
            }
            break;
        case ENDTURN_MIST:
            while (gBattleStruct->turnSideTracker < 2)
            {
                side = gBattleStruct->turnSideTracker;
                gActiveBattler = gBattlerAttacker = gSideTimers[side].mistBattlerId;
                if (gSideTimers[side].mistTimer != 0 && --gSideTimers[side].mistTimer == 0)
                {
                    gSideStatuses[side] &= ~SIDE_STATUS_MIST;
                    BattleScriptExecute(BattleScript_SideStatusWoreOff);
                    gBattleCommunication[MULTISTRING_CHOOSER] = side;
                    PREPARE_MOVE_BUFFER(gBattleTextBuff1, MOVE_MIST);
                    effect++;
                }
                gBattleStruct->turnSideTracker++;
                if (effect != 0)
                    break;
            }
            if (effect == 0)
            {
                gBattleStruct->turnCountersTracker++;
                gBattleStruct->turnSideTracker = 0;
            }
            break;
        case ENDTURN_SAFEGUARD:
            while (gBattleStruct->turnSideTracker < 2)
            {
                side = gBattleStruct->turnSideTracker;
                gActiveBattler = gBattlerAttacker = gSideTimers[side].safeguardBattlerId;
                if (gSideStatuses[side] & SIDE_STATUS_SAFEGUARD)
                {
                    if (--gSideTimers[side].safeguardTimer == 0)
                    {
                        gSideStatuses[side] &= ~SIDE_STATUS_SAFEGUARD;
                        BattleScriptExecute(BattleScript_SafeguardEnds);
                        effect++;
                    }
                }
                gBattleStruct->turnSideTracker++;
                if (effect != 0)
                    break;
            }
            if (effect == 0)
            {
                gBattleStruct->turnCountersTracker++;
                gBattleStruct->turnSideTracker = 0;
            }
            break;
        case ENDTURN_WISH:
            while (gBattleStruct->turnSideTracker < gBattlersCount)
            {
                gActiveBattler = gBattlerByTurnOrder[gBattleStruct->turnSideTracker];
                if (gWishFutureKnock.wishCounter[gActiveBattler] != 0
                 && --gWishFutureKnock.wishCounter[gActiveBattler] == 0
                 && gBattleMons[gActiveBattler].hp != 0)
                {
                    gBattlerTarget = gActiveBattler;
                    BattleScriptExecute(BattleScript_WishComesTrue);
                    effect++;
                }
                gBattleStruct->turnSideTracker++;
                if (effect != 0)
                    break;
            }
            if (effect == 0)
            {
                gBattleStruct->turnCountersTracker++;
            }
            break;
        case ENDTURN_RAIN:
            if (gBattleWeather & B_WEATHER_RAIN)
            {
                if (!(gBattleWeather & B_WEATHER_RAIN_PERMANENT))
                {
                    if (--gWishFutureKnock.weatherDuration == 0)
                    {
                        gBattleWeather &= ~B_WEATHER_RAIN_TEMPORARY;
                        gBattleWeather &= ~B_WEATHER_RAIN_DOWNPOUR;
                        gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_RAIN_STOPPED;
                    }
                    else if (gBattleWeather & B_WEATHER_RAIN_DOWNPOUR)
                        gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_DOWNPOUR_CONTINUES;
                    else
                        gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_RAIN_CONTINUES;
                }
                else if (gBattleWeather & B_WEATHER_RAIN_DOWNPOUR)
                {
                    gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_DOWNPOUR_CONTINUES;
                }
                else
                {
                    gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_RAIN_CONTINUES;
                }

                BattleScriptExecute(BattleScript_RainContinuesOrEnds);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_SANDSTORM:
            if (gBattleWeather & B_WEATHER_SANDSTORM)
            {
                if (!(gBattleWeather & B_WEATHER_SANDSTORM_PERMANENT) && --gWishFutureKnock.weatherDuration == 0)
                {
                    gBattleWeather &= ~B_WEATHER_SANDSTORM_TEMPORARY;
                    gBattlescriptCurrInstr = BattleScript_SandStormHailEnds;
                }
                else
                {
                    gBattlescriptCurrInstr = BattleScript_DamagingWeatherContinues;
                }

                gBattleScripting.animArg1 = B_ANIM_SANDSTORM_CONTINUES;
                gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_SANDSTORM;
                BattleScriptExecute(gBattlescriptCurrInstr);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_SUN:
            if (gBattleWeather & B_WEATHER_SUN)
            {
                if (!(gBattleWeather & B_WEATHER_SUN_PERMANENT) && --gWishFutureKnock.weatherDuration == 0)
                {
                    gBattleWeather &= ~B_WEATHER_SUN_TEMPORARY;
                    gBattlescriptCurrInstr = BattleScript_SunlightFaded;
                }
                else
                {
                    gBattlescriptCurrInstr = BattleScript_SunlightContinues;
                }

                BattleScriptExecute(gBattlescriptCurrInstr);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_HAIL:
            if (gBattleWeather & B_WEATHER_HAIL)
            {
                if (--gWishFutureKnock.weatherDuration == 0)
                {
                    gBattleWeather &= ~B_WEATHER_HAIL_TEMPORARY;
                    gBattlescriptCurrInstr = BattleScript_SandStormHailEnds;
                }
                else
                {
                    gBattlescriptCurrInstr = BattleScript_DamagingWeatherContinues;
                }

                gBattleScripting.animArg1 = B_ANIM_HAIL_CONTINUES;
                gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_HAIL;
                BattleScriptExecute(gBattlescriptCurrInstr);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_FIELD_COUNT:
            effect++;
            break;
        }
    } while (effect == 0);
    return (gBattleMainFunc != BattleTurnPassed);
}

enum
{
    ENDTURN_INGRAIN,
    ENDTURN_ABILITIES,
    ENDTURN_ITEMS1,
    ENDTURN_LEECH_SEED,
    ENDTURN_POISON,
    ENDTURN_BAD_POISON,
    ENDTURN_BURN,
    ENDTURN_NIGHTMARES,
    ENDTURN_CURSE,
    ENDTURN_WRAP,
    ENDTURN_UPROAR,
    ENDTURN_THRASH,
    ENDTURN_DISABLE,
    ENDTURN_ENCORE,
    ENDTURN_LOCK_ON,
    ENDTURN_CHARGE,
    ENDTURN_TAUNT,
    ENDTURN_YAWN,
    ENDTURN_ITEMS2,
    ENDTURN_BATTLER_COUNT
};

u8 DoBattlerEndTurnEffects(void)
{
    u8 effect = 0;

    gHitMarker |= (HITMARKER_GRUDGE | HITMARKER_IGNORE_BIDE);
    while (gBattleStruct->turnEffectsBattlerId < gBattlersCount && gBattleStruct->turnEffectsTracker <= ENDTURN_BATTLER_COUNT)
    {
        gActiveBattler = gBattlerAttacker = gBattlerByTurnOrder[gBattleStruct->turnEffectsBattlerId];
        if (gAbsentBattlerFlags & gBitTable[gActiveBattler])
        {
            gBattleStruct->turnEffectsBattlerId++;
        }
        else
        {
            switch (gBattleStruct->turnEffectsTracker)
            {
            case ENDTURN_INGRAIN:  // ingrain
                if ((gStatuses3[gActiveBattler] & STATUS3_ROOTED)
                 && gBattleMons[gActiveBattler].hp != gBattleMons[gActiveBattler].maxHP
                 && gBattleMons[gActiveBattler].hp != 0)
                {
                    gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 16;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    gBattleMoveDamage *= -1;
                    BattleScriptExecute(BattleScript_IngrainTurnHeal);
                    effect++;
                }
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_ABILITIES:  // end turn abilities
                if (AbilityBattleEffects(ABILITYEFFECT_ENDTURN, gActiveBattler, 0, 0, 0))
                    effect++;
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_ITEMS1:  // item effects
                if (ItemBattleEffects(ITEMEFFECT_NORMAL, gActiveBattler, FALSE))
                    effect++;
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_ITEMS2:  // item effects again
                if (ItemBattleEffects(ITEMEFFECT_NORMAL, gActiveBattler, TRUE))
                    effect++;
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_LEECH_SEED:  // leech seed
                if ((gStatuses3[gActiveBattler] & STATUS3_LEECHSEED)
                 && gBattleMons[gStatuses3[gActiveBattler] & STATUS3_LEECHSEED_BATTLER].hp != 0
                 && gBattleMons[gActiveBattler].hp != 0)
                {
                    gBattlerTarget = gStatuses3[gActiveBattler] & STATUS3_LEECHSEED_BATTLER; // Notice gBattlerTarget is actually the HP receiver.
                    gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 8;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    gBattleScripting.animArg1 = gBattlerTarget;
                    gBattleScripting.animArg2 = gBattlerAttacker;
                    BattleScriptExecute(BattleScript_LeechSeedTurnDrain);
                    effect++;
                }
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_POISON:  // poison
                if ((gBattleMons[gActiveBattler].status1 & STATUS1_POISON) && gBattleMons[gActiveBattler].hp != 0)
                {
                    gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 8;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    BattleScriptExecute(BattleScript_PoisonTurnDmg);
                    effect++;
                }
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_BAD_POISON:  // toxic poison
                if ((gBattleMons[gActiveBattler].status1 & STATUS1_TOXIC_POISON) && gBattleMons[gActiveBattler].hp != 0)
                {
                    gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 16;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    if ((gBattleMons[gActiveBattler].status1 & STATUS1_TOXIC_COUNTER) != STATUS1_TOXIC_TURN(15)) // not 16 turns
                        gBattleMons[gActiveBattler].status1 += STATUS1_TOXIC_TURN(1);
                    gBattleMoveDamage *= (gBattleMons[gActiveBattler].status1 & STATUS1_TOXIC_COUNTER) >> 8;
                    BattleScriptExecute(BattleScript_PoisonTurnDmg);
                    effect++;
                }
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_BURN:  // burn
                if ((gBattleMons[gActiveBattler].status1 & STATUS1_BURN) && gBattleMons[gActiveBattler].hp != 0)
                {
                    gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 8;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    BattleScriptExecute(BattleScript_BurnTurnDmg);
                    effect++;
                }
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_NIGHTMARES:  // spooky nightmares
                if ((gBattleMons[gActiveBattler].status2 & STATUS2_NIGHTMARE) && gBattleMons[gActiveBattler].hp != 0)
                {
                    // R/S does not perform this sleep check, which causes the nightmare effect to
                    // persist even after the affected Pokémon has been awakened by Shed Skin.
                    if (gBattleMons[gActiveBattler].status1 & STATUS1_SLEEP)
                    {
                        gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 4;
                        if (gBattleMoveDamage == 0)
                            gBattleMoveDamage = 1;
                        BattleScriptExecute(BattleScript_NightmareTurnDmg);
                        effect++;
                    }
                    else
                    {
                        gBattleMons[gActiveBattler].status2 &= ~STATUS2_NIGHTMARE;
                    }
                }
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_CURSE:  // curse
                if ((gBattleMons[gActiveBattler].status2 & STATUS2_CURSED) && gBattleMons[gActiveBattler].hp != 0)
                {
                    gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 4;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    BattleScriptExecute(BattleScript_CurseTurnDmg);
                    effect++;
                }
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_WRAP:  // wrap
                if ((gBattleMons[gActiveBattler].status2 & STATUS2_WRAPPED) && gBattleMons[gActiveBattler].hp != 0)
                {
                    gBattleMons[gActiveBattler].status2 -= STATUS2_WRAPPED_TURN(1);
                    if (gBattleMons[gActiveBattler].status2 & STATUS2_WRAPPED)  // damaged by wrap
                    {
                        // This is the only way I could get this array access to match.
                        gBattleScripting.animArg1 = *(gBattleStruct->wrappedMove + gActiveBattler * 2 + 0);
                        gBattleScripting.animArg2 = *(gBattleStruct->wrappedMove + gActiveBattler * 2 + 1);
                        gBattleTextBuff1[0] = B_BUFF_PLACEHOLDER_BEGIN;
                        gBattleTextBuff1[1] = B_BUFF_MOVE;
                        gBattleTextBuff1[2] = *(gBattleStruct->wrappedMove + gActiveBattler * 2 + 0);
                        gBattleTextBuff1[3] = *(gBattleStruct->wrappedMove + gActiveBattler * 2 + 1);
                        gBattleTextBuff1[4] = EOS;
                        gBattlescriptCurrInstr = BattleScript_WrapTurnDmg;
                        gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 16;
                        if (gBattleMoveDamage == 0)
                            gBattleMoveDamage = 1;
                    }
                    else  // broke free
                    {
                        gBattleTextBuff1[0] = B_BUFF_PLACEHOLDER_BEGIN;
                        gBattleTextBuff1[1] = B_BUFF_MOVE;
                        gBattleTextBuff1[2] = *(gBattleStruct->wrappedMove + gActiveBattler * 2 + 0);
                        gBattleTextBuff1[3] = *(gBattleStruct->wrappedMove + gActiveBattler * 2 + 1);
                        gBattleTextBuff1[4] = EOS;
                        gBattlescriptCurrInstr = BattleScript_WrapEnds;
                    }
                    BattleScriptExecute(gBattlescriptCurrInstr);
                    effect++;
                }
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_UPROAR:  // uproar
                if (gBattleMons[gActiveBattler].status2 & STATUS2_UPROAR)
                {
                    for (gBattlerAttacker = 0; gBattlerAttacker < gBattlersCount; gBattlerAttacker++)
                    {
                        if ((gBattleMons[gBattlerAttacker].status1 & STATUS1_SLEEP)
                         && gBattleMons[gBattlerAttacker].ability != ABILITY_SOUNDPROOF)
                        {
                            gBattleMons[gBattlerAttacker].status1 &= ~STATUS1_SLEEP;
                            gBattleMons[gBattlerAttacker].status2 &= ~STATUS2_NIGHTMARE;
                            gBattleCommunication[MULTISTRING_CHOOSER] = 1;
                            BattleScriptExecute(BattleScript_MonWokeUpInUproar);
                            gActiveBattler = gBattlerAttacker;
                            BtlController_EmitSetMonData(BUFFER_A, REQUEST_STATUS_BATTLE, 0, 4, &gBattleMons[gActiveBattler].status1);
                            MarkBattlerForControllerExec(gActiveBattler);
                            break;
                        }
                    }
                    if (gBattlerAttacker != gBattlersCount)
                    {
                        effect = 2;  // a Pokémon was awaken
                        break;
                    }
                    else
                    {
                        gBattlerAttacker = gActiveBattler;
                        gBattleMons[gActiveBattler].status2 -= STATUS2_UPROAR_TURN(1);
                        if (WasUnableToUseMove(gActiveBattler))
                        {
                            CancelMultiTurnMoves(gActiveBattler);
                            gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_UPROAR_ENDS;
                        }
                        else if (gBattleMons[gActiveBattler].status2 & STATUS2_UPROAR)
                        {
                            gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_UPROAR_CONTINUES;
                            gBattleMons[gActiveBattler].status2 |= STATUS2_MULTIPLETURNS;
                        }
                        else
                        {
                            gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_UPROAR_ENDS;
                            CancelMultiTurnMoves(gActiveBattler);
                        }
                        BattleScriptExecute(BattleScript_PrintUproarOverTurns);
                        effect = 1;
                    }
                }
                if (effect != 2)
                    gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_THRASH:  // thrash
                if (gBattleMons[gActiveBattler].status2 & STATUS2_LOCK_CONFUSE)
                {
                    gBattleMons[gActiveBattler].status2 -= STATUS2_LOCK_CONFUSE_TURN(1);
                    if (WasUnableToUseMove(gActiveBattler))
                        CancelMultiTurnMoves(gActiveBattler);
                    else if (!(gBattleMons[gActiveBattler].status2 & STATUS2_LOCK_CONFUSE)
                     && (gBattleMons[gActiveBattler].status2 & STATUS2_MULTIPLETURNS))
                    {
                        gBattleMons[gActiveBattler].status2 &= ~STATUS2_MULTIPLETURNS;
                        if (!(gBattleMons[gActiveBattler].status2 & STATUS2_CONFUSION))
                        {
                            gBattleCommunication[MOVE_EFFECT_BYTE] = MOVE_EFFECT_CONFUSION | MOVE_EFFECT_AFFECTS_USER;
                            SetMoveEffect(TRUE, 0);
                            if (gBattleMons[gActiveBattler].status2 & STATUS2_CONFUSION)
                                BattleScriptExecute(BattleScript_ThrashConfuses);
                            effect++;
                        }
                    }
                }
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_DISABLE:  // disable
                if (gDisableStructs[gActiveBattler].disableTimer != 0)
                {
                    s32 i;
                    for (i = 0; i < MAX_MON_MOVES; i++)
                    {
                        if (gDisableStructs[gActiveBattler].disabledMove == gBattleMons[gActiveBattler].moves[i])
                            break;
                    }
                    if (i == MAX_MON_MOVES)  // Pokémon does not have the disabled move anymore
                    {
                        gDisableStructs[gActiveBattler].disabledMove = MOVE_NONE;
                        gDisableStructs[gActiveBattler].disableTimer = 0;
                    }
                    else if (--gDisableStructs[gActiveBattler].disableTimer == 0)  // disable ends
                    {
                        gDisableStructs[gActiveBattler].disabledMove = MOVE_NONE;
                        BattleScriptExecute(BattleScript_DisabledNoMore);
                        effect++;
                    }
                }
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_ENCORE:  // encore
                if (gDisableStructs[gActiveBattler].encoreTimer != 0)
                {
                    if (gBattleMons[gActiveBattler].moves[gDisableStructs[gActiveBattler].encoredMovePos] != gDisableStructs[gActiveBattler].encoredMove)  // Pokémon does not have the encored move anymore
                    {
                        gDisableStructs[gActiveBattler].encoredMove = MOVE_NONE;
                        gDisableStructs[gActiveBattler].encoreTimer = 0;
                    }
                    else if (--gDisableStructs[gActiveBattler].encoreTimer == 0
                     || gBattleMons[gActiveBattler].pp[gDisableStructs[gActiveBattler].encoredMovePos] == 0)
                    {
                        gDisableStructs[gActiveBattler].encoredMove = MOVE_NONE;
                        gDisableStructs[gActiveBattler].encoreTimer = 0;
                        BattleScriptExecute(BattleScript_EncoredNoMore);
                        effect++;
                    }
                }
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_LOCK_ON:  // lock-on decrement
                if (gStatuses3[gActiveBattler] & STATUS3_ALWAYS_HITS)
                    gStatuses3[gActiveBattler] -= STATUS3_ALWAYS_HITS_TURN(1);
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_CHARGE:  // charge
                if (gDisableStructs[gActiveBattler].chargeTimer && --gDisableStructs[gActiveBattler].chargeTimer == 0)
                    gStatuses3[gActiveBattler] &= ~STATUS3_CHARGED_UP;
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_TAUNT:  // taunt
                if (gDisableStructs[gActiveBattler].tauntTimer)
                    gDisableStructs[gActiveBattler].tauntTimer--;
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_YAWN:  // yawn
                if (gStatuses3[gActiveBattler] & STATUS3_YAWN)
                {
                    gStatuses3[gActiveBattler] -= STATUS3_YAWN_TURN(1);
                    if (!(gStatuses3[gActiveBattler] & STATUS3_YAWN) && !(gBattleMons[gActiveBattler].status1 & STATUS1_ANY)
                     && gBattleMons[gActiveBattler].ability != ABILITY_VITAL_SPIRIT
                     && gBattleMons[gActiveBattler].ability != ABILITY_INSOMNIA && !UproarWakeUpCheck(gActiveBattler))
                    {
                        CancelMultiTurnMoves(gActiveBattler);
                        gBattleMons[gActiveBattler].status1 |= STATUS1_SLEEP_TURN((Random() & 3) + 2); // 2-5 turns of sleep
                        BtlController_EmitSetMonData(BUFFER_A, REQUEST_STATUS_BATTLE, 0, 4, &gBattleMons[gActiveBattler].status1);
                        MarkBattlerForControllerExec(gActiveBattler);
                        gEffectBattler = gActiveBattler;
                        BattleScriptExecute(BattleScript_YawnMakesAsleep);
                        effect++;
                    }
                }
                gBattleStruct->turnEffectsTracker++;
                break;
            case ENDTURN_BATTLER_COUNT:  // done
                gBattleStruct->turnEffectsTracker = 0;
                gBattleStruct->turnEffectsBattlerId++;
                break;
            }
            if (effect != 0)
                return effect;
        }
    }
    gHitMarker &= ~(HITMARKER_GRUDGE | HITMARKER_IGNORE_BIDE);
    return 0;
}

bool8 HandleWishPerishSongOnTurnEnd(void)
{
    gHitMarker |= (HITMARKER_GRUDGE | HITMARKER_IGNORE_BIDE);

    switch (gBattleStruct->wishPerishSongState)
    {
    case 0:
        while (gBattleStruct->wishPerishSongBattlerId < gBattlersCount)
        {
            gActiveBattler = gBattleStruct->wishPerishSongBattlerId;
            if (gAbsentBattlerFlags & gBitTable[gActiveBattler])
            {
                gBattleStruct->wishPerishSongBattlerId++;
                continue;
            }

            gBattleStruct->wishPerishSongBattlerId++;
            if (gWishFutureKnock.futureSightCounter[gActiveBattler] != 0
             && --gWishFutureKnock.futureSightCounter[gActiveBattler] == 0
             && gBattleMons[gActiveBattler].hp != 0)
            {
                if (gWishFutureKnock.futureSightMove[gActiveBattler] == MOVE_FUTURE_SIGHT)
                    gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_FUTURE_SIGHT;
                else
                    gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_DOOM_DESIRE;

                PREPARE_MOVE_BUFFER(gBattleTextBuff1, gWishFutureKnock.futureSightMove[gActiveBattler]);

                gBattlerTarget = gActiveBattler;
                gBattlerAttacker = gWishFutureKnock.futureSightAttacker[gActiveBattler];
                gBattleMoveDamage = gWishFutureKnock.futureSightDmg[gActiveBattler];
                gSpecialStatuses[gBattlerTarget].shellBellDmg = IGNORE_SHELL_BELL;
                BattleScriptExecute(BattleScript_MonTookFutureAttack);

                if (gWishFutureKnock.futureSightCounter[gActiveBattler] == 0
                 && gWishFutureKnock.futureSightCounter[BATTLE_PARTNER(gActiveBattler)] == 0)
                {
                    gSideStatuses[GET_BATTLER_SIDE(gBattlerTarget)] &= ~SIDE_STATUS_FUTUREATTACK;
                }
                return TRUE;
            }
        }
        // Why do I have to keep doing this to match?
        {
            u8 *state = &gBattleStruct->wishPerishSongState;
            *state = 1;
            gBattleStruct->wishPerishSongBattlerId = 0;
        }
        // fall through
    case 1:
        while (gBattleStruct->wishPerishSongBattlerId < gBattlersCount)
        {
            gActiveBattler = gBattlerAttacker = gBattlerByTurnOrder[gBattleStruct->wishPerishSongBattlerId];
            if (gAbsentBattlerFlags & gBitTable[gActiveBattler])
            {
                gBattleStruct->wishPerishSongBattlerId++;
                continue;
            }
            gBattleStruct->wishPerishSongBattlerId++;
            if (gStatuses3[gActiveBattler] & STATUS3_PERISH_SONG)
            {
                PREPARE_BYTE_NUMBER_BUFFER(gBattleTextBuff1, 1, gDisableStructs[gActiveBattler].perishSongTimer);
                if (gDisableStructs[gActiveBattler].perishSongTimer == 0)
                {
                    gStatuses3[gActiveBattler] &= ~STATUS3_PERISH_SONG;
                    gBattleMoveDamage = gBattleMons[gActiveBattler].hp;
                    gBattlescriptCurrInstr = BattleScript_PerishSongTakesLife;
                }
                else
                {
                    gDisableStructs[gActiveBattler].perishSongTimer--;
                    gBattlescriptCurrInstr = BattleScript_PerishSongCountGoesDown;
                }
                BattleScriptExecute(gBattlescriptCurrInstr);
                return TRUE;
            }
        }
        // Hm...
        {
            u8 *state = &gBattleStruct->wishPerishSongState;
            *state = 2;
            gBattleStruct->wishPerishSongBattlerId = 0;
        }
        // fall through
    case 2:
        if ((gBattleTypeFlags & BATTLE_TYPE_ARENA)
         && gBattleStruct->arenaTurnCounter == 2
         && gBattleMons[0].hp != 0 && gBattleMons[1].hp != 0)
        {
            s32 i;

            for (i = 0; i < 2; i++)
                CancelMultiTurnMoves(i);

            gBattlescriptCurrInstr = BattleScript_ArenaDoJudgment;
            BattleScriptExecute(BattleScript_ArenaDoJudgment);
            gBattleStruct->wishPerishSongState++;
            return TRUE;
        }
        break;
    }

    gHitMarker &= ~(HITMARKER_GRUDGE | HITMARKER_IGNORE_BIDE);

    return FALSE;
}

#define FAINTED_ACTIONS_MAX_CASE 7

bool8 HandleFaintedMonActions(void)
{
    if (gBattleTypeFlags & BATTLE_TYPE_SAFARI)
        return FALSE;
    do
    {
        s32 i;
        switch (gBattleStruct->faintedActionsState)
        {
        case 0:
            gBattleStruct->faintedActionsBattlerId = 0;
            gBattleStruct->faintedActionsState++;
            for (i = 0; i < gBattlersCount; i++)
            {
                if (gAbsentBattlerFlags & gBitTable[i] && !HasNoMonsToSwitch(i, PARTY_SIZE, PARTY_SIZE))
                    gAbsentBattlerFlags &= ~(gBitTable[i]);
            }
            // fall through
        case 1:
            do
            {
                gBattlerFainted = gBattlerTarget = gBattleStruct->faintedActionsBattlerId;
                if (gBattleMons[gBattleStruct->faintedActionsBattlerId].hp == 0
                 && !(gBattleStruct->givenExpMons & gBitTable[gBattlerPartyIndexes[gBattleStruct->faintedActionsBattlerId]])
                 && !(gAbsentBattlerFlags & gBitTable[gBattleStruct->faintedActionsBattlerId]))
                {
                    BattleScriptExecute(BattleScript_GiveExp);
                    gBattleStruct->faintedActionsState = 2;
                    return TRUE;
                }
            } while (++gBattleStruct->faintedActionsBattlerId != gBattlersCount);
            gBattleStruct->faintedActionsState = 3;
            break;
        case 2:
            OpponentSwitchInResetSentPokesToOpponentValue(gBattlerFainted);
            if (++gBattleStruct->faintedActionsBattlerId == gBattlersCount)
                gBattleStruct->faintedActionsState = 3;
            else
                gBattleStruct->faintedActionsState = 1;
            break;
        case 3:
            gBattleStruct->faintedActionsBattlerId = 0;
            gBattleStruct->faintedActionsState++;
            // fall through
        case 4:
            do
            {
                gBattlerFainted = gBattlerTarget = gBattleStruct->faintedActionsBattlerId;
                if (gBattleMons[gBattleStruct->faintedActionsBattlerId].hp == 0
                 && !(gAbsentBattlerFlags & gBitTable[gBattleStruct->faintedActionsBattlerId]))
                {
                    BattleScriptExecute(BattleScript_HandleFaintedMon);
                    gBattleStruct->faintedActionsState = 5;
                    return TRUE;
                }
            } while (++gBattleStruct->faintedActionsBattlerId != gBattlersCount);
            gBattleStruct->faintedActionsState = 6;
            break;
        case 5:
            if (++gBattleStruct->faintedActionsBattlerId == gBattlersCount)
                gBattleStruct->faintedActionsState = 6;
            else
                gBattleStruct->faintedActionsState = 4;
            break;
        case 6:
            if (AbilityBattleEffects(ABILITYEFFECT_INTIMIDATE1, 0, 0, 0, 0)
             || AbilityBattleEffects(ABILITYEFFECT_TRACE, 0, 0, 0, 0)
             || ItemBattleEffects(ITEMEFFECT_NORMAL, 0, TRUE)
             || AbilityBattleEffects(ABILITYEFFECT_FORECAST, 0, 0, 0, 0))
                return TRUE;
            gBattleStruct->faintedActionsState++;
            break;
        case FAINTED_ACTIONS_MAX_CASE:
            break;
        }
    } while (gBattleStruct->faintedActionsState != FAINTED_ACTIONS_MAX_CASE);
    return FALSE;
}

void TryClearRageStatuses(void)
{
    s32 i;
    for (i = 0; i < gBattlersCount; i++)
    {
        if ((gBattleMons[i].status2 & STATUS2_RAGE) && gChosenMoveByBattler[i] != MOVE_RAGE)
            gBattleMons[i].status2 &= ~STATUS2_RAGE;
    }
}

enum
{
    CANCELLER_FLAGS,
    CANCELLER_ASLEEP,
    CANCELLER_FROZEN,
    CANCELLER_TRUANT,
    CANCELLER_RECHARGE,
    CANCELLER_FLINCH,
    CANCELLER_DISABLED,
    CANCELLER_TAUNTED,
    CANCELLER_IMPRISONED,
    CANCELLER_CONFUSED,
    CANCELLER_PARALYSED,
    CANCELLER_IN_LOVE,
    CANCELLER_BIDE,
    CANCELLER_THAW,
    CANCELLER_END,
};

u8 AtkCanceller_UnableToUseMove(void)
{
    u8 effect = 0;
    s32 *bideDmg = &gBattleScripting.bideDmg;
    do
    {
        switch (gBattleStruct->atkCancellerTracker)
        {
        case CANCELLER_FLAGS: // flags clear
            gBattleMons[gBattlerAttacker].status2 &= ~STATUS2_DESTINY_BOND;
            gStatuses3[gBattlerAttacker] &= ~STATUS3_GRUDGE;
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_ASLEEP: // check being asleep
            if (gBattleMons[gBattlerAttacker].status1 & STATUS1_SLEEP)
            {
                if (UproarWakeUpCheck(gBattlerAttacker))
                {
                    gBattleMons[gBattlerAttacker].status1 &= ~STATUS1_SLEEP;
                    gBattleMons[gBattlerAttacker].status2 &= ~STATUS2_NIGHTMARE;
                    BattleScriptPushCursor();
                    gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_WOKE_UP_UPROAR;
                    gBattlescriptCurrInstr = BattleScript_MoveUsedWokeUp;
                    effect = 2;
                }
                else
                {
                    u8 toSub;
                    if (gBattleMons[gBattlerAttacker].ability == ABILITY_EARLY_BIRD)
                        toSub = 2;
                    else
                        toSub = 1;
                    if ((gBattleMons[gBattlerAttacker].status1 & STATUS1_SLEEP) < toSub)
                        gBattleMons[gBattlerAttacker].status1 &= ~STATUS1_SLEEP;
                    else
                        gBattleMons[gBattlerAttacker].status1 -= toSub;
                    if (gBattleMons[gBattlerAttacker].status1 & STATUS1_SLEEP)
                    {
                        if (gCurrentMove != MOVE_SNORE && gCurrentMove != MOVE_SLEEP_TALK)
                        {
                            gBattlescriptCurrInstr = BattleScript_MoveUsedIsAsleep;
                            gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                            effect = 2;
                        }
                    }
                    else
                    {
                        gBattleMons[gBattlerAttacker].status2 &= ~STATUS2_NIGHTMARE;
                        BattleScriptPushCursor();
                        gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_WOKE_UP;
                        gBattlescriptCurrInstr = BattleScript_MoveUsedWokeUp;
                        effect = 2;
                    }
                }
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_FROZEN: // check being frozen
            if (gBattleMons[gBattlerAttacker].status1 & STATUS1_FREEZE)
            {
                if (Random() % 5)
                {
                    if (gBattleMoves[gCurrentMove].effect != EFFECT_THAW_HIT) // unfreezing via a move effect happens in case 13
                    {
                        gBattlescriptCurrInstr = BattleScript_MoveUsedIsFrozen;
                        gHitMarker |= HITMARKER_NO_ATTACKSTRING;
                    }
                    else
                    {
                        gBattleStruct->atkCancellerTracker++;
                        break;
                    }
                }
                else // unfreeze
                {
                    gBattleMons[gBattlerAttacker].status1 &= ~STATUS1_FREEZE;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_MoveUsedUnfroze;
                    gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_DEFROSTED;
                }
                effect = 2;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_TRUANT: // truant
            if (gBattleMons[gBattlerAttacker].ability == ABILITY_TRUANT && gDisableStructs[gBattlerAttacker].truantCounter)
            {
                CancelMultiTurnMoves(gBattlerAttacker);
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_LOAFING;
                gBattlescriptCurrInstr = BattleScript_MoveUsedLoafingAround;
                gMoveResultFlags |= MOVE_RESULT_MISSED;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_RECHARGE: // recharge
            if (gBattleMons[gBattlerAttacker].status2 & STATUS2_RECHARGE)
            {
                gBattleMons[gBattlerAttacker].status2 &= ~STATUS2_RECHARGE;
                gDisableStructs[gBattlerAttacker].rechargeTimer = 0;
                CancelMultiTurnMoves(gBattlerAttacker);
                gBattlescriptCurrInstr = BattleScript_MoveUsedMustRecharge;
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_FLINCH: // flinch
            if (gBattleMons[gBattlerAttacker].status2 & STATUS2_FLINCHED)
            {
                gBattleMons[gBattlerAttacker].status2 &= ~STATUS2_FLINCHED;
                gProtectStructs[gBattlerAttacker].flinchImmobility = 1;
                CancelMultiTurnMoves(gBattlerAttacker);
                gBattlescriptCurrInstr = BattleScript_MoveUsedFlinched;
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_DISABLED: // disabled move
            if (gDisableStructs[gBattlerAttacker].disabledMove == gCurrentMove && gDisableStructs[gBattlerAttacker].disabledMove != MOVE_NONE)
            {
                gProtectStructs[gBattlerAttacker].usedDisabledMove = 1;
                gBattleScripting.battler = gBattlerAttacker;
                CancelMultiTurnMoves(gBattlerAttacker);
                gBattlescriptCurrInstr = BattleScript_MoveUsedIsDisabled;
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_TAUNTED: // taunt
            if (gDisableStructs[gBattlerAttacker].tauntTimer && gBattleMoves[gCurrentMove].power == 0)
            {
                gProtectStructs[gBattlerAttacker].usedTauntedMove = 1;
                CancelMultiTurnMoves(gBattlerAttacker);
                gBattlescriptCurrInstr = BattleScript_MoveUsedIsTaunted;
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_IMPRISONED: // imprisoned
            if (GetImprisonedMovesCount(gBattlerAttacker, gCurrentMove))
            {
                gProtectStructs[gBattlerAttacker].usedImprisonedMove = 1;
                CancelMultiTurnMoves(gBattlerAttacker);
                gBattlescriptCurrInstr = BattleScript_MoveUsedIsImprisoned;
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_CONFUSED: // confusion
            if (gBattleMons[gBattlerAttacker].status2 & STATUS2_CONFUSION)
            {
                gBattleMons[gBattlerAttacker].status2 -= STATUS2_CONFUSION_TURN(1);
                if (gBattleMons[gBattlerAttacker].status2 & STATUS2_CONFUSION)
                {
                    if (Random() & 1)
                    {
                        // The MULTISTRING_CHOOSER is used here as a bool to signal
                        // to BattleScript_MoveUsedIsConfused whether or not damage was taken
                        gBattleCommunication[MULTISTRING_CHOOSER] = FALSE;
                        BattleScriptPushCursor();
                    }
                    else // confusion dmg
                    {
                        gBattleCommunication[MULTISTRING_CHOOSER] = TRUE;
                        gBattlerTarget = gBattlerAttacker;
                        gBattleMoveDamage = CalculateBaseDamage(&gBattleMons[gBattlerAttacker], &gBattleMons[gBattlerAttacker], MOVE_POUND, 0, 40, 0, gBattlerAttacker, gBattlerAttacker);
                        gProtectStructs[gBattlerAttacker].confusionSelfDmg = 1;
                        gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                    }
                    gBattlescriptCurrInstr = BattleScript_MoveUsedIsConfused;
                }
                else // snapped out of confusion
                {
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_MoveUsedIsConfusedNoMore;
                }
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_PARALYSED: // paralysis
            if ((gBattleMons[gBattlerAttacker].status1 & STATUS1_PARALYSIS) && (Random() % 4) == 0)
            {
                gProtectStructs[gBattlerAttacker].prlzImmobility = 1;
                // This is removed in FRLG and Emerald for some reason
                //CancelMultiTurnMoves(gBattlerAttacker);
                gBattlescriptCurrInstr = BattleScript_MoveUsedIsParalyzed;
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_IN_LOVE: // infatuation
            if (gBattleMons[gBattlerAttacker].status2 & STATUS2_INFATUATION)
            {
                gBattleScripting.battler = CountTrailingZeroBits((gBattleMons[gBattlerAttacker].status2 & STATUS2_INFATUATION) >> 0x10);
                if (Random() & 1)
                {
                    BattleScriptPushCursor();
                }
                else
                {
                    BattleScriptPush(BattleScript_MoveUsedIsInLoveCantAttack);
                    gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                    gProtectStructs[gBattlerAttacker].loveImmobility = 1;
                    CancelMultiTurnMoves(gBattlerAttacker);
                }
                gBattlescriptCurrInstr = BattleScript_MoveUsedIsInLove;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_BIDE: // bide
            if (gBattleMons[gBattlerAttacker].status2 & STATUS2_BIDE)
            {
                gBattleMons[gBattlerAttacker].status2 -= STATUS2_BIDE_TURN(1);
                if (gBattleMons[gBattlerAttacker].status2 & STATUS2_BIDE)
                {
                    gBattlescriptCurrInstr = BattleScript_BideStoringEnergy;
                }
                else
                {
                    // This is removed in FRLG and Emerald for some reason
                    //gBattleMons[gBattlerAttacker].status2 &= ~STATUS2_MULTIPLETURNS;
                    if (gBideDmg[gBattlerAttacker])
                    {
                        gCurrentMove = MOVE_BIDE;
                        *bideDmg = gBideDmg[gBattlerAttacker] * 2;
                        gBattlerTarget = gBideTarget[gBattlerAttacker];
                        if (gAbsentBattlerFlags & gBitTable[gBattlerTarget])
                            gBattlerTarget = GetMoveTarget(MOVE_BIDE, MOVE_TARGET_SELECTED + 1);
                        gBattlescriptCurrInstr = BattleScript_BideAttack;
                    }
                    else
                    {
                        gBattlescriptCurrInstr = BattleScript_BideNoEnergyToAttack;
                    }
                }
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_THAW: // move thawing
            if (gBattleMons[gBattlerAttacker].status1 & STATUS1_FREEZE)
            {
                if (gBattleMoves[gCurrentMove].effect == EFFECT_THAW_HIT)
                {
                    gBattleMons[gBattlerAttacker].status1 &= ~STATUS1_FREEZE;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_MoveUsedUnfroze;
                    gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_DEFROSTED_BY_MOVE;
                }
                effect = 2;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_END:
            break;
        }

    } while (gBattleStruct->atkCancellerTracker != CANCELLER_END && effect == 0);

    if (effect == 2)
    {
        gActiveBattler = gBattlerAttacker;
        BtlController_EmitSetMonData(BUFFER_A, REQUEST_STATUS_BATTLE, 0, 4, &gBattleMons[gActiveBattler].status1);
        MarkBattlerForControllerExec(gActiveBattler);
    }
    return effect;
}

bool8 HasNoMonsToSwitch(u8 battler, u8 partyIdBattlerOn1, u8 partyIdBattlerOn2)
{
    u8 playerId, flankId;
    struct Pokemon *party;
    s32 i;

    if (!(gBattleTypeFlags & BATTLE_TYPE_DOUBLE))
        return FALSE;

    if (gBattleTypeFlags & BATTLE_TYPE_INGAME_PARTNER)
    {
        if (GetBattlerSide(battler) == B_SIDE_PLAYER)
            party = gPlayerParty;
        else
            party = gEnemyParty;

        playerId = ((battler & BIT_FLANK) / 2);
        for (i = playerId * MULTI_PARTY_SIZE; i < playerId * MULTI_PARTY_SIZE + MULTI_PARTY_SIZE; i++)
        {
            if (GetMonData(&party[i], MON_DATA_HP) != 0
             && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_NONE
             && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_EGG)
                break;
        }
        return (i == playerId * MULTI_PARTY_SIZE + MULTI_PARTY_SIZE);
    }
    else if (gBattleTypeFlags & BATTLE_TYPE_MULTI)
    {
        if (gBattleTypeFlags & BATTLE_TYPE_TOWER_LINK_MULTI)
        {
            if (GetBattlerSide(battler) == B_SIDE_PLAYER)
            {
                party = gPlayerParty;
                flankId = GetBattlerMultiplayerId(battler);
                playerId = GetLinkTrainerFlankId(flankId);
            }
            else
            {
                party = gEnemyParty;
                if (battler == 1)
                    playerId = 0;
                else
                    playerId = 1;
            }
        }
        else
        {
            flankId = GetBattlerMultiplayerId(battler);

            if (GetBattlerSide(battler) == B_SIDE_PLAYER)
                party = gPlayerParty;
            else
                party = gEnemyParty;

            playerId = GetLinkTrainerFlankId(flankId);
        }

        for (i = playerId * MULTI_PARTY_SIZE; i < playerId * MULTI_PARTY_SIZE + MULTI_PARTY_SIZE; i++)
        {
            if (GetMonData(&party[i], MON_DATA_HP) != 0
             && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_NONE
             && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_EGG)
                break;
        }
        return (i == playerId * MULTI_PARTY_SIZE + MULTI_PARTY_SIZE);
    }
    else if ((gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS) && GetBattlerSide(battler) == B_SIDE_OPPONENT)
    {
        party = gEnemyParty;

        if (battler == 1)
            playerId = 0;
        else
            playerId = MULTI_PARTY_SIZE;

        for (i = playerId; i < playerId + MULTI_PARTY_SIZE; i++)
        {
            if (GetMonData(&party[i], MON_DATA_HP) != 0
             && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_NONE
             && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_EGG)
                break;
        }
        return (i == playerId + 3);
    }
    else
    {
        if (GetBattlerSide(battler) == B_SIDE_OPPONENT)
        {
            flankId = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
            playerId = GetBattlerAtPosition(B_POSITION_OPPONENT_RIGHT);
            party = gEnemyParty;
        }
        else
        {
            flankId = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
            playerId = GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT);
            party = gPlayerParty;
        }

        if (partyIdBattlerOn1 == PARTY_SIZE)
            partyIdBattlerOn1 = gBattlerPartyIndexes[flankId];
        if (partyIdBattlerOn2 == PARTY_SIZE)
            partyIdBattlerOn2 = gBattlerPartyIndexes[playerId];

        for (i = 0; i < PARTY_SIZE; i++)
        {
            if (GetMonData(&party[i], MON_DATA_HP) != 0
             && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_NONE
             && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_EGG
             && i != partyIdBattlerOn1 && i != partyIdBattlerOn2
             && i != *(gBattleStruct->monToSwitchIntoId + flankId) && i != playerId[gBattleStruct->monToSwitchIntoId])
                break;
        }
        return (i == PARTY_SIZE);
    }
}

u8 CastformDataTypeChange(u8 battler)
{
    u8 formChange = 0;
    if (gBattleMons[battler].species != SPECIES_CASTFORM || gBattleMons[battler].ability != ABILITY_FORECAST || gBattleMons[battler].hp == 0)
        return 0; // No change
    if (!WEATHER_HAS_EFFECT && !IS_BATTLER_OF_TYPE(battler, TYPE_NORMAL))
    {
        SET_BATTLER_TYPE(battler, TYPE_NORMAL);
        return CASTFORM_NORMAL + 1;
    }
    if (!WEATHER_HAS_EFFECT)
        return 0; // No change
    if (!(gBattleWeather & (B_WEATHER_RAIN | B_WEATHER_SUN | B_WEATHER_HAIL)) && !IS_BATTLER_OF_TYPE(battler, TYPE_NORMAL))
    {
        SET_BATTLER_TYPE(battler, TYPE_NORMAL);
        formChange = CASTFORM_NORMAL + 1;
    }
    if (gBattleWeather & B_WEATHER_SUN && !IS_BATTLER_OF_TYPE(battler, TYPE_FIRE))
    {
        SET_BATTLER_TYPE(battler, TYPE_FIRE);
        formChange = CASTFORM_FIRE + 1;
    }
    if (gBattleWeather & B_WEATHER_RAIN && !IS_BATTLER_OF_TYPE(battler, TYPE_WATER))
    {
        SET_BATTLER_TYPE(battler, TYPE_WATER);
        formChange = CASTFORM_WATER + 1;
    }
    if (gBattleWeather & B_WEATHER_HAIL && !IS_BATTLER_OF_TYPE(battler, TYPE_ICE))
    {
        SET_BATTLER_TYPE(battler, TYPE_ICE);
        formChange = CASTFORM_ICE + 1;
    }
    return formChange;
}

u8 AbilityBattleEffects(u8 caseID, u8 battler, u8 ability, u8 special, u16 moveArg)
{
    u8 effect = 0;
    struct Pokemon *pokeAtk;
    struct Pokemon *pokeDef;
    u16 speciesAtk;
    u16 speciesDef;
    u32 pidAtk;
    u32 pidDef;

    if (gBattlerAttacker >= gBattlersCount)
        gBattlerAttacker = battler;

    if (GetBattlerSide(gBattlerAttacker) == B_SIDE_PLAYER)
        pokeAtk = &gPlayerParty[gBattlerPartyIndexes[gBattlerAttacker]];
    else
        pokeAtk = &gEnemyParty[gBattlerPartyIndexes[gBattlerAttacker]];

    if (gBattlerTarget >= gBattlersCount)
        gBattlerTarget = battler;

    if (GetBattlerSide(gBattlerTarget) == B_SIDE_PLAYER)
        pokeDef = &gPlayerParty[gBattlerPartyIndexes[gBattlerTarget]];
    else
        pokeDef = &gEnemyParty[gBattlerPartyIndexes[gBattlerTarget]];

    speciesAtk = GetMonData(pokeAtk, MON_DATA_SPECIES);
    pidAtk = GetMonData(pokeAtk, MON_DATA_PERSONALITY);

    speciesDef = GetMonData(pokeDef, MON_DATA_SPECIES);
    pidDef = GetMonData(pokeDef, MON_DATA_PERSONALITY);

    if (!(gBattleTypeFlags & BATTLE_TYPE_SAFARI)) // Why isn't that check done at the beginning?
    {
        u8 moveType;
        s32 i;
        u16 move;
        u8 side;
        u8 target1;

        if (special)
            gLastUsedAbility = special;
        else
            gLastUsedAbility = gBattleMons[battler].ability;

        if (moveArg)
            move = moveArg;
        else
            move = gCurrentMove;

        GET_MOVE_TYPE(move, moveType);

        switch (caseID)
        {
        case ABILITYEFFECT_ON_SWITCHIN: // 0
            if (gBattlerAttacker >= gBattlersCount)
                gBattlerAttacker = battler;
            switch (gLastUsedAbility)
            {
            case ABILITYEFFECT_SWITCH_IN_WEATHER:
                if (!(gBattleTypeFlags & BATTLE_TYPE_RECORDED))
                {
                    switch (GetCurrentWeather())
                    {
                    case WEATHER_RAIN:
                    case WEATHER_RAIN_THUNDERSTORM:
                    case WEATHER_DOWNPOUR:
                        if (!(gBattleWeather & B_WEATHER_RAIN))
                        {
                            gBattleWeather = (B_WEATHER_RAIN_TEMPORARY | B_WEATHER_RAIN_PERMANENT);
                            gBattleScripting.animArg1 = B_ANIM_RAIN_CONTINUES;
                            gBattleScripting.battler = battler;
                            effect++;
                        }
                        break;
                    case WEATHER_SANDSTORM:
                        if (!(gBattleWeather & B_WEATHER_SANDSTORM))
                        {
                            gBattleWeather = B_WEATHER_SANDSTORM;
                            gBattleScripting.animArg1 = B_ANIM_SANDSTORM_CONTINUES;
                            gBattleScripting.battler = battler;
                            effect++;
                        }
                        break;
                    case WEATHER_DROUGHT:
                        if (!(gBattleWeather & B_WEATHER_SUN))
                        {
                            gBattleWeather = B_WEATHER_SUN;
                            gBattleScripting.animArg1 = B_ANIM_SUN_CONTINUES;
                            gBattleScripting.battler = battler;
                            effect++;
                        }
                        break;
                    }
                }
                if (effect != 0)
                {
                    gBattleCommunication[MULTISTRING_CHOOSER] = GetCurrentWeather();
                    BattleScriptPushCursorAndCallback(BattleScript_OverworldWeatherStarts);
                }
                break;
            case ABILITY_DRIZZLE:
                if (!(gBattleWeather & B_WEATHER_RAIN_PERMANENT))
                {
                    gBattleWeather = (B_WEATHER_RAIN_PERMANENT | B_WEATHER_RAIN_TEMPORARY);
                    BattleScriptPushCursorAndCallback(BattleScript_DrizzleActivates);
                    gBattleScripting.battler = battler;
                    effect++;
                }
                break;
            case ABILITY_SAND_STREAM:
                if (!(gBattleWeather & B_WEATHER_SANDSTORM_PERMANENT))
                {
                    gBattleWeather = B_WEATHER_SANDSTORM;
                    BattleScriptPushCursorAndCallback(BattleScript_SandstreamActivates);
                    gBattleScripting.battler = battler;
                    effect++;
                }
                break;
            case ABILITY_DROUGHT:
                if (!(gBattleWeather & B_WEATHER_SUN_PERMANENT))
                {
                    gBattleWeather = B_WEATHER_SUN;
                    BattleScriptPushCursorAndCallback(BattleScript_DroughtActivates);
                    gBattleScripting.battler = battler;
                    effect++;
                }
                break;
            case ABILITY_INTIMIDATE:
                if (!(gSpecialStatuses[battler].intimidatedMon))
                {
                    gStatuses3[battler] |= STATUS3_INTIMIDATE_POKES;
                    gSpecialStatuses[battler].intimidatedMon = 1;
                }
                break;
            case ABILITY_FORECAST:
                effect = CastformDataTypeChange(battler);
                if (effect != 0)
                {
                    BattleScriptPushCursorAndCallback(BattleScript_CastformChange);
                    gBattleScripting.battler = battler;
                    *(&gBattleStruct->formToChangeInto) = effect - 1;
                }
                break;
            case ABILITY_TRACE:
                if (!(gSpecialStatuses[battler].traced))
                {
                    gStatuses3[battler] |= STATUS3_TRACE;
                    gSpecialStatuses[battler].traced = 1;
                }
                break;
            case ABILITY_CLOUD_NINE:
            case ABILITY_AIR_LOCK:
                {
                    // that's a weird choice for a variable, why not use i or battler?
                    for (target1 = 0; target1 < gBattlersCount; target1++)
                    {
                        effect = CastformDataTypeChange(target1);
                        if (effect != 0)
                        {
                            BattleScriptPushCursorAndCallback(BattleScript_CastformChange);
                            gBattleScripting.battler = target1;
                            *(&gBattleStruct->formToChangeInto) = effect - 1;
                            break;
                        }
                    }
                }
                break;
            }
            break;
        case ABILITYEFFECT_ENDTURN: // 1
            if (gBattleMons[battler].hp != 0)
            {
                gBattlerAttacker = battler;
                switch (gLastUsedAbility)
                {
                case ABILITY_RAIN_DISH:
                    if (WEATHER_HAS_EFFECT && (gBattleWeather & B_WEATHER_RAIN)
                     && gBattleMons[battler].maxHP > gBattleMons[battler].hp)
                    {
                        gLastUsedAbility = ABILITY_RAIN_DISH; // why
                        BattleScriptPushCursorAndCallback(BattleScript_RainDishActivates);
                        gBattleMoveDamage = gBattleMons[battler].maxHP / 16;
                        if (gBattleMoveDamage == 0)
                            gBattleMoveDamage = 1;
                        gBattleMoveDamage *= -1;
                        effect++;
                    }
                    break;
                case ABILITY_SHED_SKIN:
                    if ((gBattleMons[battler].status1 & STATUS1_ANY) && (Random() % 3) == 0)
                    {
                        if (gBattleMons[battler].status1 & (STATUS1_POISON | STATUS1_TOXIC_POISON))
                            StringCopy(gBattleTextBuff1, gStatusConditionString_PoisonJpn);
                        if (gBattleMons[battler].status1 & STATUS1_SLEEP)
                            StringCopy(gBattleTextBuff1, gStatusConditionString_SleepJpn);
                        if (gBattleMons[battler].status1 & STATUS1_PARALYSIS)
                            StringCopy(gBattleTextBuff1, gStatusConditionString_ParalysisJpn);
                        if (gBattleMons[battler].status1 & STATUS1_BURN)
                            StringCopy(gBattleTextBuff1, gStatusConditionString_BurnJpn);
                        if (gBattleMons[battler].status1 & STATUS1_FREEZE)
                            StringCopy(gBattleTextBuff1, gStatusConditionString_IceJpn);
                        gBattleMons[battler].status1 = 0;
                        gBattleMons[battler].status2 &= ~STATUS2_NIGHTMARE;  // fix nightmare glitch
                        gBattleScripting.battler = gActiveBattler = battler;
                        BattleScriptPushCursorAndCallback(BattleScript_ShedSkinActivates);
                        BtlController_EmitSetMonData(BUFFER_A, REQUEST_STATUS_BATTLE, 0, 4, &gBattleMons[battler].status1);
                        MarkBattlerForControllerExec(gActiveBattler);
                        effect++;
                    }
                    break;
                case ABILITY_SPEED_BOOST:
                    if (gBattleMons[battler].statStages[STAT_SPEED] < MAX_STAT_STAGE && gDisableStructs[battler].isFirstTurn != 2)
                    {
                        gBattleMons[battler].statStages[STAT_SPEED]++;
                        gBattleScripting.animArg1 = STAT_ANIM_PLUS1 + STAT_SPEED;
                        gBattleScripting.animArg2 = 0;
                        BattleScriptPushCursorAndCallback(BattleScript_SpeedBoostActivates);
                        gBattleScripting.battler = battler;
                        effect++;
                    }
                    break;
                case ABILITY_TRUANT:
                    gDisableStructs[gBattlerAttacker].truantCounter ^= 1;
                    break;
                }
            }
            break;
        case ABILITYEFFECT_MOVES_BLOCK: // 2
            if (gLastUsedAbility == ABILITY_SOUNDPROOF)
            {
                for (i = 0; sSoundMovesTable[i] != SOUND_MOVES_END; i++)
                {
                    if (sSoundMovesTable[i] == move)
                        break;
                }
                if (sSoundMovesTable[i] != SOUND_MOVES_END)
                {
                    if (gBattleMons[gBattlerAttacker].status2 & STATUS2_MULTIPLETURNS)
                        gHitMarker |= HITMARKER_NO_PPDEDUCT;
                    gBattlescriptCurrInstr = BattleScript_SoundproofProtected;
                    effect = 1;
                }
            }
            break;
        case ABILITYEFFECT_ABSORBING: // 3
            if (move)
            {
                switch (gLastUsedAbility)
                {
                case ABILITY_VOLT_ABSORB:
                    if (moveType == TYPE_ELECTRIC && gBattleMoves[move].power != 0)
                    {
                        if (gProtectStructs[gBattlerAttacker].notFirstStrike)
                            gBattlescriptCurrInstr = BattleScript_MoveHPDrain;
                        else
                            gBattlescriptCurrInstr = BattleScript_MoveHPDrain_PPLoss;

                        effect = 1;
                    }
                    break;
                case ABILITY_WATER_ABSORB:
                    if (moveType == TYPE_WATER && gBattleMoves[move].power != 0)
                    {
                        if (gProtectStructs[gBattlerAttacker].notFirstStrike)
                            gBattlescriptCurrInstr = BattleScript_MoveHPDrain;
                        else
                            gBattlescriptCurrInstr = BattleScript_MoveHPDrain_PPLoss;

                        effect = 1;
                    }
                    break;
                case ABILITY_FLASH_FIRE:
                    if (moveType == TYPE_FIRE && !(gBattleMons[battler].status1 & STATUS1_FREEZE))
                    {
                        if (!(gBattleResources->flags->flags[battler] & RESOURCE_FLAG_FLASH_FIRE))
                        {
                            gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_FLASH_FIRE_BOOST;
                            if (gProtectStructs[gBattlerAttacker].notFirstStrike)
                                gBattlescriptCurrInstr = BattleScript_FlashFireBoost;
                            else
                                gBattlescriptCurrInstr = BattleScript_FlashFireBoost_PPLoss;

                            gBattleResources->flags->flags[battler] |= RESOURCE_FLAG_FLASH_FIRE;
                            effect = 2;
                        }
                        else
                        {
                            gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_FLASH_FIRE_NO_BOOST;
                            if (gProtectStructs[gBattlerAttacker].notFirstStrike)
                                gBattlescriptCurrInstr = BattleScript_FlashFireBoost;
                            else
                                gBattlescriptCurrInstr = BattleScript_FlashFireBoost_PPLoss;

                            effect = 2;
                        }
                    }
                    break;
                }
                if (effect == 1)
                {
                    if (gBattleMons[battler].maxHP == gBattleMons[battler].hp)
                    {
                        if ((gProtectStructs[gBattlerAttacker].notFirstStrike))
                            gBattlescriptCurrInstr = BattleScript_MonMadeMoveUseless;
                        else
                            gBattlescriptCurrInstr = BattleScript_MonMadeMoveUseless_PPLoss;
                    }
                    else
                    {
                        gBattleMoveDamage = gBattleMons[battler].maxHP / 4;
                        if (gBattleMoveDamage == 0)
                            gBattleMoveDamage = 1;
                        gBattleMoveDamage *= -1;
                    }
                }
            }
            break;
        case ABILITYEFFECT_ON_DAMAGE: // Contact abilities and Color Change
            switch (gLastUsedAbility)
            {
            case ABILITY_COLOR_CHANGE:
                if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                 && move != MOVE_STRUGGLE
                 && gBattleMoves[move].power != 0
                 && TARGET_TURN_DAMAGED
                 && !IS_BATTLER_OF_TYPE(battler, moveType)
                 && gBattleMons[battler].hp != 0)
                {
                    SET_BATTLER_TYPE(battler, moveType);
                    PREPARE_TYPE_BUFFER(gBattleTextBuff1, moveType);
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_ColorChangeActivates;
                    effect++;
                }
                break;
            case ABILITY_ROUGH_SKIN:
                if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                 && gBattleMons[gBattlerAttacker].hp != 0
                 && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                 && TARGET_TURN_DAMAGED
                 && (gBattleMoves[move].flags & FLAG_MAKES_CONTACT))
                {
                    gBattleMoveDamage = gBattleMons[gBattlerAttacker].maxHP / 16;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_RoughSkinActivates;
                    effect++;
                }
                break;
            case ABILITY_EFFECT_SPORE:
                if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                 && gBattleMons[gBattlerAttacker].hp != 0
                 && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                 && TARGET_TURN_DAMAGED
                 && (gBattleMoves[move].flags & FLAG_MAKES_CONTACT)
                 && (Random() % 10) == 0)
                {
                    do
                    {
                        // Pick either MOVE_EFFECT_SLEEP, MOVE_EFFECT_POISON or MOVE_EFFECT_BURN
                        gBattleCommunication[MOVE_EFFECT_BYTE] = Random() & 3;
                    } while (gBattleCommunication[MOVE_EFFECT_BYTE] == 0);
                    // Replace MOVE_EFFECT_BURN with MOVE_EFFECT_PARALYSIS
                    if (gBattleCommunication[MOVE_EFFECT_BYTE] == MOVE_EFFECT_BURN)
                        gBattleCommunication[MOVE_EFFECT_BYTE] += (MOVE_EFFECT_PARALYSIS - MOVE_EFFECT_BURN);

                    gBattleCommunication[MOVE_EFFECT_BYTE] += MOVE_EFFECT_AFFECTS_USER;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_ApplySecondaryEffect;
                    gHitMarker |= HITMARKER_STATUS_ABILITY_EFFECT;
                    effect++;
                }
                break;
            case ABILITY_POISON_POINT:
                if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                 && gBattleMons[gBattlerAttacker].hp != 0
                 && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                 && TARGET_TURN_DAMAGED
                 && (gBattleMoves[move].flags & FLAG_MAKES_CONTACT)
                 && (Random() % 3) == 0)
                {
                    gBattleCommunication[MOVE_EFFECT_BYTE] = MOVE_EFFECT_AFFECTS_USER | MOVE_EFFECT_POISON;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_ApplySecondaryEffect;
                    gHitMarker |= HITMARKER_STATUS_ABILITY_EFFECT;
                    effect++;
                }
                break;
            case ABILITY_STATIC:
                if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                 && gBattleMons[gBattlerAttacker].hp != 0
                 && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                 && TARGET_TURN_DAMAGED
                 && (gBattleMoves[move].flags & FLAG_MAKES_CONTACT)
                 && (Random() % 3) == 0)
                {
                    gBattleCommunication[MOVE_EFFECT_BYTE] = MOVE_EFFECT_AFFECTS_USER | MOVE_EFFECT_PARALYSIS;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_ApplySecondaryEffect;
                    gHitMarker |= HITMARKER_STATUS_ABILITY_EFFECT;
                    effect++;
                }
                break;
            case ABILITY_FLAME_BODY:
                if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                 && gBattleMons[gBattlerAttacker].hp != 0
                 && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                 && (gBattleMoves[move].flags & FLAG_MAKES_CONTACT)
                 && TARGET_TURN_DAMAGED
                 && (Random() % 3) == 0)
                {
                    gBattleCommunication[MOVE_EFFECT_BYTE] = MOVE_EFFECT_AFFECTS_USER | MOVE_EFFECT_BURN;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_ApplySecondaryEffect;
                    gHitMarker |= HITMARKER_STATUS_ABILITY_EFFECT;
                    effect++;
                }
                break;
            case ABILITY_CUTE_CHARM:
                if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                 && gBattleMons[gBattlerAttacker].hp != 0
                 && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                 && (gBattleMoves[move].flags & FLAG_MAKES_CONTACT)
                 && TARGET_TURN_DAMAGED
                 && gBattleMons[gBattlerTarget].hp != 0
                 && (Random() % 3) == 0
                 && gBattleMons[gBattlerAttacker].ability != ABILITY_OBLIVIOUS
                 && GetGenderFromSpeciesAndPersonality(speciesAtk, pidAtk) != GetGenderFromSpeciesAndPersonality(speciesDef, pidDef)
                 && !(gBattleMons[gBattlerAttacker].status2 & STATUS2_INFATUATION)
                 && GetGenderFromSpeciesAndPersonality(speciesAtk, pidAtk) != MON_GENDERLESS
                 && GetGenderFromSpeciesAndPersonality(speciesDef, pidDef) != MON_GENDERLESS)
                {
                    gBattleMons[gBattlerAttacker].status2 |= STATUS2_INFATUATED_WITH(gBattlerTarget);
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_CuteCharmActivates;
                    effect++;
                }
                break;
            }
            break;
        case ABILITYEFFECT_IMMUNITY: // 5
            for (battler = 0; battler < gBattlersCount; battler++)
            {
                switch (gBattleMons[battler].ability)
                {
                case ABILITY_IMMUNITY:
                    if (gBattleMons[battler].status1 & (STATUS1_POISON | STATUS1_TOXIC_POISON | STATUS1_TOXIC_COUNTER))
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_PoisonJpn);
                        effect = 1;
                    }
                    break;
                case ABILITY_OWN_TEMPO:
                    if (gBattleMons[battler].status2 & STATUS2_CONFUSION)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_ConfusionJpn);
                        effect = 2;
                    }
                    break;
                case ABILITY_LIMBER:
                    if (gBattleMons[battler].status1 & STATUS1_PARALYSIS)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_ParalysisJpn);
                        effect = 1;
                    }
                    break;
                case ABILITY_INSOMNIA:
                case ABILITY_VITAL_SPIRIT:
                    if (gBattleMons[battler].status1 & STATUS1_SLEEP)
                    {
                        gBattleMons[battler].status2 &= ~STATUS2_NIGHTMARE;
                        StringCopy(gBattleTextBuff1, gStatusConditionString_SleepJpn);
                        effect = 1;
                    }
                    break;
                case ABILITY_WATER_VEIL:
                    if (gBattleMons[battler].status1 & STATUS1_BURN)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_BurnJpn);
                        effect = 1;
                    }
                    break;
                case ABILITY_MAGMA_ARMOR:
                    if (gBattleMons[battler].status1 & STATUS1_FREEZE)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_IceJpn);
                        effect = 1;
                    }
                    break;
                case ABILITY_OBLIVIOUS:
                    if (gBattleMons[battler].status2 & STATUS2_INFATUATION)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_LoveJpn);
                        effect = 3;
                    }
                    break;
                }
                if (effect != 0)
                {
                    switch (effect)
                    {
                    case 1: // status cleared
                        gBattleMons[battler].status1 = 0;
                        break;
                    case 2: // get rid of confusion
                        gBattleMons[battler].status2 &= ~STATUS2_CONFUSION;
                        break;
                    case 3: // get rid of infatuation
                        gBattleMons[battler].status2 &= ~STATUS2_INFATUATION;
                        break;
                    }

                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_AbilityCuredStatus;
                    gBattleScripting.battler = battler;
                    gActiveBattler = battler;
                    BtlController_EmitSetMonData(BUFFER_A, REQUEST_STATUS_BATTLE, 0, 4, &gBattleMons[gActiveBattler].status1);
                    MarkBattlerForControllerExec(gActiveBattler);
                    return effect;
                }
            }
            break;
        case ABILITYEFFECT_FORECAST: // 6
            for (battler = 0; battler < gBattlersCount; battler++)
            {
                if (gBattleMons[battler].ability == ABILITY_FORECAST)
                {
                    effect = CastformDataTypeChange(battler);
                    if (effect != 0)
                    {
                        BattleScriptPushCursorAndCallback(BattleScript_CastformChange);
                        gBattleScripting.battler = battler;
                        *(&gBattleStruct->formToChangeInto) = effect - 1;
                        return effect;
                    }
                }
            }
            break;
        case ABILITYEFFECT_SYNCHRONIZE: // 7
            if (gLastUsedAbility == ABILITY_SYNCHRONIZE && (gHitMarker & HITMARKER_SYNCHRONISE_EFFECT))
            {
                gHitMarker &= ~HITMARKER_SYNCHRONISE_EFFECT;
                gBattleStruct->synchronizeMoveEffect &= ~(MOVE_EFFECT_AFFECTS_USER | MOVE_EFFECT_CERTAIN);
                if (gBattleStruct->synchronizeMoveEffect == MOVE_EFFECT_TOXIC)
                    gBattleStruct->synchronizeMoveEffect = MOVE_EFFECT_POISON;

                gBattleCommunication[MOVE_EFFECT_BYTE] = gBattleStruct->synchronizeMoveEffect + MOVE_EFFECT_AFFECTS_USER;
                gBattleScripting.battler = gBattlerTarget;
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_SynchronizeActivates;
                gHitMarker |= HITMARKER_STATUS_ABILITY_EFFECT;
                effect++;
            }
            break;
        case ABILITYEFFECT_ATK_SYNCHRONIZE: // 8
            if (gLastUsedAbility == ABILITY_SYNCHRONIZE && (gHitMarker & HITMARKER_SYNCHRONISE_EFFECT))
            {
                gHitMarker &= ~HITMARKER_SYNCHRONISE_EFFECT;
                gBattleStruct->synchronizeMoveEffect &= ~(MOVE_EFFECT_AFFECTS_USER | MOVE_EFFECT_CERTAIN);
                if (gBattleStruct->synchronizeMoveEffect == MOVE_EFFECT_TOXIC)
                    gBattleStruct->synchronizeMoveEffect = MOVE_EFFECT_POISON;

                gBattleCommunication[MOVE_EFFECT_BYTE] = gBattleStruct->synchronizeMoveEffect;
                gBattleScripting.battler = gBattlerAttacker;
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_SynchronizeActivates;
                gHitMarker |= HITMARKER_STATUS_ABILITY_EFFECT;
                effect++;
            }
            break;
        case ABILITYEFFECT_INTIMIDATE1: // 9
            for (i = 0; i < gBattlersCount; i++)
            {
                if (gBattleMons[i].ability == ABILITY_INTIMIDATE && gStatuses3[i] & STATUS3_INTIMIDATE_POKES)
                {
                    gLastUsedAbility = ABILITY_INTIMIDATE;
                    gStatuses3[i] &= ~STATUS3_INTIMIDATE_POKES;
                    BattleScriptPushCursorAndCallback(BattleScript_IntimidateActivatesEnd3);
                    gBattleStruct->intimidateBattler = i;
                    effect++;
                    break;
                }
            }
            break;
        case ABILITYEFFECT_TRACE: // 11
            for (i = 0; i < gBattlersCount; i++)
            {
                if (gBattleMons[i].ability == ABILITY_TRACE && (gStatuses3[i] & STATUS3_TRACE))
                {
                    u8 target2;
                    side = BATTLE_OPPOSITE(GetBattlerPosition(i)) & BIT_SIDE; // side of the opposing Pokémon
                    target1 = GetBattlerAtPosition(side);
                    target2 = GetBattlerAtPosition(side + BIT_FLANK);
                    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
                    {
                        if (gBattleMons[target1].ability != ABILITY_NONE && gBattleMons[target1].hp != 0
                         && gBattleMons[target2].ability != ABILITY_NONE && gBattleMons[target2].hp != 0)
                        {
                            gActiveBattler = GetBattlerAtPosition(((Random() & 1) * 2) | side);
                            gBattleMons[i].ability = gBattleMons[gActiveBattler].ability;
                            gLastUsedAbility = gBattleMons[gActiveBattler].ability;
                            effect++;
                        }
                        else if (gBattleMons[target1].ability != ABILITY_NONE && gBattleMons[target1].hp != 0)
                        {
                            gActiveBattler = target1;
                            gBattleMons[i].ability = gBattleMons[gActiveBattler].ability;
                            gLastUsedAbility = gBattleMons[gActiveBattler].ability;
                            effect++;
                        }
                        else if (gBattleMons[target2].ability != ABILITY_NONE && gBattleMons[target2].hp != 0)
                        {
                            gActiveBattler = target2;
                            gBattleMons[i].ability = gBattleMons[gActiveBattler].ability;
                            gLastUsedAbility = gBattleMons[gActiveBattler].ability;
                            effect++;
                        }
                    }
                    else
                    {
                        gActiveBattler = target1;
                        if (gBattleMons[target1].ability && gBattleMons[target1].hp)
                        {
                            gBattleMons[i].ability = gBattleMons[target1].ability;
                            gLastUsedAbility = gBattleMons[target1].ability;
                            effect++;
                        }
                    }
                    if (effect != 0)
                    {
                        BattleScriptPushCursorAndCallback(BattleScript_TraceActivates);
                        gStatuses3[i] &= ~STATUS3_TRACE;
                        gBattleScripting.battler = i;

                        PREPARE_MON_NICK_WITH_PREFIX_BUFFER(gBattleTextBuff1, gActiveBattler, gBattlerPartyIndexes[gActiveBattler])
                        PREPARE_ABILITY_BUFFER(gBattleTextBuff2, gLastUsedAbility)
                        break;
                    }
                }
            }
            break;
        case ABILITYEFFECT_INTIMIDATE2: // 10
            for (i = 0; i < gBattlersCount; i++)
            {
                if (gBattleMons[i].ability == ABILITY_INTIMIDATE && (gStatuses3[i] & STATUS3_INTIMIDATE_POKES))
                {
                    gLastUsedAbility = ABILITY_INTIMIDATE;
                    gStatuses3[i] &= ~STATUS3_INTIMIDATE_POKES;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_IntimidateActivates;
                    gBattleStruct->intimidateBattler = i;
                    effect++;
                    break;
                }
            }
            break;
        case ABILITYEFFECT_CHECK_OTHER_SIDE: // 12
            side = GetBattlerSide(battler);
            for (i = 0; i < gBattlersCount; i++)
            {
                if (GetBattlerSide(i) != side && gBattleMons[i].ability == ability)
                {
                    gLastUsedAbility = ability;
                    effect = i + 1;
                }
            }
            break;
        case ABILITYEFFECT_CHECK_BATTLER_SIDE: // 13
            side = GetBattlerSide(battler);
            for (i = 0; i < gBattlersCount; i++)
            {
                if (GetBattlerSide(i) == side && gBattleMons[i].ability == ability)
                {
                    gLastUsedAbility = ability;
                    effect = i + 1;
                }
            }
            break;
        case ABILITYEFFECT_FIELD_SPORT: // 14
            switch (gLastUsedAbility)
            {
            case ABILITYEFFECT_MUD_SPORT:
                for (i = 0; i < gBattlersCount; i++)
                {
                    if (gStatuses3[i] & STATUS3_MUDSPORT)
                        effect = i + 1;
                }
                break;
            case ABILITYEFFECT_WATER_SPORT:
                for (i = 0; i < gBattlersCount; i++)
                {
                    if (gStatuses3[i] & STATUS3_WATERSPORT)
                        effect = i + 1;
                }
                break;
            default:
                for (i = 0; i < gBattlersCount; i++)
                {
                    if (gBattleMons[i].ability == ability)
                    {
                        gLastUsedAbility = ability;
                        effect = i + 1;
                    }
                }
                break;
            }
            break;
        case ABILITYEFFECT_CHECK_ON_FIELD: // 19
            for (i = 0; i < gBattlersCount; i++)
            {
                if (gBattleMons[i].ability == ability && gBattleMons[i].hp != 0)
                {
                    gLastUsedAbility = ability;
                    effect = i + 1;
                }
            }
            break;
        case ABILITYEFFECT_CHECK_FIELD_EXCEPT_BATTLER: // 15
            for (i = 0; i < gBattlersCount; i++)
            {
                if (gBattleMons[i].ability == ability && i != battler)
                {
                    gLastUsedAbility = ability;
                    effect = i + 1;
                }
            }
            break;
        case ABILITYEFFECT_COUNT_OTHER_SIDE: // 16
            side = GetBattlerSide(battler);
            for (i = 0; i < gBattlersCount; i++)
            {
                if (GetBattlerSide(i) != side && gBattleMons[i].ability == ability)
                {
                    gLastUsedAbility = ability;
                    effect++;
                }
            }
            break;
        case ABILITYEFFECT_COUNT_BATTLER_SIDE: // 17
            side = GetBattlerSide(battler);
            for (i = 0; i < gBattlersCount; i++)
            {
                if (GetBattlerSide(i) == side && gBattleMons[i].ability == ability)
                {
                    gLastUsedAbility = ability;
                    effect++;
                }
            }
            break;
        case ABILITYEFFECT_COUNT_ON_FIELD: // 18
            for (i = 0; i < gBattlersCount; i++)
            {
                if (gBattleMons[i].ability == ability && i != battler)
                {
                    gLastUsedAbility = ability;
                    effect++;
                }
            }
            break;
        }

        if (effect && caseID < ABILITYEFFECT_CHECK_OTHER_SIDE && gLastUsedAbility != 0xFF)
            RecordAbilityBattle(battler, gLastUsedAbility);
    }

    return effect;
}

void BattleScriptExecute(const u8 *BS_ptr)
{
    gBattlescriptCurrInstr = BS_ptr;
    gBattleResources->battleCallbackStack->function[gBattleResources->battleCallbackStack->size++] = gBattleMainFunc;
    gBattleMainFunc = RunBattleScriptCommands_PopCallbacksStack;
    gCurrentActionFuncId = 0;
}

void BattleScriptPushCursorAndCallback(const u8 *BS_ptr)
{
    BattleScriptPushCursor();
    gBattlescriptCurrInstr = BS_ptr;
    gBattleResources->battleCallbackStack->function[gBattleResources->battleCallbackStack->size++] = gBattleMainFunc;
    gBattleMainFunc = RunBattleScriptCommands;
}

enum
{
    ITEM_NO_EFFECT,
    ITEM_STATUS_CHANGE,
    ITEM_EFFECT_OTHER,
    ITEM_PP_CHANGE,
    ITEM_HP_CHANGE,
    ITEM_STATS_CHANGE,
};

#define TRY_EAT_CONFUSE_BERRY(flavor)                                                       \
    if (gBattleMons[battlerId].hp <= gBattleMons[battlerId].maxHP / 2 && !moveTurn)         \
    {                                                                                       \
        PREPARE_FLAVOR_BUFFER(gBattleTextBuff1, flavor);                                    \
        gBattleMoveDamage = gBattleMons[battlerId].maxHP / battlerHoldEffectParam;          \
        if (gBattleMoveDamage == 0)                                                         \
            gBattleMoveDamage = 1;                                                          \
        if (gBattleMons[battlerId].hp + gBattleMoveDamage > gBattleMons[battlerId].maxHP)   \
            gBattleMoveDamage = gBattleMons[battlerId].maxHP - gBattleMons[battlerId].hp;   \
        gBattleMoveDamage *= -1;                                                            \
        if (GetFlavorRelationByPersonality(gBattleMons[battlerId].personality, flavor) < 0) \
            BattleScriptExecute(BattleScript_BerryConfuseHealEnd2);                         \
        else                                                                                \
            BattleScriptExecute(BattleScript_ItemHealHP_RemoveItem);                        \
        effect = ITEM_HP_CHANGE;                                                            \
    }

#define TRY_EAT_STAT_UP_BERRY(stat)                                                         \
    if (gBattleMons[battlerId].hp <= gBattleMons[battlerId].maxHP / battlerHoldEffectParam  \
    && !moveTurn && gBattleMons[battlerId].statStages[stat] < MAX_STAT_STAGE)               \
    {                                                                                       \
        PREPARE_STAT_BUFFER(gBattleTextBuff1, stat);                                        \
        gEffectBattler = battlerId;                                                         \
        SET_STATCHANGER(stat, 1, FALSE);                                                    \
        gBattleScripting.animArg1 = STAT_ANIM_PLUS1 + (stat);                               \
        gBattleScripting.animArg2 = 0;                                                      \
        BattleScriptExecute(BattleScript_BerryStatRaiseEnd2);                               \
        effect = ITEM_STATS_CHANGE;                                                         \
    }

u8 ItemBattleEffects(u8 caseID, u8 battlerId, bool8 moveTurn)
{
    int i = 0;
    u8 effect = ITEM_NO_EFFECT;
    u8 changedPP = 0;
    u8 battlerHoldEffect, atkHoldEffect, UNUSED defHoldEffect;
    u8 battlerHoldEffectParam, atkHoldEffectParam, UNUSED defHoldEffectParam;
    u16 atkItem, defItem;

    gLastUsedItem = gBattleMons[battlerId].item;
    if (gLastUsedItem == ITEM_ENIGMA_BERRY)
    {
        battlerHoldEffect = gEnigmaBerries[battlerId].holdEffect;
        battlerHoldEffectParam = gEnigmaBerries[battlerId].holdEffectParam;
    }
    else
    {
        battlerHoldEffect = ItemId_GetHoldEffect(gLastUsedItem);
        battlerHoldEffectParam = ItemId_GetHoldEffectParam(gLastUsedItem);
    }

    atkItem = gBattleMons[gBattlerAttacker].item;
    if (atkItem == ITEM_ENIGMA_BERRY)
    {
        atkHoldEffect = gEnigmaBerries[gBattlerAttacker].holdEffect;
        atkHoldEffectParam = gEnigmaBerries[gBattlerAttacker].holdEffectParam;
    }
    else
    {
        atkHoldEffect = ItemId_GetHoldEffect(atkItem);
        atkHoldEffectParam = ItemId_GetHoldEffectParam(atkItem);
    }

    // def variables are unused
    defItem = gBattleMons[gBattlerTarget].item;
    if (defItem == ITEM_ENIGMA_BERRY)
    {
        defHoldEffect = gEnigmaBerries[gBattlerTarget].holdEffect;
        defHoldEffectParam = gEnigmaBerries[gBattlerTarget].holdEffectParam;
    }
    else
    {
        defHoldEffect = ItemId_GetHoldEffect(defItem);
        defHoldEffectParam = ItemId_GetHoldEffectParam(defItem);
    }

    switch (caseID)
    {
    case ITEMEFFECT_ON_SWITCH_IN:
        switch (battlerHoldEffect)
        {
        case HOLD_EFFECT_DOUBLE_PRIZE:
            if (GetBattlerSide(battlerId) == B_SIDE_PLAYER)
                gBattleStruct->moneyMultiplier = 2;
            break;
        case HOLD_EFFECT_RESTORE_STATS:
            for (i = 0; i < NUM_BATTLE_STATS; i++)
            {
                if (gBattleMons[battlerId].statStages[i] < DEFAULT_STAT_STAGE)
                {
                    gBattleMons[battlerId].statStages[i] = DEFAULT_STAT_STAGE;
                    effect = ITEM_STATS_CHANGE;
                }
            }
            if (effect != 0)
            {
                gBattleScripting.battler = battlerId;
                gPotentialItemEffectBattler = battlerId;
                gActiveBattler = gBattlerAttacker = battlerId;
                BattleScriptExecute(BattleScript_WhiteHerbEnd2);
            }
            break;
        }
        break;
    case ITEMEFFECT_NORMAL:
        if (gBattleMons[battlerId].hp)
        {
            switch (battlerHoldEffect)
            {
            case HOLD_EFFECT_RESTORE_HP:
                if (gBattleMons[battlerId].hp <= gBattleMons[battlerId].maxHP / 2 && !moveTurn)
                {
                    gBattleMoveDamage = battlerHoldEffectParam;
                    if (gBattleMons[battlerId].hp + battlerHoldEffectParam > gBattleMons[battlerId].maxHP)
                        gBattleMoveDamage = gBattleMons[battlerId].maxHP - gBattleMons[battlerId].hp;
                    gBattleMoveDamage *= -1;
                    BattleScriptExecute(BattleScript_ItemHealHP_RemoveItem);
                    effect = ITEM_HP_CHANGE;
                }
                break;
            case HOLD_EFFECT_RESTORE_PP:
                if (!moveTurn)
                {
                    struct Pokemon *mon;
                    u8 ppBonuses;
                    u16 move;

                    if (GetBattlerSide(battlerId) == B_SIDE_PLAYER)
                        mon = &gPlayerParty[gBattlerPartyIndexes[battlerId]];
                    else
                        mon = &gEnemyParty[gBattlerPartyIndexes[battlerId]];
                    for (i = 0; i < MAX_MON_MOVES; i++)
                    {
                        move = GetMonData(mon, MON_DATA_MOVE1 + i);
                        changedPP = GetMonData(mon, MON_DATA_PP1 + i);
                        ppBonuses = GetMonData(mon, MON_DATA_PP_BONUSES);
                        if (move && changedPP == 0)
                            break;
                    }
                    if (i != MAX_MON_MOVES)
                    {
                        u8 maxPP = CalculatePPWithBonus(move, ppBonuses, i);
                        if (changedPP + battlerHoldEffectParam > maxPP)
                            changedPP = maxPP;
                        else
                            changedPP = changedPP + battlerHoldEffectParam;

                        PREPARE_MOVE_BUFFER(gBattleTextBuff1, move);

                        BattleScriptExecute(BattleScript_BerryPPHealEnd2);
                        BtlController_EmitSetMonData(BUFFER_A, i + REQUEST_PPMOVE1_BATTLE, 0, 1, &changedPP);
                        MarkBattlerForControllerExec(gActiveBattler);
                        effect = ITEM_PP_CHANGE;
                    }
                }
                break;
            case HOLD_EFFECT_RESTORE_STATS:
                for (i = 0; i < NUM_BATTLE_STATS; i++)
                {
                    if (gBattleMons[battlerId].statStages[i] < DEFAULT_STAT_STAGE)
                    {
                        gBattleMons[battlerId].statStages[i] = DEFAULT_STAT_STAGE;
                        effect = ITEM_STATS_CHANGE;
                    }
                }
                if (effect != 0)
                {
                    gBattleScripting.battler = battlerId;
                    gPotentialItemEffectBattler = battlerId;
                    gActiveBattler = gBattlerAttacker = battlerId;
                    BattleScriptExecute(BattleScript_WhiteHerbEnd2);
                }
                break;
            case HOLD_EFFECT_LEFTOVERS:
                if (gBattleMons[battlerId].hp < gBattleMons[battlerId].maxHP && !moveTurn)
                {
                    gBattleMoveDamage = gBattleMons[battlerId].maxHP / 16;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    if (gBattleMons[battlerId].hp + gBattleMoveDamage > gBattleMons[battlerId].maxHP)
                        gBattleMoveDamage = gBattleMons[battlerId].maxHP - gBattleMons[battlerId].hp;
                    gBattleMoveDamage *= -1;
                    BattleScriptExecute(BattleScript_ItemHealHP_End2);
                    effect = ITEM_HP_CHANGE;
                    RecordItemEffectBattle(battlerId, battlerHoldEffect);
                }
                break;
            case HOLD_EFFECT_CONFUSE_SPICY:
                TRY_EAT_CONFUSE_BERRY(FLAVOR_SPICY);
                break;
            case HOLD_EFFECT_CONFUSE_DRY:
                TRY_EAT_CONFUSE_BERRY(FLAVOR_DRY);
                break;
            case HOLD_EFFECT_CONFUSE_SWEET:
                TRY_EAT_CONFUSE_BERRY(FLAVOR_SWEET);
                break;
            case HOLD_EFFECT_CONFUSE_BITTER:
                TRY_EAT_CONFUSE_BERRY(FLAVOR_BITTER);
                break;
            case HOLD_EFFECT_CONFUSE_SOUR:
                TRY_EAT_CONFUSE_BERRY(FLAVOR_SOUR);
                break;
            case HOLD_EFFECT_ATTACK_UP:
                if (gBattleMons[battlerId].hp <= gBattleMons[battlerId].maxHP / battlerHoldEffectParam
                && !moveTurn && gBattleMons[battlerId].statStages[STAT_ATK] < MAX_STAT_STAGE)
                {
                    PREPARE_STAT_BUFFER(gBattleTextBuff1, STAT_ATK);
                    PREPARE_STRING_BUFFER(gBattleTextBuff2, STRINGID_STATROSE); // Only the Attack stat-up berry has this
                    gEffectBattler = battlerId;
                    SET_STATCHANGER(STAT_ATK, 1, FALSE);
                    gBattleScripting.animArg1 = STAT_ANIM_PLUS1 + STAT_ATK;
                    gBattleScripting.animArg2 = 0;
                    BattleScriptExecute(BattleScript_BerryStatRaiseEnd2);
                    effect = ITEM_STATS_CHANGE;
                }
                break;
            case HOLD_EFFECT_DEFENSE_UP:
                TRY_EAT_STAT_UP_BERRY(STAT_DEF);
                break;
            case HOLD_EFFECT_SPEED_UP:
                TRY_EAT_STAT_UP_BERRY(STAT_SPEED);
                break;
            case HOLD_EFFECT_SP_ATTACK_UP:
                TRY_EAT_STAT_UP_BERRY(STAT_SPATK);
                break;
            case HOLD_EFFECT_SP_DEFENSE_UP:
                TRY_EAT_STAT_UP_BERRY(STAT_SPDEF);
                break;
            case HOLD_EFFECT_CRITICAL_UP:
                if (gBattleMons[battlerId].hp <= gBattleMons[battlerId].maxHP / battlerHoldEffectParam && !moveTurn
                    && !(gBattleMons[battlerId].status2 & STATUS2_FOCUS_ENERGY))
                {
                    gBattleMons[battlerId].status2 |= STATUS2_FOCUS_ENERGY;
                    BattleScriptExecute(BattleScript_BerryFocusEnergyEnd2);
                    effect = ITEM_EFFECT_OTHER;
                }
                break;
            case HOLD_EFFECT_RANDOM_STAT_UP:
                if (!moveTurn && gBattleMons[battlerId].hp <= gBattleMons[battlerId].maxHP / battlerHoldEffectParam)
                {
                    for (i = 0; i < NUM_STATS - 1; i++)
                    {
                        if (gBattleMons[battlerId].statStages[STAT_ATK + i] < MAX_STAT_STAGE)
                            break;
                    }
                    if (i != NUM_STATS - 1)
                    {
                        do
                        {
                            i = Random() % (NUM_STATS - 1);
                        } while (gBattleMons[battlerId].statStages[STAT_ATK + i] == MAX_STAT_STAGE);

                        PREPARE_STAT_BUFFER(gBattleTextBuff1, i + 1);

                        gBattleTextBuff2[0] = B_BUFF_PLACEHOLDER_BEGIN;
                        gBattleTextBuff2[1] = B_BUFF_STRING;
                        gBattleTextBuff2[2] = STRINGID_STATSHARPLY;
                        gBattleTextBuff2[3] = STRINGID_STATSHARPLY >> 8;
                        gBattleTextBuff2[4] = B_BUFF_STRING;
                        gBattleTextBuff2[5] = STRINGID_STATROSE;
                        gBattleTextBuff2[6] = STRINGID_STATROSE >> 8;
                        gBattleTextBuff2[7] = EOS;

                        gEffectBattler = battlerId;
                        SET_STATCHANGER(i + 1, 2, FALSE);
                        gBattleScripting.animArg1 = STAT_ANIM_PLUS2 + (i + 1);
                        gBattleScripting.animArg2 = 0;
                        BattleScriptExecute(BattleScript_BerryStatRaiseEnd2);
                        effect = ITEM_STATS_CHANGE;
                    }
                }
                break;
            case HOLD_EFFECT_CURE_PAR:
                if (gBattleMons[battlerId].status1 & STATUS1_PARALYSIS)
                {
                    gBattleMons[battlerId].status1 &= ~STATUS1_PARALYSIS;
                    BattleScriptExecute(BattleScript_BerryCurePrlzEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_PSN:
                if (gBattleMons[battlerId].status1 & STATUS1_PSN_ANY)
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_PSN_ANY | STATUS1_TOXIC_COUNTER);
                    BattleScriptExecute(BattleScript_BerryCurePsnEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_BRN:
                if (gBattleMons[battlerId].status1 & STATUS1_BURN)
                {
                    gBattleMons[battlerId].status1 &= ~STATUS1_BURN;
                    BattleScriptExecute(BattleScript_BerryCureBrnEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_FRZ:
                if (gBattleMons[battlerId].status1 & STATUS1_FREEZE)
                {
                    gBattleMons[battlerId].status1 &= ~STATUS1_FREEZE;
                    BattleScriptExecute(BattleScript_BerryCureFrzEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_SLP:
                if (gBattleMons[battlerId].status1 & STATUS1_SLEEP)
                {
                    gBattleMons[battlerId].status1 &= ~STATUS1_SLEEP;
                    gBattleMons[battlerId].status2 &= ~STATUS2_NIGHTMARE;
                    BattleScriptExecute(BattleScript_BerryCureSlpEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_CONFUSION:
                if (gBattleMons[battlerId].status2 & STATUS2_CONFUSION)
                {
                    gBattleMons[battlerId].status2 &= ~STATUS2_CONFUSION;
                    BattleScriptExecute(BattleScript_BerryCureConfusionEnd2);
                    effect = ITEM_EFFECT_OTHER;
                }
                break;
            case HOLD_EFFECT_CURE_STATUS:
                if (gBattleMons[battlerId].status1 & STATUS1_ANY || gBattleMons[battlerId].status2 & STATUS2_CONFUSION)
                {
                    i = 0;
                    if (gBattleMons[battlerId].status1 & STATUS1_PSN_ANY)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_PoisonJpn);
                        i++;
                    }
                    if (gBattleMons[battlerId].status1 & STATUS1_SLEEP)
                    {
                        gBattleMons[battlerId].status2 &= ~STATUS2_NIGHTMARE;
                        StringCopy(gBattleTextBuff1, gStatusConditionString_SleepJpn);
                        i++;
                    }
                    if (gBattleMons[battlerId].status1 & STATUS1_PARALYSIS)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_ParalysisJpn);
                        i++;
                    }
                    if (gBattleMons[battlerId].status1 & STATUS1_BURN)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_BurnJpn);
                        i++;
                    }
                    if (gBattleMons[battlerId].status1 & STATUS1_FREEZE)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_IceJpn);
                        i++;
                    }
                    if (gBattleMons[battlerId].status2 & STATUS2_CONFUSION)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_ConfusionJpn);
                        i++;
                    }
                    if (i <= 1)
                        gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_CURED_PROBLEM;
                    else
                        gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_NORMALIZED_STATUS;
                    gBattleMons[battlerId].status1 = 0;
                    gBattleMons[battlerId].status2 &= ~STATUS2_CONFUSION;
                    BattleScriptExecute(BattleScript_BerryCureChosenStatusEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_ATTRACT:
                if (gBattleMons[battlerId].status2 & STATUS2_INFATUATION)
                {
                    gBattleMons[battlerId].status2 &= ~STATUS2_INFATUATION;
                    StringCopy(gBattleTextBuff1, gStatusConditionString_LoveJpn);
                    BattleScriptExecute(BattleScript_BerryCureChosenStatusEnd2);
                    gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_CURED_PROBLEM;
                    effect = ITEM_EFFECT_OTHER;
                }
                break;
            }
            if (effect != 0)
            {
                gBattleScripting.battler = battlerId;
                gPotentialItemEffectBattler = battlerId;
                gActiveBattler = gBattlerAttacker = battlerId;
                switch (effect)
                {
                case ITEM_STATUS_CHANGE:
                    BtlController_EmitSetMonData(BUFFER_A, REQUEST_STATUS_BATTLE, 0, 4, &gBattleMons[battlerId].status1);
                    MarkBattlerForControllerExec(gActiveBattler);
                    break;
                case ITEM_PP_CHANGE:
                    if (MOVE_IS_PERMANENT(battlerId, i))
                        gBattleMons[battlerId].pp[i] = changedPP;
                    break;
                }
            }
        }
        break;
    case ITEMEFFECT_DUMMY:
        break;
    case ITEMEFFECT_MOVE_END:
        for (battlerId = 0; battlerId < gBattlersCount; battlerId++)
        {
            gLastUsedItem = gBattleMons[battlerId].item;
            if (gBattleMons[battlerId].item == ITEM_ENIGMA_BERRY)
            {
                battlerHoldEffect = gEnigmaBerries[battlerId].holdEffect;
                battlerHoldEffectParam = gEnigmaBerries[battlerId].holdEffectParam;
            }
            else
            {
                battlerHoldEffect = ItemId_GetHoldEffect(gLastUsedItem);
                battlerHoldEffectParam = ItemId_GetHoldEffectParam(gLastUsedItem);
            }
            switch (battlerHoldEffect)
            {
            case HOLD_EFFECT_CURE_PAR:
                if (gBattleMons[battlerId].status1 & STATUS1_PARALYSIS)
                {
                    gBattleMons[battlerId].status1 &= ~STATUS1_PARALYSIS;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_BerryCureParRet;
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_PSN:
                if (gBattleMons[battlerId].status1 & STATUS1_PSN_ANY)
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_PSN_ANY | STATUS1_TOXIC_COUNTER);
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_BerryCurePsnRet;
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_BRN:
                if (gBattleMons[battlerId].status1 & STATUS1_BURN)
                {
                    gBattleMons[battlerId].status1 &= ~STATUS1_BURN;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_BerryCureBrnRet;
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_FRZ:
                if (gBattleMons[battlerId].status1 & STATUS1_FREEZE)
                {
                    gBattleMons[battlerId].status1 &= ~STATUS1_FREEZE;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_BerryCureFrzRet;
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_SLP:
                if (gBattleMons[battlerId].status1 & STATUS1_SLEEP)
                {
                    gBattleMons[battlerId].status1 &= ~STATUS1_SLEEP;
                    gBattleMons[battlerId].status2 &= ~STATUS2_NIGHTMARE;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_BerryCureSlpRet;
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_CONFUSION:
                if (gBattleMons[battlerId].status2 & STATUS2_CONFUSION)
                {
                    gBattleMons[battlerId].status2 &= ~STATUS2_CONFUSION;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_BerryCureConfusionRet;
                    effect = ITEM_EFFECT_OTHER;
                }
                break;
            case HOLD_EFFECT_CURE_ATTRACT:
                if (gBattleMons[battlerId].status2 & STATUS2_INFATUATION)
                {
                    gBattleMons[battlerId].status2 &= ~STATUS2_INFATUATION;
                    StringCopy(gBattleTextBuff1, gStatusConditionString_LoveJpn);
                    BattleScriptPushCursor();
                    gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_CURED_PROBLEM;
                    gBattlescriptCurrInstr = BattleScript_BerryCureChosenStatusRet;
                    effect = ITEM_EFFECT_OTHER;
                }
                break;
            case HOLD_EFFECT_CURE_STATUS:
                if (gBattleMons[battlerId].status1 & STATUS1_ANY || gBattleMons[battlerId].status2 & STATUS2_CONFUSION)
                {
                    if (gBattleMons[battlerId].status1 & STATUS1_PSN_ANY)
                        StringCopy(gBattleTextBuff1, gStatusConditionString_PoisonJpn);

                    if (gBattleMons[battlerId].status1 & STATUS1_SLEEP)
                    {
                        gBattleMons[battlerId].status2 &= ~STATUS2_NIGHTMARE;
                        StringCopy(gBattleTextBuff1, gStatusConditionString_SleepJpn);
                    }

                    if (gBattleMons[battlerId].status1 & STATUS1_PARALYSIS)
                        StringCopy(gBattleTextBuff1, gStatusConditionString_ParalysisJpn);

                    if (gBattleMons[battlerId].status1 & STATUS1_BURN)
                        StringCopy(gBattleTextBuff1, gStatusConditionString_BurnJpn);

                    if (gBattleMons[battlerId].status1 & STATUS1_FREEZE)
                        StringCopy(gBattleTextBuff1, gStatusConditionString_IceJpn);

                    if (gBattleMons[battlerId].status2 & STATUS2_CONFUSION)
                        StringCopy(gBattleTextBuff1, gStatusConditionString_ConfusionJpn);

                    gBattleMons[battlerId].status1 = 0;
                    gBattleMons[battlerId].status2 &= ~STATUS2_CONFUSION;
                    BattleScriptPushCursor();
                    gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_CURED_PROBLEM;
                    gBattlescriptCurrInstr = BattleScript_BerryCureChosenStatusRet;
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_RESTORE_STATS:
                for (i = 0; i < NUM_BATTLE_STATS; i++)
                {
                    if (gBattleMons[battlerId].statStages[i] < DEFAULT_STAT_STAGE)
                    {
                        gBattleMons[battlerId].statStages[i] = DEFAULT_STAT_STAGE;
                        effect = ITEM_STATS_CHANGE;
                    }
                }
                if (effect != 0)
                {
                    gBattleScripting.battler = battlerId;
                    gPotentialItemEffectBattler = battlerId;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_WhiteHerbRet;
                    return effect;
                }
                break;
            }
            if (effect != 0)
            {
                gBattleScripting.battler = battlerId;
                gPotentialItemEffectBattler = battlerId;
                gActiveBattler = battlerId;
                BtlController_EmitSetMonData(BUFFER_A, REQUEST_STATUS_BATTLE, 0, 4, &gBattleMons[gActiveBattler].status1);
                MarkBattlerForControllerExec(gActiveBattler);
                break;
            }
        }
        break;
    case ITEMEFFECT_KINGSROCK_SHELLBELL:
        if (gBattleMoveDamage)
        {
            switch (atkHoldEffect)
            {
            case HOLD_EFFECT_FLINCH:
                if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                    && TARGET_TURN_DAMAGED
                    && (Random() % 100) < atkHoldEffectParam
                    && gBattleMoves[gCurrentMove].flags & FLAG_KINGS_ROCK_AFFECTED
                    && gBattleMons[gBattlerTarget].hp)
                {
                    gBattleCommunication[MOVE_EFFECT_BYTE] = MOVE_EFFECT_FLINCH;
                    BattleScriptPushCursor();
                    SetMoveEffect(FALSE, 0);
                    BattleScriptPop();
                }
                break;
            case HOLD_EFFECT_SHELL_BELL:
                if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                    && gSpecialStatuses[gBattlerTarget].shellBellDmg != 0
                    && gSpecialStatuses[gBattlerTarget].shellBellDmg != IGNORE_SHELL_BELL
                    && gBattlerAttacker != gBattlerTarget
                    && gBattleMons[gBattlerAttacker].hp != gBattleMons[gBattlerAttacker].maxHP
                    && gBattleMons[gBattlerAttacker].hp != 0)
                {
                    gLastUsedItem = atkItem;
                    gPotentialItemEffectBattler = gBattlerAttacker;
                    gBattleScripting.battler = gBattlerAttacker;
                    gBattleMoveDamage = (gSpecialStatuses[gBattlerTarget].shellBellDmg / atkHoldEffectParam) * -1;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = -1;
                    gSpecialStatuses[gBattlerTarget].shellBellDmg = 0;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_ItemHealHP_Ret;
                    effect++;
                }
                break;
            }
        }
        break;
    }

    return effect;
}

void ClearFuryCutterDestinyBondGrudge(u8 battlerId)
{
    gDisableStructs[battlerId].furyCutterCounter = 0;
    gBattleMons[battlerId].status2 &= ~STATUS2_DESTINY_BOND;
    gStatuses3[battlerId] &= ~STATUS3_GRUDGE;
}

void HandleAction_RunBattleScript(void) // identical to RunBattleScriptCommands
{
    if (gBattleControllerExecFlags == 0)
        gBattleScriptingCommandsTable[*gBattlescriptCurrInstr]();
}

u8 GetMoveTarget(u16 move, u8 setTarget)
{
    u8 targetBattler = 0;
    u8 moveTarget;
    u8 side;

    if (setTarget != NO_TARGET_OVERRIDE)
        moveTarget = setTarget - 1;
    else
        moveTarget = gBattleMoves[move].target;

    switch (moveTarget)
    {
    case MOVE_TARGET_SELECTED:
        side = BATTLE_OPPOSITE(GetBattlerSide(gBattlerAttacker));
        if (gSideTimers[side].followmeTimer && gBattleMons[gSideTimers[side].followmeTarget].hp)
            targetBattler = gSideTimers[side].followmeTarget;
        else
        {
            side = GetBattlerSide(gBattlerAttacker);
            do
            {
                targetBattler = Random() % gBattlersCount;
            } while (targetBattler == gBattlerAttacker || side == GetBattlerSide(targetBattler) || gAbsentBattlerFlags & gBitTable[targetBattler]);
            if (gBattleMoves[move].type == TYPE_ELECTRIC
                && AbilityBattleEffects(ABILITYEFFECT_COUNT_OTHER_SIDE, gBattlerAttacker, ABILITY_LIGHTNING_ROD, 0, 0)
                && gBattleMons[targetBattler].ability != ABILITY_LIGHTNING_ROD)
            {
                targetBattler ^= BIT_FLANK;
                RecordAbilityBattle(targetBattler, gBattleMons[targetBattler].ability);
                gSpecialStatuses[targetBattler].lightningRodRedirected = 1;
            }
        }
        break;
    case MOVE_TARGET_DEPENDS:
    case MOVE_TARGET_BOTH:
    case MOVE_TARGET_FOES_AND_ALLY:
    case MOVE_TARGET_OPPONENTS_FIELD:
        targetBattler = GetBattlerAtPosition(BATTLE_OPPOSITE(GET_BATTLER_SIDE(gBattlerAttacker)));
        if (gAbsentBattlerFlags & gBitTable[targetBattler])
            targetBattler ^= BIT_FLANK;
        break;
    case MOVE_TARGET_RANDOM:
        side = BATTLE_OPPOSITE(GetBattlerSide(gBattlerAttacker));
        if (gSideTimers[side].followmeTimer && gBattleMons[gSideTimers[side].followmeTarget].hp)
            targetBattler = gSideTimers[side].followmeTarget;
        else if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE && moveTarget & MOVE_TARGET_RANDOM)
        {
            if (GetBattlerSide(gBattlerAttacker) == B_SIDE_PLAYER)
            {
                if (Random() & 1)
                    targetBattler = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
                else
                    targetBattler = GetBattlerAtPosition(B_POSITION_OPPONENT_RIGHT);
            }
            else
            {
                if (Random() & 1)
                    targetBattler = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
                else
                    targetBattler = GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT);
            }
            if (gAbsentBattlerFlags & gBitTable[targetBattler])
                targetBattler ^= BIT_FLANK;
        }
        else
            targetBattler = GetBattlerAtPosition(BATTLE_OPPOSITE(GET_BATTLER_SIDE(gBattlerAttacker)));
        break;
    case MOVE_TARGET_USER_OR_SELECTED:
    case MOVE_TARGET_USER:
        targetBattler = gBattlerAttacker;
        break;
    }

    *(gBattleStruct->moveTarget + gBattlerAttacker) = targetBattler;

    return targetBattler;
}

static bool32 IsBattlerModernFatefulEncounter(u8 battlerId)
{
    if (GetBattlerSide(battlerId) == B_SIDE_OPPONENT)
        return TRUE;
    if (GetMonData(&gPlayerParty[gBattlerPartyIndexes[battlerId]], MON_DATA_SPECIES, NULL) != SPECIES_DEOXYS
        && GetMonData(&gPlayerParty[gBattlerPartyIndexes[battlerId]], MON_DATA_SPECIES, NULL) != SPECIES_MEW)
            return TRUE;
    return GetMonData(&gPlayerParty[gBattlerPartyIndexes[battlerId]], MON_DATA_MODERN_FATEFUL_ENCOUNTER, NULL);
}

// base damage formula before adding any modifiers
static inline s32 CalculateBaseDamageBeforeModifiers(u32 power, u32 userFinalAttack, u32 level, u32 targetFinalDefense)
{
    return power * userFinalAttack * (2 * level / 5 + 2) / targetFinalDefense / 50 + 2;
}

u8 IsMonDisobedient(void)
{
    s32 rnd;
    s32 calc;
    u8 obedienceLevel = 0;

    if (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED_LINK))
        return 0;
    if (GetBattlerSide(gBattlerAttacker) == B_SIDE_OPPONENT)
        return 0;

    if (IsBattlerModernFatefulEncounter(gBattlerAttacker)) // only false if illegal Mew or Deoxys
    {
        if (gBattleTypeFlags & BATTLE_TYPE_INGAME_PARTNER && GetBattlerPosition(gBattlerAttacker) == 2)
            return 0;
        if (gBattleTypeFlags & BATTLE_TYPE_FRONTIER)
            return 0;
        if (gBattleTypeFlags & BATTLE_TYPE_RECORDED)
            return 0;
        if (!IsOtherTrainer(gBattleMons[gBattlerAttacker].otId, gBattleMons[gBattlerAttacker].otName))
            return 0;
        if (FlagGet(FLAG_BADGE08_GET))
            return 0;

        obedienceLevel = 10;

        if (FlagGet(FLAG_BADGE02_GET))
            obedienceLevel = 30;
        if (FlagGet(FLAG_BADGE04_GET))
            obedienceLevel = 50;
        if (FlagGet(FLAG_BADGE06_GET))
            obedienceLevel = 70;
    }

    if (gBattleMons[gBattlerAttacker].level <= obedienceLevel)
        return 0;
    rnd = (Random() & 255);
    calc = (gBattleMons[gBattlerAttacker].level + obedienceLevel) * rnd >> 8;
    if (calc < obedienceLevel)
        return 0;

    // is not obedient
    if (gCurrentMove == MOVE_RAGE)
        gBattleMons[gBattlerAttacker].status2 &= ~STATUS2_RAGE;
    if (gBattleMons[gBattlerAttacker].status1 & STATUS1_SLEEP && (gCurrentMove == MOVE_SNORE || gCurrentMove == MOVE_SLEEP_TALK))
    {
        gBattlescriptCurrInstr = BattleScript_IgnoresWhileAsleep;
        return 1;
    }

    rnd = (Random() & 255);
    calc = (gBattleMons[gBattlerAttacker].level + obedienceLevel) * rnd >> 8;
    if (calc < obedienceLevel)
    {
        calc = CheckMoveLimitations(gBattlerAttacker, gBitTable[gCurrMovePos], MOVE_LIMITATIONS_ALL);
        if (calc == ALL_MOVES_MASK) // all moves cannot be used
        {
            // Randomly select, then print a disobedient string
            // B_MSG_LOAFING, B_MSG_WONT_OBEY, B_MSG_TURNED_AWAY, or B_MSG_PRETEND_NOT_NOTICE
            gBattleCommunication[MULTISTRING_CHOOSER] = MOD(Random(), NUM_LOAF_STRINGS);
            gBattlescriptCurrInstr = BattleScript_MoveUsedLoafingAround;
            return 1;
        }
        else // use a random move
        {
            do
            {
                gCurrMovePos = gChosenMovePos = MOD(Random(), MAX_MON_MOVES);
            } while (gBitTable[gCurrMovePos] & calc);

            gCalledMove = gBattleMons[gBattlerAttacker].moves[gCurrMovePos];
            gBattlescriptCurrInstr = BattleScript_IgnoresAndUsesRandomMove;
            gBattlerTarget = GetMoveTarget(gCalledMove, NO_TARGET_OVERRIDE);
            gHitMarker |= HITMARKER_DISOBEDIENT_MOVE;
            return 2;
        }
    }
    else
    {
        obedienceLevel = gBattleMons[gBattlerAttacker].level - obedienceLevel;

        calc = (Random() & 255);
        if (calc < obedienceLevel && !(gBattleMons[gBattlerAttacker].status1 & STATUS1_ANY) && gBattleMons[gBattlerAttacker].ability != ABILITY_VITAL_SPIRIT && gBattleMons[gBattlerAttacker].ability != ABILITY_INSOMNIA)
        {
            // try putting asleep
            int i;
            for (i = 0; i < gBattlersCount; i++)
            {
                if (gBattleMons[i].status2 & STATUS2_UPROAR)
                    break;
            }
            if (i == gBattlersCount)
            {
                gBattlescriptCurrInstr = BattleScript_IgnoresAndFallsAsleep;
                return 1;
            }
        }
        calc -= obedienceLevel;
        if (calc < obedienceLevel)
        {
            gBattleMoveDamage = CalculateBaseDamage(&gBattleMons[gBattlerAttacker], &gBattleMons[gBattlerAttacker], MOVE_POUND, 0, 40, 0, gBattlerAttacker, gBattlerAttacker);
            gBattlerTarget = gBattlerAttacker;
            gBattlescriptCurrInstr = BattleScript_IgnoresAndHitsItself;
            gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
            return 2;
        }
        else
        {
            // Randomly select, then print a disobedient string
            // B_MSG_LOAFING, B_MSG_WONT_OBEY, B_MSG_TURNED_AWAY, or B_MSG_PRETEND_NOT_NOTICE
            gBattleCommunication[MULTISTRING_CHOOSER] = MOD(Random(), NUM_LOAF_STRINGS);
            gBattlescriptCurrInstr = BattleScript_MoveUsedLoafingAround;
            return 1;
        }
    }
}

#define DAMAGE_APPLY_MODIFIER(modifier) do {               \
    dmg = uq4_12_multiply_by_int_half_down(modifier, dmg); \
} while (0)

#define V_MULTIPLE_TARGETS_DMG UQ_4_12(0.5)

#define V_CRIT_MULTIPLIER UQ_4_12(2.0)

bool32 IsBattlerAlive(u32 battler)
{
    if (gBattleMons[battler].hp == 0)
        return FALSE;
    else if (battler >= gBattlersCount)
        return FALSE;
    else if (gAbsentBattlerFlags & gBitTable[battler])
        return FALSE;
    else
        return TRUE;
}

// Gets move target before redirection effects etc. are applied
// Possible return values are defined in battle.h following MOVE_TARGET_SELECTED
u32 GetBattlerMoveTargetType(u32 battler, u32 move)
{
    u32 target;

    return gBattleMoves[move].target;
}

u32 GetMoveTargetCount(u32 move, u32 battlerAtk, u32 battlerDef)
{
    switch (GetBattlerMoveTargetType(gBattlerAttacker, move))
    {
    case MOVE_TARGET_BOTH:
        return IsBattlerAlive(battlerDef)
             + IsBattlerAlive(BATTLE_PARTNER(battlerDef));
    case MOVE_TARGET_FOES_AND_ALLY:
        return IsBattlerAlive(battlerDef)
             + IsBattlerAlive(BATTLE_PARTNER(battlerDef))
             + IsBattlerAlive(BATTLE_PARTNER(battlerAtk));
    case MOVE_TARGET_OPPONENTS_FIELD:
        return 1;
    case MOVE_TARGET_DEPENDS:
    case MOVE_TARGET_SELECTED:
    case MOVE_TARGET_RANDOM:
    case MOVE_TARGET_USER_OR_SELECTED:
        return IsBattlerAlive(battlerDef);
    case MOVE_TARGET_USER:
        return IsBattlerAlive(battlerAtk);
    default:
        return 0;
    }
}

static inline uq4_12_t GetTargetDamageModifier(u32 move, u32 battlerAtk, u32 battlerDef)
{
    if (GetMoveTargetCount(move, battlerAtk, battlerDef) >= 2)
        return V_MULTIPLE_TARGETS_DMG;
    return UQ_4_12(1.0);
}

static uq4_12_t GetWeatherDamageModifier(u32 battlerAtk, u32 move, u32 moveType, u32 holdEffectAtk, u32 holdEffectDef, u32 weather)
{
    if (weather & B_WEATHER_RAIN)
    {
        if (moveType != TYPE_FIRE && moveType != TYPE_WATER)
            return UQ_4_12(1.0);
        return (moveType == TYPE_FIRE) ? UQ_4_12(0.5) : UQ_4_12(1.5);
    }
    if (weather & B_WEATHER_SUN)
    {
        if (moveType != TYPE_FIRE && moveType != TYPE_WATER)
            return UQ_4_12(1.0);
        return (moveType == TYPE_WATER) ? UQ_4_12(0.5) : UQ_4_12(1.5);
    }
    return UQ_4_12(1.0);
}

static inline uq4_12_t GetCriticalModifier(bool32 isCrit)
{
    return isCrit ? V_CRIT_MULTIPLIER : UQ_4_12(1.0);
}

u32 GetBattlerHoldEffectParam(u32 battler)
{
    return ItemId_GetHoldEffectParam(gBattleMons[battler].item);
}

u32 CalcRolloutBasePower(u32 battlerAtk, u32 basePower, u32 rolloutTimer)
{
    u32 i;
    for (i = 1; i < (5 - rolloutTimer); i++)
        basePower *= 2;
    if (gBattleMons[battlerAtk].status2 & STATUS2_DEFENSE_CURL)
        basePower *= 2;
    return basePower;
}

u32 CalcFuryCutterBasePower(u32 basePower, u32 furyCutterCounter)
{
    u32 i;
    for (i = 1; i < furyCutterCounter; i++)
        basePower *= 2;
    return basePower;
}

static const u8 sFlailHpScaleToPowerTable[] =
{
    1, 200,
    4, 150,
    9, 100,
    16, 80,
    32, 40,
    48, 20
};

u32 GetBattlerWeight(u32 battler)
{
    u32 i;
    u32 weight = GetPokedexHeightWeight(SpeciesToNationalPokedexNum(gBattleMons[battler].species), 1);

    if (weight == 0)
        weight = 1;

    return weight;
}

// format: min. weight (hectograms), base power
static const u16 sWeightToDamageTable[] =
{
    100, 20,
    250, 40,
    500, 60,
    1000, 80,
    2000, 100,
    0xFFFF, 0xFFFF
};

static inline u32 CalcMoveBasePower(u32 move, u32 battlerAtk, u32 battlerDef, u32 abilityDef, u32 weather)
{
    u32 i;
    u32 basePower = gBattleMoves[move].power;
    u32 weight, hpFraction, speed;

    switch (gBattleMoves[move].effect)
    {
    case EFFECT_ERUPTION:
        basePower = gBattleMons[battlerAtk].hp * basePower / gBattleMons[battlerAtk].maxHP;
        break;
    case EFFECT_FLAIL:
        hpFraction = GetScaledHPFraction(gBattleMons[battlerAtk].hp, gBattleMons[battlerAtk].maxHP, 48);
        for (i = 0; i < sizeof(sFlailHpScaleToPowerTable); i += 2)
        {
            if (hpFraction <= sFlailHpScaleToPowerTable[i])
                break;
        }
        basePower = sFlailHpScaleToPowerTable[i + 1];
        break;
    case EFFECT_RETURN:
        basePower = 10 * (gBattleMons[battlerAtk].friendship) / 25;
        break;
    case EFFECT_FRUSTRATION:
        basePower = 10 * (MAX_FRIENDSHIP - gBattleMons[battlerAtk].friendship) / 25;
        break;
    case EFFECT_FURY_CUTTER:
        basePower = CalcFuryCutterBasePower(basePower, gDisableStructs[battlerAtk].furyCutterCounter);
        break;
    case EFFECT_ROLLOUT:
        basePower = CalcRolloutBasePower(battlerAtk, basePower, gDisableStructs[battlerAtk].rolloutTimer);
        break;
    case EFFECT_MAGNITUDE:
        basePower = gBattleStruct->magnitudeBasePower;
        break;
    case EFFECT_PRESENT:
        basePower = gBattleStruct->presentBasePower;
        break;
    case EFFECT_TRIPLE_KICK:
        if (gMultiHitCounter == 0) // Calc damage with max BP for move consideration
            basePower *= 6;
        else
            basePower *= (4 - gMultiHitCounter);
        break;
    case EFFECT_SPIT_UP:
        basePower = 100 * gDisableStructs[battlerAtk].stockpileCounter;
        break;
    case EFFECT_REVENGE:
        if ((gProtectStructs[battlerAtk].physicalDmg
                && gProtectStructs[battlerAtk].physicalBattlerId == battlerDef)
            || (gProtectStructs[battlerAtk].specialDmg
                && gProtectStructs[battlerAtk].specialBattlerId == battlerDef))
            basePower *= 2;
        break;
    case EFFECT_WEATHER_BALL:
        if (weather & B_WEATHER_ANY)
            basePower *= 2;
        break;
    case EFFECT_PURSUIT:
        if (gActionsByTurnOrder[GetBattlerTurnOrderNum(battlerDef)] == B_ACTION_SWITCH)
            basePower *= 2;
        break;
    case EFFECT_SMELLINGSALT:
        if (gBattleMons[battlerDef].status1 & STATUS1_PARALYSIS)
            basePower *= 2;
        break;
    case EFFECT_LOW_KICK:
        weight = GetBattlerWeight(battlerDef);
        for (i = 0; sWeightToDamageTable[i] != 0xFFFF; i += 2)
        {
            if (sWeightToDamageTable[i] > weight)
                break;
        }
        if (sWeightToDamageTable[i] != 0xFFFF)
            basePower = sWeightToDamageTable[i + 1];
        else
            basePower = 120;
        break;
    case EFFECT_HIDDEN_POWER:
    {
        u8 powerBits;

        powerBits = ((gBattleMons[battlerAtk].hpIV & 2) >> 1)
                | ((gBattleMons[battlerAtk].attackIV & 2) << 0)
                | ((gBattleMons[battlerAtk].defenseIV & 2) << 1)
                | ((gBattleMons[battlerAtk].speedIV & 2) << 2)
                | ((gBattleMons[battlerAtk].spAttackIV & 2) << 3)
                | ((gBattleMons[battlerAtk].spDefenseIV & 2) << 4);

        basePower = (40 * powerBits) / 63 + 30;
        break;
    }
    case EFFECT_BEAT_UP:
        // basePower = CalcBeatUpPowerGens3And4();
        break;
    }

    if (basePower == 0)
        basePower = 1;
    return basePower;
}

bool32 IsBattlerWeatherAffected(u32 battler, u32 weatherFlags)
{
    if (gBattleWeather & weatherFlags && WEATHER_HAS_EFFECT)
    {
        return TRUE;
    }
    return FALSE;
}

// percent in UQ_4_12 format
static const uq4_12_t sPercentToModifier[] =
{
    UQ_4_12(0.00), // 0
    UQ_4_12(0.01), // 1
    UQ_4_12(0.02), // 2
    UQ_4_12(0.03), // 3
    UQ_4_12(0.04), // 4
    UQ_4_12(0.05), // 5
    UQ_4_12(0.06), // 6
    UQ_4_12(0.07), // 7
    UQ_4_12(0.08), // 8
    UQ_4_12(0.09), // 9
    UQ_4_12(0.10), // 10
    UQ_4_12(0.11), // 11
    UQ_4_12(0.12), // 12
    UQ_4_12(0.13), // 13
    UQ_4_12(0.14), // 14
    UQ_4_12(0.15), // 15
    UQ_4_12(0.16), // 16
    UQ_4_12(0.17), // 17
    UQ_4_12(0.18), // 18
    UQ_4_12(0.19), // 19
    UQ_4_12(0.20), // 20
    UQ_4_12(0.21), // 21
    UQ_4_12(0.22), // 22
    UQ_4_12(0.23), // 23
    UQ_4_12(0.24), // 24
    UQ_4_12(0.25), // 25
    UQ_4_12(0.26), // 26
    UQ_4_12(0.27), // 27
    UQ_4_12(0.28), // 28
    UQ_4_12(0.29), // 29
    UQ_4_12(0.30), // 30
    UQ_4_12(0.31), // 31
    UQ_4_12(0.32), // 32
    UQ_4_12(0.33), // 33
    UQ_4_12(0.34), // 34
    UQ_4_12(0.35), // 35
    UQ_4_12(0.36), // 36
    UQ_4_12(0.37), // 37
    UQ_4_12(0.38), // 38
    UQ_4_12(0.39), // 39
    UQ_4_12(0.40), // 40
    UQ_4_12(0.41), // 41
    UQ_4_12(0.42), // 42
    UQ_4_12(0.43), // 43
    UQ_4_12(0.44), // 44
    UQ_4_12(0.45), // 45
    UQ_4_12(0.46), // 46
    UQ_4_12(0.47), // 47
    UQ_4_12(0.48), // 48
    UQ_4_12(0.49), // 49
    UQ_4_12(0.50), // 50
    UQ_4_12(0.51), // 51
    UQ_4_12(0.52), // 52
    UQ_4_12(0.53), // 53
    UQ_4_12(0.54), // 54
    UQ_4_12(0.55), // 55
    UQ_4_12(0.56), // 56
    UQ_4_12(0.57), // 57
    UQ_4_12(0.58), // 58
    UQ_4_12(0.59), // 59
    UQ_4_12(0.60), // 60
    UQ_4_12(0.61), // 61
    UQ_4_12(0.62), // 62
    UQ_4_12(0.63), // 63
    UQ_4_12(0.64), // 64
    UQ_4_12(0.65), // 65
    UQ_4_12(0.66), // 66
    UQ_4_12(0.67), // 67
    UQ_4_12(0.68), // 68
    UQ_4_12(0.69), // 69
    UQ_4_12(0.70), // 70
    UQ_4_12(0.71), // 71
    UQ_4_12(0.72), // 72
    UQ_4_12(0.73), // 73
    UQ_4_12(0.74), // 74
    UQ_4_12(0.75), // 75
    UQ_4_12(0.76), // 76
    UQ_4_12(0.77), // 77
    UQ_4_12(0.78), // 78
    UQ_4_12(0.79), // 79
    UQ_4_12(0.80), // 80
    UQ_4_12(0.81), // 81
    UQ_4_12(0.82), // 82
    UQ_4_12(0.83), // 83
    UQ_4_12(0.84), // 84
    UQ_4_12(0.85), // 85
    UQ_4_12(0.86), // 86
    UQ_4_12(0.87), // 87
    UQ_4_12(0.88), // 88
    UQ_4_12(0.89), // 89
    UQ_4_12(0.90), // 90
    UQ_4_12(0.91), // 91
    UQ_4_12(0.92), // 92
    UQ_4_12(0.93), // 93
    UQ_4_12(0.94), // 94
    UQ_4_12(0.95), // 95
    UQ_4_12(0.96), // 96
    UQ_4_12(0.97), // 97
    UQ_4_12(0.98), // 98
    UQ_4_12(0.99), // 99
    UQ_4_12(1.00), // 100
};

static inline u32 CalcMoveBasePowerAfterModifiers(u32 move, u32 battlerAtk, u32 battlerDef, u32 moveType, bool32 updateFlags, u32 atkAbility, u32 defAbility, u32 holdEffectAtk, u32 weather)
{
    u32 i;
    u32 holdEffectParamAtk;
    u32 basePower = CalcMoveBasePower(move, battlerAtk, battlerDef, defAbility, weather);
    uq4_12_t holdEffectModifier;
    uq4_12_t modifier = UQ_4_12(1.0);
    u32 atkSide = GetBattlerSide(battlerAtk);

    // move effect
    switch (gBattleMoves[move].effect)
    {
    case EFFECT_FACADE:
        if (gBattleMons[battlerAtk].status1 & (STATUS1_BURN | STATUS1_PSN_ANY | STATUS1_PARALYSIS))
            modifier = uq4_12_multiply(modifier, UQ_4_12(2.0));
        break;
    case EFFECT_SOLAR_BEAM:
        if (IsBattlerWeatherAffected(battlerAtk, (B_WEATHER_HAIL | B_WEATHER_SANDSTORM | B_WEATHER_RAIN)))
            modifier = uq4_12_multiply(modifier, UQ_4_12(0.5));
        break;
    }

    // various effects
    if (gProtectStructs[battlerAtk].helpingHand)
        modifier = uq4_12_multiply(modifier, UQ_4_12(1.5));
    if (gStatuses3[battlerAtk] & STATUS3_CHARGED_UP && moveType == TYPE_ELECTRIC)
        modifier = uq4_12_multiply(modifier, UQ_4_12(2.0));
    if ((moveType == TYPE_ELECTRIC && AbilityBattleEffects(ABILITYEFFECT_FIELD_SPORT, 0, 0, ABILITYEFFECT_MUD_SPORT, 0))
        || (moveType == TYPE_FIRE && AbilityBattleEffects(ABILITYEFFECT_FIELD_SPORT, 0, 0, ABILITYEFFECT_WATER_SPORT, 0)))
        modifier = uq4_12_multiply(modifier, UQ_4_12(0.5));

    holdEffectParamAtk = GetBattlerHoldEffectParam(battlerAtk);
    if (holdEffectParamAtk > 100)
        holdEffectParamAtk = 100;

    holdEffectModifier = UQ_4_12(1.0) + sPercentToModifier[holdEffectParamAtk];

    // attacker's hold effect
    switch (holdEffectAtk)
    {
    case HOLD_EFFECT_SOUL_DEW:
        if ((gBattleMons[battlerAtk].species == SPECIES_LATIAS || gBattleMons[battlerAtk].species == SPECIES_LATIOS) && !(gBattleTypeFlags & BATTLE_TYPE_FRONTIER) && IS_TYPE_SPECIAL(moveType))
            modifier = uq4_12_multiply(modifier, holdEffectModifier);
        break;
    }
    return uq4_12_multiply_by_int_half_down(modifier, basePower);
}

u32 GetBattlerAbility(u32 battler)
{
    return gBattleMons[battler].ability;
}

bool32 ShouldGetStatBadgeBoost(u16 badgeFlag, u32 battler)
{
    if (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_EREADER_TRAINER | BATTLE_TYPE_RECORDED_LINK | BATTLE_TYPE_FRONTIER))
        return FALSE;
    else if (GetBattlerSide(battler) != B_SIDE_PLAYER)
        return FALSE;
    else if (gBattleTypeFlags & BATTLE_TYPE_TRAINER && gTrainerBattleOpponent_A == TRAINER_SECRET_BASE)
        return FALSE;
    else if (FlagGet(badgeFlag))
        return TRUE;
    return FALSE;
}

static inline u32 CalcAttackStat(u32 move, u32 battlerAtk, u32 battlerDef, u32 moveType, bool32 isCrit, bool32 updateFlags, u32 atkAbility, u32 defAbility, u32 holdEffectAtk)
{
    u8 atkStage;
    u32 atkStat;
    uq4_12_t modifier;
    u16 atkBaseSpeciesId;
    u8 type;

    GET_MOVE_TYPE(move, type);

    atkBaseSpeciesId = gBattleMons[battlerAtk].species;

    if (IS_TYPE_PHYSICAL(type))
    {
        atkStat = gBattleMons[battlerAtk].attack;
        atkStage = gBattleMons[battlerAtk].statStages[STAT_ATK];
    }
    else
    {
        atkStat = gBattleMons[battlerAtk].spAttack;
        atkStage = gBattleMons[battlerAtk].statStages[STAT_SPATK];
    }

    // critical hits ignore attack stat's stage drops
    if (isCrit && atkStage < DEFAULT_STAT_STAGE)
        atkStage = DEFAULT_STAT_STAGE;

    atkStat *= gStatStageRatios[atkStage][0];
    atkStat /= gStatStageRatios[atkStage][1];

    // apply attack stat modifiers
    modifier = UQ_4_12(1.0);

    // attacker's abilities
    switch (atkAbility)
    {
    case ABILITY_HUGE_POWER:
    case ABILITY_PURE_POWER:
        if (IS_TYPE_PHYSICAL(type))
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(2.0));
        break;
    case ABILITY_FLASH_FIRE:
        if (moveType == TYPE_FIRE && gBattleResources->flags->flags[battlerAtk] & RESOURCE_FLAG_FLASH_FIRE)
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(1.5));
        break;
    case ABILITY_SWARM:
        if (moveType == TYPE_BUG && gBattleMons[battlerAtk].hp <= (gBattleMons[battlerAtk].maxHP / 3))
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(1.5));
        break;
    case ABILITY_TORRENT:
        if (moveType == TYPE_WATER && gBattleMons[battlerAtk].hp <= (gBattleMons[battlerAtk].maxHP / 3))
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(1.5));
        break;
    case ABILITY_BLAZE:
        if (moveType == TYPE_FIRE && gBattleMons[battlerAtk].hp <= (gBattleMons[battlerAtk].maxHP / 3))
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(1.5));
        break;
    case ABILITY_OVERGROW:
        if (moveType == TYPE_GRASS && gBattleMons[battlerAtk].hp <= (gBattleMons[battlerAtk].maxHP / 3))
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(1.5));
        break;
    case ABILITY_PLUS:
        if (IS_TYPE_SPECIAL(type) && IsBattlerAlive(BATTLE_PARTNER(battlerAtk)) && GetBattlerAbility(BATTLE_PARTNER(battlerAtk)) == ABILITY_MINUS)
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(1.5));
        break;
    case ABILITY_MINUS:
        if (IS_TYPE_SPECIAL(type) && IsBattlerAlive(BATTLE_PARTNER(battlerAtk)) && GetBattlerAbility(BATTLE_PARTNER(battlerAtk)) == ABILITY_PLUS)
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(1.5));
        break;
    case ABILITY_HUSTLE:
        if (IS_TYPE_PHYSICAL(type))
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(1.5));
        break;
    case ABILITY_GUTS:
        if (gBattleMons[battlerAtk].status1 & STATUS1_ANY && IS_TYPE_PHYSICAL(type))
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(1.5));
        break;
    }

    // target's abilities
    switch (defAbility)
    {
    case ABILITY_THICK_FAT:
        if (moveType == TYPE_FIRE || moveType == TYPE_ICE)
        {
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(0.5));
            if (updateFlags)
                RecordAbilityBattle(battlerDef, ABILITY_THICK_FAT);
        }
        break;
    }

    // attacker's hold effect
    switch (holdEffectAtk)
    {
    case HOLD_EFFECT_THICK_CLUB:
        if ((atkBaseSpeciesId == SPECIES_CUBONE || atkBaseSpeciesId == SPECIES_MAROWAK) && IS_TYPE_PHYSICAL(type))
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(2.0));
        break;
    case HOLD_EFFECT_DEEP_SEA_TOOTH:
        if (gBattleMons[battlerAtk].species == SPECIES_CLAMPERL && IS_TYPE_SPECIAL(type))
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(2.0));
        break;
    case HOLD_EFFECT_LIGHT_BALL:
        if (atkBaseSpeciesId == SPECIES_PIKACHU && IS_TYPE_SPECIAL(type))
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(2.0));
        break;
    case HOLD_EFFECT_CHOICE_BAND:
        if (IS_TYPE_PHYSICAL(type))
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(1.5));
        break;
    }

    // The offensive stats of a Player's Pokémon are boosted by x1.1 (+10%) if they have the 1st badge and 7th badges.
    // Having the 1st badge boosts physical attack while having the 7th badge boosts special attack.
    if (ShouldGetStatBadgeBoost(FLAG_BADGE01_GET, battlerAtk) && IS_TYPE_PHYSICAL(type))
        modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(1.1));
    if (ShouldGetStatBadgeBoost(FLAG_BADGE07_GET, battlerAtk) && IS_TYPE_SPECIAL(type))
        modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(1.1));

    return uq4_12_multiply_by_int_half_down(modifier, atkStat);
}

static inline u32 CalcDefenseStat(u32 move, u32 battlerAtk, u32 battlerDef, u32 moveType, bool32 isCrit, bool32 updateFlags, u32 atkAbility, u32 defAbility, u32 holdEffectDef, u32 weather)
{
    bool32 usesDefStat;
    u8 defStage;
    u32 defStat, def, spDef;
    uq4_12_t modifier;

    def = gBattleMons[battlerDef].defense;
    spDef = gBattleMons[battlerDef].spDefense;

    defStat = spDef;
    defStage = gBattleMons[battlerDef].statStages[STAT_SPDEF];
    usesDefStat = FALSE;

    if (gBattleMoves[gCurrentMove].effect == EFFECT_EXPLOSION)
        defStat /= 2;

    // critical hits ignore positive stat changes
    if (isCrit && defStage > DEFAULT_STAT_STAGE)
        defStage = DEFAULT_STAT_STAGE;

    defStat *= gStatStageRatios[defStage][0];
    defStat /= gStatStageRatios[defStage][1];

    // apply defense stat modifiers
    modifier = UQ_4_12(1.0);

    // target's abilities
    switch (defAbility)
    {
    case ABILITY_MARVEL_SCALE:
        if (gBattleMons[battlerDef].status1 & STATUS1_ANY && usesDefStat)
        {
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(1.5));
            if (updateFlags)
                RecordAbilityBattle(battlerDef, ABILITY_MARVEL_SCALE);
        }
        break;
    }

    // target's hold effects
    switch (holdEffectDef)
    {
    case HOLD_EFFECT_DEEP_SEA_SCALE:
        if (gBattleMons[battlerDef].species == SPECIES_CLAMPERL && !usesDefStat)
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(2.0));
        break;
    case HOLD_EFFECT_METAL_POWDER:
        if (gBattleMons[battlerDef].species == SPECIES_DITTO && usesDefStat && !(gBattleMons[battlerDef].status2 & STATUS2_TRANSFORMED))
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(2.0));
        break;
    case HOLD_EFFECT_SOUL_DEW:
        if ((gBattleMons[battlerDef].species == SPECIES_LATIAS || gBattleMons[battlerDef].species == SPECIES_LATIOS)
         && !(gBattleTypeFlags & BATTLE_TYPE_FRONTIER)
         && !usesDefStat)
            modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(1.5));
        break;
    }

    // The defensive stats of a Player's Pokémon are boosted by x1.1 (+10%) if they have the 5th badge and 7th badges.
    // Having the 5th badge boosts physical defense while having the 7th badge boosts special defense.
    if (ShouldGetStatBadgeBoost(FLAG_BADGE05_GET, battlerDef) && IS_TYPE_PHYSICAL(moveType))
        modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(1.1));
    if (ShouldGetStatBadgeBoost(FLAG_BADGE07_GET, battlerDef) && IS_TYPE_SPECIAL(moveType))
        modifier = uq4_12_multiply_half_down(modifier, UQ_4_12(1.1));

    return uq4_12_multiply_by_int_half_down(modifier, defStat);
}

static inline uq4_12_t GetSameTypeAttackBonusModifier(u32 battlerAtk, u32 moveType, u32 move, u32 abilityAtk)
{
    if (!IS_BATTLER_OF_TYPE(battlerAtk, moveType) || move == MOVE_STRUGGLE || move == MOVE_NONE)
        return UQ_4_12(1.0);
    return UQ_4_12(1.5);
}

static inline uq4_12_t GetBurnModifier(u32 battlerAtk, u32 move, u32 abilityAtk)
{
    u8 type;

    GET_MOVE_TYPE(move, type);
    
    if (gBattleMons[battlerAtk].status1 & STATUS1_BURN
        && IS_TYPE_PHYSICAL(type)
        && abilityAtk != ABILITY_GUTS)
        return UQ_4_12(0.5);
    return UQ_4_12(1.0);
}

static inline uq4_12_t GetMinimizeModifier(u32 move, u32 battlerDef)
{
    if (gBattleMoves[move].effect == EFFECT_FLINCH_MINIMIZE_HIT && gStatuses3[battlerDef] & STATUS3_MINIMIZED)
        return UQ_4_12(2.0);
    return UQ_4_12(1.0);
}

static inline uq4_12_t GetUndergroundModifier(u32 move, u32 battlerDef)
{
    if ((gBattleMoves[move].effect == EFFECT_EARTHQUAKE || gBattleMoves[move].effect == EFFECT_MAGNITUDE) && gStatuses3[battlerDef] & STATUS3_UNDERGROUND)
        return UQ_4_12(2.0);
    return UQ_4_12(1.0);
}

static inline uq4_12_t GetDiveModifier(u32 move, u32 battlerDef)
{
    if ((move == MOVE_SURF || move == MOVE_WHIRLPOOL) && gStatuses3[battlerDef] & STATUS3_UNDERWATER)
        return UQ_4_12(2.0);
    return UQ_4_12(1.0);
}

static inline uq4_12_t GetAirborneModifier(u32 move, u32 battlerDef)
{
    if ((gBattleMoves[move].effect == EFFECT_GUST || gBattleMoves[move].effect == EFFECT_TWISTER) && gStatuses3[battlerDef] & STATUS3_ON_AIR)
        return UQ_4_12(2.0);
    return UQ_4_12(1.0);
}

#define DAMAGE_MULTIPLY_MODIFIER(modifier) do {                     \
    finalModifier = uq4_12_multiply_half_down(modifier, finalModifier); \
} while (0)

static inline uq4_12_t GetOtherModifiers(u32 move, u32 moveType, u32 battlerAtk, u32 battlerDef, bool32 isCrit, uq4_12_t typeEffectivenessModifier, bool32 updateFlags,
                                         u32 abilityAtk, u32 abilityDef, u32 holdEffectAtk, u32 holdEffectDef)
{
    uq4_12_t finalModifier = UQ_4_12(1.0);
    u32 battlerDefPartner = BATTLE_PARTNER(battlerDef);
    u32 unmodifiedAttackerSpeed = gBattleMons[battlerAtk].speed;
    u32 unmodifiedDefenderSpeed = gBattleMons[battlerDef].speed;

    DAMAGE_MULTIPLY_MODIFIER(GetMinimizeModifier(move, battlerDef));
    DAMAGE_MULTIPLY_MODIFIER(GetUndergroundModifier(move, battlerDef));
    DAMAGE_MULTIPLY_MODIFIER(GetDiveModifier(move, battlerDef));
    DAMAGE_MULTIPLY_MODIFIER(GetAirborneModifier(move, battlerDef));

    return finalModifier;
}

static inline s32 DoMoveDamageCalcVars(u32 move, u32 battlerAtk, u32 battlerDef, u32 moveType, s32 fixedBasePower,
                            bool32 isCrit, bool32 lowRoll, bool32 updateFlags, uq4_12_t typeEffectivenessModifier, u32 weather,
                            u32 holdEffectAtk, u32 holdEffectDef, u32 abilityAtk, u32 abilityDef)
{
    s32 dmg;
    u32 userFinalAttack;
    u32 targetFinalDefense;

    if (fixedBasePower)
        gBattleMovePower = fixedBasePower;
    else
        gBattleMovePower = CalcMoveBasePowerAfterModifiers(move, battlerAtk, battlerDef, moveType, updateFlags, abilityAtk, abilityDef, holdEffectAtk, weather);

    userFinalAttack = CalcAttackStat(move, battlerAtk, battlerDef, moveType, isCrit, updateFlags, abilityAtk, abilityDef, holdEffectAtk);
    targetFinalDefense = CalcDefenseStat(move, battlerAtk, battlerDef, moveType, isCrit, updateFlags, abilityAtk, abilityDef, holdEffectDef, weather);

    dmg = CalculateBaseDamageBeforeModifiers(gBattleMovePower, userFinalAttack, gBattleMons[battlerAtk].level, targetFinalDefense);
    DAMAGE_APPLY_MODIFIER(GetTargetDamageModifier(move, battlerAtk, battlerDef));
    DAMAGE_APPLY_MODIFIER(GetWeatherDamageModifier(battlerAtk, move, moveType, holdEffectAtk, holdEffectDef, weather));
    DAMAGE_APPLY_MODIFIER(GetCriticalModifier(isCrit));
    // TODO: Glaive Rush (Gen IX effect)
    if (lowRoll)
    {
        dmg *= 85;
        dmg /= 100;
    }

    DAMAGE_APPLY_MODIFIER(GetSameTypeAttackBonusModifier(battlerAtk, moveType, move, abilityAtk));
    DAMAGE_APPLY_MODIFIER(typeEffectivenessModifier);
    DAMAGE_APPLY_MODIFIER(GetBurnModifier(battlerAtk, move, abilityAtk));
    DAMAGE_APPLY_MODIFIER(GetOtherModifiers(move, moveType, battlerAtk, battlerDef, isCrit, typeEffectivenessModifier, updateFlags, abilityAtk, abilityDef, holdEffectAtk, holdEffectDef));

    if (dmg == 0)
        dmg = 1;
    return dmg;
}

u32 GetBattlerHoldEffect(u32 battler)
{
    return ItemId_GetHoldEffect(gBattleMons[battler].item);
}

static inline s32 DoMoveDamageCalc(u32 move, u32 battlerAtk, u32 battlerDef, u32 moveType, s32 fixedBasePower,
                            bool32 isCrit, bool32 lowRoll, bool32 updateFlags, uq4_12_t typeEffectivenessModifier, u32 weather)
{
    u32 holdEffectAtk, holdEffectDef, abilityAtk, abilityDef;

    if (typeEffectivenessModifier == UQ_4_12(0.0))
        return 0;

    holdEffectAtk = GetBattlerHoldEffect(battlerAtk);
    holdEffectDef = GetBattlerHoldEffect(battlerDef);
    abilityAtk = GetBattlerAbility(battlerAtk);
    abilityDef = GetBattlerAbility(battlerDef);

    return DoMoveDamageCalcVars(move, battlerAtk, battlerDef, moveType, fixedBasePower, isCrit, lowRoll,
                            updateFlags, typeEffectivenessModifier, weather, holdEffectAtk, holdEffectDef, abilityAtk, abilityDef);
}

#define X UQ_4_12

static const uq4_12_t sTypeEffectivenessTable[NUMBER_OF_MON_TYPES][NUMBER_OF_MON_TYPES] =
{
//   normal  fight   flying  poison  ground  rock    bug     ghost   steel   mystery fire    water   grass  electric psychic ice     dragon  dark
    {X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(0.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0)}, // normal
    {X(2.0), X(1.0), X(0.5), X(0.5), X(1.0), X(2.0), X(0.5), X(0.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(2.0), X(1.0), X(2.0)}, // fight
    {X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(0.5), X(2.0), X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(2.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0)}, // flying
    {X(1.0), X(1.0), X(1.0), X(0.5), X(0.5), X(0.5), X(1.0), X(0.5), X(0.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0)}, // poison
    {X(1.0), X(1.0), X(0.0), X(2.0), X(1.0), X(2.0), X(0.5), X(1.0), X(2.0), X(1.0), X(2.0), X(1.0), X(0.5), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0)}, // ground
    {X(1.0), X(0.5), X(2.0), X(1.0), X(0.5), X(1.0), X(2.0), X(1.0), X(0.5), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0)}, // rock
    {X(1.0), X(0.5), X(0.5), X(0.5), X(1.0), X(1.0), X(1.0), X(0.5), X(0.5), X(1.0), X(0.5), X(1.0), X(2.0), X(1.0), X(2.0), X(1.0), X(1.0), X(2.0)}, // bug
    {X(0.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0), X(0.5)}, // ghost
    {X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0), X(0.5), X(1.0), X(0.5), X(0.5), X(1.0), X(0.5), X(1.0), X(2.0), X(1.0), X(1.0)}, // steel
    {X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0)}, // mystery
    {X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(2.0), X(1.0), X(2.0), X(1.0), X(0.5), X(0.5), X(2.0), X(1.0), X(1.0), X(2.0), X(0.5), X(1.0)}, // fire
    {X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(0.5), X(0.5), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0)}, // water
    {X(1.0), X(1.0), X(0.5), X(0.5), X(2.0), X(2.0), X(0.5), X(1.0), X(0.5), X(1.0), X(0.5), X(2.0), X(0.5), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0)}, // grass
    {X(1.0), X(1.0), X(2.0), X(1.0), X(0.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(0.5), X(0.5), X(1.0), X(1.0), X(0.5), X(1.0)}, // electric
    {X(1.0), X(2.0), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0), X(0.0)}, // psychic
    {X(1.0), X(1.0), X(2.0), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(0.5), X(0.5), X(2.0), X(1.0), X(1.0), X(0.5), X(2.0), X(1.0)}, // ice
    {X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0)}, // dragon
    {X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0), X(0.5)}, // dark
};

#undef X

uq4_12_t GetTypeModifier(u32 atkType, u32 defType)
{
    return sTypeEffectivenessTable[atkType][defType];
}

static inline void MulByTypeEffectiveness(uq4_12_t *modifier, u32 move, u32 moveType, u32 battlerDef, u32 defType, u32 battlerAtk, bool32 recordAbilities)
{
    uq4_12_t mod = GetTypeModifier(moveType, defType);

    if ((moveType == TYPE_FIGHTING || moveType == TYPE_NORMAL) && defType == TYPE_GHOST && gBattleMons[battlerDef].status2 & STATUS2_FORESIGHT && mod == UQ_4_12(0.0))
    {
        mod = UQ_4_12(1.0);
    }

    *modifier = uq4_12_multiply(*modifier, mod);
}

u8 GetBattlerType(u32 battler, u8 typeIndex)
{    
    u16 types[2] = {0};
    types[0] = gBattleMons[battler].type1;
    types[1] = gBattleMons[battler].type2;

    return types[typeIndex];
}

static inline uq4_12_t CalcTypeEffectivenessMultiplierInternal(u32 move, u32 moveType, u32 battlerAtk, u32 battlerDef, bool32 recordAbilities, uq4_12_t modifier, u32 defAbility)
{
    MulByTypeEffectiveness(&modifier, move, moveType, battlerDef, GetBattlerType(battlerDef, 0), battlerAtk, recordAbilities);
    if (GetBattlerType(battlerDef, 1) != GetBattlerType(battlerDef, 0))
        MulByTypeEffectiveness(&modifier, move, moveType, battlerDef, GetBattlerType(battlerDef, 1), battlerAtk, recordAbilities);
    if (GetBattlerType(battlerDef, 2) != TYPE_MYSTERY && GetBattlerType(battlerDef, 2) != GetBattlerType(battlerDef, 1)
        && GetBattlerType(battlerDef, 2) != GetBattlerType(battlerDef, 0))
        MulByTypeEffectiveness(&modifier, move, moveType, battlerDef, GetBattlerType(battlerDef, 2), battlerAtk, recordAbilities);

    if ((defAbility == ABILITY_WONDER_GUARD && modifier <= UQ_4_12(1.0))
        && gBattleMoves[move].power)
    {
        modifier = UQ_4_12(0.0);
        if (recordAbilities)
        {
            gLastUsedAbility = gBattleMons[battlerDef].ability;
            gMoveResultFlags |= MOVE_RESULT_MISSED;
            gLastLandedMoves[battlerDef] = 0;
            gBattleCommunication[MISS_TYPE] = B_MSG_AVOIDED_DMG;
            RecordAbilityBattle(battlerDef, gBattleMons[battlerDef].ability);
        }
    }

    return modifier;
}

static void UpdateMoveResultFlags(uq4_12_t modifier)
{
    if (modifier == UQ_4_12(0.0))
    {
        gMoveResultFlags |= MOVE_RESULT_DOESNT_AFFECT_FOE;
        gMoveResultFlags &= ~(MOVE_RESULT_NOT_VERY_EFFECTIVE | MOVE_RESULT_SUPER_EFFECTIVE);
    }
    else if (modifier == UQ_4_12(1.0))
    {
        gMoveResultFlags &= ~(MOVE_RESULT_NOT_VERY_EFFECTIVE | MOVE_RESULT_SUPER_EFFECTIVE | MOVE_RESULT_DOESNT_AFFECT_FOE);
    }
    else if (modifier > UQ_4_12(1.0))
    {
        gMoveResultFlags |= MOVE_RESULT_SUPER_EFFECTIVE;
        gMoveResultFlags &= ~(MOVE_RESULT_NOT_VERY_EFFECTIVE | MOVE_RESULT_DOESNT_AFFECT_FOE);
    }
    else if (modifier < UQ_4_12(1.0))
    {
        gMoveResultFlags |= MOVE_RESULT_NOT_VERY_EFFECTIVE;
        gMoveResultFlags &= ~(MOVE_RESULT_SUPER_EFFECTIVE | MOVE_RESULT_DOESNT_AFFECT_FOE);
    }
}

uq4_12_t CalcTypeEffectivenessMultiplier(u32 move, u32 moveType, u32 battlerAtk, u32 battlerDef, u32 defAbility, bool32 recordAbilities)
{
    uq4_12_t modifier = UQ_4_12(1.0);

    if (move != MOVE_STRUGGLE && moveType != TYPE_MYSTERY)
    {
        modifier = CalcTypeEffectivenessMultiplierInternal(move, moveType, battlerAtk, battlerDef, recordAbilities, modifier, defAbility);
    }

    if (recordAbilities)
        UpdateMoveResultFlags(modifier);
    return modifier;
}

static u32 GetWeather(void)
{
    return gBattleWeather;
}

s32 CalculateMoveDamage(u32 move, u32 battlerAtk, u32 battlerDef, u32 moveType, s32 fixedBasePower, bool32 isCrit, bool32 lowRoll, bool32 updateFlags)
{
    return DoMoveDamageCalc(move, battlerAtk, battlerDef, moveType, fixedBasePower, isCrit, lowRoll,
                            updateFlags, CalcTypeEffectivenessMultiplier(move, moveType, battlerAtk, battlerDef, GetBattlerAbility(battlerDef), updateFlags),
                            GetWeather());
}
