#include "roguelike.h"
#include "ecsTypes.h"
#include "raylib.h"
#include "stateMachine.h"
#include "aiLibrary.h"
#include "log.h"

static StateMachine *create_patrol_attack_flee_sm()
{
  StateMachine *sm = create_state_machine();

  int patrol = sm->addState(create_patrol_state(3.f));
  int moveToEnemy = sm->addState(create_move_to_enemy_state());
  int fleeFromEnemy = sm->addState(create_flee_from_enemy_state());

  sm->addTransition(create_enemy_available_transition(3.f), patrol, moveToEnemy);
  sm->addTransition(create_negate_transition(create_enemy_available_transition(5.f)), moveToEnemy, patrol);

  sm->addTransition(create_and_transition(create_hitpoints_less_than_transition(60.f), create_enemy_available_transition(5.f)),
                    moveToEnemy, fleeFromEnemy);
  sm->addTransition(create_and_transition(create_hitpoints_less_than_transition(60.f), create_enemy_available_transition(3.f)),
                    patrol, fleeFromEnemy);

  sm->addTransition(create_negate_transition(create_enemy_available_transition(7.f)), fleeFromEnemy, patrol);

  return sm;
}

static void add_patrol_attack_flee_sm(flecs::entity entity)
{
  entity.insert([](StateMachine &sm)
  {
    int patrol = sm.addState(create_patrol_state(3.f));
    int moveToEnemy = sm.addState(create_move_to_enemy_state());
    int fleeFromEnemy = sm.addState(create_flee_from_enemy_state());

    sm.addTransition(create_enemy_available_transition(3.f), patrol, moveToEnemy);
    sm.addTransition(create_negate_transition(create_enemy_available_transition(5.f)), moveToEnemy, patrol);

    sm.addTransition(create_and_transition(create_hitpoints_less_than_transition(60.f), create_enemy_available_transition(5.f)),
                     moveToEnemy, fleeFromEnemy);
    sm.addTransition(create_and_transition(create_hitpoints_less_than_transition(60.f), create_enemy_available_transition(3.f)),
                     patrol, fleeFromEnemy);

    sm.addTransition(create_negate_transition(create_enemy_available_transition(7.f)), fleeFromEnemy, patrol);
  });
}

static void add_patrol_flee_sm(flecs::entity entity)
{
  entity.insert([](StateMachine &sm)
  {
    int patrol = sm.addState(create_patrol_state(3.f));
    int fleeFromEnemy = sm.addState(create_flee_from_enemy_state());

    sm.addTransition(create_enemy_available_transition(3.f), patrol, fleeFromEnemy);
    sm.addTransition(create_negate_transition(create_enemy_available_transition(5.f)), fleeFromEnemy, patrol);
  });
}

static void add_attack_sm(flecs::entity entity)
{
  entity.insert([](StateMachine &sm)
  {
    sm.addState(create_move_to_enemy_state());
  });
}

static void add_slime_sm(flecs::entity entity, bool isSmall)
{
  entity.insert([&](StateMachine &sm)
  {
    int bigSlimeState = sm.addState(create_sm_state(create_patrol_attack_flee_sm()));
    int smallSlimeState = sm.addState(create_sm_state(create_patrol_attack_flee_sm()));
    int spawnSlimeState = sm.addState(create_spawn_slime_state());

    sm.addTransition(create_hitpoints_less_than_transition(50.f), bigSlimeState, spawnSlimeState);
    sm.addTransition(create_always_transition(), spawnSlimeState, smallSlimeState);

    if (isSmall)
      sm.switchState(smallSlimeState);
  });
}

static void add_archer_sm(flecs::entity entity)
{
  entity.insert([](StateMachine &sm)
  {
    int moveToEnemy = sm.addState(create_move_to_enemy_state());
    int shootEnemy = sm.addState(create_attack_enemy_state());
    int fleeFromEnemy = sm.addState(create_flee_from_enemy_state());

    sm.addTransition(create_enemy_available_transition(5.f), moveToEnemy, shootEnemy);
    sm.addTransition(create_negate_transition(create_enemy_available_transition(5.f)), shootEnemy, moveToEnemy);
    sm.addTransition(create_enemy_available_transition(3.f), shootEnemy, fleeFromEnemy);
    sm.addTransition(create_negate_transition(create_enemy_available_transition(7.f)), fleeFromEnemy, moveToEnemy);
  });
}

static void add_healer_sm(flecs::entity entity)
{
  entity.insert([](StateMachine &sm)
  {
    int moveToPlayer = sm.addState(create_move_to_player_state());
    int moveToEnemy = sm.addState(create_move_to_enemy_state());
    int heal = sm.addState(create_healing_state());

    sm.addTransition(create_enemy_available_transition(3.f), moveToPlayer, moveToEnemy);
    sm.addTransition(create_negate_transition(create_enemy_available_transition(3.f)), moveToEnemy, moveToPlayer);

    sm.addTransition(create_and_transition(create_ally_available_transition(1.f), create_player_hitpoints_less_than_transition(50.f)),
      moveToPlayer, heal);
    sm.addTransition(create_negate_transition(create_and_transition(create_ally_available_transition(1.f), create_player_hitpoints_less_than_transition(50.f))),
      heal, moveToPlayer);
  });
}

static flecs::entity create_monster(flecs::world &ecs, int x, int y, Color color)
{
  return ecs.entity()
    .set(Position{x, y})
    .set(MovePos{x, y})
    .set(PatrolPos{x, y})
    .set(Hitpoints{100.f})
    .set(Action{EA_NOP})
    .set(Color{color})
    .set(StateMachine{})
    .set(Team{1})
    .set(NumActions{1, 0})
    .set(MeleeDamage{20.f})
    .add<IsMob>()
    .set(MobType{MOB_HUMAN});
}

static flecs::entity create_slime(flecs::world &ecs, int x, int y, Color color)
{
  return ecs.entity()
    .set(Position{x, y})
    .set(MovePos{x, y})
    .set(PatrolPos{x, y})
    .set(Hitpoints{80.f})
    .set(Action{EA_NOP})
    .set(Color{color})
    .set(StateMachine{})
    .set(Team{1})
    .set(NumActions{1, 0})
    .set(MeleeDamage{10.f})
    .add<IsMob>()
    .set(MobType{MOB_SLIME});
}

static flecs::entity create_archer(flecs::world &ecs, int x, int y, Color color)
{
  return ecs.entity()
    .set(Position{x, y})
    .set(MovePos{x, y})
    .set(PatrolPos{x, y})
    .set(Hitpoints{100.f})
    .set(Action{EA_NOP})
    .set(Color{color})
    .set(StateMachine{})
    .set(Team{1})
    .set(NumActions{1, 0})
    .set(MeleeDamage{0.f})
    .set(RangedDamage{10.f})
    .set(RangedAttackRange{5.f})
    .add<IsMob>()
    .set(MobType{MOB_ARCHER});
}

static flecs::entity create_healer(flecs::world &ecs, int x, int y, Color color)
{
  return ecs.entity()
    .set(Position{x, y})
    .set(MovePos{x, y})
    .set(PatrolPos{x, y})
    .set(Hitpoints{100.f})
    .set(Action{EA_NOP})
    .set(Color{color})
    .set(StateMachine{})
    .set(Team{0})
    .set(NumActions{1, 0})
    .set(MeleeDamage{20.f})
    .set(HealAmount{15.f})
    .set(HealRange{1.f})
    .set(HealCooldown{5, 5})
    .add<IsMob>()
    .set(MobType{MOB_HEALER});
}

static void create_player(flecs::world &ecs, int x, int y)
{
  ecs.entity("player")
    .set(Position{x, y})
    .set(MovePos{x, y})
    .set(Hitpoints{100.f})
    .set(GetColor(0xeeeeeeff))
    .set(Action{EA_NOP})
    .add<IsPlayer>()
    .set(Team{0})
    .set(PlayerInput{})
    .set(NumActions{2, 0})
    .set(MeleeDamage{50.f})
    .add<IsMob>();
}

static void create_heal(flecs::world &ecs, int x, int y, float amount)
{
  ecs.entity()
    .set(Position{x, y})
    .set(HealAmount{amount})
    .set(GetColor(0x44ff44ff))
    .add<IsPickup>();
}

static void create_powerup(flecs::world &ecs, int x, int y, float amount)
{
  ecs.entity()
    .set(Position{x, y})
    .set(PowerupAmount{amount})
    .set(Color{255, 255, 0, 255})
    .add<IsPickup>();
}

static void register_roguelike_systems(flecs::world &ecs)
{
  ecs.system<PlayerInput, Action, const IsPlayer>()
    .each([&](PlayerInput &inp, Action &a, const IsPlayer)
    {
      bool left = IsKeyDown(KEY_LEFT);
      bool right = IsKeyDown(KEY_RIGHT);
      bool up = IsKeyDown(KEY_UP);
      bool down = IsKeyDown(KEY_DOWN);
      if (left && !inp.left)
        a.action = EA_MOVE_LEFT;
      if (right && !inp.right)
        a.action = EA_MOVE_RIGHT;
      if (up && !inp.up)
        a.action = EA_MOVE_UP;
      if (down && !inp.down)
        a.action = EA_MOVE_DOWN;
      inp.left = left;
      inp.right = right;
      inp.up = up;
      inp.down = down;
    });
  ecs.system<const Position, const Color>()
    .without<TextureSource>(flecs::Wildcard)
    .each([&](const Position &pos, const Color color)
    {
      const Rectangle rect = {float(pos.x), float(pos.y), 1, 1};
      DrawRectangleRec(rect, color);
    });
  ecs.system<const Position, const Color>()
    .with<TextureSource>(flecs::Wildcard)
    .each([&](flecs::entity e, const Position &pos, const Color color)
    {
      const auto textureSrc = e.target<TextureSource>();
      DrawTextureQuad(*textureSrc.get<Texture2D>(),
          Vector2{1, 1}, Vector2{0, 0},
          Rectangle{float(pos.x), float(pos.y), 1, 1}, color);
    });
}


void init_roguelike(flecs::world &ecs)
{
  register_roguelike_systems(ecs);

  // add_patrol_attack_flee_sm(create_monster(ecs, 5, 5, GetColor(0xee00eeff)));
  // add_patrol_attack_flee_sm(create_monster(ecs, 10, -5, GetColor(0xee00eeff)));
  // add_patrol_flee_sm(create_monster(ecs, -5, -5, GetColor(0x111111ff)));
  // add_attack_sm(create_monster(ecs, -5, 5, GetColor(0x880000ff)));

  add_archer_sm(create_archer(ecs, -5, -10, GetColor(0xee00eeff)));
  add_healer_sm(create_healer(ecs, 3, 3, GetColor(0x00ee00ff)));
  add_slime_sm(create_slime(ecs, -3, -3, GetColor(0x0000eeff)), /*isSmall*/ false);

  create_player(ecs, 0, 0);

  create_powerup(ecs, 7, 7, 10.f);
  // create_powerup(ecs, 10, -6, 10.f);
  // create_powerup(ecs, 10, -4, 10.f);

  // create_heal(ecs, -5, -5, 50.f);
  create_heal(ecs, -5, 5, 50.f);
}

static bool is_player_acted(flecs::world &ecs)
{
  static auto processPlayer = ecs.query<const IsPlayer, const Action>();
  bool playerActed = false;
  processPlayer.each([&](const IsPlayer, const Action &a)
  {
    playerActed = a.action != EA_NOP;
  });
  return playerActed;
}

static bool upd_player_actions_count(flecs::world &ecs)
{
  static auto updPlayerActions = ecs.query<const IsPlayer, NumActions>();
  bool actionsReached = false;
  updPlayerActions.each([&](const IsPlayer, NumActions &na)
  {
    na.curActions = (na.curActions + 1) % na.numActions;
    actionsReached |= na.curActions == 0;
  });
  return actionsReached;
}

static Position move_pos(Position pos, int action)
{
  if (action == EA_MOVE_LEFT)
    pos.x--;
  else if (action == EA_MOVE_RIGHT)
    pos.x++;
  else if (action == EA_MOVE_UP)
    pos.y--;
  else if (action == EA_MOVE_DOWN)
    pos.y++;
  return pos;
}

static void process_actions(flecs::world &ecs)
{
  static auto processActions = ecs.query<Action, Position, MovePos, const MeleeDamage, const Team>();
  static auto checkAttacks = ecs.query<const MovePos, Hitpoints, const Team>();
  // Process all actions
  ecs.defer([&]
  {
    processActions.each([&](flecs::entity entity, Action &a, Position &pos, MovePos &mpos, const MeleeDamage &dmg, const Team &team)
    {
      Position nextPos = move_pos(pos, a.action);
      bool blocked = false;
      checkAttacks.each([&](flecs::entity enemy, const MovePos &epos, Hitpoints &hp, const Team &enemy_team)
      {
        if (entity != enemy && epos == nextPos)
        {
          blocked = true;
          if (team.team != enemy_team.team)
            hp.hitpoints -= dmg.damage;
        }
      });
      if (blocked)
        a.action = EA_NOP;
      else
        mpos = nextPos;
    });
    // now move
    processActions.each([&](flecs::entity, Action &, Position &pos, MovePos &mpos, const MeleeDamage &, const Team&)
    {
      pos = mpos;
    });
  });

  // healing
  static auto healerQuery = ecs.query<const Team, const Action, const HealAmount, const HealRange, HealCooldown, const Position>();
  static auto allyQuery = ecs.query<const Team, Hitpoints, const Position>();

  ecs.defer([&]
  {
    healerQuery.each([&](const Team healer_team, const Action action, const HealAmount heal_amount, const HealRange heal_range, HealCooldown &cd, const Position &healer_pos)
    {
      cd.current -= 1;

      if (cd.current > 0)
        return;

      if (action.action != EA_HEAL)
        return;

      allyQuery.each([&](const Team other_team, Hitpoints &other_hp, const Position &other_pos)
      {
        if (healer_team.team != other_team.team)
          return;

        if (dist(healer_pos, other_pos) > heal_range.range)
          return;

        other_hp.hitpoints += heal_amount.amount;
        cd.current = cd.cooldown;
      });
    });
  });

  // ranged attacks
  static auto enemiesQuery = ecs.query<const Position, const Team, Hitpoints>();
  static auto rangedAttacksQuery = ecs.query<const RangedDamage, const RangedAttackRange, const Action, const Team, const Position>();
  ecs.defer([&]
  {
    rangedAttacksQuery
      .each([&](const RangedDamage dmg, const RangedAttackRange range, const Action action, const Team team, const Position& pos)
      {
        if (action.action != EA_ATTACK)
          return;

        enemiesQuery.each([&](const Position& enemy_pos, const Team enemy_team, Hitpoints &enemy_hp)
        {
          if (team.team == enemy_team.team)
            return; // it is an ally

          if (dist(pos, enemy_pos) > range.range)
            return; // out of range

          enemy_hp.hitpoints -= dmg.damage;
        });
      });
  });

  // spawning slimes
  static auto slimesQuery = ecs.query<const MobType, const Action, const Position>();
  ecs.defer([&]
  {
    slimesQuery.each([&](const MobType mob_type, const Action action, const Position &pos)
    {
      if (mob_type.type != MOB_SLIME)
        return;

      if (action.action != EA_SPAWN)
        return;

      add_slime_sm(create_slime(ecs, pos.x - 1, pos.y - 1, GetColor(0x0000eeff)), /*isSmall*/ true);
    });
  });

  static auto deleteAllDead = ecs.query<const Hitpoints>();
  ecs.defer([&]
  {
    deleteAllDead.each([&](flecs::entity entity, const Hitpoints &hp)
    {
      if (hp.hitpoints <= 0.f)
        entity.destruct();
    });
  });

  static auto playerPickup = ecs.query<const IsPlayer, const Position, Hitpoints, MeleeDamage>();
  static auto healPickup = ecs.query<const IsPickup, const Position, const HealAmount>();
  static auto powerupPickup = ecs.query<const IsPickup, const Position, const PowerupAmount>();
  ecs.defer([&]
  {
    playerPickup.each([&](const IsPlayer&, const Position &pos, Hitpoints &hp, MeleeDamage &dmg)
    {
      healPickup.each([&](flecs::entity entity, const IsPickup, const Position &ppos, const HealAmount &amt)
      {
        if (pos == ppos)
        {
          hp.hitpoints += amt.amount;
          entity.destruct();
        }
      });
      powerupPickup.each([&](flecs::entity entity, const IsPickup, const Position &ppos, const PowerupAmount &amt)
      {
        if (pos == ppos)
        {
          dmg.damage += amt.amount;
          entity.destruct();
        }
      });
    });
  });

  // finish all actions
  ecs.defer([&]
  {
    processActions.each([&](flecs::entity, Action &a, Position &, MovePos &, const MeleeDamage &, const Team&)
    {
      a.action = EA_NOP;
    });
  });
}

void process_turn(flecs::world &ecs)
{
  static auto stateMachineAct = ecs.query<StateMachine>();
  if (is_player_acted(ecs))
  {
    if (upd_player_actions_count(ecs))
    {
      // Plan action for NPCs
      ecs.defer([&]
      {
        stateMachineAct.each([&](flecs::entity e, StateMachine &sm)
        {
          sm.act(0.f, ecs, e);
        });
      });
    }
    process_actions(ecs);
  }
}

void print_stats(flecs::world &ecs)
{
  static auto playerStatsQuery = ecs.query<const IsPlayer, const Hitpoints, const MeleeDamage>();
  playerStatsQuery.each([&](const IsPlayer &, const Hitpoints &hp, const MeleeDamage &dmg)
  {
    DrawText(TextFormat("hp: %d", int(hp.hitpoints)), 20, 20, 20, WHITE);
    DrawText(TextFormat("power: %d", int(dmg.damage)), 20, 40, 20, WHITE);
  });
}

