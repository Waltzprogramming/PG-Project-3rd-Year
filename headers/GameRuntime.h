#pragma once

#include "Environment.h"
#include "GameSystems.h"
#include "GameUI.h"
#include "Mapa1.h"
#include "Player.h"
#include "Shader.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <initializer_list>
#include <filesystem>
#include <string>
#include <vector>

extern float deltaTime;
extern PlayMode currentMode;
extern bool lastToggleKey;
extern bool lastJumpKey;
extern bool lastTeleportKey;
extern bool lastShieldKey;
extern float cameraYawDegrees;
extern float cameraPitchDegrees;
extern float locked2DDepth;
extern glm::vec3 gameplayCameraPosition;
extern glm::vec3 gameplayCameraTarget;
extern bool cameraInitialized;

bool environmentUsable(const Environment& environment);
bool modeRestrictedAtX(PlayMode mode, float x);
void appendDimensionRestrictionColliders(std::vector<Bounds>& colliders, const Environment& environment, float lockedDepth);
std::string resolveAssetPath(const std::string& path);
std::string resolveFirstExistingAsset(const std::initializer_list<std::string>& paths);
std::shared_ptr<Texture2D> loadTextureFromMaterial(const LoadedMaterial& material, const std::filesystem::path& modelPath, std::vector<std::shared_ptr<Texture2D>>& cache);
void bindSceneMaterial(const Shader& shader, const Material& material);
glm::mat4 localPartMatrix(const MissionRenderablePart& part);
PlayerInput buildPlayerInput(GLFWwindow* window, const Player& player);
void updateGameplayCamera(const Player& player, const Environment& environment, const MissionManager& mission, float timeSeconds, float dt);
void uploadCommonSceneUniforms(const Shader& shader, const Environment& environment, const glm::vec3& cameraPosition, const glm::mat4& view, const glm::mat4& projection, float timeSeconds, const glm::vec3* playerLightPosition, float playerLightRatio, const std::vector<glm::vec3>* extraGlowLights);
void drawSimpleHealthHud(MenuContext& menu, int currentHealth, int maximumHealth, int width, int height);
void drawGameOverHud(MenuContext& menu, int width, int height);
void drawShieldHud(MenuContext& menu, int width, int height, bool active);
