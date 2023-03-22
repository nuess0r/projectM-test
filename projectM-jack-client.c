/** @file projectM-jack-client.c
 *
 * @brief This simple client demonstrates the C API of libprojectM
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <jack/jack.h>
#include <GL/freeglut.h>
#include <libprojectM/projectM.h>

jack_port_t *input_port1;
jack_port_t *input_port2;
jack_port_t *output_port1;
jack_port_t *output_port2;
jack_client_t *client;

projectm_handle projectm;

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
    
    int nsamples;
    nsamples = projectm_pcm_get_max_samples();
    if(nframes < nsamples) {
        nsamples = nframes;
    }
	
	in = jack_port_get_buffer (input_port1, nframes);
	out = jack_port_get_buffer (output_port1, nframes);
	memcpy (out, in,
		sizeof (jack_default_audio_sample_t) * nframes);
    in = jack_port_get_buffer (input_port2, nframes);
	out = jack_port_get_buffer (output_port2, nframes);
	memcpy (out, in,
		sizeof (jack_default_audio_sample_t) * nframes);
	return 0;

    projectm_pcm_add_float(projectm, in, nsamples, PROJECTM_STEREO);
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
    glClear(GL_COLOR_BUFFER_BIT);
	glLoadIdentity();
    projectm_render_frame(projectm);
	/*
	glBegin(GL_POLYGON);
		glColor3f(0.0,0.0,0.0);
		glVertex3f(-0.5,-0.5,-3.0);
		glColor3f(1.0,0.0,0.0);
		glVertex3f(0.5,-0.5,-3.0);
		glColor3f(0.0,0.0,1.0);
		glVertex3f(0.5,0.5,-3.0);
		*/
	glEnd();
	glFlush();			//Finish rendering
}

void reshape(int x, int y)
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
    //projectm_set_window_size(projectm, x, y);
	glViewport(0,0,x,y);  //Use the whole window for rendering
}

int main (int argc, char *argv[])
{
	const char **ports;
	const char *client_name = "projectM-jack";
	const char *server_name = NULL;
	jack_options_t options = JackNullOption;
	jack_status_t status;
    
    GLuint texture_id;
    int rating[1] = {1};
    
    if (argc == 1) {
		fprintf (stderr, "You need to specify a path to a Milkdrop preset\n");
		exit (1);
    }
	
	/* open a client connection to the JACK server */

	client = jack_client_open (client_name, options, &status, server_name);
	if (client == NULL) {
		fprintf (stderr, "ERROR: jack_client_open() failed, "
			 "status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			fprintf (stderr, "ERROR: Unable to connect to JACK server\n");
		}
		exit (1);
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
		exit (1);
	}
	
	if ((input_port2 == NULL) || (output_port2 == NULL)) {
		fprintf(stderr, "ERROR: no more JACK ports available\n");
		exit (1);
	}

	/* Tell the JACK server that we are ready to roll.  Our
	 * process() callback will start running now. */

	if (jack_activate (client)) {
		fprintf (stderr, "ERROR: cannot activate client");
		exit (1);
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
		exit (1);
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
		exit (1);
	}

	if (jack_connect (client, jack_port_name (output_port1), ports[0])) {
		fprintf (stderr, "ERROR: cannot connect output port1\n");
	}
	if (jack_connect (client, jack_port_name (output_port2), ports[1])) {
		fprintf (stderr, "ERROR: cannot connect output port2\n");
	}
    
	free (ports);
    
    
    /* Initialize GLUT */
	glutInit(&argc, argv);
    glutInitContextVersion(3, 3);
    glutInitContextProfile(GLUT_CORE_PROFILE);
	glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
	glutInitWindowSize(300, 300);
	//Create a window with rendering context and everything else we need
	glutCreateWindow("projectM-jack");
	glClearColor(0.0,0.0,0.0,0.0);
	//Assign the two used Msg-routines
	glutDisplayFunc(render);
	glutReshapeFunc(reshape);
    
    printf("INFO: GL_VERSION: %s\n", glGetString(GL_VERSION));
    printf("INFO: GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("INFO: GL_VENDOR: %s\n", glGetString(GL_VENDOR));
    
    /* Initialize projectM */
    printf("ProjectM max samples: %d\n",projectm_pcm_get_max_samples());
    projectm = projectm_create("/home/chrigi/.projectM/config.inp", 0);
    if (projectm == NULL) {
		fprintf (stderr, "projectm_create() failed\n");
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
    if (projectm_get_error_loading_current_preset(projectm) == false) {
		fprintf (stderr, "projectm_select_preset() failed\n");
		exit (1);
    }
    projectm_lock_preset(projectm, true);
    
    //projectm_render_frame(projectm);
    
	//Let GLUT get the msgs
	glutMainLoop();

	jack_client_close (client);
    projectm_destroy(projectm);
	exit (0);
}
