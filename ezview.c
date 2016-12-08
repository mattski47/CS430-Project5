#define GLFW_DLL 1

#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLFW/glfw3.h>

#include "deps/linmath.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#define CHANNEL_SIZE 255

typedef struct {
	float position[2];
	float TexCoord[2];
} Vertex;

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} Color;

FILE* sourcefp;
char format;
int h;
int w;
int mc;
char c;
Color* image;

void read_data_to_buffer();
void skip_ws(FILE*);
static void error_callback(int, const char*);
static void key_callback(GLFWwindow*, int, int, int, int);
void glCompileShaderOrDie(GLuint);

Vertex vertices[] = {
	{{1, -1}, {0.99999, 0}},
	{{1, 1},  {0.99999, 0.99999}},
	{{-1, 1}, {0, 0.99999}},
	{{-1, -1}, {0, 0}}
};

const GLubyte indices[] = {
  0, 1, 2,
  2, 3, 0
};

static const char* vertex_shader_text =
"uniform mat4 MVP;\n"
"attribute vec2 TexCoordIn;\n"
"attribute vec2 vPos;\n"
"varying lowp vec2 TexCoordOut;\n"
"void main()\n"
"{\n"
"    gl_Position = MVP * vec4(vPos, 0.0, 1.0);\n"
"    TexCoordOut = TexCoordIn;\n"
"}\n";

static const char* fragment_shader_text =
"varying lowp vec2 TexCoordOut;\n"
"uniform sampler2D Texture;\n"
"void main()\n"
"{\n"
"    gl_FragColor = texture2D(Texture, TexCoordOut);\n"
"}\n";

int main(int argc, char** argv)
{
	// check for correct number of inputs
    if (argc != 2) {
        fprintf(stderr, "Error: Arguments should be in format: 'format' 'source' 'dest'.");
        return(1);
    }
    
    // open source file
    sourcefp = fopen(argv[1], "r");
    
    // check that source exists
    if (!sourcefp) {
        fprintf(stderr, "Error: File not found.");
        return(1);
    }
    
    skip_ws(sourcefp);
	
    // checks that source is either P3 or P6
    if (fgetc(sourcefp) != 'P') {
        fprintf(stderr, "Error: Invalid image format. '%s' needs to be either 'P3' or 'P6'.", argv[1]);
        return(1);
    }
    // saves input format
    format = fgetc(sourcefp);
    if (format != '3' && format != '6') {
        fprintf(stderr, "Error: Invalid image format. '%s' needs to be either 'P3' or 'P6'.", argv[1]);
        return(1);
    }
    
    // initialize c and move through whitespace
    skip_ws(sourcefp);
    c = fgetc(sourcefp);
	
    // check for comments
    while (c == '#') {
        while (c != '\n')
            c = fgetc(sourcefp);
        
		while(isspace(c))
			c = fgetc(sourcefp);
    }
    
	ungetc(c, sourcefp);
	
    // get width and height
    fscanf(sourcefp, "%d", &w);
    fscanf(sourcefp, "%d", &h);
	printf("%d %d", w, h);
    // check width and height
    if (h < 1 || w < 1) {
        fprintf(stderr, "Error: Invalid dimensions.");
        return(1);
    }
    
    // get channel size
    fscanf(sourcefp, "%d", &mc);
    // check channel size
    if (mc != CHANNEL_SIZE) {
        fprintf(stderr, "Error: Channel size must be 8 bits.");
        return(1);
    }
    
    image = malloc(sizeof(Color)*h*w);
	
    // skip white space before data is read
    fgetc(sourcefp);
    read_data_to_buffer();
    // close source
    fclose(sourcefp);
	
    GLFWwindow* window;
    GLuint vertex_buffer, vertex_shader, fragment_shader, program;
	GLuint index_buffer;
    GLint mvp_location, vpos_location, vcol_location;

    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
        exit(EXIT_FAILURE);

	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    window = glfwCreateWindow(w, h, "ezview", NULL, NULL);
    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSetKeyCallback(window, key_callback);

    glfwMakeContextCurrent(window);
    // gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glfwSwapInterval(1);

    // NOTE: OpenGL error checks have been omitted for brevity

    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	
	glGenBuffers(1, &index_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
    glCompileShaderOrDie(vertex_shader);

    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
    glCompileShaderOrDie(fragment_shader);

    program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    // more error checking! glLinkProgramOrDie!

    mvp_location = glGetUniformLocation(program, "MVP");
    assert(mvp_location != -1);

    vpos_location = glGetAttribLocation(program, "vPos");
    assert(vpos_location != -1);

    GLint texcoord_location = glGetAttribLocation(program, "TexCoordIn");
    assert(texcoord_location != -1);

    GLint tex_location = glGetUniformLocation(program, "Texture");
    assert(tex_location != -1);

    glEnableVertexAttribArray(vpos_location);
    glEnableVertexAttribArray(texcoord_location);

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);

    while (!glfwWindowShouldClose(window)) {
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        float ratio;
        int width, height;
        mat4x4 m, p, mvp;

        glfwGetFramebufferSize(window, &width, &height);
        ratio = width / (float) height;

        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);

        mat4x4_identity(m);
        //mat4x4_rotate_Z(m, m, (float) glfwGetTime());
        mat4x4_ortho(p, -ratio, ratio, 1.f, -1.f, -1.f, 1.f);
        mat4x4_mul(mvp, p, m);

        glUseProgram(program);
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, (const GLfloat*) mvp);
        //glDrawArrays(GL_TRIANGLES, 0, 3);
		
		glVertexAttribPointer(vpos_location, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*) 0);
		glVertexAttribPointer(texcoord_location, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*) (sizeof(float) * 2));
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texID);
		glUniform1i(tex_location, 0);
		
		glDrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(GLubyte), GL_UNSIGNED_BYTE, 0);

        glfwSwapBuffers(window);
		glfwSetKeyCallback(window, key_callback);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);

    glfwTerminate();
    exit(EXIT_SUCCESS);
}

// reads data from input file into buffer
void read_data_to_buffer()
{
    Color color;
    
    // if input is P3
    if (format == '3') {
        for(int i=0; i<(h*w); i++) {
			int temp;
            // reads chars into Color color's rgb spots
			fscanf(sourcefp, "%d", &temp);
			color.r = (unsigned char) temp;
			
			fscanf(sourcefp, "%d", &temp);
			color.g = (unsigned char) temp;
			
			fscanf(sourcefp, "%d", &temp);
			color.b = (unsigned char) temp;
            // puts Color into image buffer
            image[i] = color;
        }
        
    // if input is P6
    } else {
        for(int i=0; i<(h*w); i++) {
            // reads chars into Color color's rgb spots
			color.r = (unsigned char) fgetc(sourcefp);
			color.g = (unsigned char) fgetc(sourcefp);
			color.b = (unsigned char) fgetc(sourcefp);
            // puts Color into image buffer
            image[i] = color;
        }
    }
}

// skips white space in file
void skip_ws(FILE* json)
{
    int c = fgetc(json);
    
    while(isspace(c))
        c = fgetc(json);
    
    ungetc(c, json);
}

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS) {
		switch(key)
		{
			case GLFW_KEY_ESCAPE:
				glfwSetWindowShouldClose(window, GLFW_TRUE);
				break;
			case GLFW_KEY_UP:
				vertices[0].position[1] -= 0.1;
				vertices[1].position[1] -= 0.1;
				vertices[2].position[1] -= 0.1;
				vertices[3].position[1] -= 0.1;
				break;
			case GLFW_KEY_RIGHT:
				vertices[0].position[0] += 0.1;
				vertices[1].position[0] += 0.1;
				vertices[2].position[0] += 0.1;
				vertices[3].position[0] += 0.1;
				break;
			case GLFW_KEY_DOWN:
				vertices[0].position[1] += 0.1;
				vertices[1].position[1] += 0.1;
				vertices[2].position[1] += 0.1;
				vertices[3].position[1] += 0.1;
				break;
			case GLFW_KEY_LEFT:
				vertices[0].position[0] -= 0.1;
				vertices[1].position[0] -= 0.1;
				vertices[2].position[0] -= 0.1;
				vertices[3].position[0] -= 0.1;
				break;
			case GLFW_KEY_W:
				vertices[0].position[0] *= 1.1;
				vertices[0].position[1] *= 1.1;
				vertices[1].position[0] *= 1.1;
				vertices[1].position[1] *= 1.1;
				vertices[2].position[0] *= 1.1;
				vertices[2].position[1] *= 1.1;
				vertices[3].position[0] *= 1.1;
				vertices[3].position[1] *= 1.1;
				break;
			case GLFW_KEY_S:
				vertices[0].position[0] *= 0.9;
				vertices[0].position[1] *= 0.9;
				vertices[1].position[0] *= 0.9;
				vertices[1].position[1] *= 0.9;
				vertices[2].position[0] *= 0.9;
				vertices[2].position[1] *= 0.9;
				vertices[3].position[0] *= 0.9;
				vertices[3].position[1] *= 0.9;
				break;
			default:
				printf("Invalid key.\n");
				break;
		}
	}
}

void glCompileShaderOrDie(GLuint shader)
{
	GLint compiled;
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	
	if (!compiled) {
		GLint infoLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
		
		char* info = malloc(infoLen+1);
		GLint done;
		glGetShaderInfoLog(shader, infoLen, &done, info);
		printf("Unable to compile shader: %s\n", info);
		exit(1);
	}
}

