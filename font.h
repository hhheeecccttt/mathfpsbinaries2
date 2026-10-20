#ifndef FONT_CLASS
#define FONT_CLASS
#include <ft2build.h>
#include FT_FREETYPE_H
#include <map>
#include <iostream>
#include <glm/glm.hpp>
#include "VAO.h"
#include "VBO.h"

struct TextCharacter {
    unsigned int TextureID;
    glm::ivec2   Size;
    glm::ivec2   Bearing;
    unsigned int Advance;
};

class Font {
public:
    VAO vao;
    VBO vbo;
    std::map<char, TextCharacter> Characters;

    Font(const char* directory) {
        // Correct order: bind VAO first, then VBO
        vao.GenerateID();  // ADD
        vbo.GenerateID();  // ADD
        vao.Bind();
        vbo.Bind();
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        vbo.Unbind();
        vao.Unbind();

        FT_Library ft;
        if (FT_Init_FreeType(&ft)) {
            std::cout << "ERROR::FREETYPE: Could not init FreeType Library\n";
            throw std::runtime_error("FreeType init failed");
        }

        FT_Face face;
        if (FT_New_Face(ft, directory, 0, &face)) {
            std::cout << "ERROR::FREETYPE: Failed to load font\n";
            FT_Done_FreeType(ft);
            throw std::runtime_error("Font load failed");
        }


        FT_Set_Pixel_Sizes(face, 0, 48);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (unsigned char c = 0; c < 128; c++) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                std::cout << "ERROR::FREETYPE: Failed to load Glyph " << c << "\n";
                continue;
            }

            unsigned int texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                face->glyph->bitmap.width,
                face->glyph->bitmap.rows,
                0, GL_RED, GL_UNSIGNED_BYTE,
                face->glyph->bitmap.buffer
            );
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            Characters[c] = {
                texture,
                glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                (unsigned int)face->glyph->advance.x
            };
        }

        FT_Done_Face(face);
        FT_Done_FreeType(ft);
    }
};

#endif