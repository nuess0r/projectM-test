/** @file org_brain4free_jprojectm_ProjectM.c
 *
 * @brief This simple client demonstrates the C API of OpenGL
 * 
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright 2022 Christoph Zimmermann.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * See 'LICENSE' included within this release
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/* TODO: Make JACK optional with define */
#include <jack/jack.h>

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <libprojectM/projectM.h>

#include "org_brain4free_jprojectm_ProjectM.h"

/*-----------------------------------------------------------------------------
 * Global variables
 * ---------------------------------------------------------------------------*/
jack_port_t *input_port1;
jack_port_t *input_port2;
jack_port_t *output_port1;
jack_port_t *output_port2;
jack_client_t *client;

projectm_handle projectm;

GLuint vao;
GLuint vbo;
GLuint idx;
GLuint texture_id;
GLuint vertexShader;
GLuint fragmentShader;
GLuint program;
int width = 320;
int height = 240;

/*-----------------------------------------------------------------------------
 * Shaders (to be removed)
 * ---------------------------------------------------------------------------*/
const char *vertexSource = "#version 330\n\
in mediump vec3 point;\n\
in mediump vec2 texcoord;\n\
out mediump vec2 UV;\n\
void main()\n\
{\n\
  gl_Position = vec4(point, 1);\n\
  UV = texcoord;\n\
}";

const char *fragmentSource = "#version 330\n\
in mediump vec2 UV;\n\
out mediump vec3 fragColor;\n\
uniform sampler2D texture_id;\n\
void main()\n\
{\n\
  fragColor = texture(texture_id, UV).rgb;\n\
}";

GLfloat vertices[] = {
   0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
  -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
  -0.5f, -0.5f,  0.0f, 0.0f, 0.0f
};

unsigned int indices[] = { 0, 1, 2 };

/*-----------------------------------------------------------------------------
 * Call back functions
 * ---------------------------------------------------------------------------*/
/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 *
 * This client does nothing more than copy data from its input
 * port to its output port. It will exit when stopped by 
 * the user (e.g. using Ctrl-C on a unix-ish operating system)
 */
int process (jack_nframes_t nframes, void *arg)
{
	jack_default_audio_sample_t *in, *out;
	
	in = jack_port_get_buffer (input_port1, nframes);
	out = jack_port_get_buffer (output_port1, nframes);
	memcpy (out, in,
		sizeof (jack_default_audio_sample_t) * nframes);
    in = jack_port_get_buffer (input_port2, nframes);
	out = jack_port_get_buffer (output_port2, nframes);
	memcpy (out, in,
		sizeof (jack_default_audio_sample_t) * nframes);
	return 0;
}

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void jack_shutdown (void *arg)
{
	exit (1);
}

void render(void)
{
    //projectm_render_frame(projectm);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, (void *)0);
    glutSwapBuffers();
}

void reshape(int w, int h)
{
	width = w; height = h;
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);
    //projectm_set_window_size(projectm, x, y);
}

/*-----------------------------------------------------------------------------
 * Internal functions
 * ---------------------------------------------------------------------------*/
void printError(const char *context)
{
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    fprintf(stderr, "%s: %s\n", context, gluErrorString(error));
  }
}

void printStatus(const char *step, GLuint context, GLuint status)
{
  GLint result = GL_FALSE;
  if (status == GL_COMPILE_STATUS)
    glGetShaderiv(context, status, &result);
  else
    glGetProgramiv(context, status, &result);
  if (result == GL_FALSE) {
    char buffer[1024];
    if (status == GL_COMPILE_STATUS)
      glGetShaderInfoLog(context, 1024, NULL, buffer);
    else
      glGetProgramInfoLog(context, 1024, NULL, buffer);
    if (buffer[0]) {
      fprintf(stderr, "%s: %s\n", step, buffer);
    }
  }
}

void printCompileStatus(const char *step, GLuint context)
{
  printStatus(step, context, GL_COMPILE_STATUS);
}

void printLinkStatus(const char *step, GLuint context)
{
  printStatus(step, context, GL_LINK_STATUS);
}

/*-----------------------------------------------------------------------------
 * Functions exported to Java
 * ---------------------------------------------------------------------------*/
JNIEXPORT jboolean JNICALL Java_org_brain4free_jprojectm_ProjectM_initJackPorts
  (JNIEnv* env, jobject thisObject)
{
	const char **ports;
	const char *client_name = "projectM-jack";
	const char *server_name = NULL;
	jack_options_t options = JackNullOption;
	jack_status_t status;
	
	/* open a client connection to the JACK server */

	client = jack_client_open (client_name, options, &status, server_name);
	if (client == NULL) {
		fprintf (stderr, "ERROR: jack_client_open() failed, "
			 "status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			fprintf (stderr, "ERROR: Unable to connect to JACK server\n");
		}
		return(JNI_TRUE);
	}
	if (status & JackServerStarted) {
		fprintf (stderr, "INFO: JACK server started\n");
	}
	if (status & JackNameNotUnique) {
		client_name = jack_get_client_name(client);
		fprintf (stderr, "ERROR: unique name `%s' assigned\n", client_name);
	}

	/* tell the JACK server to call `process()' whenever
	   there is work to be done.
	*/

	jack_set_process_callback (client, process, 0);

	/* tell the JACK server to call `jack_shutdown()' if
	   it ever shuts down, either entirely, or if it
	   just decides to stop calling us.
	*/

	jack_on_shutdown (client, jack_shutdown, 0);

	/* display the current sample rate. 
	 */ 

	printf ("INFO: engine sample rate: %" PRIu32 "\n",
		jack_get_sample_rate (client));

	/* create two ports */

	input_port1 = jack_port_register (client, "input_FL",
					 JACK_DEFAULT_AUDIO_TYPE,
					 JackPortIsInput, 0);
    input_port2 = jack_port_register (client, "input_FR",
					 JACK_DEFAULT_AUDIO_TYPE,
					 JackPortIsInput, 0);
	output_port1 = jack_port_register (client, "output_FL",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsOutput, 0);
    output_port2 = jack_port_register (client, "output_FR",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsOutput, 0);

	if ((input_port1 == NULL) || (output_port1 == NULL)) {
		fprintf(stderr, "ERROR: no more JACK ports available\n");
		return(JNI_TRUE);
	}
	
	if ((input_port2 == NULL) || (output_port2 == NULL)) {
		fprintf(stderr, "ERROR: no more JACK ports available\n");
		return(JNI_TRUE);
	}

	/* Tell the JACK server that we are ready to roll.  Our
	 * process() callback will start running now. */

	if (jack_activate (client)) {
		fprintf (stderr, "ERROR: cannot activate client");
		return(JNI_TRUE);
	}

	/* Connect the ports.  You can't do this before the client is
	 * activated, because we can't make connections to clients
	 * that aren't running.  Note the confusing (but necessary)
	 * orientation of the driver backend ports: playback ports are
	 * "input" to the backend, and capture ports are "output" from
	 * it.
	 */

	ports = jack_get_ports (client, NULL, NULL,
				JackPortIsPhysical|JackPortIsOutput);
	if (ports == NULL) {
		fprintf(stderr, "ERROR: no physical capture ports\n");
		return(JNI_TRUE);
	}

	if (jack_connect (client, ports[0], jack_port_name (input_port1))) {
		fprintf (stderr, "ERROR: cannot connect input port1\n");
	}
	if (jack_connect (client, ports[1], jack_port_name (input_port2))) {
		fprintf (stderr, "ERROR: cannot connect input port2\n");
	}

	free (ports);
	
	ports = jack_get_ports (client, NULL, NULL,
				JackPortIsPhysical|JackPortIsInput);
	if (ports == NULL) {
		fprintf(stderr, "ERROR: no physical playback ports\n");
		return(JNI_TRUE);
	}

	if (jack_connect (client, jack_port_name (output_port1), ports[0])) {
		fprintf (stderr, "ERROR: cannot connect output port1\n");
	}
	if (jack_connect (client, jack_port_name (output_port2), ports[1])) {
		fprintf (stderr, "ERROR: cannot connect output port2\n");
	}
    
	free (ports);
    
    return(JNI_FALSE);
}

JNIEXPORT jboolean JNICALL Java_org_brain4free_jprojectm_ProjectM_initGlWindow
  (JNIEnv* env, jobject thisObject)
{
    int argc = 1;
    char arg[] = "libjprojectm";
    char *argv;
    argv = arg;

    /* Initialize GLUT */
	glutInit(&argc, &argv);
    glutInitContextVersion(3, 3);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    
	//Create a window with rendering context and everything else we need
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(width, height);
	glutCreateWindow("projectM-jack");
    printf("INFO: GL_VERSION: %s\n", glGetString(GL_VERSION));
    printf("INFO: GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("INFO: GL_VENDOR: %s\n", glGetString(GL_VENDOR));

    
    glewExperimental = GL_TRUE;
    glewInit();

    return(JNI_FALSE);
}

JNIEXPORT jboolean JNICALL Java_org_brain4free_jprojectm_ProjectM_initShaders
  (JNIEnv* env, jobject thisObject)
{
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    printCompileStatus("Vertex shader", vertexShader);

    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    printCompileStatus("Fragment shader", fragmentShader);

    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    printLinkStatus("Shader program", program);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &idx);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idx);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(glGetAttribLocation(program, "point"), 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glVertexAttribPointer(glGetAttribLocation(program, "texcoord"), 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));

    glEnable(GL_DEPTH_TEST);

    glUseProgram(program);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
  
	//Assign the two used Msg-routines
	glutDisplayFunc(render);
	glutReshapeFunc(reshape);
    
    return(JNI_FALSE);
}

JNIEXPORT jboolean JNICALL Java_org_brain4free_jprojectm_ProjectM_initTexture
  (JNIEnv* env, jobject thisObject, jint image_size)
{
    /* Create some dummy image with a color gradient */
    uint8_t *image_data = malloc(image_size * image_size * 3 * sizeof(uint8_t));
    for(int i=0;i<image_size;i++) {
       for(int j=0;j<image_size;j++) {
           int p = 3*(i*image_size+j);
           image_data[p+0] = i;
           image_data[p+1] = j;
           image_data[p+2] = 255-i;
       }
    }
    
    /* load a texture */
        
    /* Create one OpenGL texture */
    glGenTextures(1, &texture_id);
    printf("INFO: texture_id %d\n", texture_id);

    /* "Bind" the newly created texture : all future texture functions will modify this texture */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, image_size, image_size, 0, GL_RGB, GL_UNSIGNED_BYTE, image_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    free(image_data);
    
    return(JNI_FALSE);
}

JNIEXPORT jboolean JNICALL Java_org_brain4free_jprojectm_ProjectM_initProjectm
  (JNIEnv* env, jobject thisObject)
{
    /* Initialize projectM */
    printf("ProjectM max samples: %d\n",projectm_pcm_get_max_samples());
    //projectm = projectm_create("/home/chrigi/.projectM/config.inp", 0);
    if (projectm == NULL) {
		fprintf (stderr, "projectm_create() failed\n");
		return(JNI_TRUE);
    }
    //texture_id = projectm_init_render_to_texture(projectm);
    //projectm_set_texture_size(projectm, 2048);
    //projectm_set_window_size(projectm, 300, 300);
    //projectm_set_mesh_size(projectm, 128, 128);
    
    return(JNI_FALSE);
}

JNIEXPORT void JNICALL Java_org_brain4free_jprojectm_ProjectM_init
  (JNIEnv* env, jobject thisObject)
{
    printf ("INFO: Init\n");
    Java_org_brain4free_jprojectm_ProjectM_initJackPorts(env, thisObject);
    Java_org_brain4free_jprojectm_ProjectM_initGlWindow(env, thisObject);
    Java_org_brain4free_jprojectm_ProjectM_initTexture(env, thisObject, 256);
    Java_org_brain4free_jprojectm_ProjectM_initProjectm(env, thisObject);
    Java_org_brain4free_jprojectm_ProjectM_startMainLoop(env, thisObject);
}


JNIEXPORT void JNICALL Java_org_brain4free_jprojectm_ProjectM_destroyJack
  (JNIEnv* env, jobject thisObject)
{
    /* Clean up everything */
	jack_client_close (client);
}

JNIEXPORT void JNICALL Java_org_brain4free_jprojectm_ProjectM_destroyProjectm
  (JNIEnv* env, jobject thisObject)
{    
    projectm_destroy(projectm);
}

JNIEXPORT void JNICALL Java_org_brain4free_jprojectm_ProjectM_destroyGl
  (JNIEnv* env, jobject thisObject)
{
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &texture_id);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &idx);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &vbo);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);

    glDetachShader(program, vertexShader);
    glDetachShader(program, fragmentShader);
    glDeleteProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

JNIEXPORT void JNICALL Java_org_brain4free_jprojectm_ProjectM_destroy
  (JNIEnv* env, jobject thisObject)
{
    printf ("INFO: Destroy\n");
    Java_org_brain4free_jprojectm_ProjectM_destroyJack(env, thisObject);
    Java_org_brain4free_jprojectm_ProjectM_destroyGl(env, thisObject);
    Java_org_brain4free_jprojectm_ProjectM_destroyProjectm(env, thisObject);
}

JNIEXPORT void JNICALL Java_org_brain4free_jprojectm_ProjectM_render
  (JNIEnv* env, jobject thisObject)
{
    //projectm_render_frame(projectm);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, (void *)0);
    glutSwapBuffers();
}

JNIEXPORT void JNICALL Java_org_brain4free_jprojectm_ProjectM_renderTexture
  (JNIEnv* env, jobject thisObject)
{
    //projectm_render_frame(projectm);
    glBindTexture(GL_TEXTURE_2D, texture_id);
}

JNIEXPORT void JNICALL Java_org_brain4free_jprojectm_ProjectM_reshape
  (JNIEnv* env, jobject thisObject, jint w, jint h)
{
	width = w; height = h;
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);
    //projectm_set_window_size(projectm, x, y);
}

JNIEXPORT jboolean JNICALL Java_org_brain4free_jprojectm_ProjectM_loadPreset
  (JNIEnv* env, jobject thisObject, jstring presetUrl)
{
    const char* presetUrlCharPointer = (*env)->GetStringUTFChars(env, presetUrl, 0);
    int rating[1] = {1};
    
    if (projectm == NULL) {
        fprintf (stderr, "ERROR: projectM not Initialized!\n");
        return(JNI_TRUE);
    }

    printf("INFO: New preset %s\n" , presetUrlCharPointer);
    
    /* Preset handling */
    projectm_clear_playlist(projectm);
    projectm_insert_preset_url(projectm, 0, presetUrlCharPointer, "test", rating, 0);
    projectm_select_preset(projectm, 0, true);
    if (projectm_get_error_loading_current_preset(projectm) == false) {
        fprintf (stderr, "projectm_select_preset() failed\n");
        return(JNI_TRUE);
    }
    projectm_lock_preset(projectm, true);
    
    (*env)->ReleaseStringUTFChars(env, presetUrl, presetUrlCharPointer); 
    
    return(JNI_FALSE);
}

JNIEXPORT jboolean JNICALL Java_org_brain4free_jprojectm_ProjectM_startMainLoop
  (JNIEnv* env, jobject thisObject)
{    
	/* Let GLUT get the msgs */
	glutMainLoop();
    
    return(JNI_FALSE);
}
