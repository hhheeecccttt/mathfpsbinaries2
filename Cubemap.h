#ifndef CUBEMAP_CLASS
#define CUBEMAP_CLASS

#include <vector>

#include "stb_image.h"
#include <glad/glad.h>

#include "shaderClass.h"

class Cubemap {
public:
	GLuint ID;

    Cubemap() {
    };

	Cubemap(std::vector<std::string>& faces) {
        FillCubemap(faces);
	};

    void FillCubemap(std::vector<std::string>& faces) {

        if (ID != 0) {
            Delete();
        }

        glGenTextures(1, &ID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, ID);

        int width, height, nrChannels;
        for (unsigned int i = 0; i < faces.size(); i++)
        {
            unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
            if (data)
            {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                    0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
                );
                stbi_image_free(data);
            }
            else
            {
                std::cout << "Cubemap tex failed to load at path: " << faces[i] << std::endl;
                stbi_image_free(data);
            }
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    void Bind() {
        glBindTexture(GL_TEXTURE_CUBE_MAP, ID);
    };
    void Unbind() {
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    };
    void Delete() {
        glDeleteTextures(1, &ID);
    };
};

#endif