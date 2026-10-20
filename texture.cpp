#include "texture.h"

Texture::Texture(const char* filename) : path(filename)
{
    int widthImg, heightImg, nrComponents;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* bytes = stbi_load(filename, &widthImg, &heightImg, &nrComponents, 0);
    if (not bytes)
    {
        std::cout << "Failed to load texture" << std::endl;
        throw;
    }
    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_2D, ID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    GLenum format;
    if (nrComponents == 1)
        format = GL_RED;
    else if (nrComponents == 3)
        format = GL_RGB;
    else if (nrComponents == 4)
        format = GL_RGBA;

    glTexImage2D(GL_TEXTURE_2D, 0, format, widthImg, heightImg, 0, format, GL_UNSIGNED_BYTE, bytes);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(bytes);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::texUnit(Shader shader, const char* uniform, GLuint unit) {
    shader.SetInt(uniform,unit);
}

void Texture::Bind() {
    glBindTexture(GL_TEXTURE_2D, ID);
}

void Texture::Unbind() {
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::Delete() {
    glDeleteTextures(1, &ID);
}