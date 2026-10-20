#ifndef BOX_CLASS
#define BOX_CLASS

#include <iostream>
#include "common.h"
#include "VAO.h"
#include "VBO.h"
#include "QuadVertices.h"
#include "shaderClass.h"
#include "texture.h"
#include "t2d.h"
#include "font.h"
#include "RenderSystem.h"
#include "Object.h"

class Box : public Renderable, public Object {
public:
    VAO boxVAO;
    VBO boxVBO;

    glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
    float     Opacity = 1.0f;
    t2d_package t2d;
    float z = 0.0f;
    float rotation = 0.0f;
    float rounding = 0.0f;

    Box() {
        shadertype = ShaderType::BoxShader;
        boxVBO.GenerateID();
        boxVAO.GenerateID();
        boxVAO.Bind();
        boxVBO.BufferData(&quadVertices, sizeof(quadVertices));
        boxVAO.LinkVBO(boxVBO, 0, 2, GL_FLOAT, 4 * sizeof(float), (void*)0);
        boxVAO.LinkVBO(boxVBO, 1, 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    }

    virtual void Render(Shader& ShaderProgram) override {
        if (Opacity <= 0.0f) return;
        ShaderProgram.Activate();
        t2d.Recalculate();
        ShaderProgram.Set4F("Color", { Color, Opacity });
        ShaderProgram.Set2F("normalizedCenterScale", t2d.NormalizedCenterScale);
        ShaderProgram.Set2F("normalizedCenterPos", t2d.NormalizedCenterPos);
        ShaderProgram.Set1F("z", z);
        ShaderProgram.Set1F("rotation", rotation);
        ShaderProgram.Set2F("pixelScale", t2d.pixelsize);
        ShaderProgram.Set2F("pixelPos", t2d.pixelposition);
        ShaderProgram.Set1F("rounding", rounding);
        boxVAO.Bind();
        glDrawArrays(GL_TRIANGLES, 0, 6);
        boxVAO.Unbind();
    }
};

class BoxButton : public Box {
public:
    bool clicked = false;

    void UpdateClicked(float mousex, float mousey) {
        if (mousex > t2d.pixelposition.x && mousey > t2d.pixelposition.y &&
            mousex < t2d.pixelposition.x + t2d.pixelsize.x &&
            mousey < t2d.pixelposition.y + t2d.pixelsize.y)
        {
            clicked = true;
            std::cout << "CLICKED\n";
        }
        else {
            clicked = false;
        }
    }

    BoxButton() {}
};

class TextBox : public Box {
public:
    Font* font = nullptr;
    std::string text;
    float       fontsize = 1.0f;

    // textCenter: 0,0 = text top-left anchored to box top-left
    //             0.5,0.5 = text centered in box
    //             1,1 = text bottom-right anchored to box bottom-right
    glm::vec2 textCenter = { 0.0f, 0.0f };

    TextBox() {
        shadertype = ShaderType::TextShader;
    }

    virtual void Render(Shader& ShaderProgram) override {
        if (!font || Opacity <= 0.0f) return;
        t2d.Recalculate();

        // Box bounds in pixels
        const float boxLeft = t2d.pixelposition.x;
        const float boxTop = t2d.pixelposition.y;
        const float boxRight = boxLeft + t2d.pixelsize.x;
        const float boxBottom = boxTop + t2d.pixelsize.y;

        // Split text into lines
        std::vector<std::string> lines;
        std::string current;
        for (char c : text) {
            if (c == '\n') { lines.push_back(current); current.clear(); }
            else current += c;
        }
        lines.push_back(current);

        // Measure line height
        float lineHeight = 0.0f;
        float ascent = 0.0f;  // glyphs are drawn up from the baseline, so lines start this far below the top
        for (auto& [key, ch] : font->Characters) {
            lineHeight = glm::max(lineHeight, (float)ch.Size.y * fontsize);
            ascent = glm::max(ascent, (float)ch.Bearing.y * fontsize);
        }
        const float lineSpacing = lineHeight * 1.2f;
        const float totalBlockH = lineSpacing * (float)lines.size();

        // Vertical start: textCenter.y controls where the block sits inside the box
        // 0 = top, 0.5 = center, 1 = bottom
        float blockTop = boxTop + (t2d.pixelsize.y - totalBlockH) * textCenter.y;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Scissor to clip text to box bounds
        glEnable(GL_SCISSOR_TEST);
        glScissor(
            (GLint)boxLeft,
            (GLint)(window->height - boxBottom),  // OpenGL scissor Y is from bottom
            (GLsizei)t2d.pixelsize.x,
            (GLsizei)t2d.pixelsize.y
        );

        ShaderProgram.Activate();
        ShaderProgram.Set4F("textColor", { Color, Opacity });
        ShaderProgram.Set2F("screenSize", glm::vec2(window->width, window->height));
        ShaderProgram.SetInt("text", 0);
        glActiveTexture(GL_TEXTURE0);
        font->vao.Bind();

        for (int i = 0; i < (int)lines.size(); i++) {
            // Measure this line's width
            float lineWidth = 0.0f;
            for (char c : lines[i])
                lineWidth += (float)(font->Characters[(unsigned char)c].Advance >> 6) * fontsize;

            // Horizontal start: textCenter.x controls where the line sits inside the box
            float x = boxLeft + (t2d.pixelsize.x - lineWidth) * textCenter.x;
            float y = blockTop + ascent + lineSpacing * (float)i;  // baseline

            for (char c : lines[i]) {
                TextCharacter ch = font->Characters[(unsigned char)c];

                if (ch.Size.x == 0 || ch.Size.y == 0) {
                    x += (float)(ch.Advance >> 6) * fontsize;
                    continue;
                }

                float x0 = x + ch.Bearing.x * fontsize;
                float y0 = y + (ch.Size.y - ch.Bearing.y) * fontsize;
                float x1 = x0 + ch.Size.x * fontsize;
                float y1 = y0 - ch.Size.y * fontsize;

                float vertices[6][4] = {
                    { x0, y1, 0.0f, 0.0f },
                    { x0, y0, 0.0f, 1.0f },
                    { x1, y0, 1.0f, 1.0f },
                    { x0, y1, 0.0f, 0.0f },
                    { x1, y0, 1.0f, 1.0f },
                    { x1, y1, 1.0f, 0.0f }
                };

                glBindTexture(GL_TEXTURE_2D, ch.TextureID);
                font->vbo.Bind();
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
                font->vbo.Unbind();
                glDrawArrays(GL_TRIANGLES, 0, 6);

                x += (float)(ch.Advance >> 6) * fontsize;
            }
        }

        font->vao.Unbind();
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_SCISSOR_TEST);
    }
};

class ImageBox : public Box {
public:
    Texture* tex = nullptr;

    ImageBox() {
        shadertype = ShaderType::ImageBoxShader;
    }

    virtual void Render(Shader& ShaderProgram) override {
        if (Opacity <= 0.0f) return;
        ShaderProgram.Activate();
        t2d.Recalculate();
        ShaderProgram.Set4F("Color", { Color, Opacity });
        ShaderProgram.Set2F("normalizedCenterScale", t2d.NormalizedCenterScale);
        ShaderProgram.Set2F("normalizedCenterPos", t2d.NormalizedCenterPos);
        ShaderProgram.Set1F("z", z);
        ShaderProgram.Set1F("rotation", rotation);
        ShaderProgram.Set2F("pixelScale", t2d.pixelsize);
        ShaderProgram.Set2F("pixelPos", t2d.pixelposition);
        ShaderProgram.Set1F("rounding", rounding);
        glActiveTexture(GL_TEXTURE0);
        if (tex) {
            glBindTexture(GL_TEXTURE_2D, tex->ID);
            ShaderProgram.SetInt("tex", 0);
        }
        else {
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        boxVAO.Bind();
        glDrawArrays(GL_TRIANGLES, 0, 6);
        boxVAO.Unbind();
    }
};

// Customizable crosshair: four arms plus an optional center dot, all in pixels.
// Change any setting, then call Apply().
class Crosshair {
public:
    glm::vec3 Color = { 0.0f, 1.0f, 0.0f };
    float     Opacity = 1.0f;
    float     Length = 9.0f;     // arm length
    float     Thickness = 2.2f;  // arm width
    float     Gap = 4.5f;        // distance from center to the start of each arm
    bool      Dot = false;       // center dot
    bool      Visible = true;

    Crosshair() {
        for (Box*& b : parts) {
            b = new Box();
            b->t2d.center = { 0.5f, 0.5f };
            b->BindToRenderSystem();
        }
        Apply();
    }

    void Apply() {
        const float offset = Gap + Length * 0.5f;
        // up, down, left, right
        const glm::vec2 offsets[4] = { {0, -offset}, {0, offset}, {-offset, 0}, {offset, 0} };
        for (int i = 0; i < 4; i++) {
            const bool vertical = i < 2;
            Setup(parts[i], offsets[i],
                  vertical ? Thickness : Length,
                  vertical ? Length : Thickness, Visible);
        }
        Setup(parts[4], { 0.0f, 0.0f }, Thickness, Thickness, Visible && Dot);
    }

private:
    Box* parts[5];

    void Setup(Box* b, glm::vec2 offset, float w, float h, bool visible) {
        b->Color = Color;
        b->Opacity = visible ? Opacity : 0.0f;
        b->t2d.position = { 0.5f, 0.5f, offset.x, offset.y };
        b->t2d.size = { 0.0f, 0.0f, w, h };
    }
};

#endif