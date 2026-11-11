#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>

#include "game.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <initializer_list>
#include <random>
#include <string>
#include <vector>

using namespace crypt;

namespace {

constexpr int TileX = 35, TileY = 18, Footer = 88, TitleWidth = 270;
constexpr COLORREF Ink = RGB(20, 20, 20), Pale = RGB(237, 222, 190);
constexpr COLORREF Muted = RGB(174, 161, 135), Gold = RGB(237, 188, 105);

COLORREF shade(COLORREF color, int delta) {
    auto adjust = [delta](int v) { return std::clamp(v + delta, 0, 255); };
    return RGB(adjust(GetRValue(color)), adjust(GetGValue(color)), adjust(GetBValue(color)));
}

struct Canvas {
    HDC dc;
    HFONT small, normal, title;

    void box(int x, int y, int w, int h, COLORREF color) const {
        RECT rect{x, y, x + w, y + h};
        SetDCBrushColor(dc, color);
        FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    }

    void poly(std::initializer_list<POINT> points, COLORREF fill, COLORREF edge) const {
        SetDCBrushColor(dc, fill);
        SetDCPenColor(dc, edge);
        Polygon(dc, points.begin(), static_cast<int>(points.size()));
    }

    void line(int x, int y, int x2, int y2, COLORREF color) const {
        SetDCPenColor(dc, color);
        MoveToEx(dc, x, y, nullptr);
        LineTo(dc, x2, y2);
    }

    void oval(int x, int y, int w, int h, COLORREF fill, COLORREF edge) const {
        SetDCBrushColor(dc, fill);
        SetDCPenColor(dc, edge);
        Ellipse(dc, x, y, x + w, y + h);
    }

    void text(int x, int y, const std::wstring& label, COLORREF color = Pale,
              HFONT font = nullptr, UINT align = TA_LEFT) const {
        SelectObject(dc, font ? font : normal);
        SetTextColor(dc, color);
        SetTextAlign(dc, align | TA_TOP);
        TextOutW(dc, x, y, label.c_str(), static_cast<int>(label.size()));
    }

    void panel(int x, int y, int w, int h) const {
        box(x, y, w, h, RGB(119, 97, 63));
        box(x + 1, y + 1, w - 2, h - 2, RGB(24, 24, 25));
    }
};

struct View {
    int w = 1120, h = 720;
    Point camera = center(Game::Spawn);

    POINT project(Point p) const {
        float x = p.x - camera.x, y = p.y - camera.y;
        return {w / 2 + std::lround((x - y) * TileX), (h - Footer) / 2 + std::lround((x + y) * TileY)};
    }

    Cell pick(int x, int y) const {
        float a = static_cast<float>(x - w / 2) / TileX;
        float b = static_cast<float>(y - (h - Footer) / 2) / TileY;
        return cellAt({camera.x + (a + b) / 2, camera.y + (b - a) / 2});
    }

    bool visible(POINT p) const {
        return p.x > -90 && p.x < w + 90 && p.y > -35 && p.y < h + 90;
    }
};

enum class Region { Town, Road, Stairs, Crypt, Boss, Passage };

Region area(const Game& game, Cell p) {
    if (game.tile(p) == Tile::Town) return Region::Town;
    if (p == Game::RoadGate || p == Game::CryptGate || (p.x <= 32 && p.y <= 15)) return Region::Passage;
    if (p.x < 34) return Region::Road;
    if (p.y <= 12 && p.x <= 43) return Region::Boss;
    if (game.tile(p) == Tile::Stairs) return Region::Stairs;
    return Region::Crypt;
}

constexpr const wchar_t* Areas[]{L"ASHWICK REFUGE", L"THE CINDER ROAD", L"RUINED STAIRWAY",
                                L"THE HOLLOW CRYPT", L"WARDEN'S CHAMBER", L"THE RETURN PASSAGE"};

bool caveTile(Tile tile) {
    return tile == Tile::Crypt || tile == Tile::Gate;
}

int grain(Cell p) {
    return (p.x * 917 + p.y * 613 + p.x * p.y * 109) & 255;
}

COLORREF ground(Tile tile, bool cave) {
    switch (tile) {
    case Tile::Town: return RGB(91, 83, 66);
    case Tile::Grass: return RGB(51, 62, 42);
    case Tile::Road: return RGB(101, 84, 59);
    case Tile::Stairs: return RGB(83, 78, 71);
    case Tile::Gate:
    case Tile::Crypt: return RGB(65, 69, 70);
    case Tile::Building:
    case Tile::Wall: return RGB(67, 61, 53);
    default: return cave ? RGB(26, 29, 31) : RGB(36, 45, 32);
    }
}

void floorArt(const Canvas& c, const Game& game, Cell at, POINT p, bool cave) {
    Tile tile = game.tile(at);
    int n = grain(at);
    int x = p.x, y = p.y;
    auto color = shade(ground(tile, cave), n % 15 - 7);
    c.poly({{x - TileX, y}, {x, y - TileY}, {x + TileX, y}, {x, y + TileY}}, color, shade(color, -6));
    if (tile == Tile::Crypt && at.x >= 36 && at.x <= 40 && at.y >= 7 && at.y <= 11) {
        int distanceFromSeal = std::abs(at.x - Game::Boss.x) + std::abs(at.y - Game::Boss.y);
        if (distanceFromSeal <= 2) {
            COLORREF seal = distanceFromSeal == 2 ? RGB(123, 89, 67) : RGB(93, 68, 59);
            c.poly({{x - TileX, y}, {x, y - TileY}, {x + TileX, y}, {x, y + TileY}}, seal, shade(seal, -22));
            if (distanceFromSeal == 1) {
                c.line(x - 11, y, x + 11, y, Gold);
                c.line(x, y - 5, x, y + 5, Gold);
            }
            return;
        }
    }
    if (caveTile(tile) || tile == Tile::Town) {
        c.line(x - 18, y - 9, x + 18, y + 9, shade(color, -17));
        c.line(x, y, x + 17, y - 9, shade(color, 10));
        c.line(x - 18, y + 9, x, y, shade(color, -13));
    } else if (tile == Tile::Stairs) {
        for (int i = -2; i <= 2; ++i) {
            c.line(x - 16 + i * 7, y + 8 + i * 3, x + 16 + i * 7, y - 8 + i * 3,
                   shade(color, i % 2 ? 25 : -19));
        }
    } else if (tile != Tile::Void || n % 3 == 0) {
        c.line(x - 13, y + n % 7, x - 8, y + n % 7 - 2, shade(color, 12));
        c.line(x + 7, y - n % 7, x + 13, y - n % 7 + 1, shade(color, -12));
    }
}

void stone(const Canvas& c, POINT p, int w, int h, COLORREF color) {
    int x = p.x, y = p.y, d = w / 2;
    c.poly({{x - w, y - h}, {x, y + d - h}, {x, y + d}, {x - w, y}}, color, shade(color, -20));
    c.poly({{x, y + d - h}, {x + w, y - h}, {x + w, y}, {x, y + d}},
           shade(color, -16), shade(color, -28));
    c.poly({{x - w, y - h}, {x, y - d - h}, {x + w, y - h}, {x, y + d - h}},
           shade(color, 19), shade(color, -11));
    for (int i = 9; i < h; i += 9) {
        c.line(x - w, y - i, x, y + d - i, shade(color, -16));
        c.line(x, y + d - i, x + w, y - i, shade(color, -23));
    }
}

void pine(const Canvas& c, POINT p, int n) {
    int height = 33 + n % 27;
    c.oval(p.x - 23, p.y - 8, 46, 15, RGB(26, 34, 25), RGB(26, 34, 25));
    c.box(p.x - 3, p.y - 24, 6, 24, RGB(70, 51, 35));
    for (int i = 0; i < 3; ++i) {
        int y = p.y - 9 - i * height / 5, w = height / 2 - i * 5, x = p.x;
        auto green = RGB(44 + 6 * i, 62 + 7 * i, 45 + 5 * i);
        c.poly({{x - w, y}, {x, y - height / 2}, {x + w, y}, {x, y + 7}}, green, shade(green, -21));
        c.poly({{x, y - height / 2}, {x + w, y}, {x, y + 7}}, shade(green, -12), shade(green, -12));
    }
}

void torch(const Canvas& c, POINT p, float time) {
    int x = p.x, y = p.y - 19;
    int flicker = static_cast<int>(std::sin(time * 12 + static_cast<float>(x)) * 4);
    c.oval(x - 12, y - 9, 24, 20, RGB(88, 63, 39), RGB(88, 63, 39));
    c.box(x - 2, y, 4, 20, RGB(105, 71, 41));
    c.poly({{x - 6, y + 2}, {x + flicker, y - 13}, {x + 6, y + 1}, {x, y + 6}},
           RGB(224, 126, 49), RGB(144, 75, 36));
    c.poly({{x - 3, y + 2}, {x + flicker / 2, y - 7}, {x + 3, y + 1}}, Gold, Gold);
}

void scenery(const Canvas& c, const Game& game, Cell at, POINT p, bool cave) {
    Tile tile = game.tile(at);
    int n = grain(at), x = p.x, y = p.y;
    if (tile == Tile::Wall) {
        stone(c, p, 34, 18, shade(RGB(98, 88, 72), n % 13 - 6));
    } else if (tile == Tile::Building) {
        stone(c, p, 34, 33, RGB(113, 94, 70));
        c.poly({{x - 35, y - 33}, {x, y - 64}, {x + 35, y - 33}, {x, y - 15}},
               RGB(87, 54, 47), RGB(45, 38, 34));
        c.line(x, y - 64, x, y - 15, RGB(120, 82, 64));
    } else if (tile == Tile::Gate) {
        if (game.victory) {
            torch(c, {x - 17, y}, game.playTime);
            torch(c, {x + 17, y}, game.playTime);
        } else {
            for (int post = -24; post <= 24; post += 8) {
                c.box(x + post, y - 35 + post / 4, 4, 37, RGB(135, 121, 94));
            }
            c.line(x - 24, y - 28, x + 26, y - 15, Gold);
            c.line(x - 24, y - 11, x + 26, y + 2, RGB(166, 145, 107));
        }
    } else if (tile == Tile::Void) {
        if (caveTile(game.tile({at.x + 1, at.y})) || caveTile(game.tile({at.x, at.y + 1}))) {
            stone(c, p, 34, 23, shade(RGB(76, 77, 79), n % 17 - 8));
        } else if (!cave && at.x >= 17 && at.x < 32 && at.y >= 23 && n % 5 < 2) {
            stone(c, p, 31, 24 + n % 27, shade(RGB(76, 76, 67), n % 11 - 5));
        } else if (!cave && at.y >= 12 && n % 4 == 0) {
            pine(c, p, n);
        }
    } else if (tile == Tile::Crypt && (at == Cell{39, 15} || at == Cell{41, 15} ||
               at == Cell{47, 23} || at == Cell{53, 21} || at == Cell{50, 8} || at == Cell{52, 8})) {
        stone(c, p, 15, 11, RGB(99, 96, 85));
        c.line(x - 6, y - 12, x + 6, y - 6, RGB(146, 132, 107));
        c.line(x - 2, y - 7, x + 2, y - 12, RGB(146, 132, 107));
    } else if ((tile == Tile::Crypt && (n % 19 == 0 || at == Cell{36, 8} || at == Cell{41, 8})) ||
               at == Cell{34, 17} || at == Cell{34, 19} || at == Cell{13, 20} || at == Cell{13, 22}) {
        torch(c, p, game.playTime);
    } else if (tile == Tile::Grass && n % 5 == 0) {
        stone(c, p, 9, 6, RGB(82, 84, 72));
    }
}

void figure(const Canvas& c, POINT p, int size, COLORREF body, COLORREF face, bool sword, bool attack) {
    int x = p.x, y = p.y;
    c.oval(x - size, y - 6, size * 2, 10, RGB(29, 28, 26), RGB(29, 28, 26));
    c.line(x - size / 3, y - size, x - size / 2, y, shade(body, -20));
    c.line(x + size / 3, y - size, x + size / 2, y, shade(body, -20));
    c.poly({{x - size, y - size * 2}, {x + size, y - size * 2}, {x + size / 2, y - 6},
            {x - size / 2, y - 6}}, body, shade(body, -32));
    c.poly({{x - size / 2, y - size * 2}, {x + size / 2, y - size * 2}, {x, y - size / 2}},
           shade(body, 21), body);
    c.oval(x - size / 2, y - size * 3, size, size + 3, face, shade(face, -34));
    if (sword) {
        c.line(x + size / 2, y - size, x + size + 10, y - size - (attack ? 17 : 7), RGB(221, 222, 208));
        c.line(x + size / 2, y - size + 1, x + size + 10, y - size - (attack ? 16 : 6), RGB(132, 142, 143));
    }
}

COLORREF rarity(const Item& item) {
    constexpr COLORREF Colors[]{Pale, RGB(133, 206, 229), Gold};
    return Colors[item.quality];
}

void foeArt(const Canvas& c, POINT p, const Enemy& enemy, bool selected) {
    int x = p.x, y = p.y;
    if (enemy.hp == 0) {
        c.oval(x - 13, y - 4, 26, 9, RGB(60, 39, 36), RGB(48, 33, 31));
        c.line(x - 9, y + 1, x + 9, y - 3, RGB(117, 109, 94));
        return;
    }
    constexpr COLORREF Bodies[]{RGB(141, 81, 57), RGB(146, 140, 119), RGB(89, 111, 105), RGB(118, 46, 58)};
    int size = enemy.kind == Kind::Warden ? 18 : enemy.kind == Kind::Guard ? 12 : 9;
    if (enemy.windup > 0 || selected) {
        c.oval(x - size - 7, y - 8, size * 2 + 14, 17, RGB(104, 59, 44),
               enemy.windup > 0 ? RGB(248, 108, 65) : Gold);
    }
    figure(c, p, size, enemy.flash > 0 ? RGB(225, 152, 129) : Bodies[static_cast<int>(enemy.kind)],
           enemy.kind == Kind::Raider ? RGB(157, 119, 80) : RGB(211, 199, 166), true, enemy.windup > 0);
    if (enemy.kind == Kind::Warden) {
        c.poly({{x - 8, y - 47}, {x - 14, y - 61}, {x - 1, y - 52}}, Gold, shade(Gold, -70));
        c.poly({{x + 8, y - 47}, {x + 14, y - 61}, {x + 1, y - 52}}, Gold, shade(Gold, -70));
    }
    if (enemy.hp < enemy.maxHp || selected || enemy.kind == Kind::Warden) {
        c.box(x - size, y - size * 3 - 13, size * 2, 5, RGB(47, 32, 30));
        c.box(x - size + 1, y - size * 3 - 12, (size * 2 - 2) * enemy.hp / enemy.maxHp, 3, RGB(192, 67, 53));
    }
}

enum class SpriteType { Scenery, Hero, Enemy, Drop, Healer, Chest };

struct Sprite {
    SpriteType type;
    Point position;
    int index;
};

void drawWorld(const Canvas& c, const Game& game, const View& view) {
    auto caveBackdrop = [](Cell at) {
        return (at.x >= 37 && at.y <= 26) || (at.x >= 30 && at.y <= 14);
    };
    c.box(0, 0, view.w, view.h, RGB(22, 26, 24));
    std::vector<Sprite> sprites;
    for (int y = -3; y <= Height + 2; ++y) {
        for (int x = -3; x <= Width + 2; ++x) {
            Point pos = center({x, y});
            POINT p = view.project(pos);
            if (view.visible(p)) {
                floorArt(c, game, {x, y}, p, caveBackdrop({x, y}));
                sprites.push_back({SpriteType::Scenery, pos, 0});
            }
        }
    }
    sprites.push_back({SpriteType::Hero, game.player, 0});
    sprites.push_back({SpriteType::Healer, center(Game::Healer), 0});
    sprites.push_back({SpriteType::Chest, center(Game::Chest), 0});
    for (int i = 0; i < static_cast<int>(game.enemies.size()); ++i) {
        sprites.push_back({SpriteType::Enemy, game.enemies[i].pos, i});
    }
    for (int i = 0; i < static_cast<int>(game.drops.size()); ++i) {
        sprites.push_back({SpriteType::Drop, game.drops[i].pos, i});
    }
    std::stable_sort(sprites.begin(), sprites.end(), [](const Sprite& a, const Sprite& b) {
        return a.position.x + a.position.y < b.position.x + b.position.y;
    });
    for (const Sprite& sprite : sprites) {
        POINT p = view.project(sprite.position);
        if (!view.visible(p)) continue;
        int x = p.x, y = p.y;
        switch (sprite.type) {
        case SpriteType::Scenery:
            scenery(c, game, cellAt(sprite.position), p, caveBackdrop(cellAt(sprite.position)));
            break;
        case SpriteType::Hero:
            if (game.dead) {
                c.oval(x - 15, y - 7, 30, 13, RGB(110, 60, 53), Ink);
            } else {
                figure(c, p, 12, game.hurt > 0.20f ? RGB(211, 92, 69) : RGB(78, 121, 148),
                       RGB(224, 185, 141), true, game.swing > 0);
                if (game.swing > 0.31f || game.skill > 3.03f) {
                    c.poly({{x + 8, y - 22}, {x + 34, y - 38}, {x + 40, y - 21},
                            {x + 24, y - 5}, {x + 32, y - 23}}, Pale, shade(Pale, -54));
                }
            }
            break;
        case SpriteType::Enemy:
            foeArt(c, p, game.enemies[sprite.index], sprite.index == game.target());
            break;
        case SpriteType::Drop: {
            auto color = rarity(game.drops[sprite.index].item);
            c.poly({{x - 7, y - 5}, {x, y - 12}, {x + 7, y - 5}, {x, y + 1}}, color, Ink);
            c.line(x, y - 12, x, y - 24, color);
            break;
        }
        case SpriteType::Healer:
            figure(c, p, 10, RGB(128, 115, 73), RGB(219, 198, 156), false, false);
            c.text(x, y - 49, L"Healer", Pale, c.small, TA_CENTER);
            break;
        case SpriteType::Chest:
            stone(c, p, 12, 13, RGB(151, 105, 60));
            c.box(x - 2, y - 10, 4, 6, Gold);
            c.text(x, y - 37, L"Stash", Pale, c.small, TA_CENTER);
            break;
        }
    }
    for (const Effect& effect : game.effects) {
        POINT p = view.project(effect.pos);
        c.text(p.x, p.y - 33 - static_cast<int>((0.65f - effect.life) * 40), std::to_wstring(effect.amount),
               effect.player ? RGB(254, 127, 96) : Pale, c.normal, TA_CENTER);
    }
}

class Audio {
public:
    bool muted = false;

    Audio() {
        WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, Rate, Rate * 2, 2, 16, 0};
        if (waveOutOpen(&device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            device = nullptr;
        }
    }

    ~Audio() {
        if (!device) return;
        waveOutReset(device);
        for (auto& buffer : buffers) {
            if (buffer.header.dwFlags & WHDR_PREPARED) {
                waveOutUnprepareHeader(device, &buffer.header, sizeof(WAVEHDR));
            }
        }
        waveOutClose(device);
    }

    void update(bool underground, const std::vector<Cue>& cues) {
        if (!device) return;
        if (!muted) {
            for (Cue cue : cues) voices.push_back({cue, 0});
        }
        for (auto& buffer : buffers) {
            if (buffer.header.dwFlags & WHDR_INQUEUE) continue;
            for (short& sample : buffer.samples) {
                time = (time + 1) % (Rate * 10);
                float t = static_cast<float>(time) / Rate;
                blend += ((underground ? 1.0f : 0.0f) - blend) * 0.00017f;
                noise = noise * 1664525u + 1013904223u;
                float hiss = static_cast<float>(noise >> 16) / 32768.0f - 1.0f;
                float sound = (1 - blend) * (145 * hiss * (1 + sine(0.4f * t)) + 125 * sine(77 * t)) +
                              blend * (90 * hiss + 220 * sine(41 * t) + 100 * sine(62 * t));
                for (auto& voice : voices) {
                    constexpr float Pitch[]{120, 82, 58, 520, 280, 165, 75};
                    constexpr float Sweep[]{-450, -95, -75, 800, 570, 80, -80};
                    constexpr float Length[]{0.10f, 0.14f, 0.18f, 0.24f, 0.26f, 0.62f, 0.52f};
                    int k = static_cast<int>(voice.cue);
                    float elapsed = static_cast<float>(voice.age++) / Rate;
                    float fade = std::max(0.0f, 1 - elapsed / Length[k]);
                    sound += 2900 * fade * fade *
                             (0.8f * sine(elapsed * (Pitch[k] + Sweep[k] * elapsed)) + 0.2f * hiss);
                }
                sample = muted ? 0 : static_cast<short>(std::clamp(sound, -30000.0f, 30000.0f));
            }
            std::erase_if(voices, [](const Voice& v) { return v.age >= Rate * 2 / 3; });
            if (!(buffer.header.dwFlags & WHDR_PREPARED)) {
                buffer.header.lpData = reinterpret_cast<char*>(buffer.samples.data());
                buffer.header.dwBufferLength = sizeof(short) * static_cast<DWORD>(buffer.samples.size());
                if (waveOutPrepareHeader(device, &buffer.header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) continue;
            }
            waveOutWrite(device, &buffer.header, sizeof(WAVEHDR));
        }
    }

private:
    static constexpr int Rate = 16000;

    struct Buffer {
        std::array<short, 480> samples{};
        WAVEHDR header{};
    };

    struct Voice {
        Cue cue;
        int age;
    };

    static float sine(float cycles) { return std::sin(cycles * 6.2831853f); }

    HWAVEOUT device = nullptr;
    std::array<Buffer, 4> buffers;
    std::vector<Voice> voices;
    float blend = 0;
    int time = 0;
    unsigned noise = 173;
};

constexpr int PackWidth = 290, PackTop = 110, PackHeight = 360, RowsTop = 222, RowHeight = 24;

std::wstring describe(const Item& item) {
    return item.name + L"  +" + std::to_wstring(item.power) +
           (item.slot == Slot::Weapon ? L" damage" : L" armor");
}

void drawHud(const Canvas& c, const Game& game, const View& view, bool pack, bool stash, bool muted) {
    int w = view.w, h = view.h, top = h - Footer;
    c.panel(17, 17, TitleWidth, 68);
    c.text(30, 24, L"THE HOLLOW CRYPT", Gold, c.title);
    c.text(31, 56, Areas[static_cast<int>(area(game, cellAt(game.player)))], Muted, c.small);

    c.panel(w - 168, 17, 151, 85);
    for (int y = 0; y < Height; ++y) {
        for (int x = 0; x < Width; ++x) {
            if (game.walkable({x, y})) {
                c.box(w - 161 + x * 2, 24 + y * 2, 2, 2, shade(ground(game.tile({x, y}), false), 26));
            }
        }
    }
    if (!game.victory) {
        c.box(w - 162 + Game::Boss.x * 2, 23 + Game::Boss.y * 2, 4, 4, RGB(199, 82, 65));
    }
    Cell pos = cellAt(game.player);
    c.box(w - 162 + pos.x * 2, 23 + pos.y * 2, 4, 4, Gold);
    c.text(w - 181, 19, L"Run " + std::to_wstring(game.runs), Pale, c.small, TA_RIGHT);
    c.text(w - 181, 40, std::to_wstring(game.kills) + L" slain", Muted, c.small, TA_RIGHT);

    c.panel(0, top, w, Footer);
    c.text(19, top + 9, L"LIFE", Gold, c.small);
    c.box(19, top + 33, 163, 17, RGB(64, 38, 36));
    c.box(20, top + 34, 161 * game.hp / 100, 15, RGB(164, 57, 47));
    c.text(100, top + 32, std::to_wstring(game.hp) + L" / 100", Pale, c.small, TA_CENTER);
    c.text(19, top + 59, L"Q   Draughts  " + std::to_wstring(game.potions), Pale, c.small);
    c.text(207, top + 9, L"Left click   Move / attack / loot", Pale, c.small);
    c.text(207, top + 33, game.skill <= 0 ? L"Right click   Cleave ready" :
           L"Right click   Cleave  " + std::to_wstring(static_cast<int>(std::ceil(game.skill))) + L"s",
           game.skill <= 0 ? Gold : Muted, c.small);
    c.text(207, top + 59, L"I   Pack      E   Interact      M   Sound", muted ? Muted : Pale, c.small);
    c.text(w - 19, top + 9, game.victory ? L"Return to the refuge" : L"Slay the Crypt Warden",
           Gold, c.normal, TA_RIGHT);
    c.text(w - 19, top + 35, game.victory && game.inTown() ? L"R   Begin another run" :
           game.victory ? L"The return passage is open" : L"Follow the road to the ruins", Muted, c.small, TA_RIGHT);
    if (game.byStash()) {
        c.text(w - 19, top + 59, L"E   Open stash", Pale, c.small, TA_RIGHT);
    } else if (game.inTown() && distance(game.player, center(Game::Healer)) <= 2.3f) {
        c.text(w - 19, top + 59, L"E   Visit healer", Pale, c.small, TA_RIGHT);
    }

    if (game.noticeTime > 0) {
        SIZE size{};
        SelectObject(c.dc, c.normal);
        GetTextExtentPoint32W(c.dc, game.message.c_str(), static_cast<int>(game.message.size()), &size);
        c.panel((w - size.cx) / 2 - 13, top - 42, size.cx + 26, 30);
        c.text(w / 2, top - 37, game.message, Pale, c.normal, TA_CENTER);
    }

    auto drawRows = [&](int x, const std::vector<Item>& items) {
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            int y = RowsTop + i * RowHeight;
            c.box(x + 9, y, PackWidth - 18, RowHeight - 2, RGB(39, 39, 39));
            c.text(x + 14, y + 1, describe(items[i]), rarity(items[i]), c.small);
        }
    };
    if (pack) {
        int x = w - PackWidth - 18;
        c.panel(x, PackTop, PackWidth, PackHeight);
        c.text(x + 14, 120, L"Pack  " + std::to_wstring(game.bag.size()) + L" / 10", Gold, c.title);
        c.text(x + 14, 153, describe(game.weapon), rarity(game.weapon), c.small);
        c.text(x + 14, 173, describe(game.armor), rarity(game.armor), c.small);
        c.text(x + 14, 199, stash ? L"Click equip  |  Right click store" : L"Click equip  |  Right click drop", Muted, c.small);
        drawRows(x, game.bag);
        if (stash) {
            x -= PackWidth + 14;
            c.panel(x, PackTop, PackWidth, PackHeight);
            c.text(x + 14, 120, L"Stash  " + std::to_wstring(game.stash.size()) + L" / 10", Gold, c.title);
            c.text(x + 14, 199, L"Click an item to take it", Muted, c.small);
            drawRows(x, game.stash);
        }
    }
    if (game.dead) {
        c.panel(w / 2 - 190, top / 2 - 53, 380, 105);
        c.text(w / 2, top / 2 - 36, L"YOU HAVE FALLEN", Gold, c.title, TA_CENTER);
        c.text(w / 2, top / 2 + 7, L"Press E to wake at Ashwick Refuge", Pale, c.normal, TA_CENTER);
    }
}

class App {
public:
    Game game{std::random_device{}()};
    View view;
    Audio audio;
    HFONT small = font(16, FW_NORMAL, L"Segoe UI");
    HFONT normal = font(19, FW_NORMAL, L"Georgia");
    HFONT title = font(25, FW_BOLD, L"Georgia");
    bool pack = false, stash = false;
    std::chrono::steady_clock::time_point frame = std::chrono::steady_clock::now();

    ~App() {
        DeleteObject(small);
        DeleteObject(normal);
        DeleteObject(title);
    }

    void tick() {
        auto now = std::chrono::steady_clock::now();
        float dt = std::min(0.05f, std::chrono::duration<float>(now - frame).count());
        frame = now;
        game.tick(dt);
        float easing = std::min(1.0f, dt * 10);
        view.camera.x += (game.player.x - view.camera.x) * easing;
        view.camera.y += (game.player.y - view.camera.y) * easing;
        if (!game.byStash() || game.dead) stash = false;
        audio.update(area(game, cellAt(game.player)) >= Region::Stairs, game.cues);
        game.cues.clear();
    }

    void draw(HDC dc) const {
        int saved = SaveDC(dc);
        SelectObject(dc, GetStockObject(DC_PEN));
        SelectObject(dc, GetStockObject(DC_BRUSH));
        SetBkMode(dc, TRANSPARENT);
        Canvas canvas{dc, small, normal, title};
        drawWorld(canvas, game, view);
        drawHud(canvas, game, view, pack, stash, audio.muted);
        RestoreDC(dc, saved);
    }

    void key(WPARAM key) {
        if (key == 'I' && !game.dead) {
            pack = !pack;
            stash = false;
        } else if (key == VK_ESCAPE) {
            pack = stash = false;
        } else if (key == 'Q') {
            game.usePotion();
        } else if (key == 'M') {
            audio.muted = !audio.muted;
            game.notice(audio.muted ? L"Sound muted." : L"Sound enabled.");
        } else if (key == 'E') {
            if (!game.dead && game.byStash()) {
                pack = stash = !stash;
            } else {
                bool respawn = game.dead;
                game.interact();
                if (respawn) {
                    view.camera = game.player;
                    pack = stash = false;
                }
            }
        } else if (key == 'R' && game.victory && game.inTown() && !game.dead) {
            game.restart();
            view.camera = game.player;
            pack = stash = false;
        }
    }

    void click(int x, int y, bool right) {
        if (game.dead || y >= view.h - Footer) return;
        if ((x < 17 + TitleWidth && y < 85) || (x >= view.w - 168 && y <= 102)) return;
        int bagX = view.w - PackWidth - 18;
        auto inPanel = [&](int panelX) {
            return x >= panelX && x < panelX + PackWidth && y >= PackTop && y < PackTop + PackHeight;
        };
        if (pack && inPanel(bagX)) {
            if (y >= RowsTop) {
                int row = (y - RowsTop) / RowHeight;
                if (right && stash) game.store(row);
                if (!right) game.equip(row);
            }
            return;
        }
        if (stash && inPanel(bagX - PackWidth - 14)) {
            if (!right && y >= RowsTop) game.withdraw((y - RowsTop) / RowHeight);
            return;
        }
        if (right) {
            game.cleave();
            return;
        }
        int selected = -1;
        float depth = -1;
        for (int i = 0; i < static_cast<int>(game.enemies.size()); ++i) {
            const Enemy& enemy = game.enemies[i];
            if (enemy.hp <= 0) continue;
            POINT p = view.project(enemy.pos);
            int size = enemy.kind == Kind::Warden ? 23 : 14;
            float enemyDepth = enemy.pos.x + enemy.pos.y;
            if (std::abs(x - p.x) <= size && y >= p.y - size * 3 && y <= p.y + 7 && enemyDepth > depth) {
                selected = i;
                depth = enemyDepth;
            }
        }
        if (selected >= 0) {
            game.attack(selected);
            return;
        }
        for (int i = 0; i < static_cast<int>(game.drops.size()); ++i) {
            POINT p = view.project(game.drops[i].pos);
            if (std::abs(x - p.x) <= 13 && y >= p.y - 24 && y <= p.y + 7) {
                game.pickUp(i);
                return;
            }
        }
        for (Cell service : {Game::Healer, Game::Chest}) {
            POINT p = view.project(center(service));
            if (std::abs(x - p.x) <= 15 && y >= p.y - 33 && y <= p.y + 7) {
                if (distance(game.player, center(service)) <= 2.3f) {
                    if (service == Game::Chest) {
                        key('E');
                    } else {
                        game.interact();
                    }
                } else {
                    game.walkTo(service);
                }
                return;
            }
        }
        game.walkTo(view.pick(x, y));
    }

private:
    static HFONT font(int size, int weight, const wchar_t* face) {
        return CreateFontW(size, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                           CLEARTYPE_QUALITY, 0, face);
    }
};

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        auto* params = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(params->lpCreateParams));
    }
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = {850, 630};
        return 0;
    case WM_SIZE:
        if (app && wParam != SIZE_MINIMIZED) {
            app->view.w = LOWORD(lParam);
            app->view.h = HIWORD(lParam);
        }
        return 0;
    case WM_TIMER:
        if (app) {
            app->tick();
            if (!IsIconic(window)) InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        if (app && app->view.w > 0 && app->view.h > 0) {
            HDC buffer = CreateCompatibleDC(dc);
            HBITMAP bitmap = CreateCompatibleBitmap(dc, app->view.w, app->view.h);
            if (buffer && bitmap) {
                HGDIOBJ old = SelectObject(buffer, bitmap);
                app->draw(buffer);
                BitBlt(dc, 0, 0, app->view.w, app->view.h, buffer, 0, 0, SRCCOPY);
                SelectObject(buffer, old);
            } else {
                app->draw(dc);
            }
            if (bitmap) DeleteObject(bitmap);
            if (buffer) DeleteDC(buffer);
        }
        EndPaint(window, &paint);
        return 0;
    }
    case WM_KEYDOWN:
        if (app && !(lParam & (1L << 30))) app->key(wParam);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (app) app->click(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), message == WM_RBUTTONDOWN);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        KillTimer(window, 1);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDPIAware();
    App app;
    WNDCLASSW wc{};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"HollowCryptWindow";
    if (!RegisterClassW(&wc)) return 1;
    RECT size{0, 0, app.view.w, app.view.h};
    AdjustWindowRect(&size, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowW(wc.lpszClassName, L"The Hollow Crypt", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, size.right - size.left, size.bottom - size.top,
                               nullptr, nullptr, instance, &app);
    if (!window) return 1;
    if (!SetTimer(window, 1, 16, nullptr)) {
        DestroyWindow(window);
        return 1;
    }
    ShowWindow(window, show);
    UpdateWindow(window);

    MSG message{};
    int result;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return result < 0 ? 1 : static_cast<int>(message.wParam);
}




