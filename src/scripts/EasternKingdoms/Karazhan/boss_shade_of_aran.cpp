/*
 * Copyright (C) 2013  BlizzLikeGroup
 * BlizzLikeCore integrates as part of this file: CREDITS.md and LICENSE.md
 */

/* ScriptData
Name: Boss_Shade_of_Aran
Complete(%): 95
Comment: Flame wreath missing cast animation, mods won't triggere.
Category: Karazhan
EndScriptData */

#include "ScriptPCH.h"
#include "ScriptedSimpleAI.h"
#include "karazhan.h"
#include "GameObject.h"

#define SAY_AGGRO1                  -1532073
#define SAY_AGGRO2                  -1532074
#define SAY_AGGRO3                  -1532075
#define SAY_FLAMEWREATH1            -1532076
#define SAY_FLAMEWREATH2            -1532077
#define SAY_BLIZZARD1               -1532078
#define SAY_BLIZZARD2               -1532079
#define SAY_EXPLOSION1              -1532080
#define SAY_EXPLOSION2              -1532081
#define SAY_DRINK                   -1532082                //Low Mana / AoE Pyroblast
#define SAY_ELEMENTALS              -1532083
#define SAY_KILL1                   -1532084
#define SAY_KILL2                   -1532085
#define SAY_TIMEOVER                -1532086
#define SAY_DEATH                   -1532087
#define SAY_ATIESH                  -1532088                //Atiesh is equipped by a raid member

//Spells
#define SPELL_FROSTBOLT     29954
#define SPELL_FIREBALL      29953
#define SPELL_ARCMISSLE     29955
#define SPELL_CHAINSOFICE   29991
#define SPELL_DRAGONSBREATH 29964
#define SPELL_MASSSLOW      30035
#define SPELL_FLAME_WREATH  29946
#define SPELL_AOE_CS        29961
#define SPELL_PLAYERPULL    32265
#define SPELL_AEXPLOSION    29973
#define SPELL_MASS_POLY     29963
#define SPELL_BLINK_CENTER  29967
#define SPELL_ELEMENTALS    29962
#define SPELL_CONJURE       29975
#define SPELL_DRINK         30024
#define SPELL_POTION        32453
#define SPELL_AOE_PYROBLAST 29978

//Creature Spells
#define SPELL_CIRCULAR_BLIZZARD     29951                   //29952 is the REAL circular blizzard that leaves persistant blizzards that last for 10 seconds
#define SPELL_WATERBOLT             31012
#define SPELL_SHADOW_PYRO           29978

//Creatures
#define CREATURE_WATER_ELEMENTAL    17167
#define CREATURE_SHADOW_OF_ARAN     18254
#define CREATURE_ARAN_BLIZZARD      17161

enum SuperSpell
{
    SUPER_FLAME = 0,
    SUPER_BLIZZARD,
    SUPER_AE,
};

struct boss_aranAI : public ScriptedAI
{
    boss_aranAI(Creature* c) : ScriptedAI(c)
    {
        pInstance = c->GetInstanceData();
    }

    ScriptedInstance* pInstance;

    uint32 SecondarySpellTimer;
    uint32 NormalCastTimer;
    uint32 SuperCastTimer;
    uint32 BerserkTimer;
    uint32 CloseDoorTimer;                                  // Don't close the door right on aggro in case some people are still entering.

    uint8 LastSuperSpell;

    uint32 FlameWreathTimer;
    uint32 FlameWreathCheckTime;
    uint64 FlameWreathTarget[3];
    float FWTargPosX[3];
    float FWTargPosY[3];

    uint32 CurrentNormalSpell;
    uint32 ArcaneCooldown;
    uint32 FireCooldown;
    uint32 FrostCooldown;

    uint32 DrinkInturruptTimer;

    bool ElementalsSpawned;
    bool Drinking;
    bool DrinkInturrupted;

    void Reset()
    {
        SecondarySpellTimer = 5000;
        NormalCastTimer = 0;
        SuperCastTimer = 35000;
        BerserkTimer = 720000;
        CloseDoorTimer = 15000;

        LastSuperSpell = rand()%3;

        FlameWreathTimer = 0;
        FlameWreathCheckTime = 0;

        CurrentNormalSpell = 0;
        ArcaneCooldown = 0;
        FireCooldown = 0;
        FrostCooldown = 0;

        DrinkInturruptTimer = 10000;

        ElementalsSpawned = false;
        Drinking = false;
        DrinkInturrupted = false;

        if (pInstance)
        {
            // Not in progress
            pInstance->SetData(TYPE_ARAN, NOT_STARTED);
            pInstance->HandleGameObject(pInstance->GetData64(DATA_GO_LIBRARY_DOOR), true);
        }
    }

    void KilledUnit(Unit* /*victim*/)
    {
        DoScriptText(RAND(SAY_KILL1,SAY_KILL2), me);
    }

    void JustDied(Unit* /*victim*/)
    {
        DoScriptText(SAY_DEATH, me);

        if (pInstance)
        {
            pInstance->SetData(TYPE_ARAN, DONE);
            pInstance->HandleGameObject(pInstance->GetData64(DATA_GO_LIBRARY_DOOR), true);
        }
    }

    void EnterCombat(Unit* /*who*/)
    {
        DoScriptText(RAND(SAY_AGGRO1,SAY_AGGRO2,SAY_AGGRO3), me);

        if (pInstance)
        {
            pInstance->SetData(TYPE_ARAN, IN_PROGRESS);
            pInstance->HandleGameObject(pInstance->GetData64(DATA_GO_LIBRARY_DOOR), false);
        }
    }

    void FlameWreathEffect()
    {
        std::vector<Unit*> targets;
        std::list<HostileReference *> t_list = me->getThreatManager().getThreatList();

        if (!t_list.size())
            return;

        //store the threat list in a different container
        for (std::list<HostileReference *>::const_iterator itr = t_list.begin(); itr!= t_list.end(); ++itr)
        {
            Unit* pTarget = Unit::GetUnit(*me, (*itr)->getUnitGuid());
            //only on alive players
            if (pTarget && pTarget->isAlive() && pTarget->GetTypeId() == TYPEID_PLAYER)
                targets.push_back(pTarget);
        }

        //cut down to size if we have more than 3 targets
        while (targets.size() > 3)
            targets.erase(targets.begin()+rand()%targets.size());

        uint32 i = 0;
        for (std::vector<Unit*>::const_iterator itr = targets.begin(); itr!= targets.end(); ++itr)
        {
            if (*itr)
            {
                FlameWreathTarget[i] = (*itr)->GetGUID();
                FWTargPosX[i] = (*itr)->GetPositionX();
                FWTargPosY[i] = (*itr)->GetPositionY();
                DoCast((*itr), SPELL_FLAME_WREATH, true);
                ++i;
            }
        }
    }

    void UpdateAI(const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        if (CloseDoorTimer)
        {
            if (CloseDoorTimer <= diff)
            {
                if (pInstance)
                {
                    pInstance->HandleGameObject(pInstance->GetData64(DATA_GO_LIBRARY_DOOR), false);
                    CloseDoorTimer = 0;
                }
            } else CloseDoorTimer -= diff;
        }

        //Cooldowns for casts
        if (ArcaneCooldown)
        {
            if (ArcaneCooldown >= diff)
                ArcaneCooldown -= diff;
        else ArcaneCooldown = 0;
        }

        if (FireCooldown)
        {
            if (FireCooldown >= diff)
                FireCooldown -= diff;
        else FireCooldown = 0;
        }

        if (FrostCooldown)
        {
            if (FrostCooldown >= diff)
                FrostCooldown -= diff;
        else FrostCooldown = 0;
        }

        if (!Drinking && me->GetMaxPower(POWER_MANA) && (me->GetPower(POWER_MANA)*100 / me->GetMaxPower(POWER_MANA)) < 20)
        {
            Drinking = true;
            me->InterruptNonMeleeSpells(false);

            DoScriptText(SAY_DRINK, me);

            if (!DrinkInturrupted)
            {
                DoCast(me, SPELL_MASS_POLY, true);
                DoCast(me, SPELL_CONJURE, false);
                DoCast(me, SPELL_DRINK, false);
                me->SetStandState(UNIT_STAND_STATE_SIT);
                DrinkInturruptTimer = 10000;
            }
        }

        //Drink Inturrupt
        if (Drinking && DrinkInturrupted)
        {
            Drinking = false;
            me->RemoveAurasDueToSpell(SPELL_DRINK);
            me->SetStandState(UNIT_STAND_STATE_STAND);
            me->SetPower(POWER_MANA, me->GetMaxPower(POWER_MANA)-32000);
            DoCast(me, SPELL_POTION, false);
        }

        //Drink Inturrupt Timer
        if (Drinking && !DrinkInturrupted)
        {
            if (DrinkInturruptTimer >= diff)
                DrinkInturruptTimer -= diff;
        }
        else
        {
            me->SetStandState(UNIT_STAND_STATE_STAND);
            DoCast(me, SPELL_POTION, true);
            DoCast(me, SPELL_AOE_PYROBLAST, false);
            DrinkInturrupted = true;
            Drinking = false;
        }

        //Don't execute any more code if we are drinking
        if (Drinking)
            return;

        //Normal casts
        if (NormalCastTimer <= diff)
        {
            if (!me->IsNonMeleeSpellCasted(false))
            {
                Unit* pTarget = SelectTarget(SELECT_TARGET_RANDOM, 0, 100, true);
                if (!pTarget)
                    return;

                uint32 Spells[3];
                uint8 AvailableSpells = 0;

                //Check for what spells are not on cooldown
                if (!ArcaneCooldown)
                {
                    Spells[AvailableSpells] = SPELL_ARCMISSLE;
                    ++AvailableSpells;
                }
                if (!FireCooldown)
                {
                    Spells[AvailableSpells] = SPELL_FIREBALL;
                    ++AvailableSpells;
                }
                if (!FrostCooldown)
                {
                    Spells[AvailableSpells] = SPELL_FROSTBOLT;
                    ++AvailableSpells;
                }

                //If no available spells wait 1 second and try again
                if (AvailableSpells)
                {
                    CurrentNormalSpell = Spells[rand() % AvailableSpells];
                    DoCast(pTarget, CurrentNormalSpell);
                }
            }
            NormalCastTimer = 1000;
        } else NormalCastTimer -= diff;

        if (SecondarySpellTimer <= diff)
        {
            switch (urand(0,1))
            {
                case 0:
                    DoCast(me, SPELL_AOE_CS);
                    break;
                case 1:
                    if (Unit* pTarget = SelectTarget(SELECT_TARGET_RANDOM, 0, 100, true))
                        DoCast(pTarget, SPELL_CHAINSOFICE);
                    break;
            }
            SecondarySpellTimer = urand(5000,20000);
        } else SecondarySpellTimer -= diff;

        if (SuperCastTimer <= diff)
        {
            uint8 Available[2];

            switch (LastSuperSpell)
            {
                case SUPER_AE:
                    Available[0] = SUPER_FLAME;
                    Available[1] = SUPER_BLIZZARD;
                    break;
                case SUPER_FLAME:
                    Available[0] = SUPER_AE;
                    Available[1] = SUPER_BLIZZARD;
                    break;
                case SUPER_BLIZZARD:
                    Available[0] = SUPER_FLAME;
                    Available[1] = SUPER_AE;
                    break;
            }

            LastSuperSpell = Available[urand(0,1)];

            switch (LastSuperSpell)
            {
                case SUPER_AE:
                    DoScriptText(RAND(SAY_EXPLOSION1,SAY_EXPLOSION2), me);

                    DoCast(me, SPELL_BLINK_CENTER, true);
                    DoCast(me, SPELL_PLAYERPULL, true);
                    DoCast(me, SPELL_MASSSLOW, true);
                    DoCast(me, SPELL_AEXPLOSION, false);
                    break;

                case SUPER_FLAME:
                    DoScriptText(RAND(SAY_FLAMEWREATH1,SAY_FLAMEWREATH2), me);

                    FlameWreathTimer = 20000;
                    FlameWreathCheckTime = 500;

                    FlameWreathTarget[0] = 0;
                    FlameWreathTarget[1] = 0;
                    FlameWreathTarget[2] = 0;

                    FlameWreathEffect();
                    break;

                case SUPER_BLIZZARD:
                    DoScriptText(RAND(SAY_BLIZZARD1,SAY_BLIZZARD2), me);

                    if (Creature* pSpawn = me->SummonCreature(CREATURE_ARAN_BLIZZARD, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 25000))
                    {
                        pSpawn->setFaction(me->getFaction());
                        pSpawn->CastSpell(pSpawn, SPELL_CIRCULAR_BLIZZARD, false);
                    }
                    break;
            }

            SuperCastTimer = urand(35000,40000);
        } else SuperCastTimer -= diff;

        if (!ElementalsSpawned && me->GetHealth()*100 / me->GetMaxHealth() < 40)
        {
            ElementalsSpawned = true;

            for (uint32 i = 0; i < 4; ++i)
            {
                if (Creature* pUnit = me->SummonCreature(CREATURE_WATER_ELEMENTAL, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 90000))
                {
                    pUnit->Attack(me->getVictim(), true);
                    pUnit->setFaction(me->getFaction());
                }
            }

            DoScriptText(SAY_ELEMENTALS, me);
        }

        if (BerserkTimer <= diff)
        {
            for (uint32 i = 0; i < 5; ++i)
            {
                if (Creature* pUnit = me->SummonCreature(CREATURE_SHADOW_OF_ARAN, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 5000))
                {
                    pUnit->Attack(me->getVictim(), true);
                    pUnit->setFaction(me->getFaction());
                }
            }

            DoScriptText(SAY_TIMEOVER, me);

            BerserkTimer = 60000;
        } else BerserkTimer -= diff;

        //Flame Wreath check
        if (FlameWreathTimer)
        {
            if (FlameWreathTimer >= diff)
                FlameWreathTimer -= diff;
            else FlameWreathTimer = 0;

            if (FlameWreathCheckTime <= diff)
            {
                for (uint8 i = 0; i < 3; ++i)
                {
                    if (!FlameWreathTarget[i])
                        continue;

                    Unit* pUnit = Unit::GetUnit(*me, FlameWreathTarget[i]);
                    if (pUnit && !pUnit->IsWithinDist2d(FWTargPosX[i], FWTargPosY[i], 3))
                    {
                        pUnit->CastSpell(pUnit, 20476, true, 0, 0, me->GetGUID());
                        pUnit->CastSpell(pUnit, 11027, true);
                        FlameWreathTarget[i] = 0;
                    }
                }
                FlameWreathCheckTime = 500;
            } else FlameWreathCheckTime -= diff;
        }

        if (ArcaneCooldown && FireCooldown && FrostCooldown)
            DoMeleeAttackIfReady();
    }

    void DamageTaken(Unit* /*pAttacker*/, uint32 &damage)
    {
        if (!DrinkInturrupted && Drinking && damage)
            DrinkInturrupted = true;
    }

    void SpellHit(Unit* /*pAttacker*/, const SpellEntry* Spell)
    {
        //We only care about inturrupt effects and only if they are durring a spell currently being casted
        if ((Spell->Effect[0] != SPELL_EFFECT_INTERRUPT_CAST &&
            Spell->Effect[1] != SPELL_EFFECT_INTERRUPT_CAST &&
            Spell->Effect[2] != SPELL_EFFECT_INTERRUPT_CAST) || !me->IsNonMeleeSpellCasted(false))
            return;

        //Inturrupt effect
        me->InterruptNonMeleeSpells(false);

        //Normally we would set the cooldown equal to the spell duration
        //but we do not have access to the DurationStore

        switch (CurrentNormalSpell)
        {
            case SPELL_ARCMISSLE: ArcaneCooldown = 5000; break;
            case SPELL_FIREBALL: FireCooldown = 5000; break;
            case SPELL_FROSTBOLT: FrostCooldown = 5000; break;
        }
    }
};

struct water_elementalAI : public ScriptedAI
{
    water_elementalAI(Creature* c) : ScriptedAI(c) {}

    uint32 CastTimer;

    void Reset()
    {
        CastTimer = 2000 + (rand()%3000);
    }

    void EnterCombat(Unit* /*who*/) {}

    void UpdateAI(const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        if (CastTimer <= diff)
        {
            DoCast(me->getVictim(), SPELL_WATERBOLT);
            CastTimer = urand(2000,5000);
        } else CastTimer -= diff;
    }
};

CreatureAI* GetAI_boss_aran(Creature* pCreature)
{
    return new boss_aranAI (pCreature);
}

CreatureAI* GetAI_water_elemental(Creature* pCreature)
{
    return new water_elementalAI (pCreature);
}

// CONVERT TO ACID
CreatureAI* GetAI_shadow_of_aran(Creature* pCreature)
{
    outstring_log("BSCR: Convert simpleAI script for Creature Entry %u to ACID", pCreature->GetEntry());
    SimpleAI* ai = new SimpleAI (pCreature);

    ai->Spell[0].Enabled = true;
    ai->Spell[0].Spell_Id = SPELL_SHADOW_PYRO;
    ai->Spell[0].Cooldown = 5000;
    ai->Spell[0].First_Cast = 1000;
    ai->Spell[0].Cast_Target_Type = CAST_HOSTILE_TARGET;

    ai->EnterEvadeMode();

    return ai;
}

void AddSC_boss_shade_of_aran()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name = "boss_shade_of_aran";
    newscript->GetAI = &GetAI_boss_aran;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_shadow_of_aran";
    newscript->GetAI = &GetAI_shadow_of_aran;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_aran_elemental";
    newscript->GetAI = &GetAI_water_elemental;
    newscript->RegisterSelf();
}

