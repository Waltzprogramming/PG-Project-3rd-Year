#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr int PreviewWidth = 820;
constexpr int PreviewHeight = 520;
constexpr int WorldCount = 4;
constexpr int PauseResume = 10;
constexpr int PauseExit = 11;

const Color Ink{10, 12, 16, 255};
const Color Paper{247, 249, 246, 255};
const Color PaperBlue{25, 91, 176, 255};
const Color DeepBlue{10, 43, 92, 255};
const Color SignalRed{220, 39, 52, 255};
const Color DeepRed{119, 14, 27, 255};
const Color PaperGreen{35, 151, 91, 255};
const Color DeepGreen{13, 78, 48, 255};
const Color Muted{158, 168, 176, 255};

enum class MenuScreen {
    Home,
    HowToPlay,
    Credits,
    Worlds,
    Pause
};

struct WorldEntry {
    const char* title;
    const char* subtitle;
    const char* modelPath;
    Color accent;
    bool available;
    Vector3 previewPosition;
    float previewScale;
    Vector3 previewRotation{0.0f, 0.0f, 0.0f};
    bool nightPreview{false};
    const char* previewCaption{"ROTATION Y // LOCKED TO PLANE"};
};

struct AnimatedButton {
    Rectangle bounds{};
    float hover{0.0f};
    bool hovered{false};
};

struct PreviewPart {
    Model model{};
    Texture2D texture{};
    bool ownsTexture{false};
};

struct PreviewModel {
    std::vector<PreviewPart> parts;
    bool loaded{false};
    bool loadAttempted{false};
    int texturedMaterials{0};
    Vector3 minBounds{0.0f, 0.0f, 0.0f};
    Vector3 maxBounds{0.0f, 0.0f, 0.0f};
    float minY{0.0f};
    float maxY{0.0f};
    float autoScale{1.0f};
    Vector3 center{0.0f, 0.0f, 0.0f};
};

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float lerpValue(float start, float end, float amount) {
    return start + (end - start)*amount;
}

float easeOutBack(float value) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    const float t = value - 1.0f;
    return 1.0f + c3*t*t*t + c1*t*t;
}

float easeOutCubic(float value) {
    const float t = 1.0f - clamp01(value);
    return 1.0f - t*t*t;
}

Vector3 rotatePreviewVector(Vector3 value, Vector3 rotationDegrees) {
    const auto rotateX = [](Vector3 vector, float degrees) {
        const float radians = degrees*DEG2RAD;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        return Vector3{
            vector.x,
            vector.y*cosine - vector.z*sine,
            vector.y*sine + vector.z*cosine
        };
    };
    const auto rotateY = [](Vector3 vector, float degrees) {
        const float radians = degrees*DEG2RAD;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        return Vector3{
            vector.x*cosine + vector.z*sine,
            vector.y,
            -vector.x*sine + vector.z*cosine
        };
    };
    const auto rotateZ = [](Vector3 vector, float degrees) {
        const float radians = degrees*DEG2RAD;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        return Vector3{
            vector.x*cosine - vector.y*sine,
            vector.x*sine + vector.y*cosine,
            vector.z
        };
    };

    value = rotateX(value, rotationDegrees.x);
    value = rotateY(value, rotationDegrees.y);
    return rotateZ(value, rotationDegrees.z);
}

BoundingBox rotatedPreviewBounds(const PreviewModel& model, Vector3 rotationDegrees) {
    const std::array<Vector3, 8> corners{{
        {model.minBounds.x, model.minBounds.y, model.minBounds.z},
        {model.minBounds.x, model.minBounds.y, model.maxBounds.z},
        {model.minBounds.x, model.maxBounds.y, model.minBounds.z},
        {model.minBounds.x, model.maxBounds.y, model.maxBounds.z},
        {model.maxBounds.x, model.minBounds.y, model.minBounds.z},
        {model.maxBounds.x, model.minBounds.y, model.maxBounds.z},
        {model.maxBounds.x, model.maxBounds.y, model.minBounds.z},
        {model.maxBounds.x, model.maxBounds.y, model.maxBounds.z}
    }};

    BoundingBox bounds{
        {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()},
        {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()}
    };
    for (Vector3 corner : corners) {
        const Vector3 rotated = rotatePreviewVector(corner, rotationDegrees);
        bounds.min.x = std::min(bounds.min.x, rotated.x);
        bounds.min.y = std::min(bounds.min.y, rotated.y);
        bounds.min.z = std::min(bounds.min.z, rotated.z);
        bounds.max.x = std::max(bounds.max.x, rotated.x);
        bounds.max.y = std::max(bounds.max.y, rotated.y);
        bounds.max.z = std::max(bounds.max.z, rotated.z);
    }
    return bounds;
}

Color withAlpha(Color color, float alpha) {
    color.a = static_cast<unsigned char>(255.0f*clamp01(alpha));
    return color;
}

void drawQuad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color color) {
    DrawTriangle(a, c, b, color);
    DrawTriangle(a, d, c, color);
}

void drawQuadOutline(Vector2 a, Vector2 b, Vector2 c, Vector2 d, float thickness, Color color) {
    DrawLineEx(a, b, thickness, color);
    DrawLineEx(b, c, thickness, color);
    DrawLineEx(c, d, thickness, color);
    DrawLineEx(d, a, thickness, color);
}

void drawPaperStrip(Rectangle bounds, float skew, Color fill, Color outline = Ink) {
    const Vector2 a{bounds.x + skew, bounds.y};
    const Vector2 b{bounds.x + bounds.width, bounds.y + 2.0f};
    const Vector2 c{bounds.x + bounds.width - skew*0.55f, bounds.y + bounds.height};
    const Vector2 d{bounds.x, bounds.y + bounds.height - 3.0f};
    drawQuad(a, b, c, d, fill);
    drawQuadOutline(a, b, c, d, 4.0f, outline);
}

void drawPaperBurst(Vector2 center, float radius, float rotation, Color color) {
    constexpr int points = 14;
    std::array<Vector2, points> vertices{};
    for (int index = 0; index < points; ++index) {
        const float angle = rotation + index*(2.0f*PI/points);
        const float pointRadius = index%2 == 0 ? radius : radius*0.48f;
        vertices[index] = {
            center.x + std::cos(angle)*pointRadius,
            center.y + std::sin(angle)*pointRadius
        };
    }
    for (int index = 1; index < points - 1; ++index) {
        DrawTriangle(center, vertices[index + 1], vertices[index], color);
    }
    DrawTriangle(center, vertices[0], vertices[points - 1], color);
}

void drawDynamicBackground(float timeSeconds, int width, int height) {
    ClearBackground(Paper);

    const float drift = std::fmod(timeSeconds*72.0f, 260.0f);
    for (int index = -3; index < 11; ++index) {
        const float x = index*225.0f + drift - 300.0f;
        DrawLineEx({x, 0.0f}, {x - 280.0f, static_cast<float>(height)}, 12.0f, {205, 209, 214, 255});
    }

    // Large offset sheets create the cardboard-layered silhouette.
    drawQuad(
        {width*0.46f + 18.0f, -38.0f},
        {width + 70.0f, -28.0f},
        {width + 70.0f, height*0.78f + 18.0f},
        {width*0.67f + 18.0f, height + 70.0f},
        withAlpha(Ink, 0.78f));
    drawQuad(
        {width*0.46f, -52.0f},
        {width + 60.0f, -42.0f},
        {width + 60.0f, height*0.78f},
        {width*0.67f, height + 60.0f},
        PaperBlue);
    drawQuad(
        {width*0.71f, -20.0f},
        {width*0.79f, -18.0f},
        {width*0.52f, height + 35.0f},
        {width*0.43f, height + 35.0f},
        Paper);
    drawQuad(
        {width*0.78f, -30.0f},
        {width*0.88f, -24.0f},
        {width*0.62f, height + 42.0f},
        {width*0.52f, height + 42.0f},
        SignalRed);
    drawQuad(
        {-80.0f, height*0.87f},
        {width*0.60f, height*0.81f},
        {width*0.67f, height + 42.0f},
        {-80.0f, height + 42.0f},
        PaperGreen);

    drawPaperBurst({width - 86.0f, height - 92.0f}, 146.0f, timeSeconds*0.07f, DeepBlue);
    drawPaperBurst({width*0.54f, height + 38.0f}, 88.0f, -timeSeconds*0.10f, DeepGreen);

    for (int index = 0; index < 13; ++index) {
        const float y = std::fmod(index*67.0f + timeSeconds*31.0f, height + 90.0f) - 45.0f;
        const float length = 32.0f + static_cast<float>((index*31)%82);
        const Color scrap = index%3 == 0 ? SignalRed : (index%3 == 1 ? PaperGreen : Paper);
        drawPaperStrip(
            {width - length - 24.0f, y, length, 8.0f},
            5.0f,
            withAlpha(scrap, 0.82f),
            withAlpha(Ink, 0.78f));
    }
}

void drawBrand(float timeSeconds, float entry, int width, const char* eyebrow) {
    const float slide = (1.0f - easeOutBack(entry))*-520.0f;
    const float jitter = std::sin(timeSeconds*14.0f)*1.4f;
    const Vector2 titlePosition{62.0f + slide + jitter, 38.0f};

    drawPaperStrip({titlePosition.x - 19.0f, titlePosition.y + 10.0f, 322.0f, 69.0f}, 24.0f, withAlpha(Ink, 0.86f));
    drawPaperStrip({titlePosition.x - 27.0f, titlePosition.y, 322.0f, 67.0f}, 24.0f, PaperBlue, Paper);
    drawPaperStrip({titlePosition.x - 4.0f, titlePosition.y + 65.0f, 350.0f, 80.0f}, 20.0f, withAlpha(Ink, 0.88f));
    drawPaperStrip({titlePosition.x - 13.0f, titlePosition.y + 55.0f, 350.0f, 79.0f}, 20.0f, SignalRed, Paper);
    DrawTextEx(GetFontDefault(), "PAPER", titlePosition, 64.0f, 3.0f, Paper);
    DrawTextEx(GetFontDefault(), "PINIX", {titlePosition.x + 15.0f, titlePosition.y + 58.0f}, 78.0f, 2.0f, Paper);
    drawPaperStrip({titlePosition.x - 7.0f, 184.0f, 356.0f, 11.0f}, 7.0f, PaperGreen, Ink);
    DrawTextEx(GetFontDefault(), eyebrow, {titlePosition.x + 4.0f, 207.0f}, 19.0f, 2.0f, Ink);

    const float badgeX = width - 222.0f;
    drawPaperStrip({badgeX + 9.0f, 34.0f, 177.0f, 60.0f}, 16.0f, withAlpha(Ink, 0.78f));
    drawPaperStrip({badgeX, 24.0f, 177.0f, 60.0f}, 16.0f, PaperGreen, Paper);
    DrawTextEx(GetFontDefault(), "02", {width - 116.0f, 31.0f}, 43.0f, 1.0f, Paper);
}

AnimatedButton drawSkewButton(
    Rectangle bounds,
    const char* label,
    const char* tag,
    float hoverValue,
    bool enabled,
    float timeSeconds,
    Color accent = SignalRed
) {
    AnimatedButton result;
    result.hovered = enabled && CheckCollisionPointRec(GetMousePosition(), bounds);
    const float target = result.hovered ? 1.0f : 0.0f;
    result.hover = lerpValue(hoverValue, target, clamp01(GetFrameTime()*17.0f));

    const float motion = result.hover*22.0f + std::sin(timeSeconds*5.5f + bounds.y*0.01f)*1.7f;
    const float lift = result.hover*3.0f;
    const float skew = 27.0f + result.hover*8.0f;
    const Rectangle moved{bounds.x + motion, bounds.y - lift, bounds.width + result.hover*12.0f, bounds.height + result.hover*4.0f};
    const Vector2 a{moved.x + skew, moved.y};
    const Vector2 b{moved.x + moved.width, moved.y};
    const Vector2 c{moved.x + moved.width - skew, moved.y + moved.height};
    const Vector2 d{moved.x, moved.y + moved.height};

    drawQuad(
        {a.x + 11.0f, a.y + 12.0f},
        {b.x + 11.0f, b.y + 12.0f},
        {c.x + 11.0f, c.y + 12.0f},
        {d.x + 11.0f, d.y + 12.0f},
        withAlpha(Ink, 0.88f));

    const bool highlighted = enabled && (result.hovered || result.hover > 0.45f);
    const Color fill = !enabled ? Color{43, 47, 53, 245} : accent;
    const Vector2 underA{a.x - 7.0f, a.y + 5.0f};
    const Vector2 underB{b.x + 7.0f, b.y + 4.0f};
    const Vector2 underC{c.x + 4.0f, c.y + 7.0f};
    const Vector2 underD{d.x - 9.0f, d.y + 5.0f};
    drawQuad(underA, underB, underC, underD, highlighted ? Paper : DeepBlue);
    drawQuadOutline(underA, underB, underC, underD, 4.0f, Ink);
    drawQuad(a, b, c, d, fill);
    drawQuadOutline(a, b, c, d, highlighted ? 5.0f : 4.0f, enabled ? Ink : Muted);
    DrawLineEx({a.x + 13.0f, a.y + 8.0f}, {b.x - 14.0f, b.y + 8.0f}, 2.0f, withAlpha(Paper, 0.78f));

    if (highlighted) {
        DrawTriangle(
            {moved.x - 18.0f, moved.y + moved.height*0.5f},
            {moved.x + 5.0f, moved.y + 8.0f},
            {moved.x + 5.0f, moved.y + moved.height - 8.0f},
            PaperGreen);
        drawPaperStrip(
            {moved.x + moved.width*0.56f, moved.y - 7.0f, 78.0f, 13.0f},
            8.0f,
            Paper,
            Ink);
    }

    const Color textColor = enabled ? Paper : Muted;
    const float fontSize = 31.0f + result.hover*4.0f;
    DrawTextEx(GetFontDefault(), label, {moved.x + 39.0f, moved.y + 12.0f}, fontSize, 1.0f, textColor);
    drawPaperStrip(
        {moved.x + moved.width - 78.0f, moved.y + 11.0f, 58.0f, moved.height - 22.0f},
        7.0f,
        Ink,
        Paper);
    DrawTextEx(GetFontDefault(), tag, {moved.x + moved.width - 67.0f, moved.y + 17.0f}, 20.0f, 1.0f, enabled ? Paper : Muted);

    result.bounds = moved;
    return result;
}

void drawMusicStatus(float timeSeconds, int width, int height, bool musicLoaded) {
    const float panelWidth = 390.0f;
    const float panelHeight = 82.0f;
    const float x = width - panelWidth - 34.0f;
    const float y = height - panelHeight - 27.0f;

    drawPaperStrip({x + 9.0f, y + 10.0f, panelWidth, panelHeight}, 15.0f, withAlpha(Ink, 0.82f), Ink);
    drawPaperStrip({x, y, panelWidth, panelHeight}, 15.0f, DeepBlue, Paper);
    DrawRectangle(static_cast<int>(x + 91.0f), static_cast<int>(y + 9.0f), 5, 62, SignalRed);

    const Vector2 recordCenter{x + 53.0f, y + 41.0f};
    DrawCircleV({recordCenter.x + 4.0f, recordCenter.y + 5.0f}, 31.0f, withAlpha(Ink, 0.50f));
    DrawCircleV(recordCenter, 31.0f, Ink);
    DrawRing(recordCenter, 13.0f, 16.0f, 0.0f, 360.0f, 40, withAlpha(Paper, 0.26f));
    DrawRing(recordCenter, 22.0f, 24.0f, 0.0f, 360.0f, 40, withAlpha(PaperBlue, 0.65f));
    DrawCircleV(recordCenter, 9.0f, SignalRed);
    DrawCircleV(recordCenter, 3.0f, Paper);

    const float angle = timeSeconds*185.0f*DEG2RAD;
    const Vector2 marker{
        recordCenter.x + std::cos(angle)*25.0f,
        recordCenter.y + std::sin(angle)*25.0f
    };
    DrawLineEx(recordCenter, marker, 3.0f, PaperGreen);
    DrawCircleV(marker, 3.0f, Paper);

    DrawTextEx(GetFontDefault(), musicLoaded ? "NOW PLAYING" : "AUDIO UNAVAILABLE", {x + 112.0f, y + 13.0f}, 17.0f, 1.0f, PaperGreen);
    DrawTextEx(GetFontDefault(), "BREAKING THE LIMITS", {x + 112.0f, y + 36.0f}, 24.0f, 1.0f, Paper);
    DrawTextEx(GetFontDefault(), "MAIN MENU", {x + 112.0f, y + 62.0f}, 13.0f, 1.0f, withAlpha(Paper, 0.72f));
}

void drawCreditsPanel(float entry, float timeSeconds, int width, int height) {
    const float panelWidth = std::min(750.0f, width - 180.0f);
    const float panelHeight = std::min(430.0f, height - 220.0f);
    const float x = (width - panelWidth)*0.5f + (1.0f - easeOutBack(entry))*width;
    const float y = 225.0f;

    drawQuad(
        {x + 23.0f, y + 24.0f},
        {x + panelWidth + 18.0f, y + 13.0f},
        {x + panelWidth - 8.0f, y + panelHeight + 22.0f},
        {x + 4.0f, y + panelHeight + 14.0f},
        withAlpha(Ink, 0.84f));
    drawQuad(
        {x - 13.0f, y + 4.0f},
        {x + panelWidth + 8.0f, y - 10.0f},
        {x + panelWidth - 14.0f, y + panelHeight + 7.0f},
        {x - 25.0f, y + panelHeight - 5.0f},
        PaperBlue);
    drawQuadOutline(
        {x - 13.0f, y + 4.0f},
        {x + panelWidth + 8.0f, y - 10.0f},
        {x + panelWidth - 14.0f, y + panelHeight + 7.0f},
        {x - 25.0f, y + panelHeight - 5.0f},
        7.0f,
        Ink);

    drawPaperStrip({x + 36.0f, y + 28.0f, panelWidth - 72.0f, 65.0f}, 20.0f, SignalRed, Paper);
    DrawTextEx(GetFontDefault(), "CREDITS", {x + 70.0f, y + 40.0f}, 42.0f, 2.0f, Paper);

    const std::array<const char*, 4> names{{
        "Chamorro Mayen Holman Lennin",
        "Solorzano Walter Uriel",
        "Pérez García Leonardo Miguel",
        "Mathias Eli Baldizon Orozco"
    }};
    for (int index = 0; index < static_cast<int>(names.size()); ++index) {
        const float nameY = y + 125.0f + index*48.0f;
        const Color stripColor = index%2 == 0 ? DeepBlue : DeepGreen;
        drawPaperStrip({x + 54.0f, nameY, panelWidth - 108.0f, 38.0f}, 12.0f, stripColor, Paper);
        DrawTextEx(GetFontDefault(), names[index], {x + 76.0f, nameY + 8.0f}, 22.0f, 1.0f, Paper);
    }

    drawPaperBurst({x + panelWidth - 101.0f, y + panelHeight - 76.0f}, 61.0f, timeSeconds*0.12f, PaperGreen);
    DrawTextEx(GetFontDefault(), "TEAM", {x + panelWidth - 155.0f, y + panelHeight - 96.0f}, 23.0f, 1.0f, Paper);
    DrawTextEx(GetFontDefault(), "PINIX", {x + panelWidth - 164.0f, y + panelHeight - 68.0f}, 37.0f, 1.0f, Paper);
}

void drawHowToPlayPanel(float entry, int width, int height) {
    const float panelWidth = std::min(900.0f, width - 170.0f);
    const float panelHeight = std::min(430.0f, height - 190.0f);
    const float x = (width - panelWidth)*0.5f + (1.0f - easeOutBack(entry))*width;
    const float y = std::clamp(height - panelHeight - 60.0f, 170.0f, 230.0f);

    drawQuad(
        {x + 24.0f, y + 28.0f},
        {x + panelWidth + 16.0f, y + 12.0f},
        {x + panelWidth - 8.0f, y + panelHeight + 22.0f},
        {x + 3.0f, y + panelHeight + 16.0f},
        withAlpha(Ink, 0.84f));
    drawQuad(
        {x - 14.0f, y + 4.0f},
        {x + panelWidth + 8.0f, y - 10.0f},
        {x + panelWidth - 16.0f, y + panelHeight + 8.0f},
        {x - 26.0f, y + panelHeight - 6.0f},
        DeepGreen);
    drawQuadOutline(
        {x - 14.0f, y + 4.0f},
        {x + panelWidth + 8.0f, y - 10.0f},
        {x + panelWidth - 16.0f, y + panelHeight + 8.0f},
        {x - 26.0f, y + panelHeight - 6.0f},
        7.0f,
        Paper);

    drawPaperStrip({x + 36.0f, y + 27.0f, panelWidth - 72.0f, 62.0f}, 20.0f, SignalRed, Paper);
    DrawTextEx(GetFontDefault(), "HOW TO PLAY", {x + 70.0f, y + 39.0f}, 40.0f, 2.0f, Paper);
    DrawTextEx(GetFontDefault(), "ESSENTIALS BY WORLD", {x + panelWidth - 295.0f, y + 51.0f}, 18.0f, 1.0f, withAlpha(Paper, 0.86f));

    const std::array<const char*, WorldCount> titles{{
        "WORLD 1",
        "WORLD 2",
        "WORLD 3",
        "WORLD 4"
    }};
    const std::array<const char*, WorldCount> controlLines{{
        "Move with WASD, jump with SPACE, switch 2D/3D with TAB.",
        "Use TAB for 2D/3D. Press E to dodge in 3D or parry in 2D.",
        "Shoot in 2D, block with E, and refill light with suns.",
        "Platform through 2D/3D, shoot with mouse, parry with F."
    }};
    const std::array<const char*, WorldCount> clearLines{{
        "Collect the coins, talk to Toad with F, then take the star.",
        "Survive the enemy waves and reach the finish line.",
        "Collect every coin while staying alive.",
        "Use gems at the shop and claim the final star."
    }};
    const std::array<Color, WorldCount> rowColors{{PaperBlue, SignalRed, PaperGreen, DeepBlue}};

    const bool compactRows = panelHeight < 455.0f;
    const float rowStartY = y + (compactRows ? 112.0f : 118.0f);
    const float rowHeight = compactRows ? 58.0f : 63.0f;
    const float rowGap = compactRows ? 9.0f : 15.0f;
    for (int index = 0; index < WorldCount; ++index) {
        const float rowY = rowStartY + index*(rowHeight + rowGap);
        drawPaperStrip({x + 48.0f, rowY, panelWidth - 96.0f, rowHeight}, 13.0f, index%2 == 0 ? withAlpha(Ink, 0.82f) : withAlpha(DeepBlue, 0.86f), Paper);
        drawPaperStrip({x + 67.0f, rowY + 11.0f, 128.0f, 39.0f}, 10.0f, rowColors[index], Paper);
        DrawTextEx(GetFontDefault(), titles[index], {x + 84.0f, rowY + 20.0f}, 18.0f, 1.0f, Paper);
        DrawTextEx(GetFontDefault(), controlLines[index], {x + 222.0f, rowY + 12.0f}, 18.0f, 1.0f, Paper);
        DrawTextEx(GetFontDefault(), clearLines[index], {x + 222.0f, rowY + 35.0f}, 16.0f, 1.0f, withAlpha(Paper, 0.78f));
    }
}

PreviewModel loadTexturedModel(const char* path, Shader lightingShader, Texture2D fallbackTexture) {
    PreviewModel result;
    result.loadAttempted = true;
    result.minBounds = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
    result.maxBounds = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    };
    result.minY = std::numeric_limits<float>::max();
    result.maxY = std::numeric_limits<float>::lowest();
    if (!FileExists(path)) {
        TraceLog(LOG_WARNING, "PREVIEW: manifest not found: %s", path);
        return result;
    }

    const std::filesystem::path manifestPath(path);
    const std::filesystem::path manifestDirectory = manifestPath.parent_path();
    std::ifstream manifest(manifestPath);
    std::string line;
    int meshCount = 0;
    while (std::getline(manifest, line)) {
        if (line.empty() || line.front() == '#') {
            continue;
        }

        std::array<std::string, 5> fields{};
        std::stringstream parser(line);
        for (std::string& field : fields) {
            if (!std::getline(parser, field, '|')) {
                field.clear();
            }
        }
        if (fields[0].empty()) {
            continue;
        }

        const std::filesystem::path partPath = manifestDirectory/fields[0];
        PreviewPart part;
        part.model = LoadModel(partPath.string().c_str());
        if (part.model.meshCount <= 0) {
            continue;
        }
        const BoundingBox bounds = GetModelBoundingBox(part.model);
        result.minBounds.x = std::min(result.minBounds.x, bounds.min.x);
        result.minBounds.y = std::min(result.minBounds.y, bounds.min.y);
        result.minBounds.z = std::min(result.minBounds.z, bounds.min.z);
        result.maxBounds.x = std::max(result.maxBounds.x, bounds.max.x);
        result.maxBounds.y = std::max(result.maxBounds.y, bounds.max.y);
        result.maxBounds.z = std::max(result.maxBounds.z, bounds.max.z);
        result.minY = std::min(result.minY, bounds.min.y);
        result.maxY = std::max(result.maxY, bounds.max.y);

        const float red = fields[2].empty() ? 0.82f : std::stof(fields[2]);
        const float green = fields[3].empty() ? 0.82f : std::stof(fields[3]);
        const float blue = fields[4].empty() ? 0.82f : std::stof(fields[4]);
        const Color materialColor{
            static_cast<unsigned char>(255.0f*clamp01(red)),
            static_cast<unsigned char>(255.0f*clamp01(green)),
            static_cast<unsigned char>(255.0f*clamp01(blue)),
            255
        };

        Texture2D diffuse = fallbackTexture;
        if (!fields[1].empty() && fields[1] != "-") {
            const std::filesystem::path texturePath = manifestDirectory/fields[1];
            part.texture = LoadTexture(texturePath.string().c_str());
            if (part.texture.id > 0) {
                diffuse = part.texture;
                part.ownsTexture = true;
                result.texturedMaterials += 1;
                SetTextureFilter(diffuse, TEXTURE_FILTER_BILINEAR);
            }
        }

        for (int materialIndex = 0; materialIndex < part.model.materialCount; ++materialIndex) {
            Material& material = part.model.materials[materialIndex];
            material.shader = lightingShader;
            material.maps[MATERIAL_MAP_DIFFUSE].color = materialColor;
            SetMaterialTexture(&material, MATERIAL_MAP_DIFFUSE, diffuse);
        }
        meshCount += part.model.meshCount;
        result.parts.push_back(std::move(part));
    }

    result.loaded = !result.parts.empty();
    if (!result.loaded) {
        result.minBounds = {0.0f, 0.0f, 0.0f};
        result.maxBounds = {0.0f, 0.0f, 0.0f};
        result.minY = 0.0f;
        result.maxY = 0.0f;
    } else {
        result.center = {
            (result.minBounds.x + result.maxBounds.x)*0.5f,
            (result.minBounds.y + result.maxBounds.y)*0.5f,
            (result.minBounds.z + result.maxBounds.z)*0.5f
        };
    }
    TraceLog(
        LOG_INFO,
        "PREVIEW: %s loaded with %i parts, %i meshes and %i diffuse textures",
        path,
        static_cast<int>(result.parts.size()),
        meshCount,
        result.texturedMaterials);
    return result;
}

PreviewModel loadDirectModel(const char* path, Shader lightingShader, Texture2D fallbackTexture) {
    PreviewModel result;
    result.loadAttempted = true;
    if (!FileExists(path)) {
        TraceLog(LOG_WARNING, "PREVIEW: model not found: %s", path);
        return result;
    }

    PreviewPart part;
    part.model = LoadModel(path);
    if (part.model.meshCount <= 0) {
        TraceLog(LOG_WARNING, "PREVIEW: model has no meshes: %s", path);
        return result;
    }

    const BoundingBox bounds = GetModelBoundingBox(part.model);
    result.minBounds = bounds.min;
    result.maxBounds = bounds.max;
    result.minY = bounds.min.y;
    result.maxY = bounds.max.y;
    result.center = {
        (bounds.min.x + bounds.max.x)*0.5f,
        (bounds.min.y + bounds.max.y)*0.5f,
        (bounds.min.z + bounds.max.z)*0.5f
    };
    const float extent = std::max({
        bounds.max.x - bounds.min.x,
        bounds.max.y - bounds.min.y,
        bounds.max.z - bounds.min.z
    });
    result.autoScale = 5.4f/std::max(extent, 0.0001f);

    for (int materialIndex = 0; materialIndex < part.model.materialCount; ++materialIndex) {
        Material& material = part.model.materials[materialIndex];
        material.shader = lightingShader;
        if (material.maps[MATERIAL_MAP_DIFFUSE].texture.id > 0) {
            SetTextureFilter(material.maps[MATERIAL_MAP_DIFFUSE].texture, TEXTURE_FILTER_BILINEAR);
            result.texturedMaterials += 1;
        } else {
            SetMaterialTexture(&material, MATERIAL_MAP_DIFFUSE, fallbackTexture);
        }
    }

    result.parts.push_back(std::move(part));
    result.loaded = true;
    TraceLog(
        LOG_INFO,
        "PREVIEW: %s loaded directly with %i meshes and %i diffuse textures",
        path,
        result.parts.front().model.meshCount,
        result.texturedMaterials);
    return result;
}

PreviewModel loadPreviewModel(const char* path, Shader lightingShader, Texture2D fallbackTexture) {
    const std::filesystem::path previewPath(path);
    if (previewPath.extension() == ".preview") {
        return loadTexturedModel(path, lightingShader, fallbackTexture);
    }
    return loadDirectModel(path, lightingShader, fallbackTexture);
}

void configureLightingShader(Shader shader) {
    const Vector3 lightDirection{-0.42f, -0.86f, -0.34f};
    const Vector3 lightColor{1.08f, 1.02f, 0.92f};
    const Vector3 ambientColor{0.44f, 0.49f, 0.58f};

    const int directionLocation = GetShaderLocation(shader, "lightDirection");
    const int colorLocation = GetShaderLocation(shader, "lightColor");
    const int ambientLocation = GetShaderLocation(shader, "ambientColor");
    SetShaderValue(shader, directionLocation, &lightDirection, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, colorLocation, &lightColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, ambientLocation, &ambientColor, SHADER_UNIFORM_VEC3);
}

void configurePreviewLighting(Shader shader, bool nightMode) {
    const Vector3 lightDirection = nightMode
        ? Vector3{-0.18f, -0.94f, -0.28f}
        : Vector3{-0.42f, -0.86f, -0.34f};
    const Vector3 lightColor = nightMode
        ? Vector3{0.42f, 0.48f, 0.70f}
        : Vector3{1.08f, 1.02f, 0.92f};
    const Vector3 ambientColor = nightMode
        ? Vector3{0.14f, 0.17f, 0.26f}
        : Vector3{0.44f, 0.49f, 0.58f};

    const int directionLocation = GetShaderLocation(shader, "lightDirection");
    const int colorLocation = GetShaderLocation(shader, "lightColor");
    const int ambientLocation = GetShaderLocation(shader, "ambientColor");
    SetShaderValue(shader, directionLocation, &lightDirection, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, colorLocation, &lightColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, ambientLocation, &ambientColor, SHADER_UNIFORM_VEC3);
}

void drawConceptPreview(int activeWorld, float rotationY, float groundY) {
    rlPushMatrix();
    rlTranslatef(0.0f, groundY, 0.0f);
    rlRotatef(rotationY, 0.0f, 1.0f, 0.0f);

    if (activeWorld == 2) {
        DrawCylinder({0.0f, 0.55f, 0.0f}, 2.55f, 2.15f, 1.10f, 8, {55, 48, 70, 255});
        DrawCylinder({0.0f, 1.18f, 0.0f}, 1.72f, 1.42f, 0.32f, 8, {235, 39, 58, 255});
        DrawCylinder({0.0f, 1.42f, 0.0f}, 1.25f, 0.72f, 0.46f, 8, {62, 55, 76, 255});

        for (int index = 0; index < 6; ++index) {
            const float angle = index*(2.0f*PI/6.0f);
            const Vector3 position{
                std::cos(angle)*1.35f,
                1.62f,
                std::sin(angle)*1.35f
            };
            DrawCube(position, 0.36f, 1.12f + (index%2)*0.38f, 0.36f, index%2 == 0 ? SignalRed : PaperGreen);
            DrawCubeWires(position, 0.36f, 1.12f + (index%2)*0.38f, 0.36f, Ink);
        }

        DrawCylinder({0.0f, 2.02f, 0.0f}, 0.48f, 0.12f, 1.55f, 10, {42, 37, 52, 255});
        DrawSphere({0.0f, 3.32f, 0.0f}, 0.42f, SignalRed);
        DrawSphere({0.0f, 3.32f, 0.0f}, 0.25f, PaperGreen);
    } else {
        DrawCylinder({0.0f, 0.42f, 0.0f}, 2.45f, 2.05f, 0.84f, 10, {58, 75, 112, 255});
        DrawCylinder({0.0f, 0.92f, 0.0f}, 1.82f, 1.58f, 0.28f, 10, Paper);

        for (int index = 0; index < 5; ++index) {
            const float angle = index*(2.0f*PI/5.0f);
            const Vector3 position{
                std::cos(angle)*1.28f,
                1.62f,
                std::sin(angle)*1.28f
            };
            DrawCylinder(position, 0.30f, 0.22f, 1.42f, 7, index%2 == 0 ? PaperBlue : PaperGreen);
            DrawCylinderWires(position, 0.30f, 0.22f, 1.42f, 7, Ink);
        }

        DrawSphere({0.0f, 2.22f, 0.0f}, 0.78f, PaperBlue);
        DrawSphereWires({0.0f, 2.22f, 0.0f}, 0.80f, 12, 18, Paper);
        DrawCylinder({0.0f, 3.20f, 0.0f}, 0.10f, 0.10f, 1.18f, 8, SignalRed);
        DrawSphere({0.0f, 3.90f, 0.0f}, 0.27f, PaperGreen);
    }

    rlPopMatrix();
}

void drawPreview(
    RenderTexture2D target,
    std::array<PreviewModel, WorldCount>& models,
    Shader lightingShader,
    const std::array<WorldEntry, WorldCount>& worlds,
    int activeWorld,
    float timeSeconds
) {
    constexpr float groundY = -2.9f;
    const WorldEntry* activeEntry = activeWorld >= 0 && activeWorld < WorldCount
        ? &worlds[activeWorld]
        : nullptr;
    const bool nightMode = activeEntry != nullptr && activeEntry->nightPreview;
    float cameraTargetY = 0.0f;
    if (activeWorld >= 0 && activeWorld < WorldCount && models[activeWorld].loaded) {
        const WorldEntry& world = worlds[activeWorld];
        const float previewScale = world.previewScale*models[activeWorld].autoScale;
        const BoundingBox transformedBounds = rotatedPreviewBounds(models[activeWorld], world.previewRotation);
        const float modelHeight =
            (transformedBounds.max.y - transformedBounds.min.y)*previewScale;
        cameraTargetY = groundY + modelHeight*0.48f;
    }

    const Camera3D camera{
        .position = {7.6f, cameraTargetY + 5.8f, 8.8f},
        .target = {0.0f, cameraTargetY, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 38.0f,
        .projection = CAMERA_PERSPECTIVE
    };

    const int viewLocation = GetShaderLocation(lightingShader, "viewPosition");
    SetShaderValue(lightingShader, viewLocation, &camera.position, SHADER_UNIFORM_VEC3);
    configurePreviewLighting(lightingShader, nightMode);

    BeginTextureMode(target);
    ClearBackground(nightMode ? Color{5, 10, 22, 255} : Color{11, 23, 41, 255});
    for (int index = 0; index < PreviewHeight; index += 32) {
        DrawRectangle(0, index, PreviewWidth, 2, nightMode ? Color{16, 30, 74, 85} : Color{25, 91, 176, 90});
    }
    if (nightMode) {
        DrawCircle(686, 98, 46.0f, Color{233, 238, 255, 210});
        DrawCircle(686, 98, 60.0f, Color{233, 238, 255, 32});
        DrawCircle(728, 140, 16.0f, Color{24, 40, 82, 255});
    }

    BeginMode3D(camera);
    DrawPlane({0.0f, groundY, 0.0f}, {13.0f, 13.0f}, nightMode ? Color{8, 18, 34, 255} : Color{19, 39, 65, 255});
    rlPushMatrix();
    rlTranslatef(0.0f, groundY + 0.01f, 0.0f);
    if (nightMode) {
        DrawGrid(26, 0.5f);
        DrawCube({0.0f, groundY + 0.08f, 0.0f}, 13.0f, 0.02f, 13.0f, Color{9, 14, 26, 150});
    } else {
        DrawGrid(26, 0.5f);
    }
    rlPopMatrix();

    const float rotationY = std::fmod(timeSeconds*5.5f, 360.0f);
    if (activeWorld >= 0 && activeWorld < WorldCount && models[activeWorld].loaded) {
        const WorldEntry& world = worlds[activeWorld];
        const PreviewModel& model = models[activeWorld];
        const float previewScale = world.previewScale*model.autoScale;
        const BoundingBox transformedBounds = rotatedPreviewBounds(model, world.previewRotation);
        const Vector3 transformedCenter{
            (transformedBounds.min.x + transformedBounds.max.x)*0.5f,
            (transformedBounds.min.y + transformedBounds.max.y)*0.5f,
            (transformedBounds.min.z + transformedBounds.max.z)*0.5f
        };
        rlDisableBackfaceCulling();
        rlPushMatrix();
        rlTranslatef(world.previewPosition.x, groundY + world.previewPosition.y, world.previewPosition.z);
        rlRotatef(rotationY, 0.0f, 1.0f, 0.0f);
        rlScalef(previewScale, previewScale, previewScale);
        rlTranslatef(-transformedCenter.x, -transformedBounds.min.y, -transformedCenter.z);
        rlRotatef(world.previewRotation.x, 1.0f, 0.0f, 0.0f);
        rlRotatef(world.previewRotation.y, 0.0f, 1.0f, 0.0f);
        rlRotatef(world.previewRotation.z, 0.0f, 0.0f, 1.0f);
        for (const PreviewPart& part : models[activeWorld].parts) {
            DrawModel(part.model, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
        }
        rlPopMatrix();
        rlEnableBackfaceCulling();
    } else if (activeWorld == 2 || activeWorld == 3) {
        drawConceptPreview(activeWorld, rotationY, groundY);
    } else {
        const float pulse = 1.0f + std::sin(timeSeconds*2.8f)*0.08f;
        DrawCubeWiresV({0.0f, 0.0f, 0.0f}, {3.1f*pulse, 2.1f*pulse, 3.1f*pulse}, Muted);
        DrawSphereWires({0.0f, 0.0f, 0.0f}, 1.35f, 12, 18, withAlpha(SignalRed, 0.75f));
    }
    EndMode3D();

    DrawRectangle(0, PreviewHeight - 78, PreviewWidth, 78, withAlpha(nightMode ? DeepBlue : DeepGreen, 0.92f));
    DrawRectangle(0, PreviewHeight - 78, PreviewWidth, 7, nightMode ? PaperBlue : SignalRed);
    DrawTextEx(GetFontDefault(), activeEntry != nullptr ? activeEntry->previewCaption : "", {24.0f, PreviewHeight - 50.0f}, 22.0f, 1.0f, Paper);
    EndTextureMode();
}

void drawPreviewPanel(RenderTexture2D preview, const WorldEntry& world, float entry, int width, int height) {
    const float panelWidth = std::min(800.0f, width*0.60f);
    const float panelHeight = std::min(520.0f, height - 150.0f);
    const float x = width - panelWidth - 42.0f + (1.0f - easeOutCubic(entry))*620.0f;
    const float y = 92.0f;
    const Rectangle panel{x, y, panelWidth, panelHeight};

    DrawRectangle(static_cast<int>(x + 18.0f), static_cast<int>(y + 20.0f), static_cast<int>(panelWidth), static_cast<int>(panelHeight), withAlpha(Ink, 0.86f));
    drawQuad(
        {x - 19.0f, y - 17.0f},
        {x + panelWidth + 22.0f, y - 8.0f},
        {x + panelWidth + 13.0f, y + panelHeight + 17.0f},
        {x - 26.0f, y + panelHeight + 7.0f},
        PaperBlue);
    drawQuadOutline(
        {x - 19.0f, y - 17.0f},
        {x + panelWidth + 22.0f, y - 8.0f},
        {x + panelWidth + 13.0f, y + panelHeight + 17.0f},
        {x - 26.0f, y + panelHeight + 7.0f},
        6.0f,
        Ink);
    drawQuad(
        {x - 10.0f, y - 10.0f},
        {x + panelWidth + 13.0f, y - 3.0f},
        {x + panelWidth + 7.0f, y + panelHeight + 9.0f},
        {x - 16.0f, y + panelHeight + 3.0f},
        Paper);
    DrawTexturePro(
        preview.texture,
        {0.0f, 0.0f, static_cast<float>(preview.texture.width), -static_cast<float>(preview.texture.height)},
        panel,
        {0.0f, 0.0f},
        0.0f,
        WHITE);

    drawQuad(
        {x - 18.0f, y - 22.0f},
        {x + panelWidth*0.58f, y - 22.0f},
        {x + panelWidth*0.53f, y + 45.0f},
        {x - 38.0f, y + 45.0f},
        world.accent);
    drawQuadOutline(
        {x - 18.0f, y - 22.0f},
        {x + panelWidth*0.58f, y - 22.0f},
        {x + panelWidth*0.53f, y + 45.0f},
        {x - 38.0f, y + 45.0f},
        5.0f,
        Ink);
    DrawTextEx(GetFontDefault(), world.title, {x + 18.0f, y - 11.0f}, 38.0f, 1.0f, Paper);

    const float labelY = y + panelHeight + 10.0f;
    drawPaperStrip({x, labelY, panelWidth, 45.0f}, 13.0f, DeepGreen, Paper);
    DrawRectangle(static_cast<int>(x + 4.0f), static_cast<int>(labelY + 4.0f), 9, 35, SignalRed);
    DrawTextEx(GetFontDefault(), world.subtitle, {x + 20.0f, labelY + 9.0f}, 23.0f, 1.0f, Paper);
    DrawTextEx(
        GetFontDefault(),
        world.available ? "CLICK TO ENTER" : "IN DEVELOPMENT",
        {x + panelWidth - 226.0f, labelY + 10.0f},
        21.0f,
        1.0f,
        world.available ? Paper : Muted);
}

bool hasArgument(int argc, char** argv, const char* argument) {
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], argument) == 0) {
            return true;
        }
    }
    return false;
}

const char* argumentValue(int argc, char** argv, const char* argument) {
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::strcmp(argv[index], argument) == 0) {
            return argv[index + 1];
        }
    }
    return nullptr;
}

void writeWindowPosition(const char* path) {
    if (path == nullptr || path[0] == '\0') {
        return;
    }

    const Vector2 position = GetWindowPosition();
    std::ofstream output(path, std::ios::trunc);
    if (output.is_open()) {
        output << static_cast<int>(std::round(position.x)) << ' '
               << static_cast<int>(std::round(position.y)) << std::endl;
    }
}
}

int main(int argc, char** argv) {
    const std::array<WorldEntry, WorldCount> worlds{{
        {"WORLD 1", "FREEZEEZY PEAK", "Menu/RaylibMenu/generated/world2_preview.preview", PaperBlue, true, {0.0f, 0.02f, 0.0f}, 0.92f, {90.0f, 0.0f, 0.0f}},
        {"WORLD 2", "THIRD WORLD ADVENTURE", "assets/mundo3/game_pirate_adventure_map/scene_map3.gltf", SignalRed, true, {0.0f, 0.02f, 0.0f}, 1.45f, {0.0f, 0.0f, 0.0f}},
        {"WORLD 3", "FINAL CHALLENGE", "Menu/RaylibMenu/generated/world1_preview.preview", DeepGreen, true, {0.0f, 0.02f, 0.0f}, 1.65f, {0.0f, 0.0f, 0.0f}, true, "NIGHT PREVIEW // FINAL CHALLENGE"},
        {"WORLD 4", "ISLANDS OF THE FIRST JOURNEY", "Menu/RaylibMenu/generated/world4_preview.preview", PaperGreen, true, {0.0f, 0.02f, 0.0f}, 1.55f, {0.0f, 0.0f, 0.0f}}
    }};

    const bool pauseMode = hasArgument(argc, argv, "--pause");
    const bool startWorlds = hasArgument(argc, argv, "--worlds") || hasArgument(argc, argv, "--world-select");
    const char* screenshotPath = argumentValue(argc, argv, "--screenshot");
    const char* screenshotScreen = argumentValue(argc, argv, "--screen");
    const char* windowPositionOutPath = argumentValue(argc, argv, "--window-position-out");
    const char* previewWorldArgument = argumentValue(argc, argv, "--world");
    const int previewWorld = previewWorldArgument != nullptr
        ? std::clamp(std::atoi(previewWorldArgument) - 1, 0, WorldCount - 1)
        : 1;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, pauseMode ? "Paper Pinix - Pause" : "Paper Pinix - World Select");
    const std::filesystem::path projectRoot = std::filesystem::weakly_canonical(
        std::filesystem::path(GetApplicationDirectory())/".."/".."/"..");
    ChangeDirectory(projectRoot.string().c_str());
    std::ofstream diagnostics;
    if (screenshotPath != nullptr) {
        diagnostics.open("Menu/RaylibMenu/menu_diagnostic.txt", std::ios::trunc);
        diagnostics << "cwd=" << GetWorkingDirectory() << std::endl;
    }
    SetWindowMinSize(1080, 640);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    Music menuMusic{};
    bool musicLoaded = false;
    const bool playMenuMusic = !pauseMode && screenshotPath == nullptr;
    if (playMenuMusic) {
        InitAudioDevice();
        if (IsAudioDeviceReady()) {
            menuMusic = LoadMusicStream("Menu/RaylibMenu/audio/Breaking_The_Limits.mp3");
            musicLoaded = menuMusic.ctxData != nullptr;
            if (musicLoaded) {
                menuMusic.looping = true;
                SetMusicVolume(menuMusic, 0.55f);
                PlayMusicStream(menuMusic);
            }
        }
    }

    const Shader lightingShader = LoadShader(
        "Menu/RaylibMenu/shaders/preview_lighting.vs",
        "Menu/RaylibMenu/shaders/preview_lighting.fs");
    configureLightingShader(lightingShader);
    if (diagnostics.is_open()) {
        diagnostics << "shader=" << lightingShader.id << std::endl;
    }

    Image fallbackImage = GenImageColor(4, 4, WHITE);
    Texture2D fallbackTexture = LoadTextureFromImage(fallbackImage);
    UnloadImage(fallbackImage);

    std::array<PreviewModel, WorldCount> models{};
    auto ensurePreviewLoaded = [&](int worldIndex) {
        if (worldIndex >= 0
            && worldIndex < WorldCount
            && !models[worldIndex].loaded
            && !models[worldIndex].loadAttempted) {
            if (diagnostics.is_open()) {
                diagnostics << "loading world=" << worldIndex + 1 << std::endl;
            }
            if (worlds[worldIndex].modelPath != nullptr && FileExists(worlds[worldIndex].modelPath)) {
                models[worldIndex] = loadPreviewModel(
                    worlds[worldIndex].modelPath,
                    lightingShader,
                    fallbackTexture);
            } else {
                models[worldIndex].loadAttempted = true;
            }
            if (diagnostics.is_open()) {
                diagnostics << "loaded=" << models[worldIndex].loaded
                            << " textures=" << models[worldIndex].texturedMaterials << std::endl;
            }
        }
    };

    RenderTexture2D preview = LoadRenderTexture(PreviewWidth, PreviewHeight);
    MenuScreen screen = MenuScreen::Home;
    if (pauseMode) {
        screen = MenuScreen::Pause;
    } else if (startWorlds) {
        screen = MenuScreen::Worlds;
    } else if (screenshotPath != nullptr && screenshotScreen != nullptr && std::strcmp(screenshotScreen, "credits") == 0) {
        screen = MenuScreen::Credits;
    } else if (screenshotPath != nullptr && screenshotScreen != nullptr && std::strcmp(screenshotScreen, "how") == 0) {
        screen = MenuScreen::HowToPlay;
    } else if (screenshotPath != nullptr && (screenshotScreen == nullptr || std::strcmp(screenshotScreen, "worlds") == 0)) {
        screen = MenuScreen::Worlds;
    }
    float screenStarted = static_cast<float>(GetTime());
    std::array<float, 3> homeHover{};
    float creditsHover = 0.0f;
    float howToPlayHover = 0.0f;
    std::array<float, WorldCount> worldHover{};
    std::array<float, 3> pauseHover{};
    int activeWorld = screenshotPath != nullptr ? previewWorld : 0;
    int resultCode = pauseMode ? PauseResume : 0;
    int screenshotFrame = 0;
    float unavailableUntil = 0.0f;
    bool finished = false;

    while (!WindowShouldClose() && !finished) {
        if (diagnostics.is_open() && screenshotFrame == 0) {
            diagnostics << "first frame" << std::endl;
        }
        const float now = static_cast<float>(GetTime());
        const float entry = clamp01((now - screenStarted)/0.48f);
        const int width = GetScreenWidth();
        const int height = GetScreenHeight();

        if (musicLoaded) {
            UpdateMusicStream(menuMusic);
        }

        if (screen == MenuScreen::Worlds) {
            if (screenshotPath == nullptr) {
                for (int index = 0; index < WorldCount; ++index) {
                    const Rectangle hitbox{42.0f, 280.0f + index*76.0f, 360.0f, 62.0f};
                    if (CheckCollisionPointRec(GetMousePosition(), hitbox)) {
                        activeWorld = index;
                    }
                }
            }
            ensurePreviewLoaded(activeWorld);
            drawPreview(preview, models, lightingShader, worlds, activeWorld, now);
        }

        BeginDrawing();
        drawDynamicBackground(now, width, height);
        const char* eyebrow = screen == MenuScreen::Pause
            ? "PAUSE // PAPER PINIX"
            : (screen == MenuScreen::Credits
                ? "CREDITS // TEAM PINIX"
                : (screen == MenuScreen::HowToPlay ? "HOW TO PLAY // QUICK GUIDE" : "WORLD SELECT // RAYLIB C++"));
        drawBrand(now, clamp01(now/0.62f), width, eyebrow);

        if (screen == MenuScreen::Home) {
            const float buttonX = 82.0f + easeOutBack(entry)*70.0f;
            AnimatedButton start = drawSkewButton(
                {buttonX, height - 336.0f, 390.0f, 70.0f},
                "START",
                ">>",
                homeHover[0],
                true,
                now,
                PaperBlue);
            homeHover[0] = start.hover;

            AnimatedButton howToPlay = drawSkewButton(
                {buttonX + 9.0f, height - 252.0f, 372.0f, 62.0f},
                "HOW TO PLAY",
                "?",
                homeHover[1],
                true,
                now,
                SignalRed);
            homeHover[1] = howToPlay.hover;

            AnimatedButton credits = drawSkewButton(
                {buttonX + 18.0f, height - 180.0f, 354.0f, 62.0f},
                "CREDITS",
                "+",
                homeHover[2],
                true,
                now,
                PaperGreen);
            homeHover[2] = credits.hover;

            drawPaperStrip({75.0f, height - 102.0f, 425.0f, 38.0f}, 13.0f, DeepGreen, Paper);
            DrawTextEx(GetFontDefault(), "CHOOSE YOUR NEXT DESTINATION", {93.0f, height - 94.0f}, 23.0f, 1.0f, Paper);
            DrawTextEx(GetFontDefault(), "ESC  EXIT", {46.0f, height - 42.0f}, 18.0f, 1.0f, Ink);
            drawMusicStatus(now, width, height, musicLoaded || screenshotPath != nullptr);

            if (start.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                screen = MenuScreen::Worlds;
                screenStarted = now;
            }
            if (howToPlay.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                screen = MenuScreen::HowToPlay;
                screenStarted = now;
            }
            if (credits.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                screen = MenuScreen::Credits;
                screenStarted = now;
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                resultCode = 0;
                finished = true;
            }
        } else if (screen == MenuScreen::HowToPlay) {
            drawHowToPlayPanel(entry, width, height);
            AnimatedButton back = drawSkewButton(
                {56.0f, height - 94.0f, 252.0f, 56.0f},
                "BACK",
                "<",
                howToPlayHover,
                true,
                now,
                SignalRed);
            howToPlayHover = back.hover;

            if ((back.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) || IsKeyPressed(KEY_ESCAPE)) {
                screen = MenuScreen::Home;
                screenStarted = now;
            }
        } else if (screen == MenuScreen::Credits) {
            drawCreditsPanel(entry, now, width, height);
            AnimatedButton back = drawSkewButton(
                {56.0f, height - 94.0f, 252.0f, 56.0f},
                "BACK",
                "<",
                creditsHover,
                true,
                now,
                SignalRed);
            creditsHover = back.hover;

            if ((back.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) || IsKeyPressed(KEY_ESCAPE)) {
                screen = MenuScreen::Home;
                screenStarted = now;
            }
        } else if (screen == MenuScreen::Pause) {
            const float staggeredEntry = easeOutBack(entry);
            drawPaperStrip({52.0f, 252.0f, 340.0f, 55.0f}, 18.0f, DeepGreen, Paper);
            DrawTextEx(GetFontDefault(), "GAME PAUSED", {70.0f, 264.0f}, 34.0f, 1.0f, Paper);

            const std::array<const char*, 3> labels{"RESUME", "CHANGE WORLD", "EXIT"};
            const std::array<const char*, 3> tags{">>", "[]", "X"};
            for (int index = 0; index < 3; ++index) {
                const float stagger = clamp01(entry*1.55f - index*0.12f);
                const float x = -430.0f + easeOutBack(stagger)*514.0f;
                AnimatedButton button = drawSkewButton(
                    {x, 326.0f + index*88.0f, 430.0f, 70.0f},
                    labels[index],
                    tags[index],
                    pauseHover[index],
                    true,
                    now,
                    index == 0 ? PaperBlue : (index == 1 ? PaperGreen : SignalRed));
                pauseHover[index] = button.hover;

                if (button.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (index == 0) {
                        resultCode = PauseResume;
                        finished = true;
                    } else if (index == 1) {
                        screen = MenuScreen::Worlds;
                        screenStarted = now;
                    } else {
                        resultCode = PauseExit;
                        finished = true;
                    }
                }
            }

            const float cardX = width*0.55f + (1.0f - staggeredEntry)*520.0f;
            drawQuad(
                {cardX + 17.0f, 261.0f},
                {width - 58.0f, 235.0f},
                {width - 101.0f, 585.0f},
                {cardX - 21.0f, 609.0f},
                withAlpha(Ink, 0.88f));
            drawQuad(
                {cardX, 244.0f},
                {width - 75.0f, 218.0f},
                {width - 118.0f, 568.0f},
                {cardX - 38.0f, 592.0f},
                PaperBlue);
            drawQuadOutline(
                {cardX, 244.0f},
                {width - 75.0f, 218.0f},
                {width - 118.0f, 568.0f},
                {cardX - 38.0f, 592.0f},
                7.0f,
                Paper);
            drawPaperBurst({width - 260.0f, 395.0f}, 116.0f, now*0.16f, SignalRed);
            drawPaperBurst({width - 260.0f, 395.0f}, 83.0f, -now*0.12f, DeepGreen);
            DrawTextEx(GetFontDefault(), "WORLD", {width - 346.0f, 330.0f}, 38.0f, 1.0f, Paper);
            DrawTextEx(GetFontDefault(), "02", {width - 342.0f, 370.0f}, 112.0f, 2.0f, Paper);
            DrawTextEx(GetFontDefault(), "ESC  RESUME", {46.0f, height - 42.0f}, 18.0f, 1.0f, withAlpha(Paper, 0.72f));

            if (IsKeyPressed(KEY_ESCAPE)) {
                resultCode = PauseResume;
                finished = true;
            }
        } else {
            const float menuEntry = easeOutBack(entry);
            drawPaperStrip({32.0f, 228.0f, 250.0f, 46.0f}, 16.0f, SignalRed, Paper);
            DrawTextEx(GetFontDefault(), "CHOOSE WORLD", {46.0f, 237.0f}, 27.0f, 1.0f, Paper);

            for (int index = 0; index < WorldCount; ++index) {
                const float stagger = clamp01(entry*1.45f - index*0.10f);
                const float x = -410.0f + easeOutBack(stagger)*452.0f;
                char tag[8]{};
                std::snprintf(tag, sizeof(tag), "%02d", index + 1);
                AnimatedButton button = drawSkewButton(
                    {x, 280.0f + index*76.0f, 360.0f, 62.0f},
                    worlds[index].title,
                    tag,
                    worldHover[index],
                    true,
                    now,
                    worlds[index].accent);
                worldHover[index] = button.hover;

                if (button.hovered && screenshotPath == nullptr) {
                    activeWorld = index;
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        if (worlds[index].available) {
                            resultCode = index + 1;
                            finished = true;
                        } else {
                            unavailableUntil = now + 1.7f;
                        }
                    }
                }
            }

            drawPreviewPanel(preview, worlds[activeWorld], menuEntry, width, height);
            DrawTextEx(GetFontDefault(), pauseMode ? "ESC  RESUME" : "ESC  BACK", {47.0f, height - 42.0f}, 18.0f, 1.0f, withAlpha(Paper, 0.70f));

            if (now < unavailableUntil) {
                const float pulse = 0.75f + std::sin(now*15.0f)*0.15f;
                drawPaperStrip({38.0f, height - 112.0f, 390.0f, 48.0f}, 13.0f, withAlpha(SignalRed, pulse), Paper);
                DrawTextEx(GetFontDefault(), "THIS WORLD IS NOT AVAILABLE YET", {54.0f, height - 99.0f}, 18.0f, 1.0f, Paper);
            }

            if (IsKeyPressed(KEY_ESCAPE)) {
                screen = pauseMode ? MenuScreen::Pause : MenuScreen::Home;
                screenStarted = now;
            }
        }

        EndDrawing();

        if (screenshotPath != nullptr) {
            screenshotFrame += 1;
            if (screenshotFrame == 90) {
                TakeScreenshot(screenshotPath);
                resultCode = 0;
                finished = true;
            }
        }
    }

    if (musicLoaded) {
        StopMusicStream(menuMusic);
        UnloadMusicStream(menuMusic);
    }
    if (playMenuMusic && IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    UnloadRenderTexture(preview);
    for (PreviewModel& model : models) {
        for (PreviewPart& part : model.parts) {
            UnloadModel(part.model);
            if (part.ownsTexture) {
                UnloadTexture(part.texture);
            }
        }
    }
    UnloadTexture(fallbackTexture);
    UnloadShader(lightingShader);
    writeWindowPosition(windowPositionOutPath);
    CloseWindow();
    return resultCode;
}
