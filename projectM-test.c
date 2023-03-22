/** @file projectM-test.c
 *
 * @brief This simple client demonstrates the C API of libprojectM
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <GL/glut.h>
#include <libprojectM/projectM.h>

projectm_handle projectm;


void Display(void)
{
    if (projectm != NULL) {
		projectm_render_frame(projectm);
    }
	glClear(GL_COLOR_BUFFER_BIT);
	glLoadIdentity();
	glBegin(GL_POLYGON);
		glColor3f(0.0,0.0,0.0);
		glVertex3f(-0.5,-0.5,-3.0);
		glColor3f(1.0,0.0,0.0);
		glVertex3f(0.5,-0.5,-3.0);
		glColor3f(0.0,0.0,1.0);
		glVertex3f(0.5,0.5,-3.0);
	glEnd();
	glFlush();			//Finish rendering
}

void Reshape(int x, int y)
{
	if (y == 0 || x == 0) return;  //Nothing is visible then, so return
	//Set a new projection matrix
	glMatrixMode(GL_PROJECTION);  
	glLoadIdentity();
	//Angle of view:40 degrees
	//Near clipping plane distance: 0.5
	//Far clipping plane distance: 20.0
	gluPerspective(40.0,(GLdouble)x/(GLdouble)y,0.5,20.0);
	glMatrixMode(GL_MODELVIEW);
    if (projectm != NULL) {
		projectm_set_window_size(projectm, x, y);
    }
	glViewport(0,0,x,y);  //Use the whole window for rendering
}

int
main (int argc, char *argv[])
{
    if (argc == 1) {
		fprintf (stderr, "You need to specify a path to a Milkdrop preset\n");
		exit (1);
    }
    
    GLuint textureID;
    int rating[1] = {1};
    
    /* Initialize GLUT */
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
	glutInitWindowSize(300, 300);
	//Create a window with rendering context and everything else we need
	glutCreateWindow("Intro");
	glClearColor(0.0,0.0,0.0,0.0);
	//Assign the two used Msg-routines
	glutDisplayFunc(Display);
	glutReshapeFunc(Reshape);
    
    printf("INFO: GL_VERSION: %s\n", glGetString(GL_VERSION));
    printf("INFO: GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("INFO: GL_VENDOR: %s\n", glGetString(GL_VENDOR));

    
    /* Initialize projectM */
    projectm = projectm_create(NULL, 0);
    if (projectm == NULL) {
		fprintf (stderr, "projectm_create() failed");
		exit (1);
    }
    //texture_id = projectm_init_render_to_texture(projectm);
    projectm_set_texture_size(projectm, 2048);
    projectm_set_window_size(projectm, 300, 300);
    projectm_set_mesh_size(projectm, 128, 128);
    
    /* Preset handling */
    projectm_clear_playlist(projectm);
    projectm_insert_preset_url(projectm, 0, argv[1], "test", rating, 0);
    projectm_select_preset(projectm, 0, true);
    
	//Let GLUT get the msgs
	glutMainLoop();

    projectm_destroy(projectm);
	exit (0);
}
