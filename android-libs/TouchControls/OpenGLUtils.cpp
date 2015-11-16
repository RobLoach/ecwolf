#include "OpenGLUtils.h"
#include <map>

namespace touchcontrols
{
float GLScaleWidth = 400;
float GLScaleHeight = -300;


#ifdef USE_GLES2
extern "C"
{

float glTranslateX,glTranslateY,glTranslateZ;

float glColorR, glColorG, glColorB, glColorA;
void glColor4f(GLfloat r,GLfloat g,GLfloat b,GLfloat a)
{
	glColorR = r;
	glColorG = g;
	glColorB = b;
	glColorA = a;
}

void glLoadIdentity()
{
	glTranslateX = glTranslateY = glTranslateZ = 0;
}

void glScalef(GLfloat x,GLfloat y,GLfloat z)
{

}

void glTranslatef(GLfloat x,GLfloat y,GLfloat z)
{
	glTranslateX+=x;
	glTranslateY-=y;
	glTranslateZ+=z;
}
}

float translateX(float X)
{
	return -(1-X*2);
}
float translateY(float Y)
{
	return (-Y*2+1);
}
float GLES2scaleX(float X)
{
	return X*2;
}
float GLES2scaleY(float Y)
{
	return Y*2;
}
const char vShaderStr [] =
		"attribute vec4 a_position;  \
			attribute vec2 a_texCoord;   \
			varying vec2 v_texCoord;     \
			uniform vec4 u_translate;    \
			void main()                 \
			{                           \
			   gl_Position = a_position + u_translate; \
			   v_texCoord = a_texCoord;  \
			}                            \
			";

const char  fShaderStr [] =
		"precision mediump float;                            \
			varying vec2 v_texCoord;                            \
			uniform sampler2D s_texture;                        \
		    uniform vec4 u_color;    \
			void main()                                         \
			{                                                   \
				vec4 alpha = texture2D( s_texture, v_texCoord ); \
				gl_FragColor = alpha*u_color;  \
			}                                                   \
			";

const char  fShaderColorStr [] =
		"precision mediump float;                            \
		    uniform vec4 u_color;    \
			void main()                                         \
			{                                                   \
				gl_FragColor = u_color;  \
			}                                                   \
			";

int loadShader(int shaderType,const char * source) {
	int shader = glCreateShader(shaderType);
	if (shader != 0) {
		glShaderSource(shader,1, &source,NULL);
		glCompileShader(shader);


		GLint  length;

		glGetShaderiv ( shader , GL_INFO_LOG_LENGTH , &length );

		if ( length ) {
			char* buffer  =  new char [ length ];
			glGetShaderInfoLog ( shader , length , NULL , buffer );
			LOGTOUCH("shader = %s", buffer);
			delete [] buffer;

			GLint success;
			glGetShaderiv( shader, GL_COMPILE_STATUS, &success );
			if ( success != GL_TRUE )  LOGTOUCH("ERROR compiling shader");
		}


	}
	return shader;
}

int createProgram(const char * vertexSource, const char *  fragmentSource) {
	int vertexShader = loadShader(GL_VERTEX_SHADER, vertexSource);
	int pixelShader = loadShader(GL_FRAGMENT_SHADER, fragmentSource);

	int program = glCreateProgram();
	if (program != 0) {
		glAttachShader(program, vertexShader);
		// checkGlError("glAttachShader");
		glAttachShader(program, pixelShader);
		// checkGlError("glAttachShader");
		glLinkProgram(program);
		/*
		int[] linkStatus = new int[1];
		glGetProgramiv(program, GLES20.GL_LINK_STATUS, linkStatus, 0);
		if (linkStatus[0] != GLES20.GL_TRUE) {
			LOGE("Could not link program: ");
			LOGE(glGetProgramInfoLog(program));
			glDeleteProgram(program);
			program = 0;
		}
		 */

	}
	return program;
}

GLfloat mTexVertices[] =
{
		0.0f, 0.0f, // TexCoord 0
		0.0f, -1.0f, // TexCoord 1
		1.0f, -1.0f, // TexCoord 2
		1.0f, 0.0f // TexCoord 3
};
/*
GLfloat mTexVertices[] =
{
		0.0f, 0.0f, // TexCoord 0
		0.0f, 1.0f, // TexCoord 1
		1.0f, 1.0f, // TexCoord 2
		1.0f, 0.0f // TexCoord 3
};
 */
// Handle to a program object
static int mProgramObject;
static int mProgramObjectColor;

// Attribute locations
static int mPositionLoc;
static int mTexCoordLoc;

// Sampler location
static int mSamplerLoc;
static int mColorLoc;

static int mPositionTranslateLoc;


static int mPositionLocColor;
static int mColorLocColor;
static int mPositionTranslateLocColor;


static int gles2InitDone = 0;

void initGLES2()
{

	// Load the shaders and get a linked program object
	mProgramObject = createProgram(vShaderStr, fShaderStr);
	mProgramObjectColor = createProgram(vShaderStr, fShaderColorStr);

	// Get the attribute locations
	mPositionLoc = glGetAttribLocation(mProgramObject, "a_position");
	mTexCoordLoc = glGetAttribLocation(mProgramObject, "a_texCoord" );

	// Get the sampler location
	mSamplerLoc = glGetUniformLocation ( mProgramObject, "s_texture" );
	mPositionTranslateLoc =glGetUniformLocation ( mProgramObject, "u_translate" );
	mColorLoc  = glGetUniformLocation ( mProgramObject, "u_color" );


	//COLOR
	mPositionLocColor            = glGetAttribLocation(mProgramObjectColor, "a_position");
	mColorLocColor               = glGetUniformLocation ( mProgramObjectColor, "u_color" );
	mPositionTranslateLocColor   =glGetUniformLocation ( mProgramObjectColor, "u_translate" );
}
#endif


#ifdef USE_GLES2
void drawRect(GLuint texture, float x, float y, GLRect &rect)
{
	//LOGTOUCH("drawRect");
	glUseProgram(mProgramObject);

	glVertexAttribPointer ( mPositionLoc, 3, GL_FLOAT,
			false,
			3 * 4, rect.vertices);

	glVertexAttribPointer ( mTexCoordLoc, 2, GL_FLOAT,
			false,
			2 * 4,
			mTexVertices );

	glEnableVertexAttribArray ( mPositionLoc );
	glEnableVertexAttribArray ( mTexCoordLoc );

	// Bind the texture
	glActiveTexture ( GL_TEXTURE0 );
	glBindTexture ( GL_TEXTURE_2D, texture );

	// Set the sampler texture unit to 0
	glUniform1i ( mSamplerLoc, 0 );

	glUniform4f(mPositionTranslateLoc,  translateX(x + glTranslateX), translateY(y + glTranslateY), 0, 0);
	glUniform4f(mColorLoc,glColorR,glColorG,glColorB,glColorA);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

#else
void drawRect(GLuint texture, float x, float y, GLRect &rect)
{

    if (texture == -1)
        return;

	//LOGTOUCH("drawRect %d",texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glVertexPointer(3, GL_FLOAT, 0, rect.vertices);
	glTexCoordPointer(2, GL_FLOAT, 0, rect.texture);

	glTranslatef(x, -y, 0);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	//Move back to original pos in case other rects to draw
	glTranslatef(-x, y, 0);

}
#endif

#ifdef USE_GLES2
void drawRect(GLfloat r,GLfloat g,GLfloat b,GLfloat a, float x, float y, GLRect &rect)
{
	glUseProgram(mProgramObjectColor);

	glColor4f(r, g, b, a );


	glVertexAttribPointer ( mPositionLocColor, 3, GL_FLOAT,
			false,
			3 * 4, rect.vertices);


	glEnableVertexAttribArray ( mPositionLocColor );

	// Bind the texture
	//glDeactiveTexture ( GL_TEXTURE0 );


	glUniform4f(mPositionTranslateLocColor,  translateX(x + glTranslateX), translateY(y + glTranslateY), 0, 0);
	glUniform4f(mColorLocColor,glColorR,glColorG,glColorB,glColorA);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glColor4f(1, 1, 1, 1 );
}

#else
void drawRect(GLfloat r,GLfloat g,GLfloat b,GLfloat a, float x, float y, GLRect &rect)
{
	glDisable(GL_TEXTURE_2D);
	glColor4f(r, g, b, a );

	glVertexPointer(3, GL_FLOAT, 0, rect.vertices);
	glTranslatef(x, -y, 0);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glColor4f(1, 1, 1, 1 );
	glEnable(GL_TEXTURE_2D);
}
#endif


#ifdef USE_GLES2
void drawLines(float x, float y, GLLines &lines)
{
	glUseProgram(mProgramObjectColor);

	glVertexAttribPointer ( mPositionLocColor, 3, GL_FLOAT,
			false,
			3 * 4, lines.vertices);



	glEnableVertexAttribArray ( mPositionLocColor );

	// Bind the texture
	glActiveTexture ( GL_TEXTURE0 );
	glBindTexture ( GL_TEXTURE_2D, 0 );

	glUniform4f(mPositionTranslateLocColor,  translateX(x + glTranslateX), translateY(y + glTranslateY), 0, 0);
	glUniform4f(mColorLocColor,glColorR,glColorG,glColorB,glColorA);

	glDrawArrays(GL_LINES, 0,  lines.len);
}

#else
void drawLines(float x, float y, GLLines &lines)
{

	glDisable(GL_TEXTURE_2D);

	glVertexPointer(3, GL_FLOAT, 0, lines.vertices);

	glTranslatef(x, -y, 0);

	glDrawArrays(GL_LINES, 0, lines.len);

	glEnable(GL_TEXTURE_2D);
}
#endif



FILE* file;

void png_read(png_structp png_ptr, png_bytep data, png_size_t length) {
	fread(data,1,length,file);
}
//This need to be set
std::string graphicsBasePath;
void setGraphicsBasePath(std::string path)
{
	graphicsBasePath = path;
}

//in case same texture is wanted
std::map <std::string, GLuint> tc_gl_textures;

static int texNumber = 20000;
GLuint loadTextureFromPNG(std::string filename, int &width, int &height)
{
	LOGTOUCH("loadTextureFromPNG %s", filename.c_str());

    if (filename == "")
    {
        LOGTOUCH("Blank texture name");
        return -1;
    }

#ifdef USE_GLES2
	if (!gles2InitDone)
		initGLES2();

	gles2InitDone = 1;
#endif

	//Check if already loaded
	std::map<std::string, GLuint>::iterator it = tc_gl_textures.find(filename);
	if(it != tc_gl_textures.end())
	{
	   //element found;
	   LOGTOUCH("PNG %s is already loaded", filename.c_str());
	   return it->second;
	}

	std::string full_file = graphicsBasePath + filename + ".png";
	file = fopen(full_file.c_str(), "r");
	if (!file) {
		LOGTOUCH_E("Error opening %s from APK", full_file.c_str());
		return TEXTURE_LOAD_ERROR;
	}

	//header for testing if it is a png
	png_byte header[8];

	//read the header
	fread( header, 1,8,file);

	//test if png
	int is_png = !png_sig_cmp(header, 0, 8);
	if (!is_png) {
		fclose(file);
		LOGTOUCH_E("Not a png file : %s", full_file.c_str());
		return TEXTURE_LOAD_ERROR;
	}

	//create png struct
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
			NULL, NULL);
	if (!png_ptr) {
		fclose(file);
		LOGTOUCH_E("Unable to create png struct : %s", full_file.c_str());
		return (TEXTURE_LOAD_ERROR);
	}

	//create png info struct
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, (png_infopp) NULL, (png_infopp) NULL);
		LOGTOUCH_E("Unable to create png info : %s", full_file.c_str());
		fclose(file);
		return (TEXTURE_LOAD_ERROR);
	}

	//create png info struct
	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
		LOGTOUCH_E("Unable to create png end info : %s", full_file.c_str());
		fclose(file);
		return (TEXTURE_LOAD_ERROR);
	}

	//png error stuff, not sure libpng man suggests this.
	if (setjmp(png_jmpbuf(png_ptr))) {
		fclose(file);
		LOGTOUCH_E("Error during setjmp : %s", full_file.c_str());
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return (TEXTURE_LOAD_ERROR);
	}

	//init png reading
	//png_init_io(png_ptr, fp);
	png_set_read_fn(png_ptr, NULL, png_read);

	//let libpng know you already read the first 8 bytes
	png_set_sig_bytes(png_ptr, 8);

	// read all the info up to the image data
	png_read_info(png_ptr, info_ptr);

	//variables to pass to get info
	int bit_depth, color_type;
	png_uint_32 twidth, theight;

	// get info about png
	png_get_IHDR(png_ptr, info_ptr, &twidth, &theight, &bit_depth, &color_type,
			NULL, NULL, NULL);

	//update width and height based on png info
	width = twidth;
	height = theight;

	// Update the png info struct.
	png_read_update_info(png_ptr, info_ptr);

	// Row size in bytes.
	int rowbytes = png_get_rowbytes(png_ptr, info_ptr);

	// Allocate the image_data as a big block, to be given to opengl
	png_byte *image_data = new png_byte[rowbytes * height];
	if (!image_data) {
		//clean up memory and close stuff
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		LOGTOUCH_E("Unable to allocate image_data while loading %s ", full_file.c_str());
		fclose(file);
		return TEXTURE_LOAD_ERROR;
	}

	//row_pointers is for pointing to image_data for reading the png with libpng
	png_bytep *row_pointers = new png_bytep[height];
	if (!row_pointers) {
		//clean up memory and close stuff
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		delete[] image_data;
		LOGTOUCH_E("Unable to allocate row_pointer while loading %s ", full_file.c_str());
		fclose(file);
		return TEXTURE_LOAD_ERROR;
	}
	// set the individual row_pointers to point at the correct offsets of image_data
	for (int i = 0; i < height; ++i)
		row_pointers[height - 1 - i] = image_data + i * rowbytes;

	//read the png into image_data through row_pointers
	png_read_image(png_ptr, row_pointers);

	//Now generate the OpenGL texture object
	GLuint texture = texNumber++;

	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, (GLvoid*) image_data);

	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


	//clean up memory and close stuff
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	delete[] image_data;
	delete[] row_pointers;
	fclose(file);

	//Add to cache
	tc_gl_textures[filename] = texture;

	return texture;

}



}
