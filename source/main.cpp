#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "AudioPlayer.h"
#include "Environment.h"
#include "Mapa1.h"
#include "Player.h"
#include "Shader.h"

#include <GL/glew.h>
#include <Windows.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace {
constexpr int WindowWidth = 1280;
constexpr int WindowHeight = 720;
constexpr int MaxLights = 24;
constexpr int MissionCoinTotal = 10;

enum class EstadoJuego {
    MENU_PRINCIPAL,
    MENU_MUNDOS,
    COMO_JUGAR,
    CREDITOS,
    MUNDO_1,
    MUNDO_2
};

struct Rect {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};
};

struct TextSprite {
    std::shared_ptr<Texture2D> texture;
    glm::vec2 size{0.0f};
};

struct MenuContext {
    Shader shader;
    Texture2D whiteTexture;
    Texture2D logoTexture;
    Texture2D cloudTexture;
    Texture2D backgroundTexture;
    GLuint vao{0};
    GLuint vbo{0};
    double notificationUntil{0.0};

    TextSprite jugar;
    TextSprite comoJugar;
    TextSprite creditos;
    TextSprite salir;
    TextSprite mundo1;
    TextSprite mundo2;
    TextSprite mundo3;
    TextSprite mundo4;
    TextSprite volver;
    TextSprite tituloComoJugar;
    TextSprite textoComoJugar;
    TextSprite tituloCreditos;
    TextSprite textoCreditos;
    TextSprite noDisponible;
    std::array<TextSprite, MissionCoinTotal + 1> coinCounters;
    std::array<TextSprite, MissionCoinTotal + 1> coinMessages;
    TextSprite estrellaLista;
    TextSprite nivelCompletado;
    TextSprite combateSolo2D;
    TextSprite vidaJugador;
    TextSprite cargandoAtaque;
    TextSprite parryActivo;
    TextSprite promptHablarToad;
    TextSprite nombreToad;
    TextSprite dialogoToad;
};

struct MissionRenderablePart {
    Mesh mesh;
    Material material;
    glm::vec3 localPosition{0.0f};
    glm::vec3 localRotation{0.0f};
    glm::vec3 localScale{1.0f};
};

struct Coin {
    glm::vec3 position{0.0f};
    bool collected{false};
    float phase{0.0f};
};

struct Star {
    glm::vec3 position{0.0f};
    bool active{false};
};

class MissionManager {
public:
    bool initialize();
    void reset(const Environment& environment, const glm::vec3& playerSpawn);
    void generarMonedas(const Environment& environment, const glm::vec3& playerSpawn);
    void update(const Player& player, float timeSeconds);
    void render(const Shader& shader, float timeSeconds) const;

    int collectedCount() const { return m_collectedCount; }
    int messageCount() const { return m_messageCount; }
    bool showCoinMessage(float timeSeconds) const { return timeSeconds <= static_cast<float>(m_coinMessageUntil); }
    bool showStarMessage(float timeSeconds) const { return timeSeconds <= static_cast<float>(m_starMessageUntil); }
    bool starFocusActive(float timeSeconds) const { return timeSeconds <= static_cast<float>(m_starFocusUntil); }
    glm::vec3 starPosition() const { return m_star.position; }
    bool levelComplete() const { return m_levelComplete; }
    const Texture2D& coinIconTexture() const { return m_coinIcon; }

private:
    bool loadCoinModel();
    bool loadStarModel();
    std::shared_ptr<Texture2D> loadMissionTexture(const std::string& path);
    Mesh createStarMesh() const;
    Bounds coinBounds(const Coin& coin) const;
    Bounds starBounds() const;
    glm::mat4 coinModelMatrix(const Coin& coin, float timeSeconds) const;
    glm::mat4 starModelMatrix(float timeSeconds) const;
    bool validCoinPosition(const glm::vec3& position, const std::vector<Bounds>& colliders, const std::vector<Coin>& placed, const glm::vec3& playerSpawn, const glm::vec3& worldMin, const glm::vec3& worldMax, float minCoinDistance) const;
    void recolectarMoneda(Coin& coin, float timeSeconds);
    void mostrarMensajeMonedaTemporal(float timeSeconds);
    void activarEstrella(float timeSeconds);
    void completarNivel(float timeSeconds);

    std::vector<MissionRenderablePart> m_coinParts;
    std::vector<MissionRenderablePart> m_starParts;
    std::vector<std::shared_ptr<Texture2D>> m_textures;
    Texture2D m_coinIcon;
    Mesh m_fallbackCoinMesh;
    Mesh m_starMesh;
    Material m_fallbackCoinMaterial;
    Material m_starMaterial;
    glm::vec3 m_coinModelMin{0.0f};
    glm::vec3 m_coinModelMax{0.0f, 1.0f, 0.0f};
    glm::vec3 m_coinModelCenter{0.0f};
    glm::vec3 m_starModelMin{0.0f};
    glm::vec3 m_starModelMax{0.0f, 1.0f, 0.0f};
    glm::vec3 m_starModelCenter{0.0f};
    float m_coinModelScale{1.0f};
    float m_starModelScale{0.95f};
    std::vector<Coin> m_coins;
    Star m_star;
    int m_collectedCount{0};
    int m_messageCount{0};
    double m_coinMessageUntil{0.0};
    double m_starMessageUntil{0.0};
    double m_starFocusUntil{0.0};
    double m_victoryTime{0.0};
    bool m_initialized{false};
    bool m_levelComplete{false};
};

class ToadNpc {
public:
    bool initialize();
    void reset(const Environment& environment, const glm::vec3& playerSpawn);
    void update(const Player& player, bool interactPressed, float timeSeconds);
    void render(const Shader& shader, float timeSeconds) const;

    bool showPrompt() const { return m_playerNearby && !m_dialogOpen; }
    bool dialogOpen() const { return m_dialogOpen; }

private:
    std::shared_ptr<Texture2D> loadNpcTexture(const std::string& path);
    void buildFallbackModel();
    glm::mat4 modelMatrix(float timeSeconds) const;
    glm::vec3 findSafePosition(const Environment& environment, const glm::vec3& playerSpawn) const;

    std::vector<MissionRenderablePart> m_parts;
    std::vector<std::shared_ptr<Texture2D>> m_textures;
    glm::vec3 m_position{0.0f};
    glm::vec3 m_modelMin{0.0f};
    glm::vec3 m_modelMax{0.0f, 1.0f, 0.0f};
    glm::vec3 m_modelCenter{0.0f};
    float m_modelScale{1.0f};
    float m_facingYaw{0.0f};
    bool m_initialized{false};
    bool m_playerNearby{false};
    bool m_dialogOpen{false};
};

struct Mundo2Runtime {
    Environment environment;
    Player player;
    MissionManager mission;
    ToadNpc toad;
    AudioPlayer music;
    bool initialized{false};
    bool musicOpen{false};
    bool musicPlaying{false};
};

bool firstMouse = true;
double lastMouseX = WindowWidth * 0.5;
double lastMouseY = WindowHeight * 0.5;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
PlayMode currentMode = PlayMode::Mode3D;
EstadoJuego appState = EstadoJuego::MENU_PRINCIPAL;
EstadoJuego lastCursorState = EstadoJuego::MENU_PRINCIPAL;
bool lastToggleKey = false;
bool lastJumpKey = false;
bool lastEscapeKey = false;
bool lastInteractKey = false;
bool lastMouseButton = false;
float cameraYawDegrees = 0.0f;
float cameraPitchDegrees = 18.0f;
float locked2DDepth = 0.0f;
glm::vec3 gameplayCameraPosition{0.0f, 6.0f, 14.0f};
glm::vec3 gameplayCameraTarget{0.0f, 2.0f, 0.0f};
bool cameraInitialized = false;

std::string resolveAssetPath(const std::string& path) {
    const std::filesystem::path requested(path);
    const std::filesystem::path candidates[] = {
        requested,
        std::filesystem::path("..") / ".." / requested,
        std::filesystem::path("Laboratorio") / requested
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate.string();
        }
    }
    return path;
}

unsigned char toByte(float value) {
    return static_cast<unsigned char>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
}

std::wstring twoDigits(int value) {
    return (value < 10 ? L"0" : L"") + std::to_wstring(value);
}

std::wstring formatCoinProgress(int count) {
    return twoDigits(std::clamp(count, 0, MissionCoinTotal)) + L"/" + twoDigits(MissionCoinTotal);
}

bool boundsIntersect(const Bounds& a, const Bounds& b) {
    const glm::vec3 delta = glm::abs(a.center - b.center);
    const glm::vec3 total = a.halfExtent + b.halfExtent;
    return delta.x < total.x && delta.y < total.y && delta.z < total.z;
}

void bindSceneMaterial(const Shader& shader, const Material& material) {
    shader.setVec3("uMaterial.baseColor", material.baseColor);
    shader.setVec3("uMaterial.emissive", material.emissive);
    shader.setFloat("uMaterial.roughness", material.roughness);
    shader.setFloat("uMaterial.checkerStrength", material.checkerStrength);
    shader.setFloat("uMaterial.fogAmount", material.fogAmount);
    shader.setFloat("uMaterial.opacity", material.opacity);
    shader.setBool("uMaterial.hasTexture", material.texture && material.texture->valid());
    if (material.texture && material.texture->valid()) {
        material.texture->bind(0);
        shader.setInt("uMaterial.albedoMap", 0);
    }
}

glm::mat4 localPartMatrix(const MissionRenderablePart& part) {
    glm::mat4 model(1.0f);
    model = glm::translate(model, part.localPosition);
    model = glm::rotate(model, glm::radians(part.localRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(part.localRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(part.localRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, part.localScale);
    return model;
}

TextSprite createTextSprite(const std::wstring& text, int fontSize, const glm::vec3& color, int maxWidth, bool multiline, bool bold) {
    TextSprite sprite;
    HDC screenDc = GetDC(nullptr);
    if (screenDc == nullptr) {
        return sprite;
    }

    HDC memoryDc = CreateCompatibleDC(screenDc);
    if (memoryDc == nullptr) {
        ReleaseDC(nullptr, screenDc);
        return sprite;
    }

    const int fontWeight = bold ? FW_HEAVY : FW_SEMIBOLD;
    HFONT font = CreateFontW(-fontSize, 0, 0, 0, fontWeight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial Rounded MT Bold");
    if (font == nullptr) {
        font = CreateFontW(-fontSize, 0, 0, 0, fontWeight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
    }
    if (font == nullptr) {
        DeleteDC(memoryDc);
        ReleaseDC(nullptr, screenDc);
        return sprite;
    }

    HGDIOBJ previousFont = SelectObject(memoryDc, font);

    const UINT format = DT_CENTER | DT_NOCLIP | (multiline ? DT_WORDBREAK : (DT_SINGLELINE | DT_VCENTER));
    RECT measure{0, 0, maxWidth, 4096};
    DrawTextW(memoryDc, text.c_str(), -1, &measure, format | DT_CALCRECT);

    const int padding = std::max(8, fontSize / 3);
    const int measuredWidth = static_cast<int>(measure.right - measure.left);
    const int measuredHeight = static_cast<int>(measure.bottom - measure.top);
    const int width = std::max(8, std::min(maxWidth + padding * 2, measuredWidth + padding * 2));
    const int height = std::max(fontSize + padding * 2, measuredHeight + padding * 2);

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* rawPixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(memoryDc, &bitmapInfo, DIB_RGB_COLORS, &rawPixels, nullptr, 0);
    if (bitmap == nullptr || rawPixels == nullptr) {
        if (previousFont != nullptr && previousFont != HGDI_ERROR) {
            SelectObject(memoryDc, previousFont);
        }
        DeleteObject(font);
        DeleteDC(memoryDc);
        ReleaseDC(nullptr, screenDc);
        return sprite;
    }

    HGDIOBJ previousBitmap = SelectObject(memoryDc, bitmap);
    if (previousBitmap == nullptr || previousBitmap == HGDI_ERROR) {
        DeleteObject(bitmap);
        if (previousFont != nullptr && previousFont != HGDI_ERROR) {
            SelectObject(memoryDc, previousFont);
        }
        DeleteObject(font);
        DeleteDC(memoryDc);
        ReleaseDC(nullptr, screenDc);
        return sprite;
    }

    RECT fillRect{0, 0, width, height};
    HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(memoryDc, &fillRect, blackBrush);
    DeleteObject(blackBrush);

    SetBkMode(memoryDc, TRANSPARENT);
    SetTextColor(memoryDc, RGB(255, 255, 255));
    RECT drawRect{padding, padding, width - padding, height - padding};
    DrawTextW(memoryDc, text.c_str(), -1, &drawRect, format);

    std::vector<unsigned char> rgba(static_cast<size_t>(width * height * 4), 0);
    const unsigned char* source = static_cast<const unsigned char*>(rawPixels);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t sourceOffset = static_cast<size_t>((y * width + x) * 4);
            const unsigned char intensity = std::max({source[sourceOffset + 0], source[sourceOffset + 1], source[sourceOffset + 2]});
            const size_t destOffset = sourceOffset;
            rgba[destOffset + 0] = toByte(color.r);
            rgba[destOffset + 1] = toByte(color.g);
            rgba[destOffset + 2] = toByte(color.b);
            rgba[destOffset + 3] = intensity;
        }
    }

    SelectObject(memoryDc, previousBitmap);
    DeleteObject(bitmap);
    if (previousFont != nullptr && previousFont != HGDI_ERROR) {
        SelectObject(memoryDc, previousFont);
    }
    DeleteObject(font);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);

    sprite.texture = std::make_shared<Texture2D>();
    sprite.texture->createFromRGBA(width, height, rgba.data(), false);
    sprite.size = {static_cast<float>(width), static_cast<float>(height)};
    return sprite;
}

Rect centeredRect(float centerX, float y, float width, float height) {
    return {centerX - width * 0.5f, y, width, height};
}

Rect scaleRect(const Rect& rect, float scale) {
    const float newWidth = rect.width * scale;
    const float newHeight = rect.height * scale;
    return {rect.x + (rect.width - newWidth) * 0.5f, rect.y + (rect.height - newHeight) * 0.5f, newWidth, newHeight};
}

bool pointInRect(const Rect& rect, const glm::vec2& point) {
    return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y && point.y <= rect.y + rect.height;
}

void drawTexture(MenuContext& menu, const Texture2D& texture, const Rect& rect, const glm::vec4& tint, bool flipY = false) {
    if (!texture.valid()) {
        return;
    }

    const float topV = flipY ? 1.0f : 0.0f;
    const float bottomV = flipY ? 0.0f : 1.0f;
    const float vertices[] = {
        rect.x, rect.y, 0.0f, topV,
        rect.x + rect.width, rect.y, 1.0f, topV,
        rect.x + rect.width, rect.y + rect.height, 1.0f, bottomV,
        rect.x, rect.y, 0.0f, topV,
        rect.x + rect.width, rect.y + rect.height, 1.0f, bottomV,
        rect.x, rect.y + rect.height, 0.0f, bottomV
    };

    menu.shader.use();
    menu.shader.setVec4("uTint", tint);
    texture.bind(0);
    menu.shader.setInt("uTexture", 0);
    glBindVertexArray(menu.vao);
    glBindBuffer(GL_ARRAY_BUFFER, menu.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void drawRect(MenuContext& menu, const Rect& rect, const glm::vec4& color) {
    drawTexture(menu, menu.whiteTexture, rect, color);
}

void drawText(MenuContext& menu, const TextSprite& text, float x, float y, const glm::vec4& tint = glm::vec4(1.0f)) {
    if (!text.texture || !text.texture->valid()) {
        return;
    }
    drawTexture(menu, *text.texture, {x, y, text.size.x, text.size.y}, tint);
}

bool drawButton(MenuContext& menu, const TextSprite& text, const Rect& rect, const glm::vec2& mouse, bool clicked, bool enabled = true) {
    const bool hovered = enabled && pointInRect(rect, mouse);
    const Rect drawArea = scaleRect(rect, hovered ? 1.055f : 1.0f);
    drawRect(menu, {drawArea.x + 7.0f, drawArea.y + 8.0f, drawArea.width, drawArea.height}, {0.02f, 0.05f, 0.12f, 0.38f});
    drawRect(menu, {drawArea.x - 4.0f, drawArea.y - 4.0f, drawArea.width + 8.0f, drawArea.height + 8.0f},
        hovered ? glm::vec4(1.0f, 0.92f, 0.36f, 0.98f) : glm::vec4(0.12f, 0.33f, 0.54f, 0.96f));
    drawRect(menu, drawArea, enabled
        ? (hovered ? glm::vec4(1.0f, 0.56f, 0.20f, 0.98f) : glm::vec4(0.16f, 0.57f, 0.86f, 0.96f))
        : glm::vec4(0.30f, 0.35f, 0.40f, 0.72f));

    const float textX = drawArea.x + (drawArea.width - text.size.x) * 0.5f;
    const float textY = drawArea.y + (drawArea.height - text.size.y) * 0.5f - 1.0f;
    drawText(menu, text, textX, textY, enabled ? glm::vec4(1.0f) : glm::vec4(0.72f, 0.78f, 0.82f, 0.85f));
    return hovered && clicked;
}

void beginUiFrame(MenuContext& menu, int width, int height) {
    glDisable(GL_DEPTH_TEST);
    menu.shader.use();
    menu.shader.setMat4("uProjection", glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, -1.0f, 1.0f));
}

void drawFloatingCloud(MenuContext& menu, float x, float y, float width, float timeSeconds, float phase, float alpha) {
    const float aspect = menu.cloudTexture.width() > 0
        ? static_cast<float>(menu.cloudTexture.height()) / static_cast<float>(menu.cloudTexture.width())
        : 0.58f;
    const float height = width * aspect;
    const float bob = std::sin(timeSeconds * 1.15f + phase) * 8.0f;
    drawTexture(menu, menu.cloudTexture, {x, y + bob, width, height}, {1.0f, 1.0f, 1.0f, alpha}, true);
}

void drawMenuBackground(MenuContext& menu, int width, int height, float timeSeconds, bool showLogo = true) {
    if (menu.backgroundTexture.valid()) {
        drawTexture(menu, menu.backgroundTexture, {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)}, glm::vec4(1.0f), true);
    } else {
        drawRect(menu, {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)}, {0.37f, 0.43f, 0.78f, 1.0f});
    }
    drawRect(menu, {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)}, {0.04f, 0.06f, 0.16f, 0.16f});

    if (showLogo) {
        drawFloatingCloud(menu, width * 0.05f, 82.0f, std::min(width * 0.23f, 290.0f), timeSeconds, 0.0f, 0.64f);
        drawFloatingCloud(menu, width * 0.72f, 92.0f, std::min(width * 0.25f, 310.0f), timeSeconds, 1.7f, 0.62f);
        drawFloatingCloud(menu, width * 0.17f, 286.0f, std::min(width * 0.16f, 200.0f), timeSeconds, 3.1f, 0.36f);
        drawFloatingCloud(menu, width * 0.69f, 292.0f, std::min(width * 0.17f, 215.0f), timeSeconds, 4.4f, 0.34f);

        const float logoAspect = menu.logoTexture.width() > 0
            ? static_cast<float>(menu.logoTexture.height()) / static_cast<float>(menu.logoTexture.width())
            : 0.62f;
        const float logoWidth = std::min(static_cast<float>(width) * 0.46f, 560.0f);
        const float logoHeight = logoWidth * logoAspect;
        const float logoX = (static_cast<float>(width) - logoWidth) * 0.5f;
        const float logoY = 30.0f + std::sin(timeSeconds * 1.15f) * 7.0f;
        drawTexture(menu, menu.logoTexture, {logoX, logoY, logoWidth, logoHeight}, {1.0f, 1.0f, 1.0f, 1.0f}, true);
    } else {
        drawFloatingCloud(menu, -42.0f, 66.0f, std::min(width * 0.24f, 300.0f), timeSeconds, 0.3f, 0.42f);
        drawFloatingCloud(menu, width * 0.76f, 64.0f, std::min(width * 0.24f, 300.0f), timeSeconds, 2.2f, 0.40f);
    }
}

void drawPanel(MenuContext& menu, const Rect& rect) {
    drawRect(menu, {rect.x + 10.0f, rect.y + 12.0f, rect.width, rect.height}, {0.02f, 0.04f, 0.10f, 0.35f});
    drawRect(menu, {rect.x - 6.0f, rect.y - 6.0f, rect.width + 12.0f, rect.height + 12.0f}, {1.0f, 0.88f, 0.36f, 0.96f});
    drawRect(menu, rect, {0.08f, 0.24f, 0.42f, 0.92f});
}

void drawUnavailableMessage(MenuContext& menu, int width, int height, float timeSeconds) {
    if (timeSeconds > static_cast<float>(menu.notificationUntil)) {
        return;
    }

    const Rect panel = centeredRect(width * 0.5f, height - 132.0f, 560.0f, 72.0f);
    drawRect(menu, {panel.x + 6.0f, panel.y + 8.0f, panel.width, panel.height}, {0.02f, 0.04f, 0.08f, 0.45f});
    drawRect(menu, panel, {0.95f, 0.52f, 0.18f, 0.96f});
    drawText(menu, menu.noDisponible, panel.x + (panel.width - menu.noDisponible.size.x) * 0.5f, panel.y + 13.0f);
}

bool MissionManager::initialize() {
    if (m_initialized) {
        return true;
    }

    m_coinIcon.loadFromFile(resolveAssetPath("assets/images/coin_spin.png"), false);
    m_fallbackCoinMesh = Mesh::cylinder(32, 0.14f, 0.48f);
    m_starMesh = createStarMesh();

    m_fallbackCoinMaterial.baseColor = {1.0f, 0.74f, 0.08f};
    m_fallbackCoinMaterial.emissive = {0.85f, 0.42f, 0.04f};
    m_fallbackCoinMaterial.roughness = 0.38f;
    m_fallbackCoinMaterial.fogAmount = 0.18f;

    m_starMaterial.baseColor = {1.0f, 0.86f, 0.12f};
    m_starMaterial.emissive = {1.55f, 1.10f, 0.24f};
    m_starMaterial.roughness = 0.28f;
    m_starMaterial.fogAmount = 0.12f;

    loadCoinModel();
    loadStarModel();
    m_initialized = true;
    return true;
}

void MissionManager::reset(const Environment& environment, const glm::vec3& playerSpawn) {
    m_collectedCount = 0;
    m_messageCount = 0;
    m_coinMessageUntil = 0.0;
    m_starMessageUntil = 0.0;
    m_starFocusUntil = 0.0;
    m_victoryTime = 0.0;
    m_levelComplete = false;
    m_star = {};
    generarMonedas(environment, playerSpawn);
}

void MissionManager::generarMonedas(const Environment& environment, const glm::vec3& playerSpawn) {
    struct SurfaceCandidate {
        Bounds bounds;
        float top{0.0f};
        float area{0.0f};
    };

    m_coins.clear();
    std::vector<SurfaceCandidate> surfaces;
    const auto& colliders = environment.collisionPreview();
    const glm::vec3 worldMin = environment.worldMin();
    const glm::vec3 worldMax = environment.worldMax();
    const float reachableMinTop = playerSpawn.y - 1.10f;
    const float reachableMaxTop = playerSpawn.y + 1.05f;

    for (const Bounds& collider : colliders) {
        const float top = collider.center.y + collider.halfExtent.y;
        const float area = (collider.halfExtent.x * 2.0f) * (collider.halfExtent.z * 2.0f);
        const bool floorLike = collider.halfExtent.y <= 0.30f && area >= 0.35f && collider.halfExtent.x >= 0.24f && collider.halfExtent.z >= 0.24f;
        const bool reachableHeight = top >= reachableMinTop && top <= reachableMaxTop;
        if (floorLike && reachableHeight && top >= worldMin.y - 0.15f && top <= worldMax.y + 0.5f) {
            surfaces.push_back({collider, top, area});
        }
    }

    std::mt19937 rng(2242);
    std::uniform_real_distribution<float> jitter(-0.32f, 0.32f);

    auto percentile = [](std::vector<float> values, float amount) {
        if (values.empty()) {
            return 0.0f;
        }
        std::sort(values.begin(), values.end());
        const size_t index = std::min(values.size() - 1, static_cast<size_t>(std::round(amount * static_cast<float>(values.size() - 1))));
        return values[index];
    };

    std::vector<float> surfaceXs;
    std::vector<float> surfaceZs;
    surfaceXs.reserve(surfaces.size());
    surfaceZs.reserve(surfaces.size());
    for (const SurfaceCandidate& surface : surfaces) {
        surfaceXs.push_back(surface.bounds.center.x);
        surfaceZs.push_back(surface.bounds.center.z);
    }

    glm::vec3 playableMin = worldMin;
    glm::vec3 playableMax = worldMax;
    if (!surfaces.empty()) {
        playableMin.x = percentile(surfaceXs, 0.12f);
        playableMax.x = percentile(surfaceXs, 0.88f);
        playableMin.z = percentile(surfaceZs, 0.12f);
        playableMax.z = percentile(surfaceZs, 0.88f);
    }

    const float playablePadX = std::max((playableMax.x - playableMin.x) * 0.05f, 0.95f);
    const float playablePadZ = std::max((playableMax.z - playableMin.z) * 0.05f, 0.95f);
    const float minX = playableMin.x + playablePadX;
    const float maxX = playableMax.x - playablePadX;
    const float minZ = playableMin.z + playablePadZ;
    const float maxZ = playableMax.z - playablePadZ;
    const glm::vec2 center{(minX + maxX) * 0.5f, (minZ + maxZ) * 0.5f};
    const std::array<glm::vec2, MissionCoinTotal> anchors = {
        glm::vec2(minX, minZ),
        glm::vec2(maxX, minZ),
        glm::vec2(minX, maxZ),
        glm::vec2(maxX, maxZ),
        glm::vec2(center.x, minZ),
        glm::vec2(center.x, maxZ),
        glm::vec2(minX, center.y),
        glm::vec2(maxX, center.y),
        glm::mix(glm::vec2(minX, minZ), center, 0.45f),
        glm::mix(glm::vec2(maxX, maxZ), center, 0.45f)
    };

    auto placeNearAnchor = [&](const glm::vec2& anchor, float minCoinDistance) {
        if (m_coins.size() >= MissionCoinTotal || surfaces.empty()) {
            return;
        }

        std::vector<size_t> order;
        order.reserve(surfaces.size());
        for (size_t i = 0; i < surfaces.size(); ++i) {
            order.push_back(i);
        }

        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            const SurfaceCandidate& sa = surfaces[a];
            const SurfaceCandidate& sb = surfaces[b];
            const glm::vec2 ca{sa.bounds.center.x, sa.bounds.center.z};
            const glm::vec2 cb{sb.bounds.center.x, sb.bounds.center.z};
            const float da = glm::length(ca - anchor) - sa.area * 0.012f;
            const float db = glm::length(cb - anchor) - sb.area * 0.012f;
            return da < db;
        });

        const size_t tryCount = std::min<size_t>(order.size(), 40);
        for (size_t i = 0; i < tryCount; ++i) {
            const SurfaceCandidate& surface = surfaces[order[i]];
            const float safeX = std::max(surface.bounds.halfExtent.x - 0.72f, 0.0f);
            const float safeZ = std::max(surface.bounds.halfExtent.z - 0.72f, 0.0f);
            glm::vec3 position{
                std::clamp(anchor.x + jitter(rng), surface.bounds.center.x - safeX, surface.bounds.center.x + safeX),
                surface.top + 0.58f,
                std::clamp(anchor.y + jitter(rng), surface.bounds.center.z - safeZ, surface.bounds.center.z + safeZ)
            };

            if (validCoinPosition(position, colliders, m_coins, playerSpawn, glm::vec3(minX, worldMin.y, minZ), glm::vec3(maxX, worldMax.y, maxZ), minCoinDistance)) {
                Coin coin;
                coin.position = position;
                coin.phase = static_cast<float>(m_coins.size()) * 0.73f;
                m_coins.push_back(coin);
                return;
            }
        }
    };

    for (const glm::vec2& anchor : anchors) {
        placeNearAnchor(anchor, 4.2f);
    }
    for (const glm::vec2& anchor : anchors) {
        placeNearAnchor(anchor, 2.8f);
    }
    for (const SurfaceCandidate& surface : surfaces) {
        if (m_coins.size() >= MissionCoinTotal) {
            break;
        }
        const glm::vec3 position{surface.bounds.center.x, surface.top + 0.58f, surface.bounds.center.z};
        if (validCoinPosition(position, colliders, m_coins, playerSpawn, glm::vec3(minX, worldMin.y, minZ), glm::vec3(maxX, worldMax.y, maxZ), 2.2f)) {
            Coin coin;
            coin.position = position;
            coin.phase = static_cast<float>(m_coins.size()) * 0.73f;
            m_coins.push_back(coin);
        }
    }
    for (const SurfaceCandidate& surface : surfaces) {
        if (m_coins.size() >= MissionCoinTotal) {
            break;
        }
        const glm::vec3 position{surface.bounds.center.x, surface.top + 0.58f, surface.bounds.center.z};
        if (validCoinPosition(position, colliders, m_coins, playerSpawn, glm::vec3(minX, worldMin.y, minZ), glm::vec3(maxX, worldMax.y, maxZ), 1.55f)) {
            Coin coin;
            coin.position = position;
            coin.phase = static_cast<float>(m_coins.size()) * 0.73f;
            m_coins.push_back(coin);
        }
    }

    m_star.position = m_coins.empty() ? playerSpawn + glm::vec3(2.2f, 1.2f, 0.0f) : m_coins.front().position + glm::vec3(0.0f, 0.18f, 0.0f);
    m_star.active = false;
}

void MissionManager::update(const Player& player, float timeSeconds) {
    if (m_levelComplete) {
        return;
    }

    const Bounds playerBounds = player.bounds();
    for (Coin& coin : m_coins) {
        if (!coin.collected && boundsIntersect(playerBounds, coinBounds(coin))) {
            recolectarMoneda(coin, timeSeconds);
            break;
        }
    }

    if (m_star.active && boundsIntersect(playerBounds, starBounds())) {
        completarNivel(timeSeconds);
    }
}

void MissionManager::render(const Shader& shader, float timeSeconds) const {
    if (!m_initialized) {
        return;
    }

    for (const Coin& coin : m_coins) {
        if (coin.collected) {
            continue;
        }

        glm::mat4 model = coinModelMatrix(coin, timeSeconds);
        if (!m_coinParts.empty()) {
            for (const MissionRenderablePart& part : m_coinParts) {
                shader.use();
                shader.setMat4("uModel", model * localPartMatrix(part));
                shader.setFloat("uTime", timeSeconds);
                bindSceneMaterial(shader, part.material);
                part.mesh.draw();
            }
        } else {
            model = glm::rotate(model, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
            shader.use();
            shader.setMat4("uModel", model);
            shader.setFloat("uTime", timeSeconds);
            bindSceneMaterial(shader, m_fallbackCoinMaterial);
            m_fallbackCoinMesh.draw();
        }
    }

    if (m_star.active) {
        const glm::mat4 model = starModelMatrix(timeSeconds);
        if (!m_starParts.empty()) {
            for (const MissionRenderablePart& part : m_starParts) {
                shader.use();
                shader.setMat4("uModel", model * localPartMatrix(part));
                shader.setFloat("uTime", timeSeconds);
                bindSceneMaterial(shader, part.material);
                part.mesh.draw();
            }
        } else {
            shader.use();
            shader.setMat4("uModel", model);
            shader.setFloat("uTime", timeSeconds);
            bindSceneMaterial(shader, m_starMaterial);
            m_starMesh.draw();
        }
    }
}

bool MissionManager::loadCoinModel() {
    LoadedModel model = ModelLoader::loadModel(resolveAssetPath("assets/items/Coin/Coin.dae"));
    if (model.meshes.empty()) {
        std::cerr << "Coin model could not be loaded. Using procedural fallback coin." << std::endl;
        return false;
    }

    m_coinModelMin = model.minBounds;
    m_coinModelMax = model.maxBounds;
    m_coinModelCenter = (m_coinModelMin + m_coinModelMax) * 0.5f;
    const glm::vec3 size = m_coinModelMax - m_coinModelMin;
    const float maxExtent = std::max({size.x, size.y, size.z, 0.001f});
    m_coinModelScale = 0.62f / maxExtent;

    m_coinParts.clear();
    m_coinParts.reserve(model.meshes.size());
    for (LoadedMesh& mesh : model.meshes) {
        MissionRenderablePart part;
        if (mesh.materialIndex < model.materials.size()) {
            const LoadedMaterial& material = model.materials[mesh.materialIndex];
            part.material.baseColor = material.diffuseColor;
            part.material.opacity = material.opacity;
            part.material.texture = loadMissionTexture(material.diffuseTexturePath);
        } else {
            part.material.baseColor = {1.0f, 0.72f, 0.05f};
        }
        part.material.emissive = {0.72f, 0.36f, 0.04f};
        part.material.roughness = 0.42f;
        part.material.fogAmount = 0.16f;
        part.mesh = std::move(mesh.mesh);
        m_coinParts.push_back(std::move(part));
    }
    return true;
}

bool MissionManager::loadStarModel() {
    LoadedModel model = ModelLoader::loadModel(resolveAssetPath("assets/items/CrystalStars/Crystal Stars/goldstar.obj"));
    if (model.meshes.empty()) {
        std::cerr << "Crystal star model could not be loaded. Using procedural fallback star." << std::endl;
        return false;
    }

    m_starModelMin = model.minBounds;
    m_starModelMax = model.maxBounds;
    m_starModelCenter = (m_starModelMin + m_starModelMax) * 0.5f;
    const glm::vec3 size = m_starModelMax - m_starModelMin;
    const float maxExtent = std::max({size.x, size.y, size.z, 0.001f});
    m_starModelScale = 1.18f / maxExtent;

    m_starParts.clear();
    m_starParts.reserve(model.meshes.size());
    for (LoadedMesh& mesh : model.meshes) {
        MissionRenderablePart part;
        if (mesh.materialIndex < model.materials.size()) {
            const LoadedMaterial& material = model.materials[mesh.materialIndex];
            part.material.baseColor = material.diffuseColor;
            part.material.opacity = material.opacity;
            part.material.texture = loadMissionTexture(material.diffuseTexturePath);
        } else {
            part.material.baseColor = {1.0f, 0.82f, 0.18f};
        }
        part.material.emissive = {1.25f, 0.86f, 0.20f};
        part.material.roughness = 0.24f;
        part.material.fogAmount = 0.10f;
        part.mesh = std::move(mesh.mesh);
        m_starParts.push_back(std::move(part));
    }
    return true;
}

std::shared_ptr<Texture2D> MissionManager::loadMissionTexture(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    const std::filesystem::path original(path);
    const std::filesystem::path fileName = original.filename();
    const std::filesystem::path candidates[] = {
        original,
        std::filesystem::path("assets") / "items" / "Coin" / fileName,
        std::filesystem::path("assets") / "items" / "CrystalStars" / "Crystal Stars" / fileName,
        std::filesystem::path("..") / ".." / "assets" / "items" / "Coin" / fileName,
        std::filesystem::path("..") / ".." / "assets" / "items" / "CrystalStars" / "Crystal Stars" / fileName
    };

    std::filesystem::path resolved = original;
    for (const auto& candidate : candidates) {
        if (!candidate.empty() && std::filesystem::exists(candidate)) {
            resolved = std::filesystem::weakly_canonical(candidate);
            break;
        }
    }

    const std::string normalized = resolved.string();
    for (const auto& texture : m_textures) {
        if (texture && texture->sourcePath() == normalized) {
            return texture;
        }
    }

    auto texture = std::make_shared<Texture2D>();
    if (!texture->loadFromFile(normalized, false)) {
        return nullptr;
    }
    m_textures.push_back(texture);
    return texture;
}

Mesh MissionManager::createStarMesh() const {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    constexpr int points = 10;
    constexpr float frontZ = 0.08f;
    constexpr float backZ = -0.08f;

    vertices.push_back({{0.0f, 0.0f, frontZ}, {0.0f, 0.0f, 1.0f}, {0.5f, 0.5f}, glm::vec4(1.0f)});
    vertices.push_back({{0.0f, 0.0f, backZ}, {0.0f, 0.0f, -1.0f}, {0.5f, 0.5f}, glm::vec4(1.0f)});
    for (int i = 0; i < points; ++i) {
        const float radius = (i % 2 == 0) ? 0.72f : 0.33f;
        const float angle = glm::half_pi<float>() + glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(points);
        const float x = std::cos(angle) * radius;
        const float y = std::sin(angle) * radius;
        vertices.push_back({{x, y, frontZ}, {0.0f, 0.0f, 1.0f}, {(x / 1.6f) + 0.5f, (y / 1.6f) + 0.5f}, glm::vec4(1.0f)});
        vertices.push_back({{x, y, backZ}, {0.0f, 0.0f, -1.0f}, {(x / 1.6f) + 0.5f, (y / 1.6f) + 0.5f}, glm::vec4(1.0f)});
    }

    for (int i = 0; i < points; ++i) {
        const unsigned int frontA = 2u + static_cast<unsigned int>(i) * 2u;
        const unsigned int backA = frontA + 1u;
        const unsigned int frontB = 2u + static_cast<unsigned int>((i + 1) % points) * 2u;
        const unsigned int backB = frontB + 1u;
        indices.insert(indices.end(), {0u, frontA, frontB});
        indices.insert(indices.end(), {1u, backB, backA});
        indices.insert(indices.end(), {frontA, backA, backB, frontA, backB, frontB});
    }

    Mesh mesh;
    mesh.upload(vertices, indices);
    return mesh;
}

Bounds MissionManager::coinBounds(const Coin& coin) const {
    return {coin.position, {0.36f, 0.44f, 0.36f}};
}

Bounds MissionManager::starBounds() const {
    return {m_star.position, {0.82f, 0.88f, 0.82f}};
}

glm::mat4 MissionManager::coinModelMatrix(const Coin& coin, float timeSeconds) const {
    glm::mat4 model(1.0f);
    const float bob = std::sin(timeSeconds * 2.2f + coin.phase) * 0.10f;
    model = glm::translate(model, coin.position + glm::vec3(0.0f, bob, 0.0f));
    model = glm::rotate(model, timeSeconds * 4.8f + coin.phase, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(m_coinModelScale));
    model = glm::translate(model, -m_coinModelCenter);
    return model;
}

glm::mat4 MissionManager::starModelMatrix(float timeSeconds) const {
    glm::mat4 model(1.0f);
    const float bob = std::sin(timeSeconds * 2.8f) * 0.12f;
    model = glm::translate(model, m_star.position + glm::vec3(0.0f, bob, 0.0f));
    model = glm::rotate(model, timeSeconds * 2.6f, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(m_starModelScale));
    model = glm::translate(model, -m_starModelCenter);
    return model;
}

bool MissionManager::validCoinPosition(const glm::vec3& position, const std::vector<Bounds>& colliders, const std::vector<Coin>& placed, const glm::vec3& playerSpawn, const glm::vec3& worldMin, const glm::vec3& worldMax, float minCoinDistance) const {
    if (position.x < worldMin.x + 0.6f || position.x > worldMax.x - 0.6f || position.z < worldMin.z + 0.6f || position.z > worldMax.z - 0.6f) {
        return false;
    }

    if (glm::length(glm::vec2(position.x - playerSpawn.x, position.z - playerSpawn.z)) < 2.4f) {
        return false;
    }

    for (const Coin& coin : placed) {
        if (glm::length(glm::vec2(position.x - coin.position.x, position.z - coin.position.z)) < minCoinDistance) {
            return false;
        }
    }

    const Bounds candidate{position, {0.30f, 0.38f, 0.30f}};
    for (const Bounds& collider : colliders) {
        if (boundsIntersect(candidate, collider)) {
            return false;
        }

        const bool horizontalOverlap =
            std::abs(collider.center.x - position.x) < collider.halfExtent.x + 0.55f &&
            std::abs(collider.center.z - position.z) < collider.halfExtent.z + 0.55f;
        const float bottom = collider.center.y - collider.halfExtent.y;
        if (horizontalOverlap && bottom > position.y + 0.22f && bottom < position.y + 6.0f) {
            return false;
        }
    }
    return true;
}

void MissionManager::recolectarMoneda(Coin& coin, float timeSeconds) {
    if (coin.collected) {
        return;
    }

    coin.collected = true;
    m_collectedCount = std::min(m_collectedCount + 1, MissionCoinTotal);
    mostrarMensajeMonedaTemporal(timeSeconds);
    if (m_collectedCount >= MissionCoinTotal) {
        activarEstrella(timeSeconds);
    }
}

void MissionManager::mostrarMensajeMonedaTemporal(float timeSeconds) {
    m_messageCount = m_collectedCount;
    m_coinMessageUntil = static_cast<double>(timeSeconds) + 3.5;
}

void MissionManager::activarEstrella(float timeSeconds) {
    if (m_star.active) {
        return;
    }
    m_star.active = true;
    m_starMessageUntil = static_cast<double>(timeSeconds) + 3.5;
    m_starFocusUntil = static_cast<double>(timeSeconds) + 2.85;
}

void MissionManager::completarNivel(float timeSeconds) {
    if (m_levelComplete || m_collectedCount < MissionCoinTotal) {
        return;
    }
    m_levelComplete = true;
    m_victoryTime = timeSeconds;
}

bool ToadNpc::initialize() {
    if (m_initialized) {
        return true;
    }

    LoadedModel model = ModelLoader::loadModel(resolveAssetPath("assets/npcs/RussT/Russ T/Russ T.dae"));
    bool hasRenderableMesh = false;
    if (model.meshes.empty()) {
        buildFallbackModel();
    } else {
        m_modelMin = model.minBounds;
        m_modelMax = model.maxBounds;
        m_parts.reserve(model.meshes.size());
        for (LoadedMesh& mesh : model.meshes) {
            MissionRenderablePart part;
            if (mesh.materialIndex < model.materials.size()) {
                const LoadedMaterial& material = model.materials[mesh.materialIndex];
                part.material.baseColor = material.diffuseColor;
                part.material.opacity = material.opacity;
                part.material.texture = loadNpcTexture(material.diffuseTexturePath);
            } else {
                part.material.baseColor = {1.0f, 0.92f, 0.78f};
            }
            part.material.roughness = 0.82f;
            part.material.fogAmount = 0.22f;
            part.mesh = std::move(mesh.mesh);
            hasRenderableMesh = hasRenderableMesh || part.mesh.valid();
            m_parts.push_back(std::move(part));
        }
        if (!hasRenderableMesh) {
            buildFallbackModel();
        }
    }

    m_modelCenter = (m_modelMin + m_modelMax) * 0.5f;
    const float modelHeight = std::max(m_modelMax.y - m_modelMin.y, 0.001f);
    m_modelScale = 0.98f / modelHeight;
    m_initialized = true;
    return true;
}

void ToadNpc::reset(const Environment& environment, const glm::vec3& playerSpawn) {
    m_position = findSafePosition(environment, playerSpawn);
    m_facingYaw = 0.0f;
    m_playerNearby = false;
    m_dialogOpen = false;
}

void ToadNpc::update(const Player& player, bool interactPressed, float) {
    const glm::vec3 delta = player.position() - m_position;
    const float distance = glm::length(glm::vec2(delta.x, delta.z));
    m_playerNearby = distance <= 2.45f && std::abs(delta.y) <= 2.2f;

    if (distance > 0.05f) {
        m_facingYaw = std::atan2(delta.x, delta.z);
    }

    if (m_playerNearby && interactPressed) {
        m_dialogOpen = !m_dialogOpen;
    } else if (!m_playerNearby) {
        m_dialogOpen = false;
    }
}

void ToadNpc::render(const Shader& shader, float timeSeconds) const {
    if (!m_initialized) {
        return;
    }

    const glm::mat4 model = modelMatrix(timeSeconds);
    for (const MissionRenderablePart& part : m_parts) {
        shader.use();
        shader.setMat4("uModel", model * localPartMatrix(part));
        shader.setFloat("uTime", timeSeconds);
        bindSceneMaterial(shader, part.material);
        part.mesh.draw();
    }
}

std::shared_ptr<Texture2D> ToadNpc::loadNpcTexture(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    const std::filesystem::path original(path);
    const std::filesystem::path fileName = original.filename();
    const std::filesystem::path candidates[] = {
        original,
        std::filesystem::path("assets") / "npcs" / "RussT" / "Russ T" / fileName,
        std::filesystem::path("..") / ".." / "assets" / "npcs" / "RussT" / "Russ T" / fileName
    };

    std::filesystem::path resolved = original;
    for (const auto& candidate : candidates) {
        if (!candidate.empty() && std::filesystem::exists(candidate)) {
            resolved = std::filesystem::weakly_canonical(candidate);
            break;
        }
    }

    const std::string normalized = resolved.string();
    for (const auto& texture : m_textures) {
        if (texture && texture->sourcePath() == normalized) {
            return texture;
        }
    }

    auto texture = std::make_shared<Texture2D>();
    if (!texture->loadFromFile(normalized, false)) {
        return nullptr;
    }
    m_textures.push_back(texture);
    return texture;
}

void ToadNpc::buildFallbackModel() {
    m_parts.clear();
    m_modelMin = {-0.58f, 0.0f, -0.36f};
    m_modelMax = {0.58f, 1.25f, 0.36f};

    auto makePart = [&](Mesh mesh, const glm::vec3& color, const glm::vec3& position, const glm::vec3& scale) {
        MissionRenderablePart part;
        part.mesh = std::move(mesh);
        part.material.baseColor = color;
        part.material.roughness = 0.82f;
        part.material.fogAmount = 0.22f;
        part.localPosition = position;
        part.localScale = scale;
        m_parts.push_back(std::move(part));
    };

    makePart(Mesh::cylinder(28), {0.20f, 0.22f, 0.72f}, {0.0f, 0.36f, 0.0f}, {0.42f, 0.56f, 0.42f});
    makePart(Mesh::cylinder(32), {1.0f, 0.86f, 0.66f}, {0.0f, 0.82f, 0.0f}, {0.43f, 0.36f, 0.43f});
    makePart(Mesh::cylinder(36), {0.94f, 0.94f, 0.90f}, {0.0f, 1.12f, 0.0f}, {0.68f, 0.24f, 0.68f});
    makePart(Mesh::cylinder(18), {0.22f, 0.38f, 0.92f}, {-0.30f, 1.20f, 0.17f}, {0.16f, 0.07f, 0.16f});
    makePart(Mesh::cylinder(18), {0.22f, 0.38f, 0.92f}, {0.30f, 1.20f, 0.17f}, {0.16f, 0.07f, 0.16f});
    makePart(Mesh::cube(), {0.02f, 0.02f, 0.025f}, {-0.13f, 0.88f, 0.34f}, {0.11f, 0.06f, 0.025f});
    makePart(Mesh::cube(), {0.02f, 0.02f, 0.025f}, {0.13f, 0.88f, 0.34f}, {0.11f, 0.06f, 0.025f});
    makePart(Mesh::cube(), {0.02f, 0.02f, 0.025f}, {0.0f, 0.88f, 0.35f}, {0.10f, 0.018f, 0.018f});
    makePart(Mesh::cube(), {0.12f, 0.10f, 0.08f}, {-0.20f, 0.05f, 0.06f}, {0.24f, 0.11f, 0.34f});
    makePart(Mesh::cube(), {0.12f, 0.10f, 0.08f}, {0.20f, 0.05f, 0.06f}, {0.24f, 0.11f, 0.34f});
}

glm::mat4 ToadNpc::modelMatrix(float timeSeconds) const {
    const float bob = std::sin(timeSeconds * 3.2f) * 0.035f;
    const float sway = std::sin(timeSeconds * 2.1f) * 0.055f;
    glm::mat4 model(1.0f);
    model = glm::translate(model, m_position + glm::vec3(0.0f, bob, 0.0f));
    model = glm::rotate(model, m_facingYaw + sway, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(m_modelScale));
    model = glm::translate(model, {-m_modelCenter.x, -m_modelMin.y, -m_modelCenter.z});
    return model;
}

glm::vec3 ToadNpc::findSafePosition(const Environment& environment, const glm::vec3& playerSpawn) const {
    const auto& colliders = environment.collisionPreview();
    const glm::vec3 worldMin = environment.worldMin();
    const glm::vec3 worldMax = environment.worldMax();
    const glm::vec2 fixedTarget{playerSpawn.x - 1.65f, playerSpawn.z + 1.35f};
    glm::vec3 best = playerSpawn + glm::vec3(-1.65f, 0.05f, 1.35f);
    float bestScore = std::numeric_limits<float>::max();

    for (const Bounds& collider : colliders) {
        const float top = collider.center.y + collider.halfExtent.y;
        const float area = (collider.halfExtent.x * 2.0f) * (collider.halfExtent.z * 2.0f);
        const bool floorLike = collider.halfExtent.y <= 0.30f && area >= 0.45f && collider.halfExtent.x >= 0.28f && collider.halfExtent.z >= 0.28f;
        if (!floorLike || top < worldMin.y - 0.15f || top > worldMax.y + 0.5f) {
            continue;
        }

        const float safeX = std::max(collider.halfExtent.x - 0.75f, 0.0f);
        const float safeZ = std::max(collider.halfExtent.z - 0.75f, 0.0f);
        glm::vec3 candidate{
            std::clamp(fixedTarget.x, collider.center.x - safeX, collider.center.x + safeX),
            top + 0.05f,
            std::clamp(fixedTarget.y, collider.center.z - safeZ, collider.center.z + safeZ)
        };

        const bool insideWorld =
            candidate.x > worldMin.x + 1.0f && candidate.x < worldMax.x - 1.0f &&
            candidate.z > worldMin.z + 1.0f && candidate.z < worldMax.z - 1.0f;
        const float spawnDistance = glm::length(glm::vec2(candidate.x - playerSpawn.x, candidate.z - playerSpawn.z));
        if (!insideWorld || spawnDistance < 1.1f || spawnDistance > 3.6f) {
            continue;
        }

        bool blockedAbove = false;
        for (const Bounds& other : colliders) {
            const bool horizontalOverlap =
                std::abs(other.center.x - candidate.x) < other.halfExtent.x + 0.65f &&
                std::abs(other.center.z - candidate.z) < other.halfExtent.z + 0.65f;
            const float bottom = other.center.y - other.halfExtent.y;
            if (horizontalOverlap && bottom > candidate.y + 0.45f && bottom < candidate.y + 2.5f) {
                blockedAbove = true;
                break;
            }
        }
        if (blockedAbove) {
            continue;
        }

        const float score = glm::length(glm::vec2(candidate.x, candidate.z) - fixedTarget) + std::abs(candidate.y - playerSpawn.y) * 0.8f;
        if (score < bestScore) {
            bestScore = score;
            best = candidate;
        }
    }

    return best;
}

void drawSpinningCoinIcon(MenuContext& menu, const Texture2D& coinIcon, float x, float y, float size, float timeSeconds, float alpha = 1.0f) {
    const float spin = 0.35f + 0.65f * std::abs(std::cos(timeSeconds * 7.2f));
    const float width = size * spin;
    const Rect iconRect{x + (size - width) * 0.5f, y, width, size};
    if (coinIcon.valid()) {
        drawTexture(menu, coinIcon, iconRect, {1.0f, 1.0f, 1.0f, alpha}, true);
    } else {
        drawRect(menu, iconRect, {1.0f, 0.72f, 0.08f, alpha});
    }
}

template <typename Mission>
void drawMissionHud(MenuContext& menu, const Mission& mission, int width, int height, float timeSeconds) {
    beginUiFrame(menu, width, height);

    const int coinCount = std::clamp(mission.collectedCount(), 0, MissionCoinTotal);
    const float counterWidth = 196.0f;
    const Rect counter{std::max(18.0f, static_cast<float>(width) - counterWidth - 28.0f), 22.0f, counterWidth, 58.0f};
    drawRect(menu, {counter.x + 6.0f, counter.y + 7.0f, counter.width, counter.height}, {0.01f, 0.02f, 0.05f, 0.45f});
    drawRect(menu, {counter.x - 4.0f, counter.y - 4.0f, counter.width + 8.0f, counter.height + 8.0f}, {1.0f, 0.84f, 0.24f, 0.96f});
    drawRect(menu, counter, {0.06f, 0.17f, 0.32f, 0.88f});
    drawSpinningCoinIcon(menu, mission.coinIconTexture(), counter.x + 14.0f, counter.y + 9.0f, 40.0f, timeSeconds);
    drawText(menu, menu.coinCounters[coinCount], counter.x + counter.width - menu.coinCounters[coinCount].size.x - 18.0f, counter.y + (counter.height - menu.coinCounters[coinCount].size.y) * 0.5f - 1.0f);

    if (mission.showCoinMessage(timeSeconds)) {
        const int messageCount = std::clamp(mission.messageCount(), 0, MissionCoinTotal);
        const Rect messagePanel = centeredRect(width * 0.5f, 92.0f, 210.0f, 62.0f);
        drawRect(menu, {messagePanel.x + 6.0f, messagePanel.y + 7.0f, messagePanel.width, messagePanel.height}, {0.01f, 0.02f, 0.05f, 0.42f});
        drawRect(menu, messagePanel, {0.95f, 0.48f, 0.12f, 0.93f});
        drawText(menu, menu.coinMessages[messageCount], messagePanel.x + 24.0f, messagePanel.y + (messagePanel.height - menu.coinMessages[messageCount].size.y) * 0.5f);
        drawSpinningCoinIcon(menu, mission.coinIconTexture(), messagePanel.x + messagePanel.width - 57.0f, messagePanel.y + 12.0f, 38.0f, timeSeconds);
    }

    if (mission.showStarMessage(timeSeconds)) {
        const Rect starPanel = centeredRect(width * 0.5f, 162.0f, 430.0f, 58.0f);
        drawRect(menu, {starPanel.x + 6.0f, starPanel.y + 7.0f, starPanel.width, starPanel.height}, {0.01f, 0.02f, 0.05f, 0.42f});
        drawRect(menu, starPanel, {0.10f, 0.27f, 0.52f, 0.94f});
        drawText(menu, menu.estrellaLista, starPanel.x + (starPanel.width - menu.estrellaLista.size.x) * 0.5f, starPanel.y + (starPanel.height - menu.estrellaLista.size.y) * 0.5f);
    }

    if (mission.levelComplete()) {
        drawRect(menu, {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)}, {0.01f, 0.02f, 0.06f, 0.58f});
        const Rect victoryPanel = centeredRect(width * 0.5f, height * 0.5f - 72.0f, 560.0f, 144.0f);
        drawPanel(menu, victoryPanel);
        drawText(menu, menu.nivelCompletado, victoryPanel.x + (victoryPanel.width - menu.nivelCompletado.size.x) * 0.5f, victoryPanel.y + (victoryPanel.height - menu.nivelCompletado.size.y) * 0.5f);
    }
}

void drawMapa1CombatHud(MenuContext& menu, const Mapa1& mapa1, int width, int height, float timeSeconds) {
    beginUiFrame(menu, width, height);
    const Rect healthPanel{22.0f, 22.0f, 282.0f, 72.0f};
    drawRect(menu, {healthPanel.x + 6.0f, healthPanel.y + 7.0f, healthPanel.width, healthPanel.height}, {0.01f, 0.02f, 0.05f, 0.48f});
    drawRect(menu, healthPanel, {0.06f, 0.12f, 0.24f, 0.94f});
    drawText(menu, menu.vidaJugador, healthPanel.x + 14.0f, healthPanel.y + 10.0f);
    const int maximumHealth = std::max(1, mapa1.maximumHealth());
    const float segmentWidth = 66.0f;
    for (int index = 0; index < maximumHealth; ++index) {
        const bool active = index < mapa1.currentHealth();
        drawRect(
            menu,
            {healthPanel.x + 14.0f + index * (segmentWidth + 9.0f), healthPanel.y + 43.0f, segmentWidth, 16.0f},
            active ? glm::vec4(0.92f, 0.18f, 0.16f, 1.0f) : glm::vec4(0.20f, 0.23f, 0.30f, 0.90f));
    }

    if (mapa1.chargingAttack()) {
        const Rect chargePanel{22.0f, 106.0f, 282.0f, 54.0f};
        drawRect(menu, chargePanel, {0.06f, 0.12f, 0.24f, 0.94f});
        drawText(menu, menu.cargandoAtaque, chargePanel.x + 14.0f, chargePanel.y + 7.0f);
        drawRect(menu, {chargePanel.x + 14.0f, chargePanel.y + 35.0f, 254.0f, 10.0f}, {0.20f, 0.23f, 0.30f, 0.90f});
        drawRect(menu, {chargePanel.x + 14.0f, chargePanel.y + 35.0f, 254.0f * mapa1.chargeRatio(), 10.0f},
            mapa1.chargeRatio() >= 1.0f
                ? glm::vec4(1.00f, 0.84f, 0.12f, 1.0f)
                : glm::vec4(0.24f, 0.88f, 1.00f, 1.0f));
    }

    if (mapa1.parryActive(timeSeconds)) {
        const Rect parryPanel = centeredRect(width * 0.5f, 38.0f, 188.0f, 48.0f);
        drawRect(menu, parryPanel, {0.08f, 0.62f, 0.68f, 0.94f});
        drawText(menu, menu.parryActivo,
            parryPanel.x + (parryPanel.width - menu.parryActivo.size.x) * 0.5f,
            parryPanel.y + (parryPanel.height - menu.parryActivo.size.y) * 0.5f);
    }

    if (mapa1.showCombatHint(timeSeconds)) {
        const Rect panel = centeredRect(width * 0.5f, static_cast<float>(height) - 116.0f, 760.0f, 62.0f);
        drawRect(menu, {panel.x + 7.0f, panel.y + 8.0f, panel.width, panel.height}, {0.01f, 0.02f, 0.05f, 0.48f});
        drawRect(menu, {panel.x - 4.0f, panel.y - 4.0f, panel.width + 8.0f, panel.height + 8.0f}, {1.0f, 0.80f, 0.20f, 0.98f});
        drawRect(menu, panel, {0.82f, 0.18f, 0.10f, 0.96f});
        drawText(menu, menu.combateSolo2D,
            panel.x + (panel.width - menu.combateSolo2D.size.x) * 0.5f,
            panel.y + (panel.height - menu.combateSolo2D.size.y) * 0.5f);
    }
}

void drawToadHud(MenuContext& menu, const ToadNpc& toad, int width, int height) {
    if (!toad.showPrompt() && !toad.dialogOpen()) {
        return;
    }

    beginUiFrame(menu, width, height);
    if (toad.dialogOpen()) {
        const float panelWidth = std::min(880.0f, static_cast<float>(width) - 96.0f);
        const Rect panel = centeredRect(width * 0.5f, static_cast<float>(height) - 250.0f, panelWidth, 205.0f);
        drawPanel(menu, panel);
        drawText(menu, menu.nombreToad, panel.x + 30.0f, panel.y + 18.0f);
        drawText(menu, menu.dialogoToad, panel.x + (panel.width - menu.dialogoToad.size.x) * 0.5f, panel.y + 58.0f);
    } else {
        const Rect prompt = centeredRect(width * 0.5f, static_cast<float>(height) - 108.0f, 360.0f, 54.0f);
        drawRect(menu, {prompt.x + 6.0f, prompt.y + 7.0f, prompt.width, prompt.height}, {0.01f, 0.02f, 0.05f, 0.45f});
        drawRect(menu, prompt, {0.07f, 0.20f, 0.38f, 0.92f});
        drawText(menu, menu.promptHablarToad, prompt.x + (prompt.width - menu.promptHablarToad.size.x) * 0.5f, prompt.y + (prompt.height - menu.promptHablarToad.size.y) * 0.5f);
    }
}

void mostrarMenuPrincipal(MenuContext& menu, int width, int height, float timeSeconds, const glm::vec2& mouse, bool clicked, GLFWwindow* window) {
    beginUiFrame(menu, width, height);
    drawMenuBackground(menu, width, height, timeSeconds);

    const float buttonWidth = 330.0f;
    const float buttonHeight = 54.0f;
    const float centerX = width * 0.5f;
    const float startY = std::max(382.0f, height * 0.53f);
    if (drawButton(menu, menu.jugar, centeredRect(centerX, startY, buttonWidth, buttonHeight), mouse, clicked)) {
        appState = EstadoJuego::MENU_MUNDOS;
        menu.notificationUntil = 0.0;
    }
    if (drawButton(menu, menu.comoJugar, centeredRect(centerX, startY + 66.0f, buttonWidth, buttonHeight), mouse, clicked)) {
        appState = EstadoJuego::COMO_JUGAR;
    }
    if (drawButton(menu, menu.creditos, centeredRect(centerX, startY + 132.0f, buttonWidth, buttonHeight), mouse, clicked)) {
        appState = EstadoJuego::CREDITOS;
    }
    if (drawButton(menu, menu.salir, centeredRect(centerX, startY + 198.0f, buttonWidth, buttonHeight), mouse, clicked)) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

void mostrarMenuMundos(MenuContext& menu, int width, int height, float timeSeconds, const glm::vec2& mouse, bool clicked) {
    beginUiFrame(menu, width, height);
    drawMenuBackground(menu, width, height, timeSeconds, false);

    const Rect panel = centeredRect(width * 0.5f, 178.0f, 620.0f, 420.0f);
    drawPanel(menu, panel);

    const float buttonWidth = 320.0f;
    const float buttonHeight = 54.0f;
    const float centerX = width * 0.5f;
    const float startY = 224.0f;
    if (drawButton(menu, menu.mundo1, centeredRect(centerX, startY, buttonWidth, buttonHeight), mouse, clicked)) {
        appState = EstadoJuego::MUNDO_1;
        menu.notificationUntil = 0.0;
    }
    if (drawButton(menu, menu.mundo2, centeredRect(centerX, startY + 68.0f, buttonWidth, buttonHeight), mouse, clicked)) {
        appState = EstadoJuego::MUNDO_2;
        menu.notificationUntil = 0.0;
    }
    if (drawButton(menu, menu.mundo3, centeredRect(centerX, startY + 136.0f, buttonWidth, buttonHeight), mouse, clicked)) {
        menu.notificationUntil = glfwGetTime() + 2.3;
    }
    if (drawButton(menu, menu.mundo4, centeredRect(centerX, startY + 204.0f, buttonWidth, buttonHeight), mouse, clicked)) {
        menu.notificationUntil = glfwGetTime() + 2.3;
    }
    if (drawButton(menu, menu.volver, centeredRect(centerX, startY + 284.0f, buttonWidth, buttonHeight), mouse, clicked)) {
        appState = EstadoJuego::MENU_PRINCIPAL;
        menu.notificationUntil = 0.0;
    }
    drawUnavailableMessage(menu, width, height, timeSeconds);
}

void mostrarComoJugar(MenuContext& menu, int width, int height, float timeSeconds, const glm::vec2& mouse, bool clicked) {
    beginUiFrame(menu, width, height);
    drawMenuBackground(menu, width, height, timeSeconds, false);

    const Rect panel = centeredRect(width * 0.5f, 155.0f, 790.0f, 420.0f);
    drawPanel(menu, panel);
    drawText(menu, menu.tituloComoJugar, panel.x + (panel.width - menu.tituloComoJugar.size.x) * 0.5f, panel.y + 34.0f);
    drawText(menu, menu.textoComoJugar, panel.x + (panel.width - menu.textoComoJugar.size.x) * 0.5f, panel.y + 118.0f);

    if (drawButton(menu, menu.volver, centeredRect(width * 0.5f, panel.y + panel.height - 83.0f, 260.0f, 55.0f), mouse, clicked)) {
        appState = EstadoJuego::MENU_PRINCIPAL;
    }
}

void mostrarCreditos(MenuContext& menu, int width, int height, float timeSeconds, const glm::vec2& mouse, bool clicked) {
    beginUiFrame(menu, width, height);
    drawMenuBackground(menu, width, height, timeSeconds, false);

    const Rect panel = centeredRect(width * 0.5f, 160.0f, 750.0f, 400.0f);
    drawPanel(menu, panel);
    drawText(menu, menu.tituloCreditos, panel.x + (panel.width - menu.tituloCreditos.size.x) * 0.5f, panel.y + 38.0f);
    drawText(menu, menu.textoCreditos, panel.x + (panel.width - menu.textoCreditos.size.x) * 0.5f, panel.y + 120.0f);

    if (drawButton(menu, menu.volver, centeredRect(width * 0.5f, panel.y + panel.height - 82.0f, 260.0f, 55.0f), mouse, clicked)) {
        appState = EstadoJuego::MENU_PRINCIPAL;
    }
}

void resetGameplayView(const Player& player) {
    currentMode = PlayMode::Mode3D;
    lastToggleKey = false;
    lastJumpKey = false;
    lastInteractKey = false;
    cameraYawDegrees = 0.0f;
    cameraPitchDegrees = 18.0f;
    cameraInitialized = false;
    firstMouse = true;
    locked2DDepth = player.position().z;
}

bool iniciarMundo2(Mundo2Runtime& mundo2) {
    if (mundo2.initialized) {
        if (mundo2.musicOpen && !mundo2.musicPlaying) {
            mundo2.musicPlaying = mundo2.music.playLoop();
        }
        return true;
    }

    mundo2.environment.create();
    mundo2.musicOpen = mundo2.music.open(resolveAssetPath("assets/audio/graffiti_underground_loop.mp3"));
    if (mundo2.musicOpen) {
        mundo2.musicPlaying = mundo2.music.playLoop();
    } else {
        std::cerr << "Background music could not be started." << std::endl;
    }

    mundo2.player.load(resolveAssetPath("assets/characters/mario64_pinix_style/model/scene.gltf"));
    mundo2.player.spawnAt(mundo2.environment.recommendedSpawnPoint());
    mundo2.mission.initialize();
    mundo2.mission.reset(mundo2.environment, mundo2.environment.recommendedSpawnPoint());
    mundo2.toad.initialize();
    mundo2.toad.reset(mundo2.environment, mundo2.environment.recommendedSpawnPoint());
    resetGameplayView(mundo2.player);

    std::cout << "Mundo 2 ready. Collision volumes: " << mundo2.environment.collisionPreview().size() << std::endl;
    std::cout << "Controls 3D: WASD move, mouse camera, Space jump, E switch to 2D, Esc back to menu." << std::endl;
    std::cout << "Controls 2D: A/D move, Space jump, E switch to 3D." << std::endl;
    mundo2.initialized = true;
    return true;
}

void volverAlMenu(Mundo2Runtime& mundo2) {
    if (mundo2.musicOpen && mundo2.musicPlaying) {
        mundo2.music.stop();
        mundo2.musicPlaying = false;
    }
    if (mundo2.musicOpen) {
        mundo2.music.close();
        mundo2.musicOpen = false;
    }
    mundo2.initialized = false;
    appState = EstadoJuego::MENU_PRINCIPAL;
}

void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouseCallback(GLFWwindow*, double x, double y) {
    if (appState != EstadoJuego::MUNDO_2) {
        lastMouseX = x;
        lastMouseY = y;
        return;
    }

    if (firstMouse) {
        lastMouseX = x;
        lastMouseY = y;
        firstMouse = false;
    }
    const float deltaX = static_cast<float>(x - lastMouseX);
    const float deltaY = static_cast<float>(lastMouseY - y);
    lastMouseX = x;
    lastMouseY = y;

    if (currentMode == PlayMode::Mode3D) {
        cameraYawDegrees -= deltaX * 0.10f;
        cameraPitchDegrees += deltaY * 0.08f;
        cameraPitchDegrees = std::clamp(cameraPitchDegrees, 10.0f, 34.0f);
    }
}

void updateCursorMode(GLFWwindow* window) {
    if (appState == lastCursorState) {
        return;
    }

    if (appState == EstadoJuego::MUNDO_2) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
    } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    lastCursorState = appState;
}

void processAppInput(GLFWwindow* window, Mapa1& mapa1, Mundo2Runtime& mundo2) {
    const bool escapeDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (escapeDown && !lastEscapeKey) {
        switch (appState) {
        case EstadoJuego::MENU_PRINCIPAL:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case EstadoJuego::MENU_MUNDOS:
        case EstadoJuego::COMO_JUGAR:
        case EstadoJuego::CREDITOS:
            appState = EstadoJuego::MENU_PRINCIPAL;
            break;
        case EstadoJuego::MUNDO_1:
            mapa1.shutdown();
            glfwSetWindowTitle(window, "Paper Pinix");
            appState = EstadoJuego::MENU_PRINCIPAL;
            break;
        case EstadoJuego::MUNDO_2:
            volverAlMenu(mundo2);
            break;
        }
    }
    lastEscapeKey = escapeDown;
}

PlayerInput buildPlayerInput(GLFWwindow* window, const Player& player) {
    const bool toggleDown = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
    if (toggleDown && !lastToggleKey) {
        if (currentMode == PlayMode::Mode3D) {
            currentMode = PlayMode::Mode2D;
            locked2DDepth = player.position().z;
        } else {
            currentMode = PlayMode::Mode3D;
        }
    }
    lastToggleKey = toggleDown;

    PlayerInput input;
    input.mode = currentMode;
    input.cameraYawRadians = glm::radians(cameraYawDegrees);
    input.lockedDepth = locked2DDepth;

    const bool left = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
    const bool right = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
    const bool forward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    const bool backward = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    input.move.x = (right ? 1.0f : 0.0f) - (left ? 1.0f : 0.0f);
    input.move.y = currentMode == PlayMode::Mode3D ? (forward ? 1.0f : 0.0f) - (backward ? 1.0f : 0.0f) : 0.0f;
    if (glm::length(input.move) > 1.0f) {
        input.move = glm::normalize(input.move);
    }

    const bool jumpDown = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    input.jumpPressed = jumpDown && !lastJumpKey;
    lastJumpKey = jumpDown;
    return input;
}

void updateGameplayCamera(const Player& player, const Environment& environment, const MissionManager& mission, float timeSeconds, float dt) {
    const glm::vec3 playerTarget = player.position() + glm::vec3(0.0f, 0.50f, 0.0f);
    glm::vec3 desiredTarget = playerTarget;
    glm::vec3 desiredPosition;

    if (mission.starFocusActive(timeSeconds)) {
        const glm::vec3 star = mission.starPosition();
        glm::vec3 viewDirection = player.position() - star;
        viewDirection.y = 0.0f;
        if (glm::length(viewDirection) < 0.1f) {
            viewDirection = glm::vec3(0.0f, 0.0f, 1.0f);
        }
        viewDirection = glm::normalize(viewDirection);
        desiredTarget = star + glm::vec3(0.0f, 0.34f, 0.0f);
        desiredPosition = desiredTarget + viewDirection * 4.8f + glm::vec3(0.0f, 2.15f, 0.0f);
    } else if (currentMode == PlayMode::Mode3D) {
        const float yaw = glm::radians(cameraYawDegrees);
        const float pitch = glm::radians(cameraPitchDegrees);
        const float distance = 4.25f;
        const float horizontalDistance = std::cos(pitch) * distance;
        const glm::vec3 orbitOffset(
            std::sin(yaw) * horizontalDistance,
            0.55f + std::sin(pitch) * distance,
            std::cos(yaw) * horizontalDistance);
        desiredPosition = desiredTarget + orbitOffset;
    } else {
        desiredTarget = player.position() + glm::vec3(0.0f, 0.56f, 0.0f);
        desiredPosition = desiredTarget + glm::vec3(0.0f, 1.28f, 6.2f);
    }

    auto clampToWorld = [&](glm::vec3 value, float margin) {
        const glm::vec3 worldMin = environment.worldMin();
        const glm::vec3 worldMax = environment.worldMax();
        if (worldMax.x > worldMin.x + margin * 2.0f) {
            value.x = std::clamp(value.x, worldMin.x + margin, worldMax.x - margin);
        }
        if (worldMax.z > worldMin.z + margin * 2.0f) {
            value.z = std::clamp(value.z, worldMin.z + margin, worldMax.z - margin);
        }
        value.y = std::clamp(value.y, worldMin.y + 0.85f, worldMax.y + 4.0f);
        return value;
    };

    desiredTarget = clampToWorld(desiredTarget, 1.05f);
    desiredPosition = clampToWorld(desiredPosition, 0.78f);
    if (glm::length(desiredPosition - desiredTarget) < 1.15f) {
        desiredPosition = clampToWorld(desiredTarget + glm::vec3(0.0f, 1.65f, 2.25f), 0.78f);
    }

    const float smoothing = 1.0f - std::exp(-7.2f * dt);
    if (!cameraInitialized) {
        gameplayCameraPosition = desiredPosition;
        gameplayCameraTarget = desiredTarget;
        cameraInitialized = true;
    } else {
        gameplayCameraPosition = glm::mix(gameplayCameraPosition, desiredPosition, smoothing);
        gameplayCameraTarget = glm::mix(gameplayCameraTarget, desiredTarget, smoothing);
    }
}

void uploadCommonSceneUniforms(const Shader& shader, const Environment& environment, const glm::vec3& cameraPosition, const glm::mat4& view, const glm::mat4& projection, float timeSeconds) {
    shader.use();
    shader.setMat4("uView", view);
    shader.setMat4("uProjection", projection);
    shader.setVec3("uCameraPosition", cameraPosition);
    shader.setVec3("uAmbientColor", {0.30f, 0.30f, 0.34f});
    shader.setVec3("uDirectionalLight.direction", {-0.30f, -0.80f, -0.40f});
    shader.setVec3("uDirectionalLight.color", {0.52f, 0.54f, 0.58f});
    shader.setFloat("uTime", timeSeconds);

    const auto& lights = environment.lights();
    const int count = std::min(static_cast<int>(lights.size()), MaxLights);
    shader.setInt("uPointLightCount", count);
    for (int i = 0; i < count; ++i) {
        const std::string prefix = "uPointLights[" + std::to_string(i) + "]";
        const float flicker = 0.88f + 0.12f * std::sin(timeSeconds * 3.0f + static_cast<float>(i) * 1.73f);
        shader.setVec3(prefix + ".position", lights[i].position);
        shader.setVec3(prefix + ".color", lights[i].color);
        shader.setFloat(prefix + ".intensity", lights[i].intensity * flicker);
        shader.setFloat(prefix + ".radius", lights[i].radius);
    }
}

bool initializeMenu(MenuContext& menu) {
    menu.shader.load(resolveAssetPath("shaders/ui.vert"), resolveAssetPath("shaders/ui.frag"));
    if (menu.shader.id() == 0) {
        return false;
    }

    const unsigned char whitePixel[] = {255, 255, 255, 255};
    menu.whiteTexture.createFromRGBA(1, 1, whitePixel, false);
    menu.logoTexture.loadFromFile(resolveAssetPath("assets/logos/PaperPinixLogo/Paper_Pinix_full_logo.png"), false);
    menu.cloudTexture.loadFromFile(resolveAssetPath("assets/ui/PaperPinix_LakituCloud_menu.png"), false);
    menu.backgroundTexture.loadFromFile(resolveAssetPath("assets/ui/PaperPinix_MenuBackground.png"), false);

    glGenVertexArrays(1, &menu.vao);
    glGenBuffers(1, &menu.vbo);
    glBindVertexArray(menu.vao);
    glBindBuffer(GL_ARRAY_BUFFER, menu.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(sizeof(float) * 2));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    const glm::vec3 white(1.0f);
    const glm::vec3 titleColor(1.0f, 0.92f, 0.35f);
    menu.jugar = createTextSprite(L"Jugar", 34, white, 280, false, true);
    menu.comoJugar = createTextSprite(L"C\u00f3mo jugar", 31, white, 310, false, true);
    menu.creditos = createTextSprite(L"Cr\u00e9ditos", 32, white, 280, false, true);
    menu.salir = createTextSprite(L"Salir", 34, white, 260, false, true);
    menu.mundo1 = createTextSprite(L"Mundo 1", 32, white, 280, false, true);
    menu.mundo2 = createTextSprite(L"Mundo 2", 32, white, 280, false, true);
    menu.mundo3 = createTextSprite(L"Mundo 3", 32, white, 280, false, true);
    menu.mundo4 = createTextSprite(L"Mundo 4", 32, white, 280, false, true);
    menu.volver = createTextSprite(L"Volver", 31, white, 250, false, true);
    menu.tituloComoJugar = createTextSprite(L"C\u00f3mo jugar", 44, titleColor, 560, false, true);
    menu.textoComoJugar = createTextSprite(
        L"- Usa las teclas de movimiento para controlar al personaje.\n"
        L"- Evita obst\u00e1culos y enemigos.\n"
        L"- Mundo 1: apunta y dispara con el mouse; mantenlo para cargar.\n"
        L"- Pulsa F justo antes del impacto para hacer parry.\n"
        L"- Completa el nivel para ganar.\n"
        L"- Presiona ESC o el bot\u00f3n Volver para regresar al men\u00fa.",
        23, white, 700, true, false);
    menu.tituloCreditos = createTextSprite(L"Cr\u00e9ditos", 44, titleColor, 520, false, true);
    menu.textoCreditos = createTextSprite(
        L"Paper Pinix\n"
        L"Proyecto avanzado\n"
        L"Desarrollado por: [Nombre del estudiante]\n"
        L"Mundo 2: [Nombre del estudiante]",
        27, white, 610, true, false);
    menu.noDisponible = createTextSprite(L"Este mundo a\u00fan no est\u00e1 disponible", 26, white, 510, false, true);
    for (int i = 0; i <= MissionCoinTotal; ++i) {
        menu.coinCounters[i] = createTextSprite(formatCoinProgress(i), 28, white, 132, false, true);
        menu.coinMessages[i] = createTextSprite(formatCoinProgress(i), 31, white, 132, false, true);
    }
    menu.estrellaLista = createTextSprite(L"\u00a1La estrella apareci\u00f3!", 29, titleColor, 400, false, true);
    menu.nivelCompletado = createTextSprite(L"\u00a1Nivel completado!", 52, titleColor, 520, false, true);
    menu.combateSolo2D = createTextSprite(L"\u00a1Peligro! Cambia a 2D con TAB para detener a los enemigos.", 27, white, 720, false, true);
    menu.vidaJugador = createTextSprite(L"VIDA", 25, white, 120, false, false);
    menu.cargandoAtaque = createTextSprite(L"CARGANDO TIRO", 21, white, 250, false, false);
    menu.parryActivo = createTextSprite(L"PARRY", 23, white, 150, false, true);
    menu.promptHablarToad = createTextSprite(L"Pulsa F para hablar", 28, white, 330, false, true);
    menu.nombreToad = createTextSprite(L"Toad", 30, titleColor, 170, false, true);
    menu.dialogoToad = createTextSprite(
        L"\u00a1Oh nooo! Todos estos enemigos vinieron a estorbar...\n"
        L"Para ganar, recoge las 10 monedas del mapa. Cuando las tengas,\n"
        L"aparecer\u00e1 una estrella de cristal. T\u00f3mala para completar el nivel.",
        21, white, 790, true, false);
    return true;
}

void shutdownMenu(MenuContext& menu) {
    if (menu.vbo != 0) {
        glDeleteBuffers(1, &menu.vbo);
        menu.vbo = 0;
    }
    if (menu.vao != 0) {
        glDeleteVertexArrays(1, &menu.vao);
        menu.vao = 0;
    }
}

void renderMundo2(GLFWwindow* window, Mundo2Runtime& mundo2, MenuContext& menu, const Shader& sceneShader, const Shader& lavaShader, float now) {
    const PlayerInput playerInput = buildPlayerInput(window, mundo2.player);
    const bool interactDown = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    const bool interactPressed = interactDown && !lastInteractKey;
    lastInteractKey = interactDown;

    mundo2.player.update(playerInput, mundo2.environment.collisionPreview(), mundo2.environment.worldMin(), mundo2.environment.worldMax(), deltaTime);
    mundo2.mission.update(mundo2.player, now);
    mundo2.toad.update(mundo2.player, interactPressed, now);
    updateGameplayCamera(mundo2.player, mundo2.environment, mundo2.mission, now, deltaTime);

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    const glm::mat4 view = glm::lookAt(gameplayCameraPosition, gameplayCameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 180.0f);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.018f, 0.026f, 0.040f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    uploadCommonSceneUniforms(sceneShader, mundo2.environment, gameplayCameraPosition, view, projection, now);
    lavaShader.use();
    lavaShader.setMat4("uView", view);
    lavaShader.setMat4("uProjection", projection);
    lavaShader.setFloat("uTime", now);

    mundo2.environment.render(sceneShader, lavaShader, now);
    mundo2.mission.render(sceneShader, now);
    mundo2.toad.render(sceneShader, now);
    mundo2.player.render(sceneShader);
    drawMissionHud(menu, mundo2.mission, width, height, now);
    drawToadHud(menu, mundo2.toad, width, height);
}
}

int main(int argc, char** argv) {
    const bool mapa1SmokeTest = argc > 1 && std::string(argv[1]) == "--smoke-mapa1";
    const bool mapa1CombatSmokeTest = argc > 1 && std::string(argv[1]) == "--smoke-mapa1-combat";

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW." << std::endl;
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (mapa1SmokeTest || mapa1CombatSmokeTest) {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

    GLFWwindow* window = glfwCreateWindow(WindowWidth, WindowHeight, "Paper Pinix", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    glGetError();

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.16f, 0.50f, 0.80f, 1.0f);

    Shader sceneShader(resolveAssetPath("shaders/scene.vert"), resolveAssetPath("shaders/scene.frag"));
    Shader lavaShader(resolveAssetPath("shaders/lava.vert"), resolveAssetPath("shaders/lava.frag"));
    if (sceneShader.id() == 0 || lavaShader.id() == 0) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    MenuContext menu;
    if (!initializeMenu(menu)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    Mapa1 mapa1;
    Mundo2Runtime mundo2;

    if (mapa1SmokeTest || mapa1CombatSmokeTest) {
        bool loaded = mapa1.initialize(false);
        if (loaded && mapa1CombatSmokeTest) {
            loaded = mapa1.runCombatSmokeTest();
        } else if (loaded) {
            mapa1.render(window, 1.0f / 60.0f);
        }
        mapa1.shutdown();
        shutdownMenu(menu);
        glfwDestroyWindow(window);
        glfwTerminate();
        return loaded ? 0 : 1;
    }

    while (!glfwWindowShouldClose(window)) {
        const float now = static_cast<float>(glfwGetTime());
        deltaTime = now - lastFrame;
        lastFrame = now;

        updateCursorMode(window);
        processAppInput(window, mapa1, mundo2);

        double cursorX = 0.0;
        double cursorY = 0.0;
        glfwGetCursorPos(window, &cursorX, &cursorY);
        const bool mouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool clicked = mouseDown && !lastMouseButton;
        lastMouseButton = mouseDown;

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);

        if (appState == EstadoJuego::MUNDO_1) {
            if (!mapa1.initialize()) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            } else {
                mapa1.render(window, deltaTime);
                drawMissionHud(menu, mapa1, width, height, now);
                drawMapa1CombatHud(menu, mapa1, width, height, now);
            }
        } else if (appState == EstadoJuego::MUNDO_2) {
            if (!iniciarMundo2(mundo2)) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            } else {
                renderMundo2(window, mundo2, menu, sceneShader, lavaShader, now);
            }
        } else {
            glDisable(GL_DEPTH_TEST);
            glClearColor(0.16f, 0.50f, 0.80f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            const glm::vec2 mouse(static_cast<float>(cursorX), static_cast<float>(cursorY));
            switch (appState) {
            case EstadoJuego::MENU_PRINCIPAL:
                mostrarMenuPrincipal(menu, width, height, now, mouse, clicked, window);
                break;
            case EstadoJuego::MENU_MUNDOS:
                mostrarMenuMundos(menu, width, height, now, mouse, clicked);
                break;
            case EstadoJuego::COMO_JUGAR:
                mostrarComoJugar(menu, width, height, now, mouse, clicked);
                break;
            case EstadoJuego::CREDITOS:
                mostrarCreditos(menu, width, height, now, mouse, clicked);
                break;
            case EstadoJuego::MUNDO_1:
            case EstadoJuego::MUNDO_2:
                break;
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    mapa1.shutdown();
    shutdownMenu(menu);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
