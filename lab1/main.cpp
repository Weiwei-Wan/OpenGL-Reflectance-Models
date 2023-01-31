// Windows includes (For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <math.h>
#include <vector> // STL dynamic memory.
//#include <glad/glad.h>
//#include <GLFW/glfw3.h>

// OpenGL includes
#include <GL/glew.h>
#include <GL/freeglut.h>

// Assimp includes
#include <assimp/cimport.h> // scene importer
#include <assimp/scene.h> // collects data
#include <assimp/postprocess.h> // various extra operations

//glm
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Project includes
#include "maths_funcs.h"
#define GLT_IMPLEMENTATION
#include "gltext.h"

/*----------------------------------------------------------------------------
MESH TO LOAD
----------------------------------------------------------------------------*/
// this mesh is a dae file format but you should be able to use any other format too, obj is typically what is used
// put the mesh in your project directory, or provide a filepath for it here
#define MESH_NAME "teapot.dae"
/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

struct ModelData
{
	size_t mPointCount = 0;
	std::vector<vec3> mVertices;
	std::vector<vec3> mNormals;
	std::vector<vec2> mTextureCoords;
};

using namespace std;
GLuint PhongID;
GLuint ToonID;
GLuint CookID;

ModelData mesh_data;
unsigned int mesh_vao = 0;
int width = 1600;
int height = 1200;

glm::mat4 persp_proj = glm::perspective(45.0f, 4.0f / 3.0f, 0.1f, 100.0f);
glm::mat4 model = glm::mat4(1.0f);
glm::mat4 model2 = glm::mat4(1.0f);
glm::mat4 model3 = glm::mat4(1.0f);
// Camera pos
GLfloat camera_pos_x = 8.0f;
GLfloat camera_pos_z = 0.0f;
GLfloat camera_pos_y = 0.0f;
GLfloat shininess = 20.0f;
GLfloat ks = 0.5f;
GLfloat metallic = 0.7f;
GLfloat roughness = 0.1f;

GLuint loc1, loc2, loc3;
static float rotate_x = 0.0f;
static float Delta = 0.5f;

#pragma region MESH LOADING
/*----------------------------------------------------------------------------
MESH LOADING FUNCTION
----------------------------------------------------------------------------*/

ModelData load_mesh(const char* file_name) {
	ModelData modelData;

	const aiScene* scene = aiImportFile(
		file_name,
		aiProcess_Triangulate | aiProcess_PreTransformVertices
	);

	if (!scene) {
		fprintf(stderr, "ERROR: reading mesh %s\n", file_name);
		return modelData;
	}

	printf("  %i materials\n", scene->mNumMaterials);
	printf("  %i meshes\n", scene->mNumMeshes);
	printf("  %i textures\n", scene->mNumTextures);

	for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
		printf("    %i vertices in mesh\n", mesh->mNumVertices);
		modelData.mPointCount += mesh->mNumVertices;
		for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
			if (mesh->HasPositions()) {
				const aiVector3D* vp = &(mesh->mVertices[v_i]);
				modelData.mVertices.push_back(vec3(vp->x, vp->y, vp->z));
			}
			if (mesh->HasNormals()) {
				const aiVector3D* vn = &(mesh->mNormals[v_i]);
				modelData.mNormals.push_back(vec3(vn->x, vn->y, vn->z));
			}
			if (mesh->HasTextureCoords(0)) {
				const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
				modelData.mTextureCoords.push_back(vec2(vt->x, vt->y));
			}
			if (mesh->HasTangentsAndBitangents()) {
			}
		}
	}

	aiReleaseImport(scene);
	return modelData;
}

#pragma endregion MESH LOADING

// Shader Functions- click on + to expand
#pragma region SHADER_FUNCTIONS
char* readShaderSource(const char* shaderFile) {
	FILE* fp;
	fopen_s(&fp, shaderFile, "rb");

	if (fp == NULL) { return NULL; }

	fseek(fp, 0L, SEEK_END);
	long size = ftell(fp);

	fseek(fp, 0L, SEEK_SET);
	char* buf = new char[size + 1];
	fread(buf, 1, size, fp);
	buf[size] = '\0';

	fclose(fp);

	return buf;
}


static void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	// create a shader object
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0) {
		std::cerr << "Error creating shader..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	const char* pShaderSource = readShaderSource(pShaderText);

	// Bind the source code to the shader, this happens before compilation
	glShaderSource(ShaderObj, 1, (const GLchar**)&pShaderSource, NULL);
	// compile the shader and check for errors
	glCompileShader(ShaderObj);
	GLint success;
	// check for shader related errors using glGetShaderiv
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar InfoLog[1024] = { '\0' };
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		std::cerr << "Error compiling "
			<< (ShaderType == GL_VERTEX_SHADER ? "vertex" : "fragment")
			<< " shader program: " << InfoLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Attach the compiled shader object to the program object
	glAttachShader(ShaderProgram, ShaderObj);
}

GLuint CompileShaders(const char* vshadername)
{
	//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID
	GLuint shaderProgramID = glCreateProgram();
	if (shaderProgramID == 0) {
		std::cerr << "Error creating shader program..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// Create two shader objects, one for the vertex, and one for the fragment shader
	AddShader(shaderProgramID, vshadername, GL_VERTEX_SHADER);
	AddShader(shaderProgramID, "simpleFragmentShader.txt", GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = { '\0' };
	// After compiling all shader objects and attaching them to the program, we can finally link it
	glLinkProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &Success);
	if (Success == 0) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Error linking shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
	glValidateProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Invalid shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Finally, use the linked shader program
	// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
	glUseProgram(shaderProgramID);
	return shaderProgramID;
}
#pragma endregion SHADER_FUNCTIONS

// VBO Functions - click on + to expand
#pragma region VBO_FUNCTIONS
void generateObjectBufferMesh(GLuint& ID) {

	mesh_data = load_mesh(MESH_NAME);
	unsigned int vp_vbo = 0;
	loc1 = glGetAttribLocation(ID, "vertex_position");
	loc2 = glGetAttribLocation(ID, "vertex_normal");
	loc3 = glGetAttribLocation(ID, "vertex_texture");

	glGenBuffers(1, &vp_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mVertices[0], GL_STATIC_DRAW);
	unsigned int vn_vbo = 0;
	glGenBuffers(1, &vn_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mNormals[0], GL_STATIC_DRAW);

	unsigned int vao = 0;
	glBindVertexArray(vao);

	glEnableVertexAttribArray(loc1);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
	glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(loc2);
	glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
	glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);
}
#pragma endregion VBO_FUNCTIONS

void drawText(const char* str, GLfloat size, glm::vec3 pos) {
	// Initialize glText
	gltInit();
	// Creating text
	GLTtext* text = gltCreateText();
	gltSetText(text, str);
	// Begin text drawing (this for instance calls glUseProgram)
	gltBeginDraw();
	// Draw any amount of text between begin and end
	gltColor(1.0f, 0.0f, 0.0f, 1.0f);
	gltDrawText2DAligned(text, 70 * (pos.x + 1), 450 - pos.y * 70, size, GLT_CENTER, GLT_CENTER);
	// Finish drawing text
	gltEndDraw();
	// Deleting text
	gltDeleteText(text);
	// Destroy glText
	gltTerminate();
}

void display() {

	// tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHTING);
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	rotate_x += Delta;

	glUseProgram(PhongID);
	//Declare your uniform variables that will be used in your shader
	glm::mat4 view = glm::lookAt(glm::vec3(camera_pos_x, camera_pos_y, camera_pos_z), // Camera is at (x,y,z), in World Space
	                             glm::vec3(0, 0, 0), // and looks at the origin
		                         glm::vec3(0, 1, 0));  // Head is up (set to 0,-1,0 to look upside-down)
	model = glm::mat4(1.0f);
	model = glm::translate(model, glm:: vec3(0.0f, 0.0f, 3.5f));
	model = glm::scale(model, glm::vec3(0.1f, 0.1f, 0.1f));
	model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, glm::radians(rotate_x), glm::vec3(0.0f, 0.0f, 1.0f));

    //Declare your uniform variables that will be used in your shader
	int model_color = glGetUniformLocation(PhongID, "color");
	int model_cameraPos = glGetUniformLocation(PhongID, "cameraPos");
	int matrix_location = glGetUniformLocation(PhongID, "model");
	int view_mat_location = glGetUniformLocation(PhongID, "view");
	int proj_mat_location = glGetUniformLocation(PhongID, "proj");
	int shininess_location = glGetUniformLocation(PhongID, "shininess");
	int ks_location = glGetUniformLocation(PhongID, "ks");
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, &persp_proj[0][0]);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, &model[0][0]);
	glUniform3f(model_color, 0.5f, 0.5f, 0.1f);
	glUniform1f(shininess_location, shininess);
	glUniform1f(ks_location, ks);
	glUniform3f(model_cameraPos, camera_pos_x, camera_pos_y, camera_pos_z);
	glDrawArrays(GL_TRIANGLES, 0, mesh_data.mPointCount);
	drawText("Phong", 4, glm::vec3(3.8f, 2.0f, 0.0f));
	drawText("shininess:", 2, glm::vec3(3.0f, -5.0f, 0.0f));
	char array[10];
	snprintf(array, sizeof(array), "%1.1f", shininess);
	drawText(array, 2, glm::vec3(4.8f, -5.0f, 0.0f));

	drawText("Ks:", 2, glm::vec3(3.0f, -6.0f, 0.0f));
	snprintf(array, sizeof(array), "%1.1f", ks);
	drawText(array, 2, glm::vec3(3.8f, -6.0f, 0.0f));

	glUseProgram(ToonID);
	model2 = glm::mat4(1.0f);
	model2 = glm::translate(model2, glm::vec3(0.0f, 0.0f, 0.0f));
	model2 = glm::scale(model2, glm::vec3(0.1f, 0.1f, 0.1f));
	model2 = glm::rotate(model2, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	model2 = glm::rotate(model2, glm::radians(rotate_x), glm::vec3(0.0f, 0.0f, 1.0f));
    //Declare your uniform variables that will be used in your shader
	int model_color2= glGetUniformLocation(ToonID, "color");
	int model_cameraPos2 = glGetUniformLocation(ToonID, "cameraPos");
	int matrix_location2 = glGetUniformLocation(ToonID, "model");
	int view_mat_location2 = glGetUniformLocation(ToonID, "view");
	int proj_mat_location2 = glGetUniformLocation(ToonID, "proj");
	glUniformMatrix4fv(proj_mat_location2, 1, GL_FALSE, &persp_proj[0][0]);
	glUniformMatrix4fv(view_mat_location2, 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(matrix_location2, 1, GL_FALSE, &model2[0][0]);
	glUniform3f(model_color2, 0.5f, 0.5f, 0.1f);
	glUniform3f(model_cameraPos2, camera_pos_x, camera_pos_y, camera_pos_z);
	glDrawArrays(GL_TRIANGLES, 0, mesh_data.mPointCount);
	drawText("Toon", 4, glm::vec3(10.5f, 2.0f, 0.0f));

	glUseProgram(CookID);
	model3 = glm::mat4(1.0f);
	model3 = glm::translate(model3, glm::vec3(0.0f, 0.0f, -3.5f));
	model3 = glm::scale(model3, glm::vec3(0.1f, 0.1f, 0.1f));
	model3 = glm::rotate(model3, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	model3 = glm::rotate(model3, glm::radians(rotate_x), glm::vec3(0.0f, 0.0f, 1.0f));
	//Declare your uniform variables that will be used in your shader
	int model_color3 = glGetUniformLocation(CookID, "color");
	int model_cameraPos3 = glGetUniformLocation(CookID, "cameraPos");
	int matrix_location3 = glGetUniformLocation(CookID, "model");
	int view_mat_location3 = glGetUniformLocation(CookID, "view");
	int proj_mat_location3 = glGetUniformLocation(CookID, "proj");
	int metallic_location = glGetUniformLocation(CookID, "metallic");
	int roughness_location = glGetUniformLocation(CookID, "roughness");
	glUniformMatrix4fv(proj_mat_location3, 1, GL_FALSE, &persp_proj[0][0]);
	glUniformMatrix4fv(view_mat_location3, 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(matrix_location3, 1, GL_FALSE, &model3[0][0]);
	glUniform3f(model_color3, 0.5f, 0.5f, 0.1f);
	glUniform3f(model_cameraPos3, camera_pos_x, camera_pos_y, camera_pos_z);
	glUniform1f(metallic_location, metallic);
	glUniform1f(roughness_location, roughness);
	glDrawArrays(GL_TRIANGLES, 0, mesh_data.mPointCount);
	drawText("Cook-Torrance", 4, glm::vec3(18.0f, 2.0f, 0.0f));

	drawText("metallic:", 2, glm::vec3(17.0f, -5.0f, 0.0f));
	snprintf(array, sizeof(array), "%1.1f", metallic);
	drawText(array, 2, glm::vec3(18.6f, -5.0f, 0.0f));

	drawText("roughness:", 2, glm::vec3(17.0f, -6.0f, 0.0f));
	snprintf(array, sizeof(array), "%1.1f", roughness);
	drawText(array, 2, glm::vec3(18.8f, -6.0f, 0.0f));
	
	glutPostRedisplay();
	glutSwapBuffers();
}

void init()
{
	// Set up the shaders
	PhongID = CompileShaders("PhongVertexShader.txt");
	// load mesh into a vertex buffer array
	generateObjectBufferMesh(PhongID);
	ToonID = CompileShaders("ToonVertexShader.txt");
	// load mesh into a vertex buffer array
	generateObjectBufferMesh(ToonID);
	CookID = CompileShaders("CookVertexShader.txt");
	// load mesh into a vertex buffer array
	generateObjectBufferMesh(CookID);

}

// Placeholder code for the keypress
void keypress(unsigned char key, int x, int y) {
	if (key == 'y') {
		camera_pos_y += 1.0f;
	}
	else if (key == 'q') {
		if (shininess > 5.0f) {
			shininess -= 5.0f;
		}
	}
	else if (key == 'w') {
		shininess += 5.0f;
	}
	else if (key == 'a') {
		if (ks > 0.5f) {
			ks -= 0.5f;
		}
	}
	else if (key == 's') {
		ks += 0.5f;
	}
	else if (key == 'r') {
		if (metallic > 0.1f) {
			metallic -= 0.1f;
		}
	}
	else if (key == 't') {
		metallic += 0.1f;
	}
	else if (key == 'f') {
		if (roughness > 0.1f) {
			roughness -= 0.1f;
		}
	}
	else if (key == 'g') {
		roughness += 0.1f;
	}
}

int main(int argc, char** argv) {
	// Set up the window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(width, height);
	glutCreateWindow("Sea Wolf");

	//texure
	glEnable(GL_DEPTH_TEST);

	// Tell glut where the display function is
	glutDisplayFunc(display);
	glutKeyboardFunc(keypress);

	// A call to glewInit() must be done after glut is initialized!
	GLenum res = glewInit();
	// Check for any errors
	if (res != GLEW_OK) {
		fprintf(stderr, "Error: '%s'\n", glewGetErrorString(res));
		return 1;
	}
	// Set up your objects and shaders
	init();
	// Begin infinite event loop
	glutMainLoop();
	return 0;
}
