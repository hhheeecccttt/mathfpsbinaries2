
#ifndef DIRLIGHT_CLASS
#define DIRLIGHT_CLASS

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>

#include "shaderClass.h"

class DirLight {

public:
    DirLight() {

    }
    DirLight(glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, glm::vec3 direction) 
    {
        Initialize(ambient,diffuse,specular,direction);
    }

    void Initialize(glm::vec3 newambient, glm::vec3 newdiffuse, glm::vec3 newspecular, glm::vec3 newdirection) {
        ambient = glm::normalize(newambient);
        diffuse = glm::normalize(newdiffuse);
        specular = glm::normalize(newspecular);
        direction = glm::normalize(newdirection);
    }

    void Bind(Shader& shader) {
        shader.Set3F("dirLight.ambient", ambient);
        shader.Set3F("dirLight.diffuse", diffuse);
        shader.Set3F("dirLight.specular", specular);
        shader.Set3F("dirLight.direction", direction);
    };

    void Unbind(Shader shader) {
        shader.Set3F("dirLight.ambient", glm::vec3(1.0f));
        shader.Set3F("dirLight.diffuse", glm::vec3(0.0f));
        shader.Set3F("dirLight.specular", glm::vec3(0.0f));
        shader.Set3F("dirLight.direction", glm::vec3(1.0f));
        //finish unbinding
    };
    
    glm::vec3 direction;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

private:

};

#endif