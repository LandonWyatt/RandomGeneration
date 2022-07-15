#include <GL\glew.h>
#include <GLFW\glfw3.h>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <glm\glm.hpp>
#include <glm\gtc\type_ptr.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <SOIL2/SOIL2.h>

using namespace std;
#define numVAOs 1
#define numVBOs 1
#define PI 3.14159265359

float cameraX, cameraY, cameraZ;
float cubeLocX, cubeLocY, cubeLocZ;

GLuint renderingProgram;
GLuint vao[numVAOs];
GLuint vbo[numVBOs];
GLuint mvLoc, projLoc, nLoc, colorLoc;
GLuint globalAmbLoc, ambLoc, diffLoc, specLoc, posLoc, mAmbLoc, mDiffLoc, mSpecLoc, mShiLoc;

string readShaderSource(const char* filePath) {
	string content;
	ifstream fileStream(filePath, ios::in);
	string line = "";
	while (!fileStream.eof()) {
		getline(fileStream, line);
		content.append(line + "\n");
	}
	fileStream.close();
	return content;
}

GLuint createShaderProgram(const char* vshader, const char* fshader) {
	string vertShaderStr = readShaderSource(vshader);
	string fragShaderStr = readShaderSource(fshader);
	const char* vertShaderSrc = vertShaderStr.c_str();
	const char* fragShaderSrc = fragShaderStr.c_str();

	GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(vShader, 1, &vertShaderSrc, NULL);
	glShaderSource(fShader, 1, &fragShaderSrc, NULL);

	glCompileShader(vShader);
	glCompileShader(fShader);

	GLuint vfProgram = glCreateProgram();
	glAttachShader(vfProgram, vShader);
	glAttachShader(vfProgram, fShader);
	glLinkProgram(vfProgram);

	return vfProgram;
}

int width, height;
float aspect, rotAmt = 0;
float avgHeight = 0.0f;
float deltaTime = 0.0f, pitch, yaw = -90.0f, lastX = 500, lastY = 500, lastFrame = 0.0f;
int initial_time = time(NULL), final_time, frame_count, fps;
bool firstMouse = true, freeControl = false;
string fpsTitle;
glm::mat4 pMat, vMat, mMat, mvMat, invTrMat;
glm::vec3 currentLightPos, lightPosV;
glm::vec3 cameraPos = glm::vec3(0.0f, 18.0f, 32.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

GLuint skydome, grass, barnText, standingTallGrassImg, standingShortGrassImg, wheatImg, particleIMG, wheatBackImg;
GLuint loadTexture(const char* texImagePath);
void takeInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void setupVertices(void);
vector<vector<float>> findVertices(int val);

// Values for plane vertices
const int planeSize = 40;
// 18 below is the number of X, Y, & Z coords in each block for two triangles
const int numVertices = (planeSize * planeSize) * 18;
vector<vector<float>> planeVertices = findVertices(planeSize);
float vertices[numVertices][3];
// Boolean to show lines
bool showLines = true;

void init(GLFWwindow* window) {
	renderingProgram = createShaderProgram("vshaderSource.glsl", "fshaderSource.glsl");
	// Call function to send all vertices, normals, and textures to buffers
	setupVertices();
}

void display(GLFWwindow* window, double currentTime) {
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_DEPTH_BUFFER_BIT);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(renderingProgram);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetKeyCallback(window, key_callback);
	takeInput(window);

	// Find deltaTime for movement
	float currentFrame = glfwGetTime();
	deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;
	
	// Keep track of framerate for testing purposes
	frame_count++;
	final_time = time(NULL);
	if (final_time - initial_time > 0) {
		fps = frame_count / (final_time - initial_time);
		frame_count = 0;
		initial_time = final_time;
	}

	// get the uniform variables for the MV and projection matrices
	mvLoc = glGetUniformLocation(renderingProgram, "mv_matrix");
	projLoc = glGetUniformLocation(renderingProgram, "proj_matrix");
	// build perspective matrix
	glfwGetFramebufferSize(window, &width, &height);
	aspect = (float)width / (float)height;
	pMat = glm::perspective(1.0472f, aspect, 0.1f, 1000.0f); // 1.0472 radians = 60 degrees

	vMat = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
	mMat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
	mvMat = vMat * mMat;

	// copy perspective and MV matrices to corresponding uniform variables
	glUniformMatrix4fv(mvLoc, 1, GL_FALSE, glm::value_ptr(mvMat));
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(pMat));

	glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	colorLoc = glGetUniformLocation(renderingProgram, "color");

	// i stops at this value similiar to numVertices, although, the planeSize^2 is multiplied
	// by 6 instead as there is no need to calculate each X, Y, & Z value separately
	for (int i = 0; i <= ((planeSize * planeSize) * 6); i = i + 3) {
		avgHeight = 0.0f;
		
		// set average height for each triangle	
		avgHeight += (vertices[i][1] + vertices[i+1][1] + vertices[i+2][1])/3;

		// Set color based on height
		
		if (avgHeight >= 0) {
			glUniform3fv(colorLoc, 1, glm::value_ptr(glm::vec3(0.0f, 0.8f, 0.0f)));
		}
		else {
			glUniform3fv(colorLoc, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 0.8f)));
		}
		
 		glDrawArrays(GL_TRIANGLES, i, 3);
		
		// Draw Lines
		glUniform3fv(colorLoc, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 0.0f)));
		glLineWidth(1.5f);
		if (showLines) {
			glDrawArrays(GL_LINES, i, 2);
			glDrawArrays(GL_LINES, i + 1, 2);
		}
		
	}
}

int main(void) {
	if (!glfwInit()) { exit(EXIT_FAILURE); }
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

	// Anti-Aliasing
	glfwWindowHint(GLFW_SAMPLES, 16);
	glEnable(GL_MULTISAMPLE);
	
	GLFWwindow* window = glfwCreateWindow(1280, 720, "RandomGeneration", NULL, NULL);
	glfwMakeContextCurrent(window);
	if (glewInit() != GLEW_OK) { exit(EXIT_FAILURE); }
	glfwSwapInterval(1);
	init(window);
	while (!glfwWindowShouldClose(window)) {
		// Add functionality to show FPS on window
		fpsTitle = "FPS: " + to_string(fps);
		char const* pchar = fpsTitle.c_str();
	    glfwSetWindowTitle(window, pchar);
		display(window, glfwGetTime());
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}

void setupVertices(void) {
	int currX = 0;
	int currLocation = 0;

	int vertexRotations[5][2] = { {0, 0}, {0, 1}, {1, 1}, {0, 0}, {1, 0} };

	// Go through each Z-value
	for (int currZ = 0; currZ < planeSize; currZ++) {
		// Go through each X-value on current Z-value
		while (currX < planeSize) {
			for (int i = 0; i < (sizeof vertexRotations / sizeof vertexRotations[0]); i++) {
				// X-value
				vertices[currLocation][0] = (currX + vertexRotations[i][0]) - planeSize / 2;
				// Y-value
				vertices[currLocation][1] = planeVertices[(currX + vertexRotations[i][0])][(currZ + vertexRotations[i][1])];
				// Z-value
				vertices[currLocation][2] = ((float)(currZ + vertexRotations[i][1])) - planeSize / 2;
				// increment to next vertex
				currLocation++;

				// if it is the corner, repeat vertice to create second triangle
				if (i == 2) {
					// X-value
					vertices[currLocation][0] = (currX + vertexRotations[i][0]) - planeSize / 2;
					// Y-value
					vertices[currLocation][1] = planeVertices[(currX + vertexRotations[i][0])][(currZ + vertexRotations[i][1])];
					// Z-value
					vertices[currLocation][2] = ((float)(currZ + vertexRotations[i][1])) - planeSize / 2;
					// increment to next vertex
					currLocation++;
					
				}
			}

			currX++;
		}
		currX = 0;
	}

	glGenVertexArrays(1, vao);
	glBindVertexArray(vao[0]);
	glGenBuffers(numVBOs, vbo);

	glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	
}

vector<vector<float>> findVertices(int val) {
	// Increase by 1 to count number of vertices rather than squares
	val++;
	vector<vector<float>> planeVertices(val, vector<float> (val, 0.0f));
	
	float avgHeight = 0;
	int maxVar = 100, minVar = 50;
	srand(time(NULL));
	for (int x = 0; x < val; x++) {
		for (int z = 0; z < val; z++) {
			if (x == 0 && z == 0) {
				planeVertices[0][0] = (rand() % maxVar - minVar) / 100.0f;
			}
			else if (x == 0) {
				planeVertices[0][z] = planeVertices[0][z - 1] + (rand() % maxVar - minVar) / 100.0f;
			}
			else if (z == 0) {
				planeVertices[x][0] = planeVertices[x - 1][0] + (rand() % maxVar - minVar) / 100.0f;
			}
			else {
				avgHeight = (planeVertices[x][z - 1] + planeVertices[x - 1][z - 1] + planeVertices[x - 1][z]) / 3;
				planeVertices[x][z] = avgHeight + (rand() % maxVar - minVar) / 100.0f;
			}
		}
	}

	return planeVertices;
}

void takeInput(GLFWwindow* window) {
	float cameraSpeed = 10.0f * deltaTime;
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { // Camera Forward
		cameraPos += cameraSpeed * cameraFront;
	}
	else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { // Camera Left
		cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
	}
	else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { // Camera Back
		cameraPos -= cameraSpeed * cameraFront;
	}
	else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { // Camera Right
		cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
	}
	else if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		glfwDestroyWindow(window);
		glfwTerminate();
		exit(EXIT_SUCCESS);
	}
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_R && action == GLFW_PRESS) { // Update landscape
		planeVertices = findVertices(planeSize);
		setupVertices();
	}
	else if (key == GLFW_KEY_F && action == GLFW_PRESS) { // toggle outlines
		if (showLines) showLines = false;
		else showLines = true;
	}
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
	if (firstMouse) {
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos;
	lastX = xpos;
	lastY = ypos;

	const float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;

	if (pitch > 89.0f) {
		pitch = 89.0f;
	}
	if (pitch < -89.0f) {
		pitch = -89.0f;
	}

	glm::vec3 direction;
	direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	direction.y = sin(glm::radians(pitch));
	direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraFront = glm::normalize(direction);
}

GLuint loadTexture(const char* texImagePath) {
	GLuint textureID;
	textureID = SOIL_load_OGL_texture(texImagePath,
		SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);
	if (textureID == 0) cout << "could not find texture file" << texImagePath << endl;
	return textureID;
}