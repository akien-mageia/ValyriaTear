////////////////////////////////////////////////////////////////////////////////
//            Copyright (C) 2004-2011 by The Allacrost Project
//            Copyright (C) 2012-2016 by Bertram (Valyria Tear)
//                         All Rights Reserved
//
// This code is licensed under the GNU GPL version 2. It is free software and
// you may modify it and/or redistribute it under the terms of this license.
// See https://www.gnu.org/copyleft/gpl.html for details.
////////////////////////////////////////////////////////////////////////////////

/** ****************************************************************************
*** \file    battle_actors.h
*** \author  Viljami Korhonen, mindflayer@allacrost.org
*** \author  Corey Hoffstein, visage@allacrost.org
*** \author  Andy Gardner, chopperdave@allacrost.org
*** \author  Yohann Ferreira, yohann ferreira orange fr
*** \brief   Header file for actors present in battles.
***
*** This code contains the implementation of battle actors (characters and
*** enemies) whom are represented on the field of battle.
*** ***************************************************************************/

#ifndef __BATTLE_ACTORS_HEADER__
#define __BATTLE_ACTORS_HEADER__

//#include "common/global/actors/global_actor.h"
#include "common/global/actors/global_character.h"
#include "common/global/actors/global_enemy.h"
#include "common/global/global_effects.h"

#include "battle_utils.h"
#include "engine/video/text.h"
#include "engine/video/particle_effect.h"

#include "script/script_read.h"

#include "common/position_2d.h"

namespace vt_battle
{

namespace private_battle
{

class BattleAction;
class BattleStatusEffectsSupervisor;

/** ****************************************************************************
*** \brief An abstract class for representing an object in the battle
*** Used to properly draw objects based on their Y coordinate.
**/
class BattleObject
{
public:
    BattleObject():
        _origin(0.0f, 0.0f),
        _location(0.0f, 0.0f)
    {}
    virtual ~BattleObject()
    {}

    float GetXOrigin() const {
        return _origin.x;
    }

    float GetYOrigin() const {
        return _origin.y;
    }

    float GetXLocation() const {
        return _location.x;
    }

    float GetYLocation() const {
        return _location.y;
    }

    void SetXOrigin(float x_origin) {
        _origin.x = x_origin;
    }

    void SetYOrigin(float y_origin) {
        _origin.y = y_origin;
    }

    void SetXLocation(float x_location) {
        _location.x = x_location;
    }

    void SetYLocation(float y_location) {
        _location.y = y_location;
    }

    virtual void DrawSprite()
    {}

    //! Tells whether the object can be removed from memory.
    //! \note: Only visual effects are throwable once used.
    virtual bool CanBeRemoved() const {
        return false;
    }

    virtual void Update()
    {}

protected:
    //! \brief The "home" coordinates for the actor's default location on the battle field
    vt_common::Position2D _origin;

    //! \brief The x and y coordinates of the actor's current location on the battle field
    vt_common::Position2D _location;
};

//! \brief A class representing particle effects used as battle objects:
//! spell effects, attack effects, ...
class BattleParticleEffect : public BattleObject
{
public:
    BattleParticleEffect(const std::string& effect_filename);

    //! Used to be drawn at the right time by the battle mode.
    void DrawSprite();

    //! Permits to start the effect.
    bool Start() {
        return _effect.Start();
    }

    //! Tells whether the effect can be removed from memory.
    bool CanBeRemoved() const {
        return !_effect.IsAlive();
    }

    void Update() {
        _effect.Update();
    }

protected:
    //! The particle effect class used internally
    vt_mode_manager::ParticleEffect _effect;
};

//! \brief A class representing animated images used as battle objects:
//! used also for spell effects, attack effects, ...
class BattleAnimation : public BattleObject
{
public:
    BattleAnimation(const std::string& animation_filename);

    //! Used to be drawn at the right time by the battle mode.
    void DrawSprite();


    void Update() {
        _animation.Update();
    }

    //! Permits to restart the animation.
    void Reset() {
        _animation.ResetAnimation();
    }

    void SetVisible(bool show) {
        _visible = show;
    }

    bool IsVisible() const {
        return _visible;
    }

    //! Tells whether the effect can be scheduled for removal from memory.
    bool CanBeRemoved() const {
        return _can_be_removed;
    }

    void Remove() {
        _can_be_removed = true;
    }

    //! Get the animatedImage for deeper manipulations.
    vt_video::AnimatedImage& GetAnimatedImage() {
        return _animation;
    }

protected:
    //! The particle effect class used internally
    vt_video::AnimatedImage _animation;

    //! Set whether the animation is drawn.
    bool _visible;

    //! Set whether the animation can be removed from memory (now useless).
    bool _can_be_removed;
};

/** ****************************************************************************
*** \brief An abstract class for representing an actor in the battle
***
*** An "actor" is a term used to represent both characters and enemies in battle.
*** This abstract class contains members and methods that are common to both types of
*** actors. As such, many of the implemented methods in this class are virtual.
***
*** The BattleActor class contains a pointer to a GlobalActor object that represents
*** the character or enemy. BattleActor contains its own members for all actor stats such
*** as HP, phys_atk, evade rating, etc. There are two reasons why BattleActor uses its own
*** members instead of directly accessing and modifying the members of the GlobalActor pointer.
*** First, various effects can occur in battle which can modify otherwise static stats such as
*** stamina. We need the ability to restore each stat to its base value, and the GlobalActor
*** class retains that unaltered value. Second, if the player loses the battle and chooses to
*** retry, we need to restore all actors back to their original state before the battle began.
*** Retrieving the values of the GlobalActor class allows us to do so.
***
*** Throughout the battle, actors progress through a series of states. The
*** standard set of states that an actor cycles through while they are "alive"
*** and participating in the battle are as follows.
***
*** -# ACTOR_STATE_IDLE
*** -# ACTOR_STATE_COMMAND
*** -# ACTOR_STATE_WARM_UP
*** -# ACTOR_STATE_READY
*** -# ACTOR_STATE_ACTING
*** -# ACTOR_STATE_COOL_DOWN
***
*** Throughout each cycle, the actor will select or be given an action to execute.
*** This action may be to attack an actor on the opposing team, heal a teammate,
*** use an item, or perform some sort of skill. Each actor is responsible for the
*** management of the action that they intend to take.
*** ***************************************************************************/
class BattleActor : public vt_global::GlobalActor, public BattleObject
{
public:
    BattleActor(vt_global::GlobalActor *actor);

    virtual ~BattleActor();

    //! \brief Returns true if the actor is considered an enemy of the character party
    virtual bool IsEnemy() const = 0;

    //! \brief Returns true as long as the actor is not in the "dead" state
    bool IsAlive() const {
        return (_state != ACTOR_STATE_DEAD);
    }

    //! \brief Returns true if the actor can still fight.
    bool CanFight() const {
        return (_state != ACTOR_STATE_DYING && IsAlive());
    }

    /** \brief Restores an actor to the initial state it was in when the battle began
    ***
    *** This includes resetting the actor's state, attributes, status effect, and animations.
    **/
    virtual void ResetActor();

    /** \brief Changes the state of the actor and modifies the actor's properties accordingly
    *** \param new_state The state to set the actor to
    **/
    virtual void ChangeState(ACTOR_STATE new_state);

    //! \brief Returns the width of the actor's sprite image
    virtual float GetSpriteWidth() const = 0;

    //! \brief Returns the height of the actor's sprite image
    virtual float GetSpriteHeight() const = 0;

    /** \brief Changes the actor's current sprite animation image
    *** \param alias The alias text used to identify the animation to change
    ***
    *** \note Not all forms of battle sprites have multiple animations or any animations at all. For
    *** example, enemies typically only have a standard set of unanimated damage frames for their
    *** sprites. The reason this method is defined for all actors is so that the same skills may be
    *** reused for both characters and enemies, since some skill implementations will wish to call
    *** this method on the actor performing the skill.
    ***
    *** \warning This function is not a pure virtual one because it needs to be accessed by Lua
    *** and there is an issue with using Luabind on pure virtual functions.
    **/
    virtual void ChangeSpriteAnimation(const std::string & /*alias*/)
    {}

    inline float GetSpriteAlpha() const {
        return _sprite_alpha;
    }

    //! Set the sprite alpha, useful for custom death sequences and other effects on sprites.
    void SetSpriteAlpha(float alpha) {
        if (alpha > 1.0f) alpha = 1.0f;
        if (alpha < 0.0f) alpha = 0.0f;
        _sprite_alpha = alpha;
    }

    /** \brief Deals damage to the actor by reducing its hit points by a certain amount
    *** \param amount The number of hit points to decrease on the actor
    ***
    *** If the state of the actor is ACTOR_STATE_DEAD, this function will print a warning and change nothing.
    *** If the amount of damage dealt is greater than the actor's current hit points, the actor will be placed
    *** in the ACTOR_STATE_DEAD state.
    **/
    virtual void RegisterDamage(uint32_t amount);

    //! \brief Steals the actor skill points
    void RegisterSPDamage(uint32_t amount);

    /** \brief Deals damage to the actor by reducing its hit points by a certain amount
    *** \param amount The number of hit points to decrease on the actor
    *** \param target A pointer to the target information that was used to inflict this damage
    ***
    *** In addition to dealing damage, this function will use the second target argument to determine if this
    *** damage was inflicted on an attack point (as opposed to the actor as a whole). If so, it analyzes the
    *** attack point on the actor to see if it has any status effect vulnerabilities. If this is indeed the
    *** case, then it applies the status effect to the actor if the random probability calculation determines
    *** that the effect has been triggered.
    **/
    void RegisterDamage(uint32_t amount, BattleTarget *target);

    /** \brief Heals the actor by restoring a certain amount of points
    *** \param amount The number of points to add to the actor.
    *** \param hit_points whether hit points of skill points will be restored.
    ***
    *** If the state of the actor is ACTOR_STATE_DEAD, this function will print a warning and change nothing.
    *** The number of hit points on the actor are not allowed to increase beyond the actor's maximum hit
    *** points.
    **/
    void RegisterHealing(uint32_t amount, bool hit_points = true);

    /** \brief Heals a dead actor and permits him to go back in shape for the battle.
    *** \param amount The number of points to add to the actor. Must be > 0.
    **/
    void RegisterRevive(uint32_t amount);

    /** \brief Indicates that an action failed to connect on this target
    *** \param was_attacked Tells whether the miss is due to a missed attack
    *** It will be used to make the actor play a dodge animation.
    **/
    void RegisterMiss(bool was_attacked = false);

    /** \brief Causes a change in a character's status
    *** \param status The type of status to change
    *** \param intensity The intensity of the change
    *** \param duration A potential duration time (in milliseconds) used to setup custom effect lengths.
    *** A fall back value is used otherwise.
    ***
    *** This is the single method for registering a change in status for an actor. It can be used to add
    *** a new status, remove an existing status, or change the intensity level of an existing status. A
    *** positive intensity argument will increase the intensity while a negative intensity will decrease
    *** the intensity. Many different changes can occur depending upon the current state of the actor and
    *** any active status effects when this function is called, as the list below describes.
    ***
    *** - If this status is not already active on the character and the intensity argument is positive,
    ***   the actor will have the new status added at that intensity.
    *** - If this status is not already active on the character and the intensity argument is negative,
    ***   no change will occur.
    *** - If this status is active and intensity is positive, intensity will be increased but will not
    ***   exceed the maximum intensity level.
    *** - If this status is active, the intensity is positive, and the current intensity of the status
    ***   is already at the maximum level, the status timer will be reset and the intensity will not change.
    *** - If this status is active and intensity is negative, intensity will be decreased but not allowed
    ***   to go lower than the neutral level. If the neutral level is reached, the status will be removed.
    *** - If this status is active and the intensity is GLOBAL_INTENSITY_NEG_EXTREME, the status will be
    ***   removed regardless of its current intensity level.
    *** - If this status has an opposite status type that is active on the actor and the intensity argument
    ***   is positive, this will decrease the intensity of the opposite status by the degree of intensity.
    ***   This may cause that opposite status to be removed and this new status to be added if the value of
    ***   the intensity change is high enough.
    ***
    *** If the desired effect does yield a change in status, this function will prepare an indicator image
    *** to be displayed representing the change in status.
    **/
    void ApplyActiveStatusEffect(vt_global::GLOBAL_STATUS status, vt_global::GLOBAL_INTENSITY intensity,
                                 uint32_t duration = 0);

    //! \brief Removes the given status effect, calling the according BattleRemove() script function.
    void RemoveActiveStatusEffect(vt_global::GLOBAL_STATUS status_effect);

    //! \brief Tells the intensity of the active status effect currently applied on the character,
    //! or GLOBAL_STATUS_NEUTRAL if there is no such effect.
    vt_global::GLOBAL_INTENSITY GetActiveStatusEffectIntensity(vt_global::GLOBAL_STATUS status);

    /** \brief Increases or decreases the current skill points of the actor
    *** \param amount The number of skill points to increase or decrease
    ***
    *** If the actor is dead, no change will take place. If the amount is positive, the actor will
    *** not be allowed to exceed above their maximum skill points.
    ***
    *** Any non-zero change in skill points will be reflected via increase/decrease text that will
    *** be drawn to the screen near the actor's sprite. If the value of the amount argument is zero,
    *** the word "Miss" will be drawn instead;
    **/
    void ChangeSkillPoints(int32_t amount);

    //! \brief Stuns the BattleActor, preventing its state timer to update.
    //! \param stun Whether the actor should be stunned.
    void SetStunned(bool stun) {
        _is_stunned = stun;
    }

    //! \brief Tells whether the actor is stunned.
    bool IsStunned() const {
        return _is_stunned;
    }

    /** \brief Updates the state of the actor
    ***
    *** The optional boolean parameter is primarily used by battle sequences which desire to update the sprite graphics
    *** but not any battle state.
    **/
    virtual void Update();

    //! \brief Draws the stamina icon - default implementation
    virtual void DrawStaminaIcon(const vt_video::Color &color = vt_video::Color::white) const;

    /** \brief Sets the action that the actor should execute next
    *** \param action A pointer to the action that the actor should execute
    ***
    *** The actor assumes responsibility for the memory management of the action that is given to it with
    *** this method and will delete the object at the appropriate time. You should only call the method
    *** when the actor is in the state ACTOR_STATE_COMMAND. Invoking it at any other time will result in a
    *** warning and no operation, and the action object will be deleted immediately. A warning is also
    *** printed in the case where the actor has another action prepared.
    **/
    void SetAction(BattleAction *action);

    //! \brief Convenience wrapper for all targets type skills
    //! This one useful for self target-type skills, and all allies/enemies target-type skills.
    void SetAction(uint32_t skill_id) {
        SetAction(skill_id, nullptr);
    }

    //! \brief Convenience wrapper for single target type skills
    void SetAction(uint32_t skill_id, BattleActor* target_actor);

    //! \brief Resets actor stats to their original values
    //@{
    void ResetHitPoints() {
        SetHitPoints(_global_actor->GetHitPoints());
    }

    void ResetMaxHitPoints() {
        SetMaxHitPoints(_global_actor->GetMaxHitPoints());
    }

    void ResetSkillPoints() {
        SetSkillPoints(_global_actor->GetSkillPoints());
    }

    void ResetMaxSkillPoints() {
        SetMaxSkillPoints(_global_actor->GetMaxSkillPoints());
    }

    void ResetPhysAtk() {
        SetPhysAtk(_global_actor->GetPhysAtk());
        SetPhysAtkModifier(1.0f);
    }

    void ResetMagAtk() {
        SetMagAtk(_global_actor->GetMagAtk());
        SetMagAtkModifier(1.0f);
    }

    void ResetPhysDef() {
        SetPhysDef(_global_actor->GetPhysDef());
        SetPhysDefModifier(1.0f);
    }

    void ResetMagDef() {
        SetMagDef(_global_actor->GetMagDef());
        SetMagDefModifier(1.0f);
    }

    void ResetStamina() {
        SetStamina(_global_actor->GetStamina());
        SetStaminaModifier(1.0f);
    }

    //! SetStamina() overloading the GlobalActor one, to permit updating the idle State timer also.
    void SetStamina(uint32_t stamina);

    //! SetStaminaModifier() overloading the GlobalActor one, to permit updating the idle State timer also.
    void SetStaminaModifier(float modifier);

    void ResetEvade() {
        SetEvade(_global_actor->GetEvade());
        SetEvadeModifier(1.0f);
    }
    //@}

    //! \name Class member access methods
    //@{
    ACTOR_STATE GetState() const {
        return _state;
    }

    vt_global::GlobalActor* GetGlobalActor() {
        return _global_actor;
    }

    const std::string& GetAmmoAnimationFile() const {
        return _ammo_animation_file;
    }

    BattleAction *GetAction() {
        return _action;
    }

    bool IsActionSet() const {
        return (_action != nullptr);
    }

    uint32_t GetIdleStateTime() const {
        return _idle_state_time;
    }

    vt_video::StillImage &GetStaminaIcon() {
        return _stamina_icon;
    }

    vt_system::SystemTimer &GetStateTimer() {
        return _state_timer;
    }

    //! \note If the actor is in the idle state, this will not affect the state timer
    void SetIdleStateTime(uint32_t time) {
        _idle_state_time = time;
    }
    //@}

protected:
    //! \brief The state that the actor is currently in
    ACTOR_STATE _state;

    //! \brief A pointer to the global actor object which the battle actor represents
    vt_global::GlobalActor* _global_actor;

    //! \brief The ammo animation filename, when the actor weapon uses ammo. Empty otherwise.
    std::string _ammo_animation_file;

    //! \brief A pointer to the action that the actor is preparing to perform or is currently performing
    BattleAction* _action;

    //! \brief The amount of time (in milliseconds) that the actor needs to wait to pass through the idle state
    uint32_t _idle_state_time;

    //! \brief A timer used as the character progresses through the standard series of actor states
    vt_system::SystemTimer _state_timer;

    //! \brief A timer telling the time the character is hurt, making it visually shaking.
    vt_system::SystemTimer _hurt_timer;

    //! \brief Tells whether the actor is stunned, preventing its idle state time to update.
    bool _is_stunned;

    //! \brief Contains the alpha value to draw the sprite at: useful for fading effects
    float _sprite_alpha;

    //! \brief Used to assist in the animation of actors as they move on the battlefield
    vt_system::SystemTimer _animation_timer;

    //! \brief The x and y coordinates of the actor's current stamina icon on the stamina bar.
    vt_common::Position2D _stamina_location;

    //! \brief An assistant class to the actor that manages all the actor's status and elemental effects
    BattleStatusEffectsSupervisor* _effects_supervisor;

    //! \brief Script object used when playing the death sequence.
    //! A default sequence is played one of those is invalid.
    vt_script::ReadScriptDescriptor _death_script;
    luabind::object _death_update;
    luabind::object _death_init;
    //! This function permits to draw something along with the Battle enemy sprite
    luabind::object _death_draw_on_sprite;

    //! \brief The battle AI script.
    vt_script::ReadScriptDescriptor _ai_script;
    //! \brief The "DecideAction" ai script.
    luabind::object _ai_decide_action;

    //! \brief Loads the potential death animation scripted functions.
    void _LoadDeathAnimationScript();

    //! \brief Loads the potential battle AI scripted function.
    void _LoadAIScript();

    /** \brief Decides what action that the hero or enemy should execute and the target
    *** This function is used as a fallback when no AI script is set for the given enemy.
    *** \note More complete AI decision making algorithms should be supported
    *** through lua scripts.
    **/
    void _DecideAction();

    //! \brief Updates the Stamina Icon position.
    void _UpdateStaminaIconPosition();

    //! \brief Updates the actor state
    void _UpdateState();

    //! \brief Initializes the Battle Actor stats values.
    //! The global actor final stats are used as base for the battle actors.
    //! This means that the final phys_atk value of the global actor is the base value
    //! of the battle actor.
    //! This way, equipment modifiers aren't touched in battles, but battle modifiers
    //! are applied on top of the global actor values and properly reset to the global
    //! (and equipment values) when the battle effects disappear.
    void _InitStats();

    //! Returns the text style corresponding to the damage/healing type and amount
    vt_video::TextStyle _GetDamageTextStyle(uint32_t amount, bool is_sp_damage);
    vt_video::TextStyle _GetHealingTextStyle(uint32_t amount, bool is_hp);
}; // class BattleActor


/** ****************************************************************************
*** \brief Represents a player-controlled character in the battle
***
*** Character actors have a series of animated images that reflect their current
*** state and actions. Each character also has a custom set of progressive damage
*** battle portraits (5 in total) that are drawn when the character is selected.
*** ***************************************************************************/
class BattleCharacter : public BattleActor
{
public:
    BattleCharacter(vt_global::GlobalCharacter *character);

    ~BattleCharacter();

    bool IsEnemy() const {
        return false;
    }

    void ChangeState(ACTOR_STATE new_state);

    float GetSpriteWidth() const {
        return _current_sprite_animation ? _current_sprite_animation->GetWidth() : 0.0f;
    }

    float GetSpriteHeight() const {
        return _current_sprite_animation ? _current_sprite_animation->GetHeight() : 0.0f;
    }

    /** \brief Changes the battle character's current sprite animation image
    *** \param alias The alias text used to identify the animation to change
    ***
    *** \note Not all forms of battle sprites have multiple animations or any animations at all. For
    *** example, enemies typically only have a standard set of unanimated damage frames for their
    *** sprites. The reason this method is defined for all actors is so that the same skills may be
    *** reused for both characters and enemies, since some skill implementations will wish to call
    *** this method on the actor performing the skill.
    **/
    void ChangeSpriteAnimation(const std::string &alias);

    //! \brief Changes the action and target selection text to reflect the character's current state
    void ChangeActionText();

    //! \brief Returns true if the player may select a command for the character to execute
    bool CanSelectCommand() const {
        return (_state == ACTOR_STATE_IDLE) || (_state == ACTOR_STATE_COMMAND);
    }

    //! \brief Updates the state of the character. Must be called every frame loop.
    void Update();

    //! \brief Draws the character's current sprite animation frame
    void DrawSprite();

    //! \brief Draws the character's damage-blended face portrait
    void DrawPortrait();

    /** \brief Draws the character's status in the bottom area of the screen
    *** \param order The order position of the character [0-3] used to determine draw positions
    *** \param character_command Tells which character the command menu is open for, if any. (can be nullptr)
    **/
    void DrawStatus(uint32_t order, BattleCharacter* character_command);

    vt_global::GlobalCharacter *GetGlobalCharacter() {
        return _global_character;
    }

    const std::string &GetSpriteAnimationAlias() const {
        return _sprite_animation_alias;
    }

protected:
    //! \brief A pointer to the global character object which the battle character represents
    vt_global::GlobalCharacter* _global_character;

    //! \brief Retrains the last HP and SP values that were rendered to text
    uint32_t _last_rendered_hp, _last_rendered_sp;

    //! \brief Contains the identifier text of the current sprite animation
    std::string _sprite_animation_alias;

    //! \brief The animation name before being attacked,
    //! used to return to it after the hurt or dodge animation.
    std::string _before_attack_sprite_animation;

    //! \brief The Animated image pointer from the global character
    //! Used to avoid calling the global character std::map find calls on each loops
    //! Don't delete it, it's just a reference to the global manager animated images
    vt_video::AnimatedImage* _current_sprite_animation;

    //! The current weapon animation loaded for the given weapon
    vt_video::AnimatedImage _current_weapon_animation;

    //! \brief Rendered text of the character's name
    vt_video::TextImage _name_text;

    //! \brief Rendered text of the character's current hit points
    vt_video::TextImage _hit_points_text;

    //! \brief Rendered text of the character's current skill points
    vt_video::TextImage _skill_points_text;

    //! \brief Rendered text of the character's currently selected action
    vt_video::TextImage _action_selection_text;

    //! \brief Rendered icon of the character's currently selected action
    vt_video::StillImage _action_selection_icon;
}; // class BattleCharacter


/** ****************************************************************************
*** \brief Represents the entity for an enemy in the battle
***
*** This class is a wrapper around a GlobalEnemy object.
*** ***************************************************************************/
class BattleEnemy : public BattleActor
{
public:
    BattleEnemy(uint32_t enemy_id);

    ~BattleEnemy();

    bool IsEnemy() const {
        return true;
    }

    void ChangeState(ACTOR_STATE new_state);

    float GetSpriteWidth() const {
        return _global_enemy->GetSpriteWidth();
    }

    float GetSpriteHeight() const {
        return _global_enemy->GetSpriteHeight();
    }

    /** \brief Changes the battle enemy's current sprite animation image
    *** \param alias The alias text used to identify the animation to change
    ***
    *** \note Not all forms of battle sprites have multiple animations or any animations at all. For
    *** example, enemies typically only have a standard set of unanimated damage frames for their
    *** sprites. The reason this method is defined for all actors is so that the same skills may be
    *** reused for both characters and enemies, since some skill implementations will wish to call
    *** this method on the actor performing the skill.
    ***
    *** \warning This function is not a pure virtual one because it needs to be accessed by Lua
    *** and there is an issue with using Luabind on pure virtual functions.
    **/
    void ChangeSpriteAnimation(const std::string &alias);

    void Update();

    //! \brief Draws the damage blended enemy sprite image on to the battle field
    void DrawSprite();

    vt_global::GlobalEnemy *GetGlobalEnemy() {
        return _global_enemy;
    }

    //! \brief See BattleActor::DrawStaminaIcon()
    void DrawStaminaIcon(const vt_video::Color &color = vt_video::Color::white) const;

protected:
    //! \brief A pointer to the global enemy object which the battle enemy represents
    vt_global::GlobalEnemy* _global_enemy;

    //! \brief A pointer to the enemy battle animations
    //! Do not delete it, the global enemy instance will take care of it.
    std::vector<vt_video::AnimatedImage>* _sprite_animations;

    //! \brief Contains the identifier text of the current sprite animation
    std::string _sprite_animation_alias;

    //! \brief Set to true when the actor is in the ACTING state and the execution of the action is complete
    bool _action_finished;
}; // class BattleEnemy

} // namespace private_battle

} // namespace vt_battle

#endif // __BATTLE_ACTORS_HEADER__
