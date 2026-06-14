#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include "AudioPlayer.h"
#include "Environment.h"
#include "GameRuntime.h"
#include "GameSystems.h"
#include "GameUI.h"
#include "Map3.h"
#include "Map4.h"
#include "Mapa1.h"
#include "Player.h"
#include "Shader.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <vector>

constexpr int WindowWidth = 1280;
constexpr int WindowHeight = 720;
constexpr int MaxLights = 24;
constexpr float DefaultCamera3DDistance = 4.25f;
constexpr float Map4Camera3DDistance = 3.10f;
constexpr float DefaultCamera2DDistance = 6.2f;
constexpr float Map4Camera2DDistance = 3.35f;
constexpr float DefaultCamera2DTargetHeight = 0.56f;
constexpr float DefaultCamera2DHeight = 0.88f;

enum class EstadoJuego {
    MENU_PRINCIPAL,
    MENU_MUNDOS,
    COMO_JUGAR,
    CREDITOS,
    CARGANDO,
    MUNDO_1,
    MUNDO_2,
    MUNDO_3,
    MUNDO_4
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
PlayMode currentMode = PlayMode::Mode2D;
EstadoJuego appState = EstadoJuego::MENU_PRINCIPAL;
EstadoJuego lastCursorState = EstadoJuego::MENU_PRINCIPAL;
EstadoJuego loadingTarget = EstadoJuego::MENU_PRINCIPAL;
bool loadingScreenPresented = false;
bool lastToggleKey = false;
bool lastJumpKey = false;
bool lastEscapeKey = false;
bool lastInteractKey = false;
bool lastMouseButton = false;
bool lastShieldKey = false;
double pendingScrollY = 0.0;
float mapa1ShopScroll = 0.0f;
float cameraYawDegrees = 0.0f;
float cameraPitchDegrees = 18.0f;
float locked2DDepth = 0.0f;
glm::vec3 gameplayCameraPosition{0.0f, 6.0f, 14.0f};
glm::vec3 gameplayCameraTarget{0.0f, 2.0f, 0.0f};
bool cameraInitialized = false;
double modeSwitchUnavailableUntil = 0.0;

void solicitarCargaMundo(EstadoJuego target) {
    loadingTarget = target;
    loadingScreenPresented = false;
    appState = EstadoJuego::CARGANDO;
}

bool environmentUsable(const Environment& environment) {
    // Comprueba si un mapa cargó colisiones y límites válidos antes de usarlo en juego.
    const glm::vec3 worldMin = environment.worldMin();
    const glm::vec3 worldMax = environment.worldMax();
    return !environment.collisionPreview().empty() &&
        worldMax.x > worldMin.x &&
        worldMax.y > worldMin.y &&
        worldMax.z > worldMin.z;
}

void appendDimensionRestrictionColliders(std::vector<Bounds>& colliders, const Environment& environment, float lockedDepth) {
    (void)colliders;
    (void)environment;
    (void)lockedDepth;
}

std::string resolveAssetPath(const std::string& path) {
    // Busca assets en varias raíces del proyecto para tolerar diferencias de ejecución.
    const std::filesystem::path requested(path);
    const std::filesystem::path candidates[] = {
        // Se prueban varias raíces porque el juego puede ejecutarse desde distintos directorios.
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

std::string resolveFirstExistingAsset(const std::initializer_list<std::string>& paths) {
    // Devuelve la primera ruta real disponible dentro de una lista de candidatos.
    for (const std::string& path : paths) {
        // Este helper evita repetir comprobaciones manuales al elegir assets alternativos.
        const std::string resolved = resolveAssetPath(path);
        if (std::filesystem::exists(resolved)) {
            return resolved;
        }
    }
    return paths.size() > 0 ? resolveAssetPath(*paths.begin()) : std::string();
}

std::shared_ptr<Texture2D> loadTextureFromMaterial(
    const LoadedMaterial& material,
    const std::filesystem::path& modelPath,
    std::vector<std::shared_ptr<Texture2D>>& cache) {
    // Resuelve texturas externas o embebidas sin duplicar cargas en memoria.
    auto loadByResolvedPath = [&](const std::filesystem::path& candidate) -> std::shared_ptr<Texture2D> {
        if (candidate.empty() || !std::filesystem::exists(candidate)) {
            return nullptr;
        }

        const std::string normalized = std::filesystem::weakly_canonical(candidate).string();
        for (const auto& cached : cache) {
            if (cached && cached->sourcePath() == normalized) {
                return cached;
            }
        }

        auto texture = std::make_shared<Texture2D>();
        if (!texture->loadFromFile(normalized, false)) {
            return nullptr;
        }
        cache.push_back(texture);
        return texture;
    };

    if (!material.diffuseTexturePath.empty()) {
        const std::filesystem::path original(material.diffuseTexturePath);
        const std::filesystem::path fileName = original.filename();
        const std::filesystem::path modelDir = modelPath.parent_path();
        const std::filesystem::path candidates[] = {
            original,
            modelDir / fileName,
            modelDir / "textures" / fileName,
            std::filesystem::path(resolveAssetPath((std::filesystem::path("assets") / "Mundos" / fileName).string())),
            std::filesystem::path(resolveAssetPath((std::filesystem::path("assets") / "mapa 4" / fileName).string()))
        };

        for (const auto& candidate : candidates) {
            auto resolved = loadByResolvedPath(candidate);
            if (resolved && resolved->valid()) {
                return resolved;
            }
        }
    }

    if (material.embeddedTextureData.empty()) {
        return nullptr;
    }

    auto embedded = std::make_shared<Texture2D>();
    if (material.embeddedTextureCompressed) {
        if (!embedded->loadFromMemory(material.embeddedTextureData.data(), static_cast<int>(material.embeddedTextureData.size()), false)) {
            return nullptr;
        }
    } else if (material.embeddedTextureWidth > 0 && material.embeddedTextureHeight > 0) {
        embedded->createFromRGBA(material.embeddedTextureWidth, material.embeddedTextureHeight, material.embeddedTextureData.data(), false);
    } else {
        return nullptr;
    }

    cache.push_back(embedded);
    return embedded;
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
    // Intersección AABB reutilizada por colisiones simples del runtime.
    const glm::vec3 delta = glm::abs(a.center - b.center);
    const glm::vec3 total = a.halfExtent + b.halfExtent;
    return delta.x < total.x && delta.y < total.y && delta.z < total.z;
}

void bindSceneMaterial(const Shader& shader, const Material& material) {
    // Centraliza el envío de materiales para que todos los renderizados usen el mismo contrato del shader.
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

glm::mat4 localPartMatrix(const MissionRenderablePart& part);


glm::mat4 localPartMatrix(const MissionRenderablePart& part) {
    // Construye la transformación local de una pieza importada dentro de un modelo compuesto.
    glm::mat4 model(1.0f);
    model = glm::translate(model, part.localPosition);
    model = glm::rotate(model, glm::radians(part.localRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(part.localRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(part.localRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, part.localScale);
    return model;
}

TextSprite createTextSprite(const std::wstring& text, int fontSize, const glm::vec3& color, int maxWidth, bool multiline, bool bold) {
    // Rasteriza texto con GDI a una textura OpenGL reutilizable para HUD y menús.
    TextSprite sprite;
    HDC screenDc = GetDC(nullptr);
    // El texto del HUD se genera una vez como textura para que dibujarlo luego sea barato.
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
    const int width = std::max(measuredWidth + padding * 2, padding * 2 + 1);
    const int height = std::max(measuredHeight + padding * 2, padding * 2 + 1);

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
    std::memset(rawPixels, 0, static_cast<size_t>(width * height * 4));
    SetBkMode(memoryDc, TRANSPARENT);
    SetTextColor(memoryDc, RGB(255, 255, 255));
    RECT drawArea{padding, padding, width - padding, height - padding};
    DrawTextW(memoryDc, text.c_str(), -1, &drawArea, format);

    std::vector<unsigned char> rgba(static_cast<size_t>(width * height * 4), 0);
    const unsigned char* source = static_cast<const unsigned char*>(rawPixels);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t offset = static_cast<size_t>((y * width + x) * 4);
            const unsigned char intensity = std::max({source[offset + 0], source[offset + 1], source[offset + 2]});
            rgba[offset + 0] = toByte(color.r);
            rgba[offset + 1] = toByte(color.g);
            rgba[offset + 2] = toByte(color.b);
            rgba[offset + 3] = intensity;
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

void beginUiFrame(MenuContext& menu, int width, int height) {
    // Prepara una proyección 2D común para todo el HUD y los menús.
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    menu.shader.use();
    menu.shader.setMat4("uProjection", glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f));
    glBindVertexArray(menu.vao);
}

void drawTexture(MenuContext& menu, const Texture2D& texture, const Rect& rect, const glm::vec4& tint, bool textured, bool flipY = true) {
    // Helper base para dibujar sprites, fondos y texto rasterizado en pantalla.
    const Texture2D& activeTexture = (textured && texture.valid()) ? texture : menu.whiteTexture;
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

    glBindBuffer(GL_ARRAY_BUFFER, menu.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    activeTexture.bind(0);
    menu.shader.setInt("uTexture", 0);
    menu.shader.setVec4("uTint", tint);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void drawRect(MenuContext& menu, const Rect& rect, const glm::vec4& color) {
    // Los paneles sólidos del HUD salen de este helper para mantener la estética consistente.
    drawTexture(menu, menu.whiteTexture, rect, color, false);
}

void drawText(MenuContext& menu, const TextSprite& text, float x, float y, const glm::vec4& tint) {
    // El texto ya llega pre-renderizado como textura; aquí solo se posiciona y tiñe.
    if (!text.texture || !text.texture->valid()) {
        // Si el sprite no se pudo rasterizar, simplemente no se dibuja.
        return;
    }
    drawTexture(menu, *text.texture, {x, y, text.size.x, text.size.y}, tint, true, false);
}

void drawPanel(MenuContext& menu, const Rect& rect) {
    // Panel temático base usado por tutoriales, victoria y diálogos.
    // El borde dorado y la sombra se reutilizan en casi toda la interfaz del juego.
    drawRect(menu, {rect.x + 7.0f, rect.y + 8.0f, rect.width, rect.height}, {0.01f, 0.02f, 0.05f, 0.45f});
    drawRect(menu, {rect.x - 4.0f, rect.y - 4.0f, rect.width + 8.0f, rect.height + 8.0f}, {1.0f, 0.80f, 0.20f, 0.98f});
    drawRect(menu, rect, {0.08f, 0.16f, 0.30f, 0.94f});
}

bool drawButton(MenuContext& menu, const TextSprite& text, const Rect& rect, const glm::vec2& mouse, bool clicked, float timeSeconds, bool enabled = true) {
    // Los botones comparten hover, escalado y brillo para que el menú se sienta consistente.
    const bool hovered =
        enabled &&
        mouse.x >= rect.x && mouse.x <= rect.x + rect.width &&
        mouse.y >= rect.y && mouse.y <= rect.y + rect.height;
    const float hoverPulse = hovered ? (0.5f + 0.5f * std::sin(timeSeconds * 8.0f)) : 0.0f;
    const Rect drawArea = scaleRect(rect, hovered ? 1.04f : 1.0f);
    const float textX = drawArea.x + (drawArea.width - text.size.x) * 0.5f;
    const float textY = drawArea.y + (drawArea.height - text.size.y) * 0.5f;

    drawRect(menu, {drawArea.x + 6.0f, drawArea.y + 7.0f, drawArea.width, drawArea.height}, {0.01f, 0.02f, 0.05f, 0.45f});
    drawRect(menu, drawArea, enabled
        ? (hovered ? glm::vec4(1.0f, 0.56f, 0.20f, 0.98f) : glm::vec4(0.16f, 0.57f, 0.86f, 0.96f))
        : glm::vec4(0.30f, 0.35f, 0.40f, 0.72f));

    if (hovered) {
        const float glowInset = 5.0f + hoverPulse * 2.0f;
        drawRect(menu,
            {drawArea.x + glowInset, drawArea.y + glowInset, drawArea.width - glowInset * 2.0f, drawArea.height - glowInset * 2.0f},
            {1.0f, 0.86f, 0.34f, 0.10f + hoverPulse * 0.08f});

        const float shineWidth = drawArea.width * 0.18f;
        const float shineTravel = std::fmod(timeSeconds * 150.0f, drawArea.width + shineWidth * 2.0f) - shineWidth;
        drawRect(menu,
            {drawArea.x + shineTravel, drawArea.y + 4.0f, shineWidth, std::max(0.0f, drawArea.height - 8.0f)},
            {1.0f, 0.98f, 0.86f, 0.12f});
    }

    drawText(menu, text, textX, textY, enabled
        ? (hovered ? glm::vec4(1.0f, 0.99f, 0.94f, 1.0f) : glm::vec4(1.0f))
        : glm::vec4(0.72f, 0.78f, 0.82f, 0.85f));
    return hovered && clicked;
}

void drawDecorCloud(MenuContext& menu, float x, float y, float width, float alpha, float phase, float timeSeconds) {
    // Nubes decorativas animadas para que el menú no se vea estático.
    if (!menu.cloudTexture.valid() || menu.cloudTexture.width() <= 0) {
        // La decoración es opcional; si falta la textura el menú sigue funcionando.
        return;
    }
    const float aspect = static_cast<float>(menu.cloudTexture.height()) / static_cast<float>(menu.cloudTexture.width());
    const float height = width * aspect;
    const float bob = std::sin(timeSeconds * 1.15f + phase) * 8.0f;
    drawTexture(menu, menu.cloudTexture, {x, y + bob, width, height}, {1.0f, 1.0f, 1.0f, alpha}, true);
}

void drawMenuBackground(MenuContext& menu, int width, int height, float timeSeconds, bool showLogo = true) {
    // Compone el fondo principal del menú a partir de textura base, nubes y logo.
    if (menu.backgroundTexture.valid()) {
        drawTexture(menu, menu.backgroundTexture, {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)}, glm::vec4(1.0f), true);
    } else {
        drawRect(menu, {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)}, {0.08f, 0.18f, 0.30f, 1.0f});
    }

    drawDecorCloud(menu, 82.0f, 84.0f, 184.0f, 0.78f, 0.0f, timeSeconds);
    drawDecorCloud(menu, static_cast<float>(width) - 264.0f, 132.0f, 208.0f, 0.68f, 1.8f, timeSeconds);
    drawDecorCloud(menu, static_cast<float>(width) * 0.5f - 120.0f, static_cast<float>(height) - 182.0f, 240.0f, 0.48f, 3.2f, timeSeconds);

    if (showLogo && menu.logoTexture.valid()) {
        const float logoWidth = std::min(static_cast<float>(width) * 0.36f, 420.0f);
        const float aspect = static_cast<float>(menu.logoTexture.height()) / std::max(1, menu.logoTexture.width());
        const float logoHeight = logoWidth * aspect;
        drawTexture(menu, menu.logoTexture, centeredRect(width * 0.5f, 48.0f, logoWidth, logoHeight), glm::vec4(1.0f), true);
    }
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

void drawModeSwitchUnavailableMessage(MenuContext& menu, int width, int height, float timeSeconds) {
    if (timeSeconds > static_cast<float>(modeSwitchUnavailableUntil)) {
        return;
    }

    beginUiFrame(menu, width, height);
    const Rect panel = centeredRect(width * 0.5f, height - 132.0f, 360.0f, 64.0f);
    drawRect(menu, {panel.x + 6.0f, panel.y + 8.0f, panel.width, panel.height}, {0.02f, 0.04f, 0.08f, 0.45f});
    drawRect(menu, panel, {0.95f, 0.52f, 0.18f, 0.96f});
    drawText(menu, menu.modoNoDisponible,
        panel.x + (panel.width - menu.modoNoDisponible.size.x) * 0.5f,
        panel.y + (panel.height - menu.modoNoDisponible.size.y) * 0.5f);
}

bool MissionManager::initialize() {
    // Carga una sola vez los recursos visuales de monedas y estrella del objetivo principal.
    if (m_initialized) {
        return true;
    }

    m_coinIcon.loadFromFile(resolveAssetPath("assets/images/coin_spin.png"), false);
    m_fallbackCoinMesh = Mesh::cylinder(32, 0.14f, 0.48f);
    m_starMesh = createStarMesh();

    m_fallbackCoinMaterial.baseColor = {1.0f, 0.74f, 0.08f};
    m_fallbackCoinMaterial.emissive = {0.0f, 0.0f, 0.0f};
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
    // Reinicia el progreso del nivel y vuelve a poblar el objetivo coleccionable.
    m_collectedCount = 0;
    m_messageCount = 0;
    m_coinMessageUntil = 0.0;
    m_starMessageUntil = 0.0;
    m_starFocusUntil = 0.0;
    m_victoryTime = 0.0;
    m_levelComplete = false;
    m_marioMapStyle = isMarioMapa4Environment(environment);
    m_star = {};
    generarMonedas(environment, playerSpawn);
}

void MissionManager::generarMonedas(const Environment& environment, const glm::vec3& playerSpawn) {
    // Distribuye las monedas sobre superficies válidas del mapa evitando zonas imposibles.
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
    const bool marioMapStyle = isMarioMapa4Environment(environment);
    const float reachableMinTop = marioMapStyle ? playerSpawn.y - 0.45f : playerSpawn.y - 1.10f;
    const float reachableMaxTop = marioMapStyle ? playerSpawn.y + 1.35f : playerSpawn.y + 1.05f;

    for (const Bounds& collider : colliders) {
        const float top = collider.center.y + collider.halfExtent.y;
        const float area = (collider.halfExtent.x * 2.0f) * (collider.halfExtent.z * 2.0f);
        const bool floorLike = collider.halfExtent.y <= 0.32f && area >= 0.20f && collider.halfExtent.x >= 0.18f && collider.halfExtent.z >= 0.18f;
        const bool reachableHeight = top >= reachableMinTop && top <= reachableMaxTop;
        const bool stableMarioSurface = !marioMapStyle ||
            (area >= 0.95f && collider.halfExtent.x >= 0.42f && collider.halfExtent.z >= 0.42f);
        if (floorLike && reachableHeight && stableMarioSurface && top >= worldMin.y - 0.15f && top <= worldMax.y + 0.5f) {
            surfaces.push_back({collider, top, area});
        }
    }

    std::mt19937 rng(2242);
    std::uniform_real_distribution<float> jitter(-0.32f, 0.32f);
    const bool useSharedCoinPlacement = !marioMapStyle;

    if (!useSharedCoinPlacement && isMarioMapa4Environment(environment) && !surfaces.empty()) {
        struct CoinCandidate {
            glm::vec3 position{0.0f};
            float coverScore{0.0f};
            float spawnDistance{0.0f};
            float surfaceArea{0.0f};
            float mapProgress{0.0f};
        };

        std::sort(surfaces.begin(), surfaces.end(), [](const SurfaceCandidate& a, const SurfaceCandidate& b) {
            if (a.bounds.center.x == b.bounds.center.x) {
                return a.area > b.area;
            }
            return a.bounds.center.x < b.bounds.center.x;
        });

        std::vector<SurfaceCandidate> orderedSurfaces;
        orderedSurfaces.reserve(surfaces.size());
        for (const SurfaceCandidate& surface : surfaces) {
            if (orderedSurfaces.empty()) {
                orderedSurfaces.push_back(surface);
                continue;
            }

            const SurfaceCandidate& previous = orderedSurfaces.back();
            const float dx = std::abs(surface.bounds.center.x - previous.bounds.center.x);
            const float dz = std::abs(surface.bounds.center.z - previous.bounds.center.z);
            if (dx > 1.9f || dz > 0.45f || surface.area > previous.area + 0.25f) {
                orderedSurfaces.push_back(surface);
            }
        }

        auto buildCandidate = [&](const SurfaceCandidate& surface, float xBias, float zBias) {
            const float safeX = std::max(surface.bounds.halfExtent.x - 0.55f, 0.0f);
            const float safeZ = std::max(surface.bounds.halfExtent.z - 0.55f, 0.0f);
            CoinCandidate candidate;
            candidate.position = {
                surface.bounds.center.x + safeX * xBias,
                surface.top + 0.62f,
                surface.bounds.center.z + safeZ * zBias
            };
            candidate.spawnDistance = glm::length(glm::vec2(candidate.position.x - playerSpawn.x, candidate.position.z - playerSpawn.z));
            candidate.surfaceArea = surface.area;

            const float mapWidth = std::max(worldMax.x - worldMin.x, 1.0f);
            candidate.mapProgress = std::clamp((candidate.position.x - worldMin.x) / mapWidth, 0.0f, 1.0f);

            const glm::vec2 fromSpawn = glm::vec2(candidate.position.x - playerSpawn.x, candidate.position.z - playerSpawn.z);
            glm::vec2 awayFromSpawn = glm::length(fromSpawn) > 0.001f ? glm::normalize(fromSpawn) : glm::vec2(1.0f, 0.0f);

            for (const Bounds& other : colliders) {
                const float otherTop = other.center.y + other.halfExtent.y;
                if (otherTop <= candidate.position.y + 0.28f) {
                    continue;
                }

                const glm::vec2 toObstacle{other.center.x - candidate.position.x, other.center.z - candidate.position.z};
                const float horizontalDistance = glm::length(toObstacle);
                if (horizontalDistance > 2.8f) {
                    continue;
                }

                const glm::vec2 obstacleDirection = horizontalDistance > 0.001f
                    ? toObstacle / horizontalDistance
                    : glm::vec2(0.0f, 0.0f);
                const float alignment = std::max(glm::dot(obstacleDirection, awayFromSpawn), 0.0f);
                const float widthWeight = std::max(other.halfExtent.x, other.halfExtent.z);
                const float heightWeight = std::clamp(other.halfExtent.y * 0.9f, 0.0f, 3.0f);
                candidate.coverScore += (3.0f - horizontalDistance) * (0.45f + alignment * 0.85f) + widthWeight * 0.16f + heightWeight * 0.28f;
            }

            return candidate;
        };

        auto tryAcceptCandidate = [&](const CoinCandidate& candidate, float minCoinDistance) {
            if (!validCoinPosition(candidate.position, colliders, m_coins, playerSpawn, worldMin, worldMax, minCoinDistance)) {
                return false;
            }

            Coin coin;
            coin.position = candidate.position;
            coin.phase = static_cast<float>(m_coins.size()) * 0.73f;
            m_coins.push_back(coin);
            return true;
        };

        const std::array<glm::vec2, 12> placementBiases = {
            glm::vec2(0.0f, 0.0f),
            glm::vec2(-0.72f, 0.0f),
            glm::vec2(0.72f, 0.0f),
            glm::vec2(-0.50f, 0.42f),
            glm::vec2(0.50f, -0.42f),
            glm::vec2(-0.82f, 0.52f),
            glm::vec2(0.82f, -0.52f),
            glm::vec2(-0.24f, -0.64f),
            glm::vec2(0.24f, 0.64f),
            glm::vec2(-0.58f, -0.58f),
            glm::vec2(0.58f, 0.58f),
            glm::vec2(0.0f, 0.72f)
        };

        std::vector<CoinCandidate> candidates;
        candidates.reserve(orderedSurfaces.size() * placementBiases.size());
        for (const SurfaceCandidate& surface : orderedSurfaces) {
            for (const glm::vec2& bias : placementBiases) {
                CoinCandidate candidate = buildCandidate(surface, bias.x, bias.y);
                if (candidate.spawnDistance >= 4.8f &&
                    validCoinPosition(candidate.position, colliders, m_coins, playerSpawn, worldMin, worldMax, 0.95f)) {
                    candidates.push_back(candidate);
                }
            }
        }

        std::vector<CoinCandidate> visibleCandidates = candidates;
        std::vector<CoinCandidate> hiddenCandidates = candidates;

        std::sort(visibleCandidates.begin(), visibleCandidates.end(), [](const CoinCandidate& a, const CoinCandidate& b) {
            if (a.coverScore == b.coverScore) {
                if (a.spawnDistance == b.spawnDistance) {
                    return a.mapProgress < b.mapProgress;
                }
                return a.spawnDistance > b.spawnDistance;
            }
            return a.coverScore < b.coverScore;
        });

        std::sort(hiddenCandidates.begin(), hiddenCandidates.end(), [](const CoinCandidate& a, const CoinCandidate& b) {
            if (a.coverScore == b.coverScore) {
                if (a.spawnDistance == b.spawnDistance) {
                    return a.mapProgress < b.mapProgress;
                }
                return a.spawnDistance > b.spawnDistance;
            }
            return a.coverScore > b.coverScore;
        });

        const size_t visibleTarget = 5;
        const size_t hiddenTarget = 5;
        size_t visibleCount = 0;
        size_t hiddenCount = 0;
        size_t leftCount = 0;
        size_t rightCount = 0;
        const float splitX = (worldMin.x + worldMax.x) * 0.5f;

        auto registerSide = [&](const glm::vec3& position) {
            if (position.x <= splitX) {
                ++leftCount;
            } else {
                ++rightCount;
            }
        };

        auto sideBalanced = [&](const glm::vec3& position) {
            const bool leftSide = position.x <= splitX;
            if (leftSide) {
                return leftCount <= rightCount + 1;
            }
            return rightCount <= leftCount + 1;
        };

        for (const CoinCandidate& candidate : visibleCandidates) {
            if (visibleCount >= visibleTarget) {
                break;
            }
            if (candidate.coverScore > 1.75f) {
                continue;
            }
            if (!sideBalanced(candidate.position)) {
                continue;
            }
            if (tryAcceptCandidate(candidate, 1.8f)) {
                ++visibleCount;
                registerSide(candidate.position);
            }
        }

        for (const CoinCandidate& candidate : hiddenCandidates) {
            if (hiddenCount >= hiddenTarget || m_coins.size() >= MissionCoinTotal) {
                break;
            }
            if (candidate.coverScore < 0.95f) {
                continue;
            }
            if (!sideBalanced(candidate.position)) {
                continue;
            }
            if (tryAcceptCandidate(candidate, 1.45f)) {
                ++hiddenCount;
                registerSide(candidate.position);
            }
        }

        if (m_coins.size() < MissionCoinTotal) {
            std::vector<CoinCandidate> balancedCandidates = candidates;
            std::sort(balancedCandidates.begin(), balancedCandidates.end(), [](const CoinCandidate& a, const CoinCandidate& b) {
                const float scoreA = a.spawnDistance * 0.28f + a.surfaceArea * 0.16f - std::abs(a.coverScore - 1.35f);
                const float scoreB = b.spawnDistance * 0.28f + b.surfaceArea * 0.16f - std::abs(b.coverScore - 1.35f);
                return scoreA > scoreB;
            });

            for (const CoinCandidate& candidate : balancedCandidates) {
                if (m_coins.size() >= MissionCoinTotal) {
                    break;
                }
                if (!sideBalanced(candidate.position)) {
                    continue;
                }
                if (tryAcceptCandidate(candidate, 1.05f)) {
                    registerSide(candidate.position);
                }
            }
        }

        if (m_coins.size() < MissionCoinTotal) {
            std::vector<SurfaceCandidate> rescueSurfaces = orderedSurfaces;
            std::sort(rescueSurfaces.begin(), rescueSurfaces.end(), [&](const SurfaceCandidate& a, const SurfaceCandidate& b) {
                const float scoreA = a.area * 0.42f + glm::length(glm::vec2(a.bounds.center.x - playerSpawn.x, a.bounds.center.z - playerSpawn.z)) * 0.18f;
                const float scoreB = b.area * 0.42f + glm::length(glm::vec2(b.bounds.center.x - playerSpawn.x, b.bounds.center.z - playerSpawn.z)) * 0.18f;
                return scoreA > scoreB;
            });

            for (const SurfaceCandidate& surface : rescueSurfaces) {
                if (m_coins.size() >= MissionCoinTotal) {
                    break;
                }
                for (const glm::vec2& bias : placementBiases) {
                    CoinCandidate candidate = buildCandidate(surface, bias.x, bias.y);
                    if (candidate.spawnDistance < 4.0f) {
                        continue;
                    }
                    if (!sideBalanced(candidate.position)) {
                        continue;
                    }
                    if (tryAcceptCandidate(candidate, 0.82f)) {
                        registerSide(candidate.position);
                        if (m_coins.size() >= MissionCoinTotal) {
                            break;
                        }
                    }
                }
            }
        }

        m_star.position = m_coins.empty() ? playerSpawn + glm::vec3(2.2f, 1.2f, 0.0f) : m_coins.back().position + glm::vec3(0.0f, 0.18f, 0.0f);
        m_star.active = false;
        std::cout << "Mapa 4 generated coins: " << m_coins.size() << " (visible " << visibleCount << ", hidden " << hiddenCount << ")" << std::endl;
        return;
    }

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

    if (m_coins.size() < MissionCoinTotal) {
        std::vector<SurfaceCandidate> fallbackSurfaces;
        for (const Bounds& collider : colliders) {
            const float top = collider.center.y + collider.halfExtent.y;
            const float area = (collider.halfExtent.x * 2.0f) * (collider.halfExtent.z * 2.0f);
            const bool usableFloor = collider.halfExtent.y <= 0.45f && area >= 0.20f;
            const bool stableMarioSurface = !m_marioMapStyle ||
                (area >= 0.95f && collider.halfExtent.x >= 0.42f && collider.halfExtent.z >= 0.42f &&
                    top >= playerSpawn.y - 0.45f && top <= playerSpawn.y + 1.35f);
            if (usableFloor && stableMarioSurface && top >= worldMin.y - 0.2f && top <= worldMax.y + 0.5f) {
                fallbackSurfaces.push_back({collider, top, area});
            }
        }

        std::sort(fallbackSurfaces.begin(), fallbackSurfaces.end(), [&](const SurfaceCandidate& a, const SurfaceCandidate& b) {
            const float distanceA = glm::length(glm::vec2(a.bounds.center.x - playerSpawn.x, a.bounds.center.z - playerSpawn.z));
            const float distanceB = glm::length(glm::vec2(b.bounds.center.x - playerSpawn.x, b.bounds.center.z - playerSpawn.z));
            if (m_marioMapStyle) {
                return distanceA > distanceB;
            }
            return distanceA < distanceB;
        });

        const glm::vec3 fallbackMin(minX, worldMin.y, minZ);
        const glm::vec3 fallbackMax(maxX, worldMax.y, maxZ);
        for (const SurfaceCandidate& surface : fallbackSurfaces) {
            if (m_coins.size() >= MissionCoinTotal) {
                break;
            }

            const float ring = 2.4f + static_cast<float>(m_coins.size()) * 0.55f;
            const float angle = static_cast<float>(m_coins.size()) * glm::two_pi<float>() / static_cast<float>(MissionCoinTotal);
            glm::vec3 position{
                surface.bounds.center.x + std::cos(angle) * std::min(ring, std::max(surface.bounds.halfExtent.x - 0.55f, 0.0f)),
                surface.top + 0.58f,
                surface.bounds.center.z + std::sin(angle) * std::min(ring, std::max(surface.bounds.halfExtent.z - 0.55f, 0.0f))
            };

            if (!validCoinPosition(position, colliders, m_coins, playerSpawn, fallbackMin, fallbackMax, m_marioMapStyle ? 3.0f : 1.2f)) {
                continue;
            }

            Coin coin;
            coin.position = position;
            coin.phase = static_cast<float>(m_coins.size()) * 0.73f;
            m_coins.push_back(coin);
        }

        if (m_marioMapStyle && m_coins.size() < MissionCoinTotal) {
            for (const SurfaceCandidate& surface : fallbackSurfaces) {
                if (m_coins.size() >= MissionCoinTotal) {
                    break;
                }

                const glm::vec3 position{surface.bounds.center.x, surface.top + 0.58f, surface.bounds.center.z};
                if (!validCoinPosition(position, colliders, m_coins, playerSpawn, fallbackMin, fallbackMax, 2.0f)) {
                    continue;
                }

                Coin coin;
                coin.position = position;
                coin.phase = static_cast<float>(m_coins.size()) * 0.73f;
                m_coins.push_back(coin);
            }
        }
    }

    m_star.position = m_coins.empty() ? playerSpawn + glm::vec3(2.2f, 1.2f, 0.0f) : m_coins.front().position + glm::vec3(0.0f, 0.18f, 0.0f);
    m_star.active = false;
}

void MissionManager::update(const Player& player, float timeSeconds) {
    // Detecta recolección de monedas, activa la estrella y resuelve la victoria del nivel.
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

void mostrarPantallaCarga(MenuContext& menu, int width, int height, float timeSeconds) {
    beginUiFrame(menu, width, height);
    drawMenuBackground(menu, width, height, timeSeconds, false);

    const Rect panel = centeredRect(width * 0.5f, height * 0.5f - 112.0f, 560.0f, 210.0f);
    drawPanel(menu, panel);
    drawText(menu, menu.cargando,
        panel.x + (panel.width - menu.cargando.size.x) * 0.5f,
        panel.y + 46.0f);
    drawText(menu, menu.preparandoMundo,
        panel.x + (panel.width - menu.preparandoMundo.size.x) * 0.5f,
        panel.y + 112.0f);

    const float dotY = panel.y + 162.0f;
    const float dotStartX = width * 0.5f - 34.0f;
    for (int index = 0; index < 3; ++index) {
        const float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 5.5f + index * 0.75f);
        drawRect(menu,
            {dotStartX + index * 28.0f, dotY, 14.0f, 14.0f},
            {1.0f, 0.84f, 0.20f, 0.35f + 0.65f * pulse});
    }
}

void MissionManager::render(const Shader& shader, float timeSeconds, const glm::vec3& cameraPosition) const {
    // Dibuja las monedas y la estrella activa usando los modelos cargados o su fallback procedural.
    if (!m_initialized) {
        return;
    }

    shader.use();
    shader.setFloat("uTime", timeSeconds);
    for (const Coin& coin : m_coins) {
        if (coin.collected) {
            continue;
        }

        glm::mat4 model = coinModelMatrix(coin, timeSeconds);
        if (!m_coinParts.empty()) {
            for (const MissionRenderablePart& part : m_coinParts) {
                shader.setMat4("uModel", model * localPartMatrix(part));
                bindSceneMaterial(shader, part.material);
                part.mesh.draw();
            }
        } else {
            model = glm::rotate(model, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
            shader.setMat4("uModel", model);
            bindSceneMaterial(shader, m_fallbackCoinMaterial);
            m_fallbackCoinMesh.draw();
        }
    }

    if (m_star.active) {
        const glm::mat4 model = starModelMatrix(timeSeconds);
        if (!m_starParts.empty()) {
            for (const MissionRenderablePart& part : m_starParts) {
                shader.setMat4("uModel", model * localPartMatrix(part));
                bindSceneMaterial(shader, part.material);
                part.mesh.draw();
            }
        } else {
            shader.setMat4("uModel", model);
            bindSceneMaterial(shader, m_starMaterial);
            m_starMesh.draw();
        }
    }
}

bool MissionManager::loadCoinModel() {
    // Prioriza el modelo clásico de moneda y conserva fallbacks para mapas especiales.
    LoadedModel model = ModelLoader::loadModel(resolveFirstExistingAsset({
        "assets/items/Coin/Coin.dae",
        "assets/mapa 4/moneda/source/moneda_1.gltf",
        "assets/mapa 4/Moneda/source/moneda_1.gltf",
        "assets/mapa 4/GoldCoin.glb",
        "assets/mapa 4/gold coin.glb",
        "assets/GoldCoin.glb",
        "assets/gold coin.glb",
        "assets/coin/source/model.gltf"
    }));
    if (model.meshes.empty()) {
        model = ModelLoader::loadModel(resolveAssetPath("assets/items/Coin/Coin.dae"));
    }
    if (model.meshes.empty()) {
        std::cerr << "Coin model could not be loaded. Using procedural fallback coin." << std::endl;
        return false;
    }

    m_coinModelMin = model.minBounds;
    m_coinModelMax = model.maxBounds;
    m_coinModelCenter = (m_coinModelMin + m_coinModelMax) * 0.5f;
    const glm::vec3 size = m_coinModelMax - m_coinModelMin;
    const float maxExtent = std::max({size.x, size.y, size.z, 0.001f});
    m_coinModelScale = 0.78f / maxExtent;

    m_coinParts.clear();
    m_coinParts.reserve(model.meshes.size());
    for (LoadedMesh& mesh : model.meshes) {
        MissionRenderablePart part;
        if (mesh.materialIndex < model.materials.size()) {
            const LoadedMaterial& material = model.materials[mesh.materialIndex];
            part.material.baseColor = material.diffuseColor;
            part.material.opacity = material.opacity;
            part.material.texture = loadMissionTexture(material);
        } else {
            part.material.baseColor = {1.0f, 0.72f, 0.05f};
        }
        if (!part.material.texture) {
            part.material.texture = loadFirstAvailableTexture({
                "assets/items/Coin/cointex.png",
                "assets/mapa 4/moneda/textures/gltf_embedded_0.png",
                "assets/mapa 4/Moneda/textures/gltf_embedded_0.png",
                "assets/mapa 4/GoldCoin.glb",
                "assets/mapa 4/gold coin.glb",
                "assets/coin/textures/gltf_embedded_0.png"
            });
        }
        part.material.emissive = {0.0f, 0.0f, 0.0f};
        part.material.roughness = 0.30f;
        part.material.fogAmount = 0.08f;
        part.mesh = std::move(mesh.mesh);
        m_coinParts.push_back(std::move(part));
    }
    return true;
}

bool MissionManager::loadStarModel() {
    // La estrella final del nivel también usa fallback procedural si falta el asset.
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
        std::filesystem::path("assets") / "mapa 4" / "moneda" / "textures" / fileName,
        std::filesystem::path("assets") / "mapa 4" / "moneda" / "source" / fileName,
        std::filesystem::path("assets") / "mapa 4" / fileName,
        std::filesystem::path("assets") / "coin" / "textures" / fileName,
        std::filesystem::path("assets") / "items" / "Coin" / fileName,
        std::filesystem::path("assets") / "items" / "CrystalStars" / "Crystal Stars" / fileName,
        std::filesystem::path("..") / ".." / "assets" / "mapa 4" / "moneda" / "textures" / fileName,
        std::filesystem::path("..") / ".." / "assets" / "mapa 4" / "moneda" / "source" / fileName,
        std::filesystem::path("..") / ".." / "assets" / "mapa 4" / fileName,
        std::filesystem::path("..") / ".." / "assets" / "coin" / "textures" / fileName,
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

std::shared_ptr<Texture2D> MissionManager::loadMissionTexture(const LoadedMaterial& material) {
    auto texture = loadMissionTexture(material.diffuseTexturePath);
    if (texture && texture->valid()) {
        return texture;
    }

    if (material.embeddedTextureData.empty()) {
        return nullptr;
    }

    auto embedded = std::make_shared<Texture2D>();
    if (material.embeddedTextureCompressed) {
        if (!embedded->loadFromMemory(material.embeddedTextureData.data(), static_cast<int>(material.embeddedTextureData.size()), false)) {
            return nullptr;
        }
    } else if (material.embeddedTextureWidth > 0 && material.embeddedTextureHeight > 0) {
        embedded->createFromRGBA(material.embeddedTextureWidth, material.embeddedTextureHeight, material.embeddedTextureData.data(), false);
    } else {
        return nullptr;
    }

    m_textures.push_back(embedded);
    return embedded;
}

std::shared_ptr<Texture2D> MissionManager::loadFirstAvailableTexture(const std::vector<std::string>& paths) {
    for (const std::string& path : paths) {
        auto texture = loadMissionTexture(path);
        if (texture && texture->valid()) {
            return texture;
        }
    }
    return nullptr;
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
    model = glm::rotate(model, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
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

    const float minimumSpawnDistance = m_marioMapStyle ? 6.8f : 2.4f;
    if (glm::length(glm::vec2(position.x - playerSpawn.x, position.z - playerSpawn.z)) < minimumSpawnDistance) {
        return false;
    }

    for (const Coin& coin : placed) {
        if (glm::length(glm::vec2(position.x - coin.position.x, position.z - coin.position.z)) < minCoinDistance) {
            return false;
        }
    }

    const float expectedFloorY = position.y - 0.58f;
    bool supportedByFloor = false;
    for (const Bounds& collider : colliders) {
        const float top = collider.center.y + collider.halfExtent.y;
        const float area = (collider.halfExtent.x * 2.0f) * (collider.halfExtent.z * 2.0f);
        const bool floorLike = collider.halfExtent.y <= 0.32f && area >= 0.20f;
        const bool insideSupport =
            position.x >= collider.center.x - collider.halfExtent.x - 0.10f &&
            position.x <= collider.center.x + collider.halfExtent.x + 0.10f &&
            position.z >= collider.center.z - collider.halfExtent.z - 0.10f &&
            position.z <= collider.center.z + collider.halfExtent.z + 0.10f;
        if (!floorLike || !insideSupport) {
            continue;
        }

        if (std::abs(top - expectedFloorY) <= 0.26f) {
            supportedByFloor = true;
            break;
        }
    }
    if (!supportedByFloor) {
        return false;
    }

    const Bounds candidate{position, m_marioMapStyle ? glm::vec3(0.22f, 0.30f, 0.22f) : glm::vec3(0.30f, 0.38f, 0.30f)};
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
        if (m_completeOnAllCoins) {
            completarNivel(timeSeconds);
        } else {
            activarEstrella(timeSeconds);
        }
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

void MissionManager::forceComplete(float timeSeconds) {
    if (m_levelComplete) {
        return;
    }
    m_levelComplete = true;
    m_victoryTime = timeSeconds;
}



bool ToadNpc::initialize() {
    // Prepara el NPC guía con modelo real o una versión de respaldo si falta el asset.
    if (m_initialized) {
        return true;
    }

    buildFallbackModel();
    m_initialized = true;
    return true;
}

std::shared_ptr<Texture2D> ToadNpc::loadNpcTexture(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    const std::filesystem::path candidate = std::filesystem::path(resolveAssetPath(path));
    if (!std::filesystem::exists(candidate)) {
        return nullptr;
    }

    const std::string normalized = std::filesystem::weakly_canonical(candidate).string();
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
    auto makePart = [&](Mesh mesh, const glm::vec3& scale, const glm::vec3& position, const glm::vec3& color, const std::string& texturePath = std::string()) {
        MissionRenderablePart part;
        part.mesh = std::move(mesh);
        part.localScale = scale;
        part.localPosition = position;
        part.material.baseColor = color;
        part.material.roughness = 0.78f;
        part.material.fogAmount = 0.20f;
        part.material.texture = loadNpcTexture(texturePath);
        m_parts.push_back(std::move(part));
    };

    makePart(Mesh::cube(), {0.62f, 0.52f, 0.62f}, {0.0f, 0.36f, 0.0f}, {0.96f, 0.92f, 0.85f}, "assets/npcs/RussT/Russ T/Toad (torso).png");
    makePart(Mesh::cube(), {0.88f, 0.48f, 0.88f}, {0.0f, 0.98f, 0.0f}, {0.98f, 0.96f, 0.92f}, "assets/npcs/RussT/Russ T/Toad (head).png");
    makePart(Mesh::cube(), {0.18f, 0.18f, 0.12f}, {-0.16f, 0.96f, 0.43f}, {0.08f, 0.08f, 0.10f}, "assets/npcs/RussT/Russ T/Toad (eyes).png");
    makePart(Mesh::cube(), {0.18f, 0.18f, 0.12f}, {0.16f, 0.96f, 0.43f}, {0.08f, 0.08f, 0.10f}, "assets/npcs/RussT/Russ T/Toad (eyes).png");
    makePart(Mesh::cube(), {0.18f, 0.34f, 0.18f}, {-0.34f, 0.46f, 0.0f}, {0.95f, 0.82f, 0.62f}, "assets/npcs/RussT/Russ T/Toad (hand).png");
    makePart(Mesh::cube(), {0.18f, 0.34f, 0.18f}, {0.34f, 0.46f, 0.0f}, {0.95f, 0.82f, 0.62f}, "assets/npcs/RussT/Russ T/Toad (hand).png");
    makePart(Mesh::cube(), {0.20f, 0.28f, 0.20f}, {-0.14f, 0.02f, 0.0f}, {0.54f, 0.26f, 0.16f}, "assets/npcs/RussT/Russ T/Toad (shoe).png");
    makePart(Mesh::cube(), {0.20f, 0.28f, 0.20f}, {0.14f, 0.02f, 0.0f}, {0.54f, 0.26f, 0.16f}, "assets/npcs/RussT/Russ T/Toad (shoe).png");

    m_modelMin = {-0.5f, -0.5f, -0.5f};
    m_modelMax = {0.5f, 1.5f, 0.5f};
    m_modelCenter = (m_modelMin + m_modelMax) * 0.5f;
    m_modelScale = 0.98f / std::max(m_modelMax.y - m_modelMin.y, 0.001f);
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
    shader.use();
    shader.setFloat("uTime", timeSeconds);
    for (const MissionRenderablePart& part : m_parts) {
        shader.setMat4("uModel", model * localPartMatrix(part));
        bindSceneMaterial(shader, part.material);
        part.mesh.draw();
    }
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
    const glm::vec2 target{playerSpawn.x - 1.65f, playerSpawn.z + 1.35f};
    glm::vec3 best = playerSpawn + glm::vec3(-1.65f, 0.05f, 1.35f);
    float bestScore = std::numeric_limits<float>::max();

    for (const Bounds& collider : colliders) {
        const float top = collider.center.y + collider.halfExtent.y;
        const float area = (collider.halfExtent.x * 2.0f) * (collider.halfExtent.z * 2.0f);
        const bool floorLike = collider.halfExtent.y <= 0.30f && area >= 0.45f && collider.halfExtent.x >= 0.28f && collider.halfExtent.z >= 0.28f;
        if (!floorLike) {
            continue;
        }

        const float safeX = std::max(collider.halfExtent.x - 0.75f, 0.0f);
        const float safeZ = std::max(collider.halfExtent.z - 0.75f, 0.0f);
        glm::vec3 candidate{
            std::clamp(target.x, collider.center.x - safeX, collider.center.x + safeX),
            top + 0.05f,
            std::clamp(target.y, collider.center.z - safeZ, collider.center.z + safeZ)
        };

        const float score = glm::length(glm::vec2(candidate.x - target.x, candidate.z - target.y)) + std::abs(candidate.y - playerSpawn.y) * 0.4f;
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
    // Reúne contador, mensajes temporales y paneles de victoria del objetivo del nivel.
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

void drawRedGemIcon(MenuContext& menu, const Texture2D& texture, const Rect& rect, float timeSeconds) {
    const float pulse = 0.92f + std::sin(timeSeconds * 5.0f) * 0.08f;
    const Rect glow = scaleRect(rect, 1.22f * pulse);
    drawRect(menu, glow, {0.84f, 0.01f, 0.04f, 0.18f});
    if (texture.valid()) {
        drawTexture(menu, texture, rect, {1.0f, 0.24f, 0.28f, 1.0f}, true);
    }
    else {
        drawRect(menu, scaleRect(rect, 0.72f), {0.94f, 0.02f, 0.06f, 1.0f});
    }
}

bool drawMapa1ShopButton(
    MenuContext& menu,
    const TextSprite& text,
    const Rect& rect,
    const glm::vec2& mouse,
    bool clicked,
    float timeSeconds,
    bool enabled = true,
    bool owned = false) {
    const bool hovered = enabled &&
        mouse.x >= rect.x && mouse.x <= rect.x + rect.width &&
        mouse.y >= rect.y && mouse.y <= rect.y + rect.height;
    const float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 7.5f);
    drawRect(menu, {rect.x + 7.0f, rect.y + 8.0f, rect.width, rect.height}, {0.0f, 0.0f, 0.0f, 0.58f});
    drawRect(menu, {rect.x - 2.0f, rect.y - 2.0f, rect.width + 4.0f, rect.height + 4.0f},
        owned ? glm::vec4(0.66f, 0.66f, 0.70f, 0.95f) : glm::vec4(0.92f, 0.04f, 0.06f, 0.98f));
    drawRect(menu, rect,
        owned ? glm::vec4(0.15f, 0.16f, 0.19f, 0.98f)
        : enabled
            ? (hovered ? glm::vec4(0.72f, 0.02f, 0.04f, 1.0f) : glm::vec4(0.22f, 0.02f, 0.04f, 0.98f))
            : glm::vec4(0.10f, 0.10f, 0.12f, 0.92f));
    if (hovered) {
        drawRect(menu, {rect.x + 5.0f, rect.y + 5.0f, rect.width - 10.0f, rect.height - 10.0f},
            {1.0f, 0.16f, 0.18f, 0.12f + pulse * 0.10f});
        drawRect(menu, {rect.x, rect.y, 7.0f, rect.height}, {1.0f, 0.18f, 0.12f, 0.90f});
    }
    drawText(menu, text,
        rect.x + (rect.width - text.size.x) * 0.5f,
        rect.y + (rect.height - text.size.y) * 0.5f,
        enabled || owned ? glm::vec4(1.0f) : glm::vec4(0.58f, 0.60f, 0.64f, 0.92f));
    return hovered && clicked;
}

void drawMapa1Shop(
    MenuContext& menu,
    Mapa1& mapa1,
    int width,
    int height,
    float timeSeconds,
    const glm::vec2& mouse,
    bool clicked,
    float scrollY) {
    drawRect(menu, {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)}, {0.01f, 0.01f, 0.015f, 0.86f});

    const float panelWidth = std::min(1160.0f, static_cast<float>(width) - 48.0f);
    const float panelHeight = std::min(680.0f, static_cast<float>(height) - 42.0f);
    const Rect panel = centeredRect(width * 0.5f, (height - panelHeight) * 0.5f, panelWidth, panelHeight);
    drawRect(menu, {panel.x + 12.0f, panel.y + 14.0f, panel.width, panel.height}, {0.0f, 0.0f, 0.0f, 0.66f});
    drawRect(menu, {panel.x - 3.0f, panel.y - 3.0f, panel.width + 6.0f, panel.height + 6.0f}, {0.72f, 0.02f, 0.04f, 0.98f});
    drawRect(menu, panel, {0.035f, 0.035f, 0.045f, 0.99f});

    const float headerHeight = 92.0f;
    drawRect(menu, {panel.x, panel.y, panel.width, headerHeight}, {0.11f, 0.01f, 0.02f, 1.0f});
    drawRect(menu, {panel.x, panel.y + headerHeight - 5.0f, panel.width, 5.0f}, {0.94f, 0.03f, 0.05f, 1.0f});
    drawRect(menu, {panel.x + 22.0f, panel.y + 18.0f, 8.0f, 56.0f}, {0.98f, 0.06f, 0.08f, 1.0f});
    drawText(menu, menu.tiendaTitulo, panel.x + 43.0f, panel.y + 9.0f);
    drawText(menu, menu.tiendaSubtitulo, panel.x + 48.0f, panel.y + 52.0f, {0.82f, 0.82f, 0.86f, 1.0f});

    const int gemCount = std::clamp(mapa1.spectralGemCount(), 0, 99);
    const Rect balance{panel.x + panel.width - 330.0f, panel.y + 20.0f, 148.0f, 54.0f};
    drawRect(menu, balance, {0.02f, 0.02f, 0.025f, 0.98f});
    drawRedGemIcon(menu, mapa1.gemIconTexture(), {balance.x + 12.0f, balance.y + 9.0f, 36.0f, 36.0f}, timeSeconds);
    drawText(menu, menu.gemCounters[gemCount], balance.x + 52.0f, balance.y + (balance.height - menu.gemCounters[gemCount].size.y) * 0.5f);

    const Rect closeRect{panel.x + panel.width - 166.0f, panel.y + 20.0f, 140.0f, 54.0f};
    if (drawMapa1ShopButton(menu, menu.tiendaCerrar, closeRect, mouse, clicked, timeSeconds)) {
        mapa1.closeShop();
        return;
    }

    const float sidebarWidth = 220.0f;
    const Rect sidebar{panel.x, panel.y + headerHeight, sidebarWidth, panel.height - headerHeight};
    drawRect(menu, sidebar, {0.055f, 0.055f, 0.065f, 1.0f});
    drawRect(menu, {sidebar.x + sidebar.width - 3.0f, sidebar.y, 3.0f, sidebar.height}, {0.28f, 0.28f, 0.32f, 1.0f});
    drawRect(menu, {sidebar.x + 16.0f, sidebar.y + 24.0f, sidebar.width - 31.0f, 58.0f}, {0.68f, 0.02f, 0.04f, 1.0f});
    drawText(menu, menu.tiendaHabilidades, sidebar.x + 27.0f, sidebar.y + 31.0f);
    drawText(menu, menu.tiendaManual, sidebar.x + 28.0f, sidebar.y + 105.0f, {0.72f, 0.72f, 0.76f, 1.0f});
    drawText(menu, menu.tiendaCabina, sidebar.x + 28.0f, sidebar.y + 151.0f, {0.72f, 0.72f, 0.76f, 1.0f});
    drawText(menu, menu.tiendaAyudaScroll, sidebar.x + 19.0f, sidebar.y + sidebar.height - 98.0f, {0.68f, 0.68f, 0.72f, 1.0f});

    const Rect viewport{
        panel.x + sidebarWidth + 22.0f,
        panel.y + headerHeight + 18.0f,
        panel.width - sidebarWidth - 48.0f,
        panel.height - headerHeight - 42.0f
    };
    constexpr float contentHeight = 1075.0f;
    const float maximumScroll = std::max(0.0f, contentHeight - viewport.height);
    mapa1ShopScroll = std::clamp(mapa1ShopScroll - scrollY * 54.0f, 0.0f, maximumScroll);

    glEnable(GL_SCISSOR_TEST);
    glScissor(
        static_cast<int>(viewport.x),
        std::max(0, height - static_cast<int>(viewport.y + viewport.height)),
        std::max(0, static_cast<int>(viewport.width)),
        std::max(0, static_cast<int>(viewport.height)));

    const float contentX = viewport.x + 5.0f;
    const float contentY = viewport.y - mapa1ShopScroll;
    const float cardWidth = viewport.width - 26.0f;
    auto drawSkillCard = [&](float y, const TextSprite& title, const TextSprite& description,
        const TextSprite& control, int cost, bool owned, bool spectral) {
        const Rect card{contentX, y, cardWidth, 184.0f};
        drawRect(menu, {card.x + 8.0f, card.y + 9.0f, card.width, card.height}, {0.0f, 0.0f, 0.0f, 0.46f});
        drawRect(menu, {card.x - 2.0f, card.y - 2.0f, card.width + 4.0f, card.height + 4.0f},
            owned ? glm::vec4(0.40f, 0.42f, 0.46f, 0.92f) : glm::vec4(0.58f, 0.02f, 0.04f, 0.96f));
        drawRect(menu, card, {0.075f, 0.075f, 0.09f, 0.99f});
        drawRect(menu, {card.x, card.y, 9.0f, card.height}, owned
            ? glm::vec4(0.62f, 0.64f, 0.68f, 1.0f)
            : glm::vec4(0.95f, 0.04f, 0.06f, 1.0f));
        drawText(menu, title, card.x + 26.0f, card.y + 15.0f);
        drawText(menu, description, card.x + 25.0f, card.y + 61.0f, {0.82f, 0.82f, 0.86f, 1.0f});
        drawText(menu, control, card.x + 25.0f, card.y + 124.0f, {0.95f, 0.24f, 0.26f, 1.0f});
        drawRedGemIcon(menu, mapa1.gemIconTexture(), {card.x + card.width - 250.0f, card.y + 19.0f, 32.0f, 32.0f}, timeSeconds);

        const bool enoughGems = mapa1.spectralGemCount() >= cost;
        const TextSprite& buttonText = owned
            ? menu.habilidadAdquirida
            : (enoughGems
                ? (spectral ? menu.comprarCincoGemas : menu.comprarTresGemas)
                : menu.gemasInsuficientes);
        const Rect buyRect{card.x + card.width - 210.0f, card.y + 116.0f, 184.0f, 48.0f};
        const bool pointerInsideViewport =
            mouse.x >= viewport.x && mouse.x <= viewport.x + viewport.width &&
            mouse.y >= viewport.y && mouse.y <= viewport.y + viewport.height;
        if (drawMapa1ShopButton(menu, buttonText, buyRect, mouse, clicked && pointerInsideViewport,
            timeSeconds, enoughGems && !owned, owned)) {
            if (spectral) {
                mapa1.purchaseSpectralStep();
            }
            else {
                mapa1.purchaseDamageParry();
            }
        }
    };

    drawSkillCard(
        contentY,
        menu.saltoEspectralTitulo,
        menu.saltoEspectralDescripcion,
        menu.saltoEspectralControl,
        mapa1.spectralGemRequirement(),
        mapa1.spectralUnlocked(),
        true);
    drawSkillCard(
        contentY + 210.0f,
        menu.parryRetornoTitulo,
        menu.parryRetornoDescripcion,
        menu.parryRetornoControl,
        mapa1.damageParryCost(),
        mapa1.damageParryUnlocked(),
        false);

    drawText(menu, menu.manualTitulo, contentX + 6.0f, contentY + 425.0f);
    const std::array<const TextSprite*, 7> manualEntries = {
        &menu.manualMovimiento,
        &menu.manualSalto,
        &menu.manualDimension,
        &menu.manualDisparo,
        &menu.manualCarga,
        &menu.manualParry,
        &menu.manualTienda
    };
    float manualY = contentY + 480.0f;
    for (size_t index = 0; index < manualEntries.size(); ++index) {
        const Rect item{contentX, manualY, cardWidth, 72.0f};
        drawRect(menu, item, index % 2 == 0
            ? glm::vec4(0.075f, 0.075f, 0.09f, 0.98f)
            : glm::vec4(0.055f, 0.055f, 0.068f, 0.98f));
        drawRect(menu, {item.x, item.y, 5.0f, item.height}, {0.82f, 0.03f, 0.05f, 0.96f});
        drawText(menu, *manualEntries[index], item.x + 18.0f, item.y + (item.height - manualEntries[index]->size.y) * 0.5f);
        manualY += 82.0f;
    }
    glDisable(GL_SCISSOR_TEST);

    const Rect scrollTrack{viewport.x + viewport.width - 8.0f, viewport.y, 5.0f, viewport.height};
    drawRect(menu, scrollTrack, {0.20f, 0.20f, 0.23f, 0.82f});
    const float thumbHeight = std::max(42.0f, viewport.height * viewport.height / contentHeight);
    const float thumbTravel = viewport.height - thumbHeight;
    const float thumbY = scrollTrack.y + (maximumScroll > 0.0f ? mapa1ShopScroll / maximumScroll : 0.0f) * thumbTravel;
    drawRect(menu, {scrollTrack.x - 2.0f, thumbY, 9.0f, thumbHeight}, {0.94f, 0.04f, 0.06f, 0.96f});
}

void drawMapa1CombatHud(
    MenuContext& menu,
    Mapa1& mapa1,
    int width,
    int height,
    float timeSeconds,
    const glm::vec2& mouse,
    bool clicked,
    float scrollY) {
    // HUD de combate de Mapa 1 reutilizado como referencia visual para otros sistemas.
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

    const Rect gemPanel{std::max(18.0f, static_cast<float>(width) - 246.0f), 92.0f, 218.0f, 58.0f};
    drawRect(menu, {gemPanel.x + 6.0f, gemPanel.y + 7.0f, gemPanel.width, gemPanel.height}, {0.0f, 0.0f, 0.0f, 0.48f});
    drawRect(menu, {gemPanel.x - 2.0f, gemPanel.y - 2.0f, gemPanel.width + 4.0f, gemPanel.height + 4.0f}, {0.76f, 0.02f, 0.04f, 0.96f});
    drawRect(menu, gemPanel, {0.07f, 0.03f, 0.05f, 0.94f});
    drawRedGemIcon(menu, mapa1.gemIconTexture(), {gemPanel.x + 12.0f, gemPanel.y + 10.0f, 38.0f, 38.0f}, timeSeconds);
    drawText(menu, menu.gemCounters[std::clamp(mapa1.spectralGemCount(), 0, 99)], gemPanel.x + 57.0f, gemPanel.y + 7.0f);

    const Rect shopButton{std::max(18.0f, static_cast<float>(width) - 246.0f), static_cast<float>(height) - 78.0f, 218.0f, 52.0f};
    if (!mapa1.shopOpen() &&
        drawMapa1ShopButton(menu, menu.tiendaBoton, shopButton, mouse, clicked, timeSeconds)) {
        mapa1.openShop();
    }

    if (!mapa1.shopOpen() && mapa1.showVanPrompt(timeSeconds)) {
        const Rect prompt = centeredRect(width * 0.5f, static_cast<float>(height) - 126.0f, 470.0f, 54.0f);
        drawRect(menu, {prompt.x + 7.0f, prompt.y + 8.0f, prompt.width, prompt.height}, {0.0f, 0.0f, 0.0f, 0.54f});
        drawRect(menu, prompt, {0.45f, 0.01f, 0.03f, 0.96f});
        drawText(menu, menu.cabinaPrompt,
            prompt.x + (prompt.width - menu.cabinaPrompt.size.x) * 0.5f,
            prompt.y + (prompt.height - menu.cabinaPrompt.size.y) * 0.5f);
    }

    if (mapa1.shopOpen()) {
        drawMapa1Shop(menu, mapa1, width, height, timeSeconds, mouse, clicked, scrollY);
    }
}

void drawSimpleHealthHud(MenuContext& menu, int currentHealth, int maximumHealth, int width, int height) {
    // Variante mínima del HUD de vida para pantallas que no necesitan más información.
    beginUiFrame(menu, width, height);
    const Rect healthPanel{22.0f, 22.0f, 282.0f, 72.0f};
    drawRect(menu, {healthPanel.x + 6.0f, healthPanel.y + 7.0f, healthPanel.width, healthPanel.height}, {0.01f, 0.02f, 0.05f, 0.48f});
    drawRect(menu, healthPanel, {0.06f, 0.12f, 0.24f, 0.94f});
    drawText(menu, menu.vidaJugador, healthPanel.x + 14.0f, healthPanel.y + 10.0f);
    const float segmentWidth = 66.0f;
    for (int index = 0; index < std::max(1, maximumHealth); ++index) {
        const bool active = index < currentHealth;
        drawRect(
            menu,
            {healthPanel.x + 14.0f + index * (segmentWidth + 9.0f), healthPanel.y + 43.0f, segmentWidth, 16.0f},
            active ? glm::vec4(0.92f, 0.18f, 0.16f, 1.0f) : glm::vec4(0.20f, 0.23f, 0.30f, 0.90f));
    }
}

void drawGameOverHud(MenuContext& menu, int width, int height) {
    // Overlay simple para estados de derrota sin reconfigurar el resto del HUD.
    beginUiFrame(menu, width, height);
    drawRect(menu, {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)}, {0.01f, 0.02f, 0.06f, 0.58f});
    const Rect panel = centeredRect(width * 0.5f, height * 0.5f - 72.0f, 560.0f, 144.0f);
    drawPanel(menu, panel);
    drawText(menu, menu.juegoTerminado, panel.x + (panel.width - menu.juegoTerminado.size.x) * 0.5f, panel.y + (panel.height - menu.juegoTerminado.size.y) * 0.5f);
}

void drawLevelCompleteHud(MenuContext& menu, int width, int height) {
    beginUiFrame(menu, width, height);
    drawRect(menu, {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)}, {0.01f, 0.02f, 0.06f, 0.58f});
    const Rect panel = centeredRect(width * 0.5f, height * 0.5f - 72.0f, 560.0f, 144.0f);
    drawPanel(menu, panel);
    drawText(menu, menu.nivelCompletado, panel.x + (panel.width - menu.nivelCompletado.size.x) * 0.5f, panel.y + (panel.height - menu.nivelCompletado.size.y) * 0.5f);
}

void drawToadHud(MenuContext& menu, const ToadNpc& toad, int width, int height) {
    // El HUD de Toad reutiliza paneles del menú para mantener coherencia visual.
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
    // Menú principal del juego con la navegación visual base entre pantallas.
    beginUiFrame(menu, width, height);
    drawMenuBackground(menu, width, height, timeSeconds);

    const float buttonWidth = 330.0f;
    const float buttonHeight = 54.0f;
    const float centerX = width * 0.5f;
    const float startY = std::max(382.0f, height * 0.53f);
    if (drawButton(menu, menu.jugar, centeredRect(centerX, startY, buttonWidth, buttonHeight), mouse, clicked, timeSeconds)) {
        appState = EstadoJuego::MENU_MUNDOS;
        menu.notificationUntil = 0.0;
    }
    if (drawButton(menu, menu.comoJugar, centeredRect(centerX, startY + 66.0f, buttonWidth, buttonHeight), mouse, clicked, timeSeconds)) {
        appState = EstadoJuego::COMO_JUGAR;
    }
    if (drawButton(menu, menu.creditos, centeredRect(centerX, startY + 132.0f, buttonWidth, buttonHeight), mouse, clicked, timeSeconds)) {
        appState = EstadoJuego::CREDITOS;
    }
    if (drawButton(menu, menu.salir, centeredRect(centerX, startY + 198.0f, buttonWidth, buttonHeight), mouse, clicked, timeSeconds)) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

void mostrarMenuMundos(MenuContext& menu, int width, int height, float timeSeconds, const glm::vec2& mouse, bool clicked) {
    // Pantalla de selección de mundos con el mismo sistema de botones del menú principal.
    beginUiFrame(menu, width, height);
    drawMenuBackground(menu, width, height, timeSeconds, false);

    const Rect panel = centeredRect(width * 0.5f, 178.0f, 620.0f, 420.0f);
    drawPanel(menu, panel);

    const float buttonWidth = 320.0f;
    const float buttonHeight = 54.0f;
    const float centerX = width * 0.5f;
    const float startY = 224.0f;
    if (drawButton(menu, menu.mundo1, centeredRect(centerX, startY, buttonWidth, buttonHeight), mouse, clicked, timeSeconds)) {
        solicitarCargaMundo(EstadoJuego::MUNDO_1);
        menu.notificationUntil = 0.0;
    }
    if (drawButton(menu, menu.mundo2, centeredRect(centerX, startY + 68.0f, buttonWidth, buttonHeight), mouse, clicked, timeSeconds)) {
        solicitarCargaMundo(EstadoJuego::MUNDO_2);
        menu.notificationUntil = 0.0;
    }
    if (drawButton(menu, menu.mundo3, centeredRect(centerX, startY + 136.0f, buttonWidth, buttonHeight), mouse, clicked, timeSeconds)) {
        solicitarCargaMundo(EstadoJuego::MUNDO_3);
        menu.notificationUntil = 0.0;
    }
    if (drawButton(menu, menu.mundo4, centeredRect(centerX, startY + 204.0f, buttonWidth, buttonHeight), mouse, clicked, timeSeconds)) {
        solicitarCargaMundo(EstadoJuego::MUNDO_4);
        menu.notificationUntil = 0.0;
    }
    if (drawButton(menu, menu.volver, centeredRect(centerX, startY + 284.0f, buttonWidth, buttonHeight), mouse, clicked, timeSeconds)) {
        appState = EstadoJuego::MENU_PRINCIPAL;
        menu.notificationUntil = 0.0;
    }
    drawUnavailableMessage(menu, width, height, timeSeconds);
}

void mostrarComoJugar(MenuContext& menu, int width, int height, float timeSeconds, const glm::vec2& mouse, bool clicked) {
    // Pantalla estática de instrucciones generales del juego.
    beginUiFrame(menu, width, height);
    drawMenuBackground(menu, width, height, timeSeconds, false);

    const Rect panel = centeredRect(width * 0.5f, 155.0f, 790.0f, 420.0f);
    drawPanel(menu, panel);
    drawText(menu, menu.tituloComoJugar, panel.x + (panel.width - menu.tituloComoJugar.size.x) * 0.5f, panel.y + 34.0f);
    drawText(menu, menu.textoComoJugar, panel.x + (panel.width - menu.textoComoJugar.size.x) * 0.5f, panel.y + 118.0f);

    if (drawButton(menu, menu.volver, centeredRect(width * 0.5f, panel.y + panel.height - 83.0f, 260.0f, 55.0f), mouse, clicked, timeSeconds)) {
        appState = EstadoJuego::MENU_PRINCIPAL;
    }
}

void mostrarCreditos(MenuContext& menu, int width, int height, float timeSeconds, const glm::vec2& mouse, bool clicked) {
    // Créditos del proyecto presentados con el mismo layout base del resto del menú.
    beginUiFrame(menu, width, height);
    drawMenuBackground(menu, width, height, timeSeconds, false);

    const Rect panel = centeredRect(width * 0.5f, 160.0f, 750.0f, 400.0f);
    drawPanel(menu, panel);
    drawText(menu, menu.tituloCreditos, panel.x + (panel.width - menu.tituloCreditos.size.x) * 0.5f, panel.y + 38.0f);
    drawText(menu, menu.textoCreditos, panel.x + (panel.width - menu.textoCreditos.size.x) * 0.5f, panel.y + 120.0f);

    if (drawButton(menu, menu.volver, centeredRect(width * 0.5f, panel.y + panel.height - 82.0f, 260.0f, 55.0f), mouse, clicked, timeSeconds)) {
        appState = EstadoJuego::MENU_PRINCIPAL;
    }
}

void resetGameplayView(const Player& player) {
    // Restaura la camara y el estado de entrada para comenzar en 2D.
    currentMode = PlayMode::Mode2D;
    lastToggleKey = false;
    lastJumpKey = false;
    lastInteractKey = false;
    cameraYawDegrees = 0.0f;
    cameraPitchDegrees = 18.0f;
    cameraInitialized = false;
    firstMouse = true;
    locked2DDepth = player.position().z;
}

void drawShieldHud(MenuContext& menu, int width, int height, bool active) {
    // Indicador visual breve para confirmar que el parry/escudo está activo.
    if (!active) {
        return;
    }
    beginUiFrame(menu, width, height);
    const Rect shieldPanel = centeredRect(width * 0.5f, 28.0f, 210.0f, 46.0f);
    drawRect(menu, {shieldPanel.x + 5.0f, shieldPanel.y + 6.0f, shieldPanel.width, shieldPanel.height}, {0.01f, 0.03f, 0.08f, 0.38f});
    drawRect(menu, shieldPanel, {0.10f, 0.46f, 0.68f, 0.92f});
    drawText(menu, menu.parryActivo,
        shieldPanel.x + (shieldPanel.width - menu.parryActivo.size.x) * 0.5f,
        shieldPanel.y + (shieldPanel.height - menu.parryActivo.size.y) * 0.5f);
}

void resetGameplayView2D(const Player& player) {
    // Variante de reinicio que deja al jugador entrando directamente en 2D.
    currentMode = PlayMode::Mode2D;
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
    // Carga el escenario, música, jugador y objetivos de Mundo 2 antes de entrar al bucle jugable.
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
    std::cout << "Controls 3D: WASD move, mouse camera, Space jump, TAB switch to 2D, Esc back to menu." << std::endl;
    std::cout << "Controls 2D: A/D move, Space jump, TAB switch to 3D." << std::endl;
    mundo2.initialized = true;
    return true;
}

void volverAlMenu(Mundo2Runtime& mundo2) {
    // Detiene audio y reinicia la sesión de Mundo 2 al salir al menú.
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
    // Mantiene el viewport sincronizado con el tamaño real de la ventana.
    glViewport(0, 0, width, height);
}

void scrollCallback(GLFWwindow*, double, double yOffset) {
    pendingScrollY += yOffset;
}

void mouseCallback(GLFWwindow*, double x, double y) {
    // Solo actualiza deltas de mouse; la cámara decide después cómo usarlos.
    if (appState != EstadoJuego::MUNDO_2 && appState != EstadoJuego::MUNDO_3 && appState != EstadoJuego::MUNDO_4) {
        // Fuera de gameplay solo se resetea el mouse para no arrastrar deltas viejos.
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
        cameraPitchDegrees = std::clamp(cameraPitchDegrees, appState == EstadoJuego::MUNDO_4 ? 8.0f : 10.0f, appState == EstadoJuego::MUNDO_4 ? 42.0f : 34.0f);
    }
}

void updateCursorMode(GLFWwindow* window) {
    // Alterna entre cursor libre en menús y capturado durante la jugabilidad 3D.
    if (appState == lastCursorState) {
        return;
    }

    if (appState == EstadoJuego::MUNDO_2 || appState == EstadoJuego::MUNDO_3 || appState == EstadoJuego::MUNDO_4) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
    } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    lastCursorState = appState;
}

void processAppInput(GLFWwindow* window, Mapa1& mapa1, Mundo2Runtime& mundo2, Map3Runtime& map3, Mapa4Runtime& mapa4) {
    // Agrupa los atajos globales del juego para no duplicar control por mapa.
    const bool escapeDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (escapeDown && !lastEscapeKey) {
        switch (appState) {
        case EstadoJuego::MENU_PRINCIPAL:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case EstadoJuego::MENU_MUNDOS:
        case EstadoJuego::COMO_JUGAR:
        case EstadoJuego::CREDITOS:
        case EstadoJuego::CARGANDO:
            loadingTarget = EstadoJuego::MENU_PRINCIPAL;
            loadingScreenPresented = false;
            appState = EstadoJuego::MENU_PRINCIPAL;
            break;
        case EstadoJuego::MUNDO_1:
            if (mapa1.shopOpen()) {
                mapa1.closeShop();
            }
            else {
                mapa1.shutdown();
                glfwSetWindowTitle(window, "Paper Pinix");
                appState = EstadoJuego::MENU_PRINCIPAL;
            }
            break;
        case EstadoJuego::MUNDO_2:
            volverAlMenu(mundo2);
            break;
        case EstadoJuego::MUNDO_3:
            volverAlMenu(map3);
            appState = EstadoJuego::MENU_PRINCIPAL;
            break;
        case EstadoJuego::MUNDO_4:
            volverAlMenu(mapa4);
            appState = EstadoJuego::MENU_PRINCIPAL;
            break;
        }
    }
    lastEscapeKey = escapeDown;
}

PlayerInput buildPlayerInput(GLFWwindow* window, const Player& player) {
    const bool toggleDown = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
    if (toggleDown && !lastToggleKey) {
        if (currentMode == PlayMode::Mode3D) {
            currentMode = PlayMode::Mode2D;
            locked2DDepth = player.position().z;
            modeSwitchUnavailableUntil = 0.0;
        } else {
            currentMode = PlayMode::Mode3D;
            modeSwitchUnavailableUntil = 0.0;
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

    const bool jumpDown = currentMode == PlayMode::Mode2D
        ? (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        : (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
    input.jumpPressed = jumpDown && !lastJumpKey;
    lastJumpKey = jumpDown;
    return input;
}

void updateGameplayCamera(const Player& player, const Environment& environment, const MissionManager& mission, float timeSeconds, float dt) {
    // Ajusta una sola cámara para 2D y 3D según el modo activo y el contexto del nivel.
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
        const bool marioMap4 = isMarioMapa4Environment(environment);
        const float yaw = glm::radians(cameraYawDegrees);
        const float pitch = glm::radians(cameraPitchDegrees);
        const float distance = marioMap4 ? Map4Camera3DDistance : DefaultCamera3DDistance;
        const float horizontalDistance = std::cos(pitch) * distance;
        const glm::vec3 orbitOffset(
            std::sin(yaw) * horizontalDistance,
            (marioMap4 ? 0.38f : 0.55f) + std::sin(pitch) * distance,
            std::cos(yaw) * horizontalDistance);
        desiredTarget = player.position() + glm::vec3(0.0f, marioMap4 ? 0.44f : 0.50f, 0.0f);
        desiredPosition = desiredTarget + orbitOffset;
    } else {
        const float camera2DTargetHeight = DefaultCamera2DTargetHeight;
        const float camera2DHeight = DefaultCamera2DHeight;
        const float camera2DDistance = isMarioMapa4Environment(environment)
            ? Map4Camera2DDistance
            : DefaultCamera2DDistance;
        desiredTarget = player.position() + glm::vec3(0.0f, camera2DTargetHeight, 0.0f);
        desiredPosition = desiredTarget + glm::vec3(0.0f, camera2DHeight, camera2DDistance);
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
//funcion map4 uplaodCommon scene uniforms
void uploadCommonSceneUniforms(const Shader& shader, const Environment& environment, const glm::vec3& cameraPosition, const glm::mat4& view, const glm::mat4& projection, float timeSeconds, const glm::vec3* playerLightPosition = nullptr, float playerLightRatio = 1.0f, const std::vector<glm::vec3>* extraGlowLights = nullptr) {
    // Envía al shader la cámara, la iluminación común y las variaciones especiales de cada mapa.
    shader.use();
    shader.setMat4("uView", view);
    shader.setMat4("uProjection", projection);
    shader.setVec3("uCameraPosition", cameraPosition);
    const bool marioMap4 = isMarioMapa4Environment(environment);
    shader.setFloat("uTime", timeSeconds);

    if (marioMap4) {
        shader.setVec3("uAmbientColor", {0.0f, 0.0f, 0.0f});
        shader.setVec3("uDirectionalLight.direction", {-0.30f, -0.80f, -0.40f});
        shader.setVec3("uDirectionalLight.color", {0.0f, 0.0f, 0.0f});
        shader.setVec3("uFogColor", {0.01f, 0.02f, 0.04f});
        shader.setFloat("uSceneExposure", 1.0f);
    } else {
        shader.setVec3("uAmbientColor", {0.30f, 0.30f, 0.34f});
        shader.setVec3("uDirectionalLight.direction", {-0.30f, -0.80f, -0.40f});
        shader.setVec3("uDirectionalLight.color", {0.52f, 0.54f, 0.58f});
        shader.setVec3("uFogColor", {0.12f, 0.025f, 0.018f});
        shader.setFloat("uSceneExposure", 1.0f);
    }

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
    // iluminacion map4
    if (marioMap4) {
        const int extraBase = count;
        const int glowCount = 0;
        const int extraCount = std::min((playerLightPosition ? 1 : 0) + glowCount, MaxLights - count);
        shader.setInt("uPointLightCount", count + extraCount);
        int nextLightIndex = extraBase;
        if (playerLightPosition && nextLightIndex < count + extraCount) {
            const std::string prefix = "uPointLights[" + std::to_string(nextLightIndex) + "]";
            shader.setVec3(prefix + ".position", *playerLightPosition + glm::vec3(0.0f, 0.46f, currentMode == PlayMode::Mode2D ? 0.36f : 0.18f));
            shader.setVec3(prefix + ".color", glm::vec3(0.94f, 0.88f, 0.72f));
            const float lightScale = std::clamp(playerLightRatio, 0.0f, 1.0f);
            const float intensity = lightScale * 1.18f;
            const float radius = lightScale * 3.45f;
            shader.setFloat(prefix + ".intensity", intensity);
            shader.setFloat(prefix + ".radius", radius);
            ++nextLightIndex;
        }
    }
}

bool initializeMenu(MenuContext& menu) {
    // Crea texturas, fuentes rasterizadas y buffers de UI usados por todo el juego.
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
    menu.cargando = createTextSprite(L"Cargando", 48, titleColor, 460, false, true);
    menu.preparandoMundo = createTextSprite(L"Preparando mundo...", 27, white, 430, false, true);
    menu.modoNoDisponible = createTextSprite(L"No est\u00e1 disponible", 28, white, 330, false, true);
    for (int i = 0; i <= MissionCoinTotal; ++i) {
        menu.coinCounters[i] = createTextSprite(formatCoinProgress(i), 28, white, 132, false, true);
        menu.coinMessages[i] = createTextSprite(formatCoinProgress(i), 31, white, 132, false, true);
    }
    menu.estrellaLista = createTextSprite(L"\u00a1La estrella apareci\u00f3!", 29, titleColor, 400, false, true);
    menu.nivelCompletado = createTextSprite(L"\u00a1Nivel completado! \u00a1Ganaste!", 48, titleColor, 660, false, true);
    menu.juegoTerminado = createTextSprite(L"Juego terminado", 52, titleColor, 520, false, true);
    menu.combateSolo2D = createTextSprite(L"\u00a1Peligro! Cambia a 2D con TAB para detener a los enemigos.", 27, white, 720, false, true);
    menu.vidaJugador = createTextSprite(L"VIDA", 25, white, 120, false, false);
    menu.luzJugador = createTextSprite(L"LUZ", 25, white, 120, false, false);
    menu.mapa4Hint = createTextSprite(L"Follow the light and find the golden coins", 20, white, 520, false, true);
    menu.cargandoAtaque = createTextSprite(L"CARGANDO TIRO", 21, white, 250, false, false);
    menu.parryActivo = createTextSprite(L"PARRY", 23, white, 150, false, true);
    menu.tiendaBoton = createTextSprite(L"TIENDA  [B]", 23, white, 190, false, true);
    menu.tiendaTitulo = createTextSprite(L"CABINA DEL CAZADOR", 38, white, 520, false, true);
    menu.tiendaSubtitulo = createTextSprite(L"ARCHIVO DE HABILIDADES // MUNDO 01", 18, glm::vec3(0.78f), 500, false, false);
    menu.tiendaCerrar = createTextSprite(L"CERRAR", 19, white, 120, false, true);
    menu.tiendaHabilidades = createTextSprite(L"HABILIDADES", 19, white, 175, false, true);
    menu.tiendaManual = createTextSprite(L"MANUAL", 19, white, 150, false, true);
    menu.tiendaCabina = createTextSprite(L"CABINA", 19, white, 150, false, true);
    menu.tiendaSaldo = createTextSprite(L"GEMAS ROJAS", 17, white, 160, false, true);
    for (int i = 0; i < static_cast<int>(menu.gemCounters.size()); ++i) {
        menu.gemCounters[static_cast<size_t>(i)] =
            createTextSprite(L"x " + twoDigits(i), 24, white, 105, false, true);
    }
    menu.saltoEspectralTitulo = createTextSprite(L"SALTO ESPECTRAL", 28, white, 330, false, true);
    menu.saltoEspectralDescripcion = createTextSprite(
        L"Rompe la distancia entre anclas azules y alcanza islas imposibles.\n"
        L"Una compra permanente hasta reiniciar completamente el mundo.",
        18, white, 570, true, false);
    menu.saltoEspectralControl = createTextSprite(L"USO: ac\u00e9rcate a un ancla azul y pulsa Q", 18, white, 500, false, true);
    menu.parryRetornoTitulo = createTextSprite(L"PARRY: RETORNO REAL", 28, white, 390, false, true);
    menu.parryRetornoDescripcion = createTextSprite(
        L"Un parry perfecto ya no solo bloquea: devuelve el proyectil\n"
        L"con fuerza suficiente para destruir al demonio que lo lanz\u00f3.",
        18, white, 590, true, false);
    menu.parryRetornoControl = createTextSprite(L"USO: pulsa F justo antes de recibir el impacto", 18, white, 510, false, true);
    menu.comprarCincoGemas = createTextSprite(L"COMPRAR  5", 18, white, 165, false, true);
    menu.comprarTresGemas = createTextSprite(L"COMPRAR  3", 18, white, 165, false, true);
    menu.habilidadAdquirida = createTextSprite(L"ADQUIRIDA", 18, white, 155, false, true);
    menu.gemasInsuficientes = createTextSprite(L"FALTAN GEMAS", 16, white, 165, false, true);
    menu.manualTitulo = createTextSprite(L"MANUAL DE COMBATE Y MOVIMIENTO", 28, white, 560, false, true);
    menu.manualMovimiento = createTextSprite(L"MOVERSE // 2D: A y D     3D: W, A, S y D", 18, white, 650, false, true);
    menu.manualSalto = createTextSprite(L"SALTAR // W en 2D     ESPACIO en 3D", 18, white, 620, false, true);
    menu.manualDimension = createTextSprite(L"CAMBIAR DIMENSI\u00d3N // TAB alterna entre 2D y 3D", 18, white, 680, false, true);
    menu.manualDisparo = createTextSprite(L"DISPARAR // apunta con el mouse y haz clic izquierdo", 18, white, 680, false, true);
    menu.manualCarga = createTextSprite(L"TIRO CARGADO // mant\u00e9n clic izquierdo y suelta al llenarse", 18, white, 720, false, true);
    menu.manualParry = createTextSprite(L"PARRY // pulsa F en el instante anterior al impacto", 18, white, 670, false, true);
    menu.manualTienda = createTextSprite(L"TIENDA REMOTA // pulsa B desde cualquier lugar del mapa", 18, white, 690, false, true);
    menu.cabinaPrompt = createTextSprite(L"PULSA E PARA USAR LA CABINA TELEF\u00d3NICA", 22, white, 440, false, true);
    menu.tiendaAyudaScroll = createTextSprite(L"RUEDA DEL MOUSE\nPARA DESPLAZAR", 16, white, 180, true, true);
    menu.enemigosRestantes = createTextSprite(L"LOS DEMONIOS SUELTAN GEMAS ROJAS", 17, white, 310, false, true);
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
    // Bucle principal de Mundo 2: entrada, actualización, cámara, render y HUD.
    const PlayerInput playerInput = buildPlayerInput(window, mundo2.player);
    const bool interactDown = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    const bool interactPressed = interactDown && !lastInteractKey;
    lastInteractKey = interactDown;

    std::vector<Bounds> playerColliders = mundo2.environment.collisionPreview();
    appendDimensionRestrictionColliders(playerColliders, mundo2.environment, locked2DDepth);
    mundo2.player.update(playerInput, playerColliders, mundo2.environment.worldMin(), mundo2.environment.worldMax(), deltaTime);
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

    mundo2.environment.render(sceneShader, lavaShader, now, gameplayCameraPosition);
    mundo2.mission.render(sceneShader, now, gameplayCameraPosition);
    mundo2.toad.render(sceneShader, now);
    mundo2.player.render(sceneShader);
    drawMissionHud(menu, mundo2.mission, width, height, now);
    drawToadHud(menu, mundo2.toad, width, height);
}

bool cargarMundoPendiente(GLFWwindow* window, Mapa1& mapa1, Mundo2Runtime& mundo2, Map3Runtime& map3, Mapa4Runtime& mapa4, MenuContext& menu) {
    bool loaded = false;
    switch (loadingTarget) {
    case EstadoJuego::MUNDO_1:
        loaded = mapa1.initialize();
        break;
    case EstadoJuego::MUNDO_2:
        loaded = iniciarMundo2(mundo2);
        break;
    case EstadoJuego::MUNDO_3:
        loaded = iniciarMap3(map3);
        break;
    case EstadoJuego::MUNDO_4:
        loaded = iniciarMapa4(mapa4);
        break;
    default:
        break;
    }

    if (loaded) {
        appState = loadingTarget;
    } else {
        appState = EstadoJuego::MENU_MUNDOS;
        menu.notificationUntil = glfwGetTime() + 2.3;
    }

    loadingTarget = EstadoJuego::MENU_PRINCIPAL;
    loadingScreenPresented = false;
    lastFrame = static_cast<float>(glfwGetTime());
    deltaTime = 0.0f;
    updateCursorMode(window);
    return loaded;
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
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    glGetError();
    glfwSwapInterval(1);

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
    Map3Runtime map3;
    Mapa4Runtime mapa4;

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
        deltaTime = std::clamp(now - lastFrame, 0.0f, 1.0f / 30.0f);
        lastFrame = now;

        updateCursorMode(window);
        processAppInput(window, mapa1, mundo2, map3, mapa4);
        if (appState == EstadoJuego::CARGANDO && loadingScreenPresented) {
            cargarMundoPendiente(window, mapa1, mundo2, map3, mapa4, menu);
        }

        double cursorX = 0.0;
        double cursorY = 0.0;
        glfwGetCursorPos(window, &cursorX, &cursorY);
        const bool mouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool clicked = mouseDown && !lastMouseButton;
        lastMouseButton = mouseDown;
        const float scrollY = static_cast<float>(pendingScrollY);
        pendingScrollY = 0.0;

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);

        if (appState == EstadoJuego::MUNDO_1) {
            if (!mapa1.initialize()) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            } else {
                mapa1.render(window, deltaTime);
                drawMissionHud(menu, mapa1, width, height, now);
                drawMapa1CombatHud(
                    menu,
                    mapa1,
                    width,
                    height,
                    now,
                    {static_cast<float>(cursorX), static_cast<float>(cursorY)},
                    clicked,
                    scrollY);
            }
        } else if (appState == EstadoJuego::MUNDO_2) {
            if (!iniciarMundo2(mundo2)) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            } else {
                renderMundo2(window, mundo2, menu, sceneShader, lavaShader, now);
                drawModeSwitchUnavailableMessage(menu, width, height, now);
            }
        } else if (appState == EstadoJuego::MUNDO_3) {
            if (!iniciarMap3(map3)) {
                appState = EstadoJuego::MENU_MUNDOS;
                menu.notificationUntil = 0.0;
            } else {
                renderMap3(window, map3, sceneShader, lavaShader, now);
                drawSimpleHealthHud(menu, map3.health, map3.maxHealth, width, height);
                drawMap3PositionHud(menu, map3, width, height);
                if (map3.mission.levelComplete()) {
                    drawLevelCompleteHud(menu, width, height);
                } else if (map3.gameOver) {
                    drawGameOverHud(menu, width, height);
                }
                drawModeSwitchUnavailableMessage(menu, width, height, now);
            }
        } else if (appState == EstadoJuego::MUNDO_4) {
            if (!iniciarMapa4(mapa4)) {
                appState = EstadoJuego::MENU_MUNDOS;
                menu.notificationUntil = glfwGetTime() + 2.3;
            } else {
                renderMapa4(window, mapa4, sceneShader, lavaShader, now);
                drawMissionHud(menu, mapa4.mission, width, height, now);
                drawSimpleHealthHud(menu, mapa4.health, mapa4.maxHealth, width, height);
                renderMapa4Hud(menu, mapa4, width, height, now);
                drawShieldHud(menu, width, height, mapa4.shieldActive);
                if (mapa4.gameOver) {
                    drawGameOverHud(menu, width, height);
                }
                drawModeSwitchUnavailableMessage(menu, width, height, now);
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
            case EstadoJuego::CARGANDO:
                mostrarPantallaCarga(menu, width, height, now);
                loadingScreenPresented = true;
                break;
            case EstadoJuego::MUNDO_1:
            case EstadoJuego::MUNDO_2:
            case EstadoJuego::MUNDO_3:
            case EstadoJuego::MUNDO_4:
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
