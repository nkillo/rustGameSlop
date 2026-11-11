#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace crypt {
namespace {

constexpr std::array<Cell, 4> Neighbors{{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}};

bool inside(Cell c) {
    return c.x >= 0 && c.y >= 0 && c.x < Width && c.y < Height;
}

int indexOf(Cell c) {
    return c.y * Width + c.x;
}

void advance(Point& pos, std::vector<Cell>& path, float budget) {
    while (!path.empty() && budget > 0) {
        Point next = center(path.back());
        float gap = distance(pos, next);
        if (gap <= budget) {
            pos = next;
            path.pop_back();
            budget -= gap;
        } else {
            pos.x += (next.x - pos.x) * budget / gap;
            pos.y += (next.y - pos.y) * budget / gap;
            return;
        }
    }
}

bool closeTo(Point point, Cell cell, float range) {
    return distance(point, center(cell)) <= range;
}

} // namespace

Cell cellAt(Point p) {
    return {static_cast<int>(std::floor(p.x + 0.5f)), static_cast<int>(std::floor(p.y + 0.5f))};
}

Point center(Cell c) {
    return {static_cast<float>(c.x), static_cast<float>(c.y)};
}

float distance(Point a, Point b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

Game::Game(unsigned seed) : rng(seed) {
    generateMap();
    restart();
}

void Game::generateMap() {
    map.fill(Tile::Void);
    auto fill = [&](int x1, int y1, int x2, int y2, Tile value) {
        for (int y = y1; y <= y2; ++y) {
            for (int x = x1; x <= x2; ++x) {
                map[indexOf({x, y})] = value;
            }
        }
    };

    fill(3, 15, 14, 26, Tile::Wall);
    fill(4, 16, 13, 25, Tile::Town);
    fill(4, 16, 5, 19, Tile::Building);
    fill(11, 16, 13, 18, Tile::Building);
    fill(5, 24, 7, 25, Tile::Building);
    fill(14, 20, 14, 22, Tile::Road);
    fill(15, 19, 22, 23, Tile::Grass);
    fill(15, 20, 22, 22, Tile::Road);
    fill(22, 17, 28, 23, Tile::Grass);
    fill(22, 19, 28, 21, Tile::Road);
    fill(27, 16, 33, 21, Tile::Grass);
    fill(27, 17, 33, 19, Tile::Road);
    fill(28, 19, 30, 21, Tile::Road);
    fill(34, 17, 37, 19, Tile::Stairs);

    fill(38, 15, 44, 22, Tile::Crypt);
    fill(45, 18, 46, 20, Tile::Crypt);
    fill(47, 17, 53, 24, Tile::Crypt);
    fill(48, 15, 50, 16, Tile::Crypt);
    fill(46, 8, 52, 14, Tile::Crypt);
    fill(44, 10, 45, 12, Tile::Crypt);
    fill(34, 6, 43, 12, Tile::Crypt);

    fill(31, 10, 32, 10, Tile::Crypt);
    fill(31, 11, 31, 15, Tile::Crypt);
    fill(RoadGate.x, RoadGate.y, RoadGate.x, RoadGate.y, Tile::Gate);
    fill(CryptGate.x, CryptGate.y, CryptGate.x, CryptGate.y, Tile::Gate);
}

void Game::restart() {
    if (runs > 0 && !victory) {
        return;
    }
    ++runs;
    player = center(Spawn);
    hp = 100;
    potions = std::max(potions, 3);
    kills = 0;
    swing = skill = hurt = playTime = planTimer = aiTimer = 0;
    victory = dead = false;
    playerPath.clear();
    enemyTarget = dropTarget = -1;
    enemies.clear();
    drops.clear();
    effects.clear();
    cues.clear();
    spawn(Kind::Raider, {23, 21});
    spawn(Kind::Raider, {26, 20});
    spawn(Kind::Raider, {29, 18});
    if (rng() % 2 == 0) {
        spawn(Kind::Raider, {27, 22});
    }
    spawn(Kind::Skeleton, {40, 17});
    spawn(Kind::Skeleton, {43, 20});
    spawn(Kind::Skeleton, {48, 19});
    spawn(Kind::Guard, {51, 22});
    spawn(rng() % 2 ? Kind::Skeleton : Kind::Guard, {52, 19});
    spawn(Kind::Guard, {48, 11});
    spawn(Kind::Skeleton, {50, 9});
    spawn(Kind::Skeleton, {51, 13});
    spawn(Kind::Warden, Boss);
    notice(L"Follow the road east. Slay the Warden beneath the ruins.");
}

void Game::spawn(Kind kind, Cell at) {
    constexpr int Health[]{22, 31, 46, 150};
    int health = Health[static_cast<int>(kind)];
    enemies.push_back({kind, center(at), center(at), health, health, 0, 0, 0, false, {}});
}

Tile Game::tile(Cell c) const {
    return inside(c) ? map[indexOf(c)] : Tile::Void;
}

bool Game::walkable(Cell c) const {
    switch (tile(c)) {
    case Tile::Void:
    case Tile::Wall:
    case Tile::Building:
        return false;
    case Tile::Gate:
        return victory;
    default:
        return true;
    }
}

std::vector<Cell> Game::route(Cell from, Cell to, bool allowTown) const {
    if (!walkable(from) || !walkable(to) || from == to) {
        return {};
    }
    std::array<int, Width * Height> parents;
    parents.fill(-1);
    std::array<Cell, Width * Height> queue;
    int head = 0;
    int tail = 0;
    queue[tail++] = from;
    parents[indexOf(from)] = indexOf(from);
    while (head < tail) {
        Cell current = queue[head++];
        for (Cell offset : Neighbors) {
            Cell next{current.x + offset.x, current.y + offset.y};
            if (!walkable(next) || (!allowTown && tile(next) == Tile::Town)) {
                continue;
            }
            int index = indexOf(next);
            if (parents[index] >= 0) {
                continue;
            }
            parents[index] = indexOf(current);
            if (next == to) {
                std::vector<Cell> path;
                while (index != indexOf(from)) {
                    path.push_back({index % Width, index / Width});
                    index = parents[index];
                }
                return path;
            }
            queue[tail++] = next;
        }
    }
    return {};
}

bool Game::inTown() const {
    return tile(cellAt(player)) == Tile::Town;
}

bool Game::byStash() const {
    return inTown() && closeTo(player, Chest, 2.3f);
}

void Game::walkTo(Cell to) {
    if (dead || !walkable(to)) {
        return;
    }
    enemyTarget = dropTarget = -1;
    playerPath = route(cellAt(player), to);
    if (playerPath.empty() && cellAt(player) == to) {
        playerPath.push_back(to);
    }
}

void Game::attack(int target) {
    if (dead || target < 0 || target >= static_cast<int>(enemies.size()) || enemies[target].hp <= 0) {
        return;
    }
    enemyTarget = target;
    dropTarget = -1;
    planTimer = 0;
    playerPath.clear();
}

void Game::cleave() {
    if (dead || skill > 0) {
        return;
    }
    skill = 3.2f;
    swing = std::max(swing, 0.32f);
    cues.push_back(Cue::Swing);
    for (Enemy& enemy : enemies) {
        if (enemy.hp > 0 && distance(player, enemy.pos) <= 1.65f &&
            (cellAt(player) == cellAt(enemy.pos) || !route(cellAt(player), cellAt(enemy.pos)).empty())) {
            strike(enemy, weapon.power * 2 + 3);
        }
    }
}

void Game::strike(Enemy& enemy, int damage) {
    enemy.hp = std::max(0, enemy.hp - damage);
    enemy.flash = 0.16f;
    enemy.awake = true;
    effects.push_back({enemy.pos, damage, 0.65f, false});
    cues.push_back(Cue::Hit);
    if (enemy.hp > 0) {
        return;
    }
    ++kills;
    if (enemy.kind == Kind::Warden) {
        victory = true;
        drops.push_back({enemy.pos, roll(2, weapon.power <= armor.power + 12 ? Slot::Weapon : Slot::Armor)});
        cues.push_back(Cue::Gate);
        notice(L"Warden slain. The return passage leads back to town.");
    } else if (kills == 1 || enemy.kind == Kind::Guard || rng() % 3 == 0) {
        int quality = enemy.kind == Kind::Guard ? 1 : 0;
        Slot slot = kills == 1 || rng() % 2 == 0 ? Slot::Weapon : Slot::Armor;
        drops.push_back({enemy.pos, roll(quality, slot)});
    }
    if (kills % 4 == 0) {
        potions = std::min(9, potions + 1);
    }
}

Item Game::roll(int quality, Slot slot) {
    constexpr const wchar_t* Weapons[]{L"Tempered blade", L"Crypt falchion", L"Warden's edge"};
    constexpr const wchar_t* Armors[]{L"Studded coat", L"Graveguard mail", L"Warden's plate"};
    int variation = static_cast<int>(rng() % 3);
    if (slot == Slot::Weapon) {
        return {slot, 14 + quality * 5 + variation, quality, Weapons[quality]};
    }
    return {slot, 3 + quality * 3 + variation, quality, Armors[quality]};
}

void Game::pickUp(int target) {
    if (dead || target < 0 || target >= static_cast<int>(drops.size())) {
        return;
    }
    dropTarget = target;
    enemyTarget = -1;
    Cell destination = cellAt(drops[target].pos);
    playerPath = route(cellAt(player), destination);
    if (cellAt(player) == destination) {
        playerPath.push_back(destination);
    }
}

void Game::collect(int index) {
    if (bag.size() >= Capacity) {
        notice(L"Pack full. Store gear in the town stash.");
        return;
    }
    notice(L"Picked up " + drops[index].item.name + L". Press I to equip.");
    bag.push_back(std::move(drops[index].item));
    drops.erase(drops.begin() + index);
    cues.push_back(Cue::Pickup);
}

void Game::usePotion() {
    if (dead || hp == 100) {
        return;
    }
    if (potions <= 0) {
        notice(L"No healing draughts left.");
        return;
    }
    --potions;
    hp = std::min(100, hp + 48);
    cues.push_back(Cue::Drink);
}

void Game::interact() {
    if (dead) {
        dead = false;
        player = center(Spawn);
        hp = 100;
        potions = std::max(3, potions);
        for (Enemy& enemy : enemies) {
            enemy.pos = enemy.home;
            enemy.path.clear();
            enemy.windup = 0;
            enemy.awake = false;
            if (enemy.hp > 0) {
                enemy.hp = enemy.maxHp;
            }
        }
        notice(L"You wake in town. Your gear is still with you.");
    } else if (inTown() && closeTo(player, Healer, 2.3f)) {
        hp = 100;
        potions = std::max(3, potions);
        cues.push_back(Cue::Drink);
        notice(L"The healer restored your health and replenished your draughts.");
    } else {
        int nearest = -1;
        float range = 1.7f;
        for (int i = 0; i < static_cast<int>(drops.size()); ++i) {
            float gap = distance(player, drops[i].pos);
            if (gap < range) {
                range = gap;
                nearest = i;
            }
        }
        if (nearest >= 0) {
            dropTarget = -1;
            collect(nearest);
        }
    }
}

void Game::equip(int index) {
    if (dead || index < 0 || index >= static_cast<int>(bag.size())) {
        return;
    }
    Item& item = bag[index];
    std::swap(item, item.slot == Slot::Weapon ? weapon : armor);
    cues.push_back(Cue::Pickup);
}

void Game::dropItem(int index) {
    if (dead || index < 0 || index >= static_cast<int>(bag.size())) {
        return;
    }
    notice(L"Dropped " + bag[index].name + L".");
    drops.push_back({player, std::move(bag[index])});
    bag.erase(bag.begin() + index);
}

void Game::store(int index) {
    if (dead || !byStash() || stash.size() >= Capacity || index < 0 || index >= static_cast<int>(bag.size())) {
        return;
    }
    stash.push_back(std::move(bag[index]));
    bag.erase(bag.begin() + index);
}

void Game::withdraw(int index) {
    if (dead || !byStash() || bag.size() >= Capacity || index < 0 || index >= static_cast<int>(stash.size())) {
        return;
    }
    bag.push_back(std::move(stash[index]));
    stash.erase(stash.begin() + index);
}

void Game::notice(std::wstring text) {
    message = std::move(text);
    noticeTime = 5.0f;
}

void Game::tick(float dt) {
    noticeTime = std::max(0.0f, noticeTime - dt);
    for (Effect& effect : effects) {
        effect.life -= dt;
    }
    std::erase_if(effects, [](const Effect& effect) { return effect.life <= 0; });
    if (dead) {
        return;
    }
    playTime += dt;
    swing = std::max(0.0f, swing - dt);
    skill = std::max(0.0f, skill - dt);
    hurt = std::max(0.0f, hurt - dt);
    planTimer -= dt;
    aiTimer -= dt;

    if (enemyTarget >= 0) {
        Enemy& enemy = enemies[enemyTarget];
        if (enemy.hp <= 0) {
            enemyTarget = -1;
            playerPath.clear();
        } else if (distance(player, enemy.pos) <= 1.16f &&
                   (cellAt(player) == cellAt(enemy.pos) || route(cellAt(player), cellAt(enemy.pos)).size() == 1)) {
            playerPath.clear();
            if (swing == 0) {
                swing = 0.47f;
                cues.push_back(Cue::Swing);
                strike(enemy, weapon.power);
            }
        } else if (planTimer <= 0) {
            playerPath = route(cellAt(player), cellAt(enemy.pos));
            planTimer = 0.14f;
        }
    }
    advance(player, playerPath, 2.05f * dt);
    if (dropTarget >= 0 && distance(player, drops[dropTarget].pos) <= 0.8f) {
        collect(dropTarget);
        dropTarget = -1;
        playerPath.clear();
    }

    for (Enemy& enemy : enemies) {
        enemy.flash = std::max(0.0f, enemy.flash - dt);
        if (enemy.hp <= 0) {
            continue;
        }
        int kind = static_cast<int>(enemy.kind);
        constexpr float Speed[]{1.35f, 1.25f, 1.10f, 1.18f};
        constexpr int Damage[]{8, 10, 15, 31};
        float reach = enemy.kind == Kind::Warden ? 1.55f : 1.12f;
        float gap = distance(player, enemy.pos);
        enemy.cooldown = std::max(0.0f, enemy.cooldown - dt);
        if (enemy.windup > 0) {
            enemy.windup -= dt;
            if (enemy.windup <= 0 && gap <= reach && !inTown()) {
                auto steps = route(cellAt(enemy.pos), cellAt(player), false);
                if (cellAt(enemy.pos) == cellAt(player) || (!steps.empty() && steps.size() <= 2)) {
                    int damage = std::max(2, Damage[kind] - armor.power);
                    hp = std::max(0, hp - damage);
                    hurt = 0.32f;
                    effects.push_back({player, damage, 0.65f, true});
                    cues.push_back(Cue::Hurt);
                    if (hp == 0) {
                        dead = true;
                        enemyTarget = dropTarget = -1;
                        playerPath.clear();
                        cues.push_back(Cue::Death);
                        break;
                    }
                }
            }
            continue;
        }
        if (aiTimer <= 0) {
            if (!inTown() && gap < 7.0f && distance(player, enemy.home) < 10.0f) {
                auto path = route(cellAt(enemy.pos), cellAt(player), false);
                enemy.awake = (!path.empty() && path.size() <= 8) || cellAt(enemy.pos) == cellAt(player);
                if (enemy.awake) {
                    enemy.path = std::move(path);
                }
            } else {
                enemy.awake = false;
            }
            if (!enemy.awake) {
                enemy.path = route(cellAt(enemy.pos), cellAt(enemy.home), false);
            }
        }
        if (enemy.awake && gap <= reach * 0.88f && !inTown()) {
            if (enemy.cooldown == 0) {
                enemy.windup = enemy.kind == Kind::Warden ? 0.78f : 0.30f;
                enemy.cooldown = enemy.kind == Kind::Warden ? 1.70f : 1.0f;
            }
        } else {
            advance(enemy.pos, enemy.path, Speed[kind] * dt);
        }
    }
    if (aiTimer <= 0) {
        aiTimer = 0.22f;
    }
}

} // namespace crypt




