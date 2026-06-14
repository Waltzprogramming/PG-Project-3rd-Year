#pragma once

#include "Shader.h"
#include "Texture2D.h"

#include <array>
#include <glm/glm.hpp>
#include <memory>
#include <string>

inline constexpr int MissionCoinTotal = 10;

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
    TextSprite modoNoDisponible;
    std::array<TextSprite, MissionCoinTotal + 1> coinCounters;
    std::array<TextSprite, MissionCoinTotal + 1> coinMessages;
    TextSprite estrellaLista;
    TextSprite nivelCompletado;
    TextSprite juegoTerminado;
    TextSprite combateSolo2D;
    TextSprite vidaJugador;
    TextSprite luzJugador;
    TextSprite map3PlayerX;
    TextSprite mapa4Hint;
    TextSprite cargandoAtaque;
    TextSprite parryActivo;
    TextSprite tiendaBoton;
    TextSprite tiendaTitulo;
    TextSprite tiendaSubtitulo;
    TextSprite tiendaCerrar;
    TextSprite tiendaHabilidades;
    TextSprite tiendaManual;
    TextSprite tiendaCabina;
    TextSprite tiendaSaldo;
    std::array<TextSprite, 100> gemCounters;
    TextSprite saltoEspectralTitulo;
    TextSprite saltoEspectralDescripcion;
    TextSprite saltoEspectralControl;
    TextSprite parryRetornoTitulo;
    TextSprite parryRetornoDescripcion;
    TextSprite parryRetornoControl;
    TextSprite comprarCincoGemas;
    TextSprite comprarTresGemas;
    TextSprite habilidadAdquirida;
    TextSprite gemasInsuficientes;
    TextSprite manualTitulo;
    TextSprite manualMovimiento;
    TextSprite manualSalto;
    TextSprite manualDimension;
    TextSprite manualDisparo;
    TextSprite manualCarga;
    TextSprite manualParry;
    TextSprite manualTienda;
    TextSprite cabinaPrompt;
    TextSprite tiendaAyudaScroll;
    TextSprite enemigosRestantes;
    TextSprite promptHablarToad;
    TextSprite nombreToad;
    TextSprite dialogoToad;
};

TextSprite createTextSprite(const std::wstring& text, int fontSize, const glm::vec3& color, int maxWidth, bool multiline, bool bold);
Rect centeredRect(float centerX, float y, float width, float height);
void beginUiFrame(MenuContext& menu, int width, int height);
void drawRect(MenuContext& menu, const Rect& rect, const glm::vec4& color);
void drawText(MenuContext& menu, const TextSprite& text, float x, float y, const glm::vec4& tint);
inline void drawText(MenuContext& menu, const TextSprite& text, float x, float y) {
    drawText(menu, text, x, y, glm::vec4(1.0f));
}
void drawPanel(MenuContext& menu, const Rect& rect);
