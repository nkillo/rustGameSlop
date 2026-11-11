#include "game.hpp"

#include <cstdlib>
#include <iostream>

using namespace crypt;

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

constexpr float Step = 1.0f / 60;

void walk(Game& game, Cell target) {
    game.walkTo(target);
    for (int i = 0; i < 3600 && distance(game.player, center(target)) > 0.02f && !game.dead; ++i) {
        game.tick(Step);
        if (game.hp < 52) game.usePotion();
    }
    check(!game.dead && distance(game.player, center(target)) <= 0.02f, "Could not reach destination");
}

void gather(Game& game) {
    while (!game.drops.empty() && game.bag.size() < Game::Capacity) {
        auto before = game.drops.size();
        game.pickUp(0);
        for (int i = 0; i < 900 && game.drops.size() == before && !game.dead; ++i) {
            game.tick(Step);
            if (game.hp < 52) game.usePotion();
        }
        if (game.drops.size() == before) std::cerr << "Loot: hp " << game.hp << " kills " << game.kills << " player " << game.player.x << "," << game.player.y << " target " << game.drops[0].pos.x << "," << game.drops[0].pos.y << "\n";
        check(game.drops.size() < before && !game.dead, "Could not collect loot");
        const Item& item = game.bag.back();
        if (item.power > (item.slot == Slot::Weapon ? game.weapon.power : game.armor.power)) {
            game.equip(static_cast<int>(game.bag.size()) - 1);
        }
    }
}

void completeRun(unsigned seed) {
    Game game(seed);
    check(!game.walkable(Game::RoadGate) && !game.walkable(Game::CryptGate), "Shortcut started open");
    check(!game.walkable({0, 0}) && !game.walkable({3, 21}), "Boundary admitted movement");

    auto routeIn = game.route(Game::Spawn, Game::Boss);
    check(routeIn.size() > 40 && routeIn.front() == Game::Boss, "No main route to Warden");
    for (Cell waypoint : {Cell{34, 18}, Cell{45, 19}, Cell{49, 16}, Cell{45, 11}}) {
        bool passed = false;
        for (Cell cell : routeIn) {
            if (distance(center(cell), center(waypoint)) <= 1) passed = true;
        }
        check(passed, "Main route bypassed a level section");
    }

    game.restart();
    check(game.runs == 1, "Unfinished run could reset");
    for (int index = 0; index < static_cast<int>(game.enemies.size()); ++index) {
        if (game.enemies[index].hp == 0) continue;
        game.attack(index);
        for (int frame = 0; frame < 2400 && game.enemies[index].hp > 0 && !game.dead; ++frame) {
            if (game.skill <= 0 && distance(game.player, game.enemies[index].pos) <= 1.6f) game.cleave();
            if (game.hp < 52) game.usePotion();
            game.tick(Step);
        }
        check(!game.dead && game.enemies[index].hp == 0, "Combat run stalled or died");
        gather(game);
    }

    check(game.victory && game.kills >= 11, "Warden did not complete the run");
    check(game.walkable(Game::RoadGate) && game.walkable(Game::CryptGate), "Shortcut did not unlock");
    auto routeOut = game.route(Game::Boss, Game::Spawn);
    check(routeOut.size() + 15 < routeIn.size(), "Return passage is not materially shorter");
    check(game.weapon.power > 11 && game.armor.power > 1, "Equipment did not improve the hero");
    check(game.weapon.quality == 2 || game.armor.quality == 2, "Warden did not award rare gear");
    walk(game, Game::Spawn);
    check(game.inTown(), "Return route did not reach town");
    walk(game, Game::Healer);
    game.interact();
    check(game.hp == 100 && game.potions >= 3, "Healer did not restore supplies");
    walk(game, Game::Chest);
    auto bagSize = game.bag.size();
    game.store(0);
    check(game.stash.size() == 1 && game.bag.size() + 1 == bagSize, "Stash did not accept item");
    game.withdraw(0);
    check(game.stash.empty() && game.bag.size() == bagSize, "Stash did not return item");
    game.store(0);

    int damage = game.weapon.power;
    game.restart();
    check(game.runs == 2 && !game.victory && game.kills == 0 && game.drops.empty(), "Run did not reset");
    check(game.weapon.power == damage && game.stash.size() == 1 && game.bag.size() + 1 == bagSize,
          "Gear did not carry over");
    check(!game.walkable(Game::RoadGate) && !game.walkable(Game::CryptGate), "Shortcut did not reset");
}

void deathAndRespawn() {
    Game game(23);
    game.player = game.enemies.front().pos;
    game.hp = 1;
    for (int i = 0; i < 180 && !game.dead; ++i) game.tick(Step);
    check(game.dead, "An enemy did not finish its attack");
    game.interact();
    check(!game.dead && game.hp == 100 && cellAt(game.player) == Game::Spawn, "Respawn failed");
}

} // namespace

int main() {
    for (unsigned seed = 1; seed <= 24; ++seed) completeRun(seed);
    deathAndRespawn();
    std::cout << "Passed 24 complete runs and a death/respawn scenario.\n";
}

