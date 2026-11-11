#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace crypt {

constexpr int Width = 67;
constexpr int Height = 34;

struct Cell {
    int x = 0;
    int y = 0;
    bool operator==(const Cell&) const = default;
};

struct Point {
    float x = 0;
    float y = 0;
};

enum class Tile { Void, Grass, Town, Road, Stairs, Crypt, Gate, Building, Wall };
enum class Kind { Raider, Skeleton, Guard, Warden };
enum class Slot { Weapon, Armor };
enum class Cue { Swing, Hit, Hurt, Pickup, Drink, Gate, Death };

struct Item {
    Slot slot;
    int power;
    int quality;
    std::wstring name;
};

struct Enemy {
    Kind kind;
    Point pos;
    Point home;
    int hp;
    int maxHp;
    float cooldown = 0;
    float windup = 0;
    float flash = 0;
    bool awake = false;
    std::vector<Cell> path;
};

struct Drop {
    Point pos;
    Item item;
};

struct Effect {
    Point pos;
    int amount;
    float life;
    bool player;
};

Cell cellAt(Point p);
Point center(Cell c);
float distance(Point a, Point b);

class Game {
public:
    explicit Game(unsigned seed);
    void restart();
    void tick(float dt);
    void walkTo(Cell target);
    void attack(int target);
    void cleave();
    void pickUp(int target);
    void usePotion();
    void interact();
    void equip(int index);
    void dropItem(int index);
    void store(int index);
    void withdraw(int index);
    bool inTown() const;
    bool byStash() const;
    bool walkable(Cell c) const;
    Tile tile(Cell c) const;
    std::vector<Cell> route(Cell from, Cell to, bool allowTown = true) const;
    void notice(std::wstring text);

    static constexpr Cell Spawn{8, 21};
    static constexpr Cell Healer{6, 18};
    static constexpr Cell Chest{9, 18};
    static constexpr Cell Boss{38, 9};
    static constexpr Cell RoadGate{31, 16};
    static constexpr Cell CryptGate{33, 10};
    static constexpr int Capacity = 10;

    Point player;
    int hp = 100;
    int potions = 3;
    int kills = 0;
    int runs = 0;
    float swing = 0;
    float skill = 0;
    float hurt = 0;
    float noticeTime = 0;
    float playTime = 0;
    bool victory = false;
    bool dead = false;
    Item weapon{Slot::Weapon, 11, 0, L"Iron sword"};
    Item armor{Slot::Armor, 1, 0, L"Worn coat"};
    std::vector<Item> bag;
    std::vector<Item> stash;
    std::vector<Enemy> enemies;
    std::vector<Drop> drops;
    std::vector<Effect> effects;
    std::vector<Cue> cues;
    std::wstring message;
    int target() const { return enemyTarget; }

private:
    std::array<Tile, Width * Height> map{};
    std::mt19937 rng;
    std::vector<Cell> playerPath;
    int enemyTarget = -1;
    int dropTarget = -1;
    float planTimer = 0;
    float aiTimer = 0;
    void generateMap();
    void spawn(Kind kind, Cell at);
    void strike(Enemy& enemy, int damage);
    void collect(int index);
    Item roll(int quality, Slot slot);
};

} // namespace crypt



