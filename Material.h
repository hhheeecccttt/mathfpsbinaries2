#ifndef MATERIAL_CLASS
#define MATERIAL_CLASS

#include "shaderClass.h"

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>

class Material {

public:
    Material() = delete;
    Material(glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, float shininess) : 
        ambient(ambient), diffuse(diffuse), specular(specular), shininess(shininess)
    { }
    void Bind(Shader& shader) {
        shader.Set3F("material.ambient", ambient);
        shader.Set3F("material.diffuse", diffuse);
        shader.Set3F("material.specular", specular);
        shader.Set1F("material.shininess", shininess);
    };

    void Unbind(Shader shader) {
        shader.Set3F("material.ambient", glm::vec3(1.0f));
        shader.Set3F("material.diffuse", glm::vec3(0.0f));
        shader.Set3F("material.specular", glm::vec3(0.0f));
        shader.Set1F("material.shininess", 0.0f);
        //finish unbinding
    };
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float shininess;

private:

};

#endif