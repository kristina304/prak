#include <glad/glad.h>
#include <GLFW/glfw3.h>


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "camera.h"

#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void renderScene(int shader);
void renderCube();
void renderQuad();

// настройки
const unsigned int SCR_WIDTH = 600;
const unsigned int SCR_HEIGHT = 400;

// камера
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = (float)SCR_WIDTH / 2.0;
float lastY = (float)SCR_HEIGHT / 2.0;
bool firstMouse = true;

// тайминги
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// меши
unsigned int planeVAO;

const char* debug_quad_vertex = "#version 330 core\n"
"layout(location = 0) in vec3 aPos;\n"
"layout(location = 1) in vec2 aTexCoords; \n"

"out vec2 TexCoords;\n"

"void main()\n"
"{\n"
"    TexCoords = aTexCoords;\n"
"    gl_Position = vec4(aPos, 1.0);\n"
"}\0";


const char* debug_quad_fragment = "#version 330 core\n"
"out vec4 FragColor;\n"

"in vec2 TexCoords;\n"

"uniform sampler2D depthMap;\n"
"uniform float near_plane;\n"
"uniform float far_plane;\n"

// требуется при использовании матрицы перспективной проекции
"float LinearizeDepth(float depth)\n"
"{\n"
"    float z = depth * 2.0 - 1.0;\n" // возвращаемся к NDC 
"    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));\n"
"}\n"

"void main()\n"
"{\n"
"    float depthValue = texture(depthMap, TexCoords).r;\n"
//"    FragColor = vec4(vec3(LinearizeDepth(depthValue) / far_plane), 1.0); \n"// перспектива
"    FragColor = vec4(vec3(depthValue), 1.0); // ортографическая\n"
"}\n\0";



const char* shadow_map_vertex = "#version 330 core\n"
"layout(location = 0) in vec3 aPos;\n"
"layout(location = 1) in vec3 aNormal;\n"
"layout(location = 2) in vec2 aTexCoords;\n"

"out vec2 TexCoords;\n"

"out VS_OUT{\n"
"    vec3 FragPos;\n"
"    vec3 Normal;\n"
"    vec2 TexCoords;\n"
"    vec4 FragPosLightSpace;\n"
"} vs_out;\n"

"uniform vec3 Color;\n"
"uniform mat4 projection;\n"
"uniform mat4 view;\n"
"uniform mat4 model;\n"
"uniform mat4 lightSpaceMatrix;\n"


"void main()\n"
"{\n"
"    vs_out.FragPos = vec3(model * vec4(aPos, 1.0));\n"
"    vs_out.Normal = transpose(inverse(mat3(model))) * aNormal;\n"
"    vs_out.TexCoords = aTexCoords;\n"
"    vs_out.FragPosLightSpace = lightSpaceMatrix * vec4(vs_out.FragPos, 1.0);\n"
"    gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
"}\0";

const char* shadow_map_fragment = "#version 330 core\n"
"out vec4 FragColor;\n"

"in VS_OUT{\n"
"    vec3 FragPos;\n"
"    vec3 Normal;\n"
"    vec2 TexCoords;\n"
"    vec4 FragPosLightSpace;\n"
"} fs_in;\n"

//"uniform sampler2D diffuseTexture;\n"
"uniform sampler2D shadowMap;\n"

"uniform vec3 lightPos;\n"
"uniform vec3 viewPos;\n"

"float ShadowCalculation(vec4 fragPosLightSpace)\n"
"{\n"
// выполняем деление перспективы
"    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;\n"
// трансформируем в диапазон [0,1]
"    projCoords = projCoords * 0.5 + 0.5;\n"
// получаем наиболее близкое значение глубины исходя из перспективы глазами источника света (используя в диапазон [0,1] fragPosLight в качестве координат)
"    float closestDepth = texture(shadowMap, projCoords.xy).r;\n"
// получаем глубину текущего фрагмента исходя из перспективы глазами источника света
"    float currentDepth = projCoords.z;\n"
// вычисляем смещение (на основе разрешения карты глубины и наклона)
"    vec3 normal = normalize(fs_in.Normal);\n"
"    vec3 lightDir = normalize(lightPos - fs_in.FragPos);\n"
"   float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);\n"
// проверка нахождения текущего фрагмента в тени
//" float shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;\n"
// PCF
"   float shadow = 0.0;\n"
"    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);\n"
"    for (int x = -1; x <= 1; ++x)\n"
"    {\n"
"        for (int y = -1; y <= 1; ++y)\n"
"        {\n"
"            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;\n"
"            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;\n"
"        }\n"
"    }\n"
"    shadow /= 9.0;\n"

// оставляем значение тени на уровне 0.0 за границей дальней плоскости пирамиды видимости глазами источника света.
"    if (projCoords.z > 1.0)\n"
"        shadow = 0.0;\n"

"    return shadow;\n"
"}\n"

"void main()\n"
"{\n"
"    vec3 color = texture(shadowMap, fs_in.TexCoords).rgb;"
"    vec3 normal = normalize(fs_in.Normal);\n"
"    vec3 lightColor = vec3(1.0);\n"
// фоновая составляющая
"    vec3 ambient = 0.3 * color;\n"
// диффузная составляющая
"    vec3 lightDir = normalize(lightPos - fs_in.FragPos);\n"
"    float diff = max(dot(lightDir, normal), 0.0);\n"
"   vec3 diffuse = diff * lightColor;\n"
// отраженная составляющая
"    vec3 viewDir = normalize(viewPos - fs_in.FragPos);\n"
"    vec3 reflectDir = reflect(-lightDir, normal);\n"
"   float spec = 0.0;\n"
"    vec3 halfwayDir = normalize(lightDir + viewDir);\n"
"    spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);\n"
"   vec3 specular = spec * lightColor;\n"
// вычисляем тень
"    float shadow = ShadowCalculation(fs_in.FragPosLightSpace);\n"
"    vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;\n"

"    FragColor = vec4(lighting, 1.0);\n"
"}\n\0";

const char* shadow_map_depth_vertex = "#version 330 core\n"//матрицу модели каждого объекта преобразует в световое пространоство
"layout(location = 0) in vec3 aPos;\n"

"uniform mat4 lightSpaceMatrix;\n"
"uniform mat4 model;\n"

"void main()\n"
"{\n"
"    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);\n"
"}\0";

const char* shadow_map_depth_fragment = "#version 330 core\n"

"void main()\n"
"{\n"
// "gl_FragDepth = gl_FragCoord.z;\n"
"}\n\0";

int main()
{
 
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "MASHGRAPH_KRISTINA", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    //курсор
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    //указатели

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    //шейдеры

    // Вершинный шейдер
    int vs1 = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs1, 1, &shadow_map_vertex, NULL);
    glCompileShader(vs1);

    int success;
    char infoLog[512];
    glGetShaderiv(vs1, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vs1, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // Фрагментный шейдер
    int fs1 = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs1, 1, &shadow_map_fragment, NULL);
    glCompileShader(fs1);

    glGetShaderiv(fs1, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fs1, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // Связывание шейдеров
    int shader = glCreateProgram();
    glAttachShader(shader, vs1);
    glAttachShader(shader, fs1);
    glLinkProgram(shader);

    glGetProgramiv(shader, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(vs1);
    glDeleteShader(fs1);



    // Вершинный шейдер
    int vs2 = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs2, 1, &shadow_map_depth_vertex, NULL);
    glCompileShader(vs2);

    int success2;
    char infoLog2[512];
    glGetShaderiv(vs2, GL_COMPILE_STATUS, &success2);
    if (!success2)
    {
        glGetShaderInfoLog(vs2, 512, NULL, infoLog2);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog2 << std::endl;
    }

    // Фрагментный шейдер
    int fs2 = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs2, 1, &shadow_map_depth_fragment, NULL);
    glCompileShader(fs2);

    glGetShaderiv(fs2, GL_COMPILE_STATUS, &success2);
    if (!success2)
    {
        glGetShaderInfoLog(fs2, 512, NULL, infoLog2);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog2 << std::endl;
    }

    // Связывание шейдеров
    int simpledepthshader = glCreateProgram();
    glAttachShader(simpledepthshader, vs2);
    glAttachShader(simpledepthshader, fs2);
    glLinkProgram(simpledepthshader);

    glGetProgramiv(simpledepthshader, GL_LINK_STATUS, &success2);
    if (!success2) {
        glGetProgramInfoLog(simpledepthshader, 512, NULL, infoLog2);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog2 << std::endl;
    }
    glDeleteShader(vs2);
    glDeleteShader(fs2);



    // Вершинный шейдер
    int vs3 = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs3, 1, &debug_quad_vertex, NULL);
    glCompileShader(vs3);

    int success3;
    char infoLog3[512];
    glGetShaderiv(vs3, GL_COMPILE_STATUS, &success3);
    if (!success3)
    {
        glGetShaderInfoLog(vs3, 512, NULL, infoLog3);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog3 << std::endl;
    }

    // Фрагментный шейдер
    int fs3 = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs3, 1, &debug_quad_fragment, NULL);
    glCompileShader(fs3);

    glGetShaderiv(fs3, GL_COMPILE_STATUS, &success3);
    if (!success3)
    {
        glGetShaderInfoLog(fs3, 512, NULL, infoLog3);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog3 << std::endl;
    }

    // Связывание шейдеров
    int debugdepthquad = glCreateProgram();
    glAttachShader(debugdepthquad, vs3);
    glAttachShader(debugdepthquad, fs3);
    glLinkProgram(debugdepthquad);

    glGetProgramiv(debugdepthquad, GL_LINK_STATUS, &success3);
    if (!success3) {
        glGetProgramInfoLog(debugdepthquad, 512, NULL, infoLog3);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog3 << std::endl;
    }
    glDeleteShader(vs3);
    glDeleteShader(fs3);

    float planeVertices[] = {
        // координаты            // нормали         // текстурные координаты
         25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
        -25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,   0.0f,  0.0f,
        -25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,

         25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
        -25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,
         25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,  25.0f, 25.0f
    };
    //пол
    unsigned int planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);


    const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    //текстура глубины
    unsigned int depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);



    glUseProgram(shader);
//    shader.setInt("diffuseTexture", 0);
    glUniform1i(glGetUniformLocation(shader, "shadowMap"), 1);
    glUseProgram(debugdepthquad);
    glUniform1i(glGetUniformLocation(debugdepthquad, "depthMap"), 0);


    glm::vec3 lightPos(-2.0f, 4.0f, -1.0f);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // изменение позиции света с течением времени
        //lightPos.x = sin(glfwGetTime()) * 3.0f;
        //lightPos.z = cos(glfwGetTime()) * 2.0f;
        //lightPos.y = 5.0 + cos(glfwGetTime()) * 1.0f;

        // рендер

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //вид - с позиции источника света
        glm::mat4 lightProjection, lightView;
        glm::mat4 lightSpaceMatrix;
        float near_plane = 1.0f, far_plane = 7.5f;
        //lightProjection = glm::perspective(glm::radians(45.0f), (GLfloat)SHADOW_WIDTH / (GLfloat)SHADOW_HEIGHT, near_plane, far_plane); // обратите внимание, что если вы используете матрицу перспективной проекции, вам придется изменить положение света, так как текущего положения света недостаточно для отображения всей сцены
        lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
        lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
        lightSpaceMatrix = lightProjection * lightView;
        // рендеринг сцены глазами источника света
        glUseProgram(simpledepthshader);
        glUniformMatrix4fv(glGetUniformLocation(simpledepthshader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        renderScene(simpledepthshader);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);


        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // сцена с картой глубины
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shader);
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform3f(glGetUniformLocation(shader, "Color"), 1.0f, 0.5f, 0.31f);
        
        
        glUniform3fv(glGetUniformLocation(shader, "viewPos"), 1, glm::value_ptr(camera.Position));
        glUniform3fv(glGetUniformLocation(shader, "lightPos"),1, glm::value_ptr(lightPos));
        glUniformMatrix4fv(glGetUniformLocation(shader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
        //glActiveTexture(GL_TEXTURE0);
        //glBindTexture(GL_TEXTURE_2D, shadowMap);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        renderScene(shader);

        //glUseProgram(debugdepthquad);
        //glUniform1fv(glGetUniformLocation(debugdepthquad, "near_plane"), 1, &near_plane);
        //glUniform1fv(glGetUniformLocation(debugdepthquad, "far_plane"), 1, &far_plane);
        //glActiveTexture(GL_TEXTURE0);
        //glBindTexture(GL_TEXTURE_2D, depthMap);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);

    glfwTerminate();
    return 0;
}

//сценa
void renderScene(int shader)
{
    // пол
    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glBindVertexArray(planeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    //кубики
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 1.5f, 0.0));
    model = glm::scale(model, glm::vec3(0.5f));
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    renderCube();
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(2.0f, 0.0f, 1.0));
    model = glm::scale(model, glm::vec3(0.5f));
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    renderCube();
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-1.0f, 0.0f, 2.0));
    model = glm::rotate(model, glm::radians(60.0f), glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
    model = glm::scale(model, glm::vec3(0.25));
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    renderCube();
}


//кубик в NDC.
unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube()
{
    // инициализация (если необходимо)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // задняя грань
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,/// 0.0f, 0.0f, // нижняя-левая
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  //1.0f, 1.0f, // верхняя-правая
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, //1.0f, 0.0f, // нижняя-правая        
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, //1.0f, 1.0f, // верхняя-правая
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, //0.0f, 0.0f, // нижняя-левая
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, //0.0f, 1.0f, // верхняя-левая
            // передняя грань
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, //0.0f, 0.0f, // нижняя-левая
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, //1.0f, 0.0f, // нижняя-правая
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, //1.0f, 1.0f, // верхняя-правая
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, //1.0f, 1.0f, // верхняя-правая
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, //0.0f, 1.0f, // верхняя-левая
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, //0.0f, 0.0f, // нижняя-левая
            // грань слева
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f,// 1.0f, 0.0f, // верхняя-правая
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, //1.0f, 1.0f, // верхняя-левая
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, //0.0f, 1.0f, // нижняя-левая
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, //0.0f, 1.0f, // нижняя-левая
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, //0.0f, 0.0f, // нижняя-правая
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, //1.0f, 0.0f, // верхняя-правая
            // грань справа
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, //1.0f, 0.0f, // верхняя-левая
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, //0.0f, 1.0f, // нижняя-правая
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, //1.0f, 1.0f, // верхняя-правая         
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, //0.0f, 1.0f, // нижняя-правая
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, //1.0f, 0.0f, // верхняя-левая
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, //0.0f, 0.0f, // нижняя-левая     
            // нижняя грань
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, //0.0f, 1.0f, // верхняя-правая
             1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, //1.0f, 1.0f, // верхняя-левая
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, //1.0f, 0.0f, // нижняя-левая
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, //1.0f, 0.0f, // нижняя-левая
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, //0.0f, 0.0f, // нижняя-правая
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, //0.0f, 1.0f, // верхняя-правая
            // верхняя грань
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, //0.0f, 1.0f, // верхняя-левая
             1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, //1.0f, 0.0f, // нижняя-правая
             1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, //1.0f, 1.0f, // верхняя-правая     
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, //1.0f, 0.0f, // нижняя-правая
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, //0.0f, 1.0f, // верхняя-левая
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, //0.0f, 0.0f  // нижняя-левая        
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // заполняем буфер
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // связываем вершинные атрибуты
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        //glEnableVertexAttribArray(2);
       // glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // рендерим ящик
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

//плоскость в NDC
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // координаты        // текстурные координаты
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // установка VAO пола
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

// Обработка всех событий ввода: запрос GLFW о нажатии/отпускании кнопки мыши в данном кадре и соответствующая обработка данных событий
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

// glfw: всякий раз, когда изменяются размеры окна (пользователем или опер. системой), вызывается данная функция
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // убеждаемся, что вьюпорт соответствует новым размерам окна; обратите внимание,
    // что ширина и высота будут значительно больше, чем указано на retina -дисплеях.
    glViewport(0, 0, width, height);
}

// glfw: всякий раз, когда перемещается мышь, вызывается данная callback-функция
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // перевернуто, так как Y-координаты идут снизу вверх

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: всякий раз, когда прокручивается колесико мыши, вызывается данная callback-функция
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(yoffset);
}