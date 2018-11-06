//-----------------------------------------------------------------------------
// name: VisualSine.cpp <--> Sandsofsound.cpp

// desc: Hourglass real-time spectrum visualizer
//
// author: Abhijeet Phatak (aphatak@stanford.edu)
// inspired by: Sndpeek by Ge Wang (ge@ccrma.stanford.edu)
//   date: fall 2018
//   uses: RtAudio by Gary Scavone
//-----------------------------------------------------------------------------
#include "RtAudio/RtAudio.h"
#include "chuck.h"
#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <Windows.h>
#include <process.h>
#include <memory.h>

//#ifdef __MACOSX_CORE__
//#include <GLUT/glut.h>
//#include <OpenGL/gl.h>
//#include <OpenGL/glu.h>
//#else
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
//#endif

//fft
#include "chuck_fft.h"

using namespace std;

//-----------------------------------------------------------------------------
// function prototypes
//-----------------------------------------------------------------------------
void help();
void initGfx();
void idleFunc();
void displayFunc();
void reshapeFunc(GLsizei width, GLsizei height);
void keyboardFunc(unsigned char, int, int);
void mouseFunc(int button, int state, int x, int y);

// our datetype
#define SAMPLE float
// corresponding format for RtAudio
#define MY_FORMAT RTAUDIO_FLOAT32
// sample rate
#define MY_SRATE 44100
// number of channels
#define MY_CHANNELS 2
// sound FFT size
#define SND_BUFFER_SIZE 1024
// sound FFT size
#define SND_FFT_SIZE ( SND_BUFFER_SIZE * 2 )
// for convenience
#define MY_PIE 3.14159265358979
// define framsize
const int FRAMESIZE = 1024;

GLfloat g_e = 2.7182818;

// width and height
long g_width = 1024;
long g_height = 720;
// global buffer
SAMPLE * g_buffer = NULL;
long g_bufferSize;

// global variables
bool g_draw_dB = false;
ChucK * the_chuck = NULL;

// narrative
GLboolean is_narrative = FALSE;
stringstream ss;

// Chuck sample rate
const t_CKFLOAT CHUCK_MY_SRATE = 44100;
// Chuck number of channels
const t_CKINT CHUCK_MY_CHANNELS = 1;

// global frequency (used for Chuck Sine Wave Testing)
float freq = 440;

// global audio buffer
SAMPLE g_fft_buffer[SND_FFT_SIZE];
SAMPLE g_audio_buffer[SND_BUFFER_SIZE]; // latest mono buffer (possibly preview)
SAMPLE g_stereo_buffer[SND_BUFFER_SIZE * 2]; // current stereo buffer (now playing)
GLfloat g_window[SND_BUFFER_SIZE]; // DFT transform window
GLfloat g_log_positions[SND_FFT_SIZE / 2]; // precompute positions for log spacing
GLint g_buffer_size = SND_BUFFER_SIZE;
GLint g_fft_size = SND_FFT_SIZE;

// real-time audio
RtAudio * g_audio = NULL;
GLboolean g_ready = FALSE;

// array of booleans for waterfall
//GLboolean * g_draw = NULL;

// spiral
GLboolean g_spiral = FALSE;
GLboolean g_randomize = FALSE;

// how much to see
GLint g_time_view = 1;
GLint g_freq_view = 2;

//global color for spectrum
GLfloat red = 1;
GLfloat green = 0.5;
GLfloat blue = 0;

// for log scaling
GLdouble g_log_space = 0;
//GLdouble g_log_factor = 1;
GLdouble g_log_factor = 0.1;

// the index associated with the waterfall
GLint g_wf = 0;
GLint g_iter = 0;
// starting a file
GLint g_starting = 0;

// delay for pseudo-Lissajous in mono stuff
GLint g_delay = SND_BUFFER_SIZE / 2;

// number of real-time audio channels
GLint g_sndout = 0;
GLint g_sndin = 2;

// for frequency waterfall
struct Pt2D { float x; float y; };
Pt2D ** g_spectrums = NULL;
GLuint g_depth = 4;

// for time domain waterfall
SAMPLE ** g_waveforms = NULL;

// keeps track of maximum amplitude
float max_amp = 0;

//Making ellipse
float cos_skew = 1.0f;
float sin_skew = 1.0f;
float fader = 1.0f;

//Input
float max_radius = 6.0;
float time_gain = 2.0f;

//Modes


//-----------------------------------------------------------------------------
// name: callme()
// desc: audio callback
//-----------------------------------------------------------------------------
int callme(void * outputBuffer, void * inputBuffer, unsigned int numFrames,
	double streamTime, RtAudioStreamStatus status, void * data)
{
	// cast!
	SAMPLE * input = (SAMPLE *)inputBuffer;
	SAMPLE * output = (SAMPLE *)outputBuffer;

	// compute chuck!
	// (TODO: to fill in)

	// fill
	the_chuck->run(input, output, FRAMESIZE);


	for (int i = 0; i < numFrames; i++)
	{
		// copy the input to visualize only the left-most channel
		g_buffer[i] = input[i*MY_CHANNELS];

		// also copy in the output from chuck to our visualizer
		g_buffer[i] = g_buffer[i] + 0.5 * output[i*MY_CHANNELS];

		if (!is_narrative) {
			// mute output -- TODO will need to disable this once ChucK produces output, in order for you to hear it!
			for (int j = 0; j < MY_CHANNELS; j++) {
				output[i*MY_CHANNELS + j] = 0;

			}
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------
// name: main()
// desc: entry point
//-----------------------------------------------------------------------------
int main(int argc, char ** argv)
{
	// instantiate RtAudio object
	RtAudio audio;
	// variables
	unsigned int bufferBytes = 0;
	// frame size
	unsigned int bufferFrames = 1024;

	// check for audio devices
	if (audio.getDeviceCount() < 1)
	{
		// nopes
		cout << "no audio devices found!" << endl;
		exit(1);
	}

	// check for chuck and set narrative

	// set up chuck
	the_chuck = new ChucK();
	char myDirectory[512];
	GetModuleFileName(NULL, myDirectory, MAX_PATH);
	cout << "Current working directory " << myDirectory << endl;
	the_chuck->setParam(CHUCK_PARAM_WORKING_DIRECTORY, myDirectory);
	the_chuck->setParam(CHUCK_PARAM_SAMPLE_RATE, CHUCK_MY_SRATE);
	the_chuck->setParam(CHUCK_PARAM_OUTPUT_CHANNELS, 2);
	the_chuck->setParam(CHUCK_PARAM_INPUT_CHANNELS, 1);

	//// Initialize ChucK
	the_chuck->init();

	// initialize GLUT
	glutInit(&argc, argv);
	// init gfx
	initGfx();

	// let RtAudio print messages to stderr.
	audio.showWarnings(true);

	// set input and output parameters
	RtAudio::StreamParameters iParams, oParams;
	iParams.deviceId = audio.getDefaultInputDevice();
	iParams.nChannels = MY_CHANNELS;
	iParams.firstChannel = 0;
	oParams.deviceId = audio.getDefaultOutputDevice();
	oParams.nChannels = MY_CHANNELS;
	oParams.firstChannel = 0;

	// create stream options
	RtAudio::StreamOptions options;

	// go for it
	try {
		// open a stream
		audio.openStream(&oParams, &iParams, MY_FORMAT, MY_SRATE, &bufferFrames, &callme, (void *)&bufferBytes, &options);
	}
	catch (RtError& e)
	{
		// error!
		cout << e.getMessage() << endl;
		exit(1);
	}

	// compute
	bufferBytes = bufferFrames * MY_CHANNELS * sizeof(SAMPLE);
	// allocate global buffer
	g_bufferSize = bufferFrames;
	g_buffer = new SAMPLE[g_bufferSize];
	memset(g_buffer, 0, sizeof(SAMPLE)*g_bufferSize);

	// go for it
	try {
		// start stream
		audio.startStream();

		// let GLUT handle the current thread from here
		glutMainLoop();

		// stop the stream.
		audio.stopStream();
	}
	catch (RtError& e)
	{
		// print error message
		cout << e.getMessage() << endl;
		goto cleanup;
	}

cleanup:
	// close if open
	if (audio.isStreamOpen())
		audio.closeStream();

	// done
	return 0;
}




//-----------------------------------------------------------------------------
// name: help()
// desc: prints out help message for the user to interact
//-----------------------------------------------------------------------------
void help()
{
	fprintf(stderr, "----------------------------------------------------\n");
	fprintf(stderr, "Sands of Sound (1.0)\n");
	fprintf(stderr, "Abhijeet Phatak\n");
	fprintf(stderr, "aphatak@stanford.edu\n");
	fprintf(stderr, "----------------------------------------------------\n");
	fprintf(stderr, "'q' - quit the program\n");
	fprintf(stderr, "'n' - play narrative\n");
	fprintf(stderr, "'s' - use spirals for visualizing\n");
	fprintf(stderr, "'c' - change colors of the visualizer");
	fprintf(stderr, "'o' - go back to circles");
	fprintf(stderr, "\n");
}
//-----------------------------------------------------------------------------
// Name: reshapeFunc( )
// Desc: called when window size changes
//-----------------------------------------------------------------------------
void initGfx()
{
	// double buffer, use rgb color, enable depth buffer
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	// initialize the window size
	glutInitWindowSize(g_width, g_height);
	// set the window postion
	glutInitWindowPosition(100, 100);
	// create the window
	glutCreateWindow("Sands of Sound");

	// set the idle function - called when idle
	glutIdleFunc(idleFunc);
	// set the display function - called when redrawing
	glutDisplayFunc(displayFunc);
	// set the reshape function - called when client area changes
	glutReshapeFunc(reshapeFunc);
	// set the keyboard function - called on keyboard events
	glutKeyboardFunc(keyboardFunc);
	// set the mouse function - called on mouse stuff
	glutMouseFunc(mouseFunc);

	// set clear color
	glClearColor(0, 0, 0, 1);
	// enable transparency
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// enable color material
	glEnable(GL_COLOR_MATERIAL);
	// enable depth test
	glEnable(GL_DEPTH_TEST);

	// initialize time domain
	g_waveforms = new SAMPLE *[g_depth];

	for (GLuint i = 0; i < g_depth; i++)
	{
		// allocate memory (stereo)
		g_waveforms[i] = new SAMPLE[g_buffer_size];
		// zero it
		memset(g_waveforms[i], 0, g_buffer_size * sizeof(SAMPLE));
	}

	// initialize frequency
	g_spectrums = new Pt2D *[g_depth];

	for (GLuint i = 0; i < g_depth; i++)
	{
		g_spectrums[i] = new Pt2D[SND_FFT_SIZE];
		memset(g_spectrums[i], 0, sizeof(Pt2D)*SND_FFT_SIZE);
	}


}


//-----------------------------------------------------------------------------
// Name: reshapeFunc( )
// Desc: called when window size changes
//-----------------------------------------------------------------------------
void reshapeFunc(GLsizei w, GLsizei h)
{
	// save the new window size
	g_width = w; g_height = h;
	// map the view port to the client area
	glViewport(0, 0, w, h);
	// set the matrix mode to project
	glMatrixMode(GL_PROJECTION);
	// load the identity matrix
	glLoadIdentity();
	// create the viewing frustum
	gluPerspective(45.0, (GLfloat)w / (GLfloat)h, 1.0, 300.0);
	// set the matrix mode to modelview
	glMatrixMode(GL_MODELVIEW);
	// load the identity matrix
	glLoadIdentity();
	// position the view point
	//gluLookAt( 5.0f, 5.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f );
	gluLookAt(20.0f, 10.0f, 20.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
}



//-----------------------------------------------------------------------------
// Name: keyboardFunc( )
// Desc: key event
//-----------------------------------------------------------------------------
void keyboardFunc(unsigned char key, int x, int y)
{
	switch (key)
	{
	case 'Q':
	case 'q':
		exit(1);
		break;

	case 'n':
		is_narrative = TRUE;
		the_chuck->compileFile("./sands_of_sound_final", "", 1);
		break;

	case 's':
		g_spiral = TRUE;
		break;

	case 'o':
		g_spiral = FALSE; //Set to circles
		break;

	case 'c':
		g_randomize = TRUE;
		red = (double)rand() / (RAND_MAX);
		green = (double)rand() / (RAND_MAX);
		blue = (double)rand() / (RAND_MAX);
		break;
	}

	glutPostRedisplay();
}




//-----------------------------------------------------------------------------
// Name: mouseFunc( )
// Desc: handles mouse stuff
//-----------------------------------------------------------------------------
void mouseFunc(int button, int state, int x, int y)
{
	if (button == GLUT_LEFT_BUTTON)
	{
		// when left mouse button is down
		if (state == GLUT_DOWN)
		{
			glMatrixMode(GL_MODELVIEW);
			// load the identity matrix
			glLoadIdentity();
			// position the view point
			gluLookAt(10.0f, 10.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
		}
		else
		{
			glLoadIdentity();
			gluLookAt(20.0f, 20.0f, 20.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

		}
	}
	else if (button == GLUT_RIGHT_BUTTON)
	{
		// when right mouse button down
		if (state == GLUT_DOWN)
		{

		}
		else
		{
		}
	}
	else
	{
	}

	glutPostRedisplay();
}


//-----------------------------------------------------------------------------
// Name: idleFunc( )
// Desc: callback from GLUT
//-----------------------------------------------------------------------------
void idleFunc()
{
	// render the scene
	glutPostRedisplay();
}


void drawCube() {
	// color
	glColor4f(.5, 1, .5, 0.7);

	// White side - BACK
	glBegin(GL_POLYGON);
	glColor3f(1.0, 1.0, 1.0);
	glVertex3f(0.5, -0.5, 0.5);
	glVertex3f(0.5, 0.5, 0.5);
	glVertex3f(-0.5, 0.5, 0.5);
	glVertex3f(-0.5, -0.5, 0.5);
	glEnd();

	// Purple side - RIGHT
	glBegin(GL_POLYGON);
	glColor3f(1.0, 0.0, 1.0);
	glVertex3f(0.5, -0.5, -0.5);
	glVertex3f(0.5, 0.5, -0.5);
	glVertex3f(0.5, 0.5, 0.5);
	glVertex3f(0.5, -0.5, 0.5);
	glEnd();

	// Green side - LEFT
	glBegin(GL_POLYGON);
	glColor3f(0.0, 1.0, 0.0);
	glVertex3f(-0.5, -0.5, 0.5);
	glVertex3f(-0.5, 0.5, 0.5);
	glVertex3f(-0.5, 0.5, -0.5);
	glVertex3f(-0.5, -0.5, -0.5);
	glEnd();

	// Blue side - TOP
	glBegin(GL_POLYGON);
	glColor3f(0.0, 0.0, 1.0);
	glVertex3f(0.5, 0.5, 0.5);
	glVertex3f(0.5, 0.5, -0.5);
	glVertex3f(-0.5, 0.5, -0.5);
	glVertex3f(-0.5, 0.5, 0.5);
	glEnd();

	// Red side - BOTTOM
	glBegin(GL_POLYGON);
	glColor3f(1.0, 0.0, 0.0);
	glVertex3f(0.5, -0.5, -0.5);
	glVertex3f(0.5, -0.5, 0.5);
	glVertex3f(-0.5, -0.5, 0.5);
	glVertex3f(-0.5, -0.5, -0.5);
	glEnd();

}


//-----------------------------------------------------------------------------
// Name: displayFunc( )
// Desc: callback function invoked to draw the client area
//-----------------------------------------------------------------------------
void displayFunc()
{

	glPushMatrix();

	drawCube();

	// local state
	static GLfloat zrot = 0.0f, c = 0.0f;

	// clear the color and depth buffers
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// draw a sphere at the center of the scene (hourglass)
	glColor3f(0.34, 0.10, 0.56);
	glutWireSphere(1, 20, 20);


	// initialize some variables
	SAMPLE * buffer = g_fft_buffer, *ptr;
	GLint i;
	GLfloat ytemp, fval, fval_time;

	// line width
	glLineWidth(2.0f);

	// define a starting point
	// soon to be used drawing offsets
	GLfloat x = -4.0f;
	GLfloat xr = -4.0f;
	// increment
	GLfloat xinc = ::fabs(x * 2 / g_bufferSize);


	// get the latest (possibly preview) window
	memset(buffer, 0, SND_FFT_SIZE * sizeof(SAMPLE));

	// copy currently playing audio into buffer
	memcpy(buffer, g_buffer, g_buffer_size * sizeof(SAMPLE));

	// copy current buffer into waterfall memory
	for (g_wf = 0; g_wf < g_depth; g_wf++) {
		//g_wf = g_iter % g_depth;
		memcpy(g_waveforms[g_wf], g_buffer, g_buffer_size * sizeof(SAMPLE));
	}

	// apply hanning window
	hanning(g_window, g_buffer_size);
	apply_window((float*)buffer, g_window, g_buffer_size);

	//--------------------------------------------------------------------------------------------

	// take forward FFT; result in buffer as FFT_SIZE/2 complex values
	rfft((float *)buffer, g_fft_size / 2, FFT_FORWARD);

	// cast to complex
	complex * cbuf = (complex *)buffer;

	// copy current magnitude spectrum into waterfall memory
	for (g_wf = 0; g_wf < g_depth; g_wf++)
	{
		//g_wf = g_iter % g_depth;
		for (i = 0; i < g_fft_size / 2; i++)
		{
			// copy x coordinate
			g_spectrums[g_wf][i].x = x;
			// copy y coordinate
			g_spectrums[g_wf][i].y = 0.9f * 1.8f * ::pow(25 * cmp_abs(cbuf[i]), .5);

		}
	}

	// line width
	glLineWidth(2);

	/*
	start making the body of the hourglass - by using only current snapshot and by decreasing the radius
	upper half will be made with time domain signal and lower half will be frequency domain.
	*/

	// begin top half for visualizing time domain-------------------------------------------------
	glBegin(GL_LINE_STRIP);
	float theta = 0.0f;
	//color
	glClear(GL_COLOR_BUFFER_BIT);

	if (g_randomize) {
		glColor4f(red, green, blue, 1);
	}
	else { glColor4f(1, 0.5, 0, 1); }

	// loop over buffer
	if (g_spiral) {
		for (int i = 0; i < g_bufferSize; i++)
		{
			float r = 2.0;
			// plot
			float xi = theta / 5;
			glVertex3f(r * pow(g_e, xi) * cos(theta), 7 * g_waveforms[0][g_bufferSize - i] + 9, r * pow(g_e, xi) * sin(theta));
			theta += 0.01;
		}
	}
	else {
		for (int i = 0; i < g_bufferSize; i++)
		{
			float r = 2.0;
			// plot
			glVertex3f(xr * r * cos(theta), 7 * g_waveforms[0][i] + 9, xr * r * sin(theta));
			theta += 0.01;
		}
	}
	glEnd();

	glBegin(GL_LINE_STRIP);
	theta = 0.0f;

	//color
	glClear(GL_COLOR_BUFFER_BIT);

	if (g_randomize) {
		glColor4f(red, green, blue, 0.8);
	}
	else { glColor4f(1, 0.5, 0, 0.8); }

	if (g_spiral) {
		for (int i = 0; i < g_bufferSize; i++)
		{
			float r = 1.8;
			// plot
			float xi = theta / 5;
			glVertex3f(r * pow(g_e, xi) * cos(theta), 7 * g_waveforms[1][g_bufferSize - i] + 7, r * pow(g_e, xi) * sin(theta));
			theta += 0.01;
		}
	}
	else {
		for (int i = 0; i < g_bufferSize; i++)
		{
			float r = 1.8;
			// plot
			glVertex3f(xr * r * cos(theta), 7 * g_waveforms[1][i] + 7, xr * r * sin(theta));
			theta += 0.01;
		}
	}
	glEnd();

	glBegin(GL_LINE_STRIP);
	theta = 0.0f;

	// color
	glClear(GL_COLOR_BUFFER_BIT);
	if (g_randomize) {
		glColor4f(red, green, blue, 0.6);
	}
	else { glColor4f(1, 0.5, 0, 0.6); }

	if (g_spiral) {
		for (int i = 0; i < g_bufferSize; i++)
		{
			float r = 1.6;
			// plot
			float xi = theta / 5;
			glVertex3f(r * pow(g_e, xi) * cos(theta), 7 * g_waveforms[2][g_bufferSize - i] + 5, r * pow(g_e, xi) * sin(theta));
			theta += 0.01;
		}
	}
	else {
		for (int i = 0; i < g_bufferSize; i++)
		{
			float r = 1.6;
			// plot
			glVertex3f(xr * r * cos(theta), 7 * g_waveforms[2][i] + 5, xr * r * sin(theta));
			theta += 0.01;
		}
	}
	glEnd();

	glBegin(GL_LINE_STRIP);
	theta = 0.0f;

	// color
	glClear(GL_COLOR_BUFFER_BIT);
	if (g_randomize) {
		glColor4f(red, green, blue, 0.4);
	}
	else { glColor4f(1, 0.5, 0, 0.4); }

	if (g_spiral) {
		for (int i = 0; i < g_bufferSize; i++)
		{
			float r = 1.3;
			// plot
			float xi = theta / 5;
			glVertex3f(r * pow(g_e, xi) * cos(theta), 7 * g_waveforms[3][g_bufferSize - i] + 3, r * pow(g_e, xi) * sin(theta));
			theta += 0.01;
		}
	}
	else {
		for (int i = 0; i < g_bufferSize; i++)
		{
			float r = 1.3;
			// plot
			glVertex3f(xr * r * cos(theta), 7 * g_waveforms[3][i] + 3, xr * r * sin(theta));
			theta += 0.01;
		}
	}
	glEnd();
	//--------------------------------------------------------------------------------------------

	//--------------------------------------------------------------------------------------------

	//Draw center piece. Will be a flat circle - TODO interesting things with this
	glBegin(GL_LINE_STRIP);
	theta = 0.0f;

	// color
	glClear(GL_COLOR_BUFFER_BIT);
	glColor4f(red, green, blue, .1);
	for (int i = 0; i < g_bufferSize; i++)
	{
		float r = 1.0f;
		// plot
		glVertex3f(xr * r * cos(theta), 7 * g_buffer[i], xr * r * sin(theta));
		theta += 0.01;
	}
	glEnd();


	//start drawing the bottom half to visualize frequency domain--------------------------------

	glBegin(GL_LINE_STRIP);
	theta = 0.0f;

	// color
	glClear(GL_COLOR_BUFFER_BIT);

	if (g_randomize) {
		glColor4f(red, green, blue, 0.4);
	}
	else { glColor4f(1, 0.5, 0, 0.4); }
	if (g_spiral) {
		for (int i = 0; i < g_fft_size / 2; i++)
		{
			float r = 1.3;
			// plot
			float xi = theta / 5;
			glVertex3f(r * pow(g_e, xi) * cos(theta), 4 * g_spectrums[3][g_fft_size / 2 - i].y - 3, r * pow(g_e, xi) * sin(theta));
			theta += 0.01;
		}
	}
	else {
		for (int i = 0; i < g_fft_size / 2; i++)
		{
			float r = 1.3;
			// plot
			glVertex3f(xr * r * cos(theta), 4 * g_spectrums[3][g_fft_size / 2 - i].y - 3, xr * r * sin(theta));
			theta += 0.01;
		}
	}
	glEnd();

	glBegin(GL_LINE_STRIP);
	theta = 0.0f;

	// color
	glClear(GL_COLOR_BUFFER_BIT);

	if (g_randomize) {
		glColor4f(red, green, blue, 0.6);
	}
	else { glColor4f(1, 0.5, 0, 0.6); }
	if (g_spiral) {
		for (int i = 0; i < g_fft_size / 2; i++)
		{
			float r = 1.6;
			// plot
			float xi = theta / 5;
			glVertex3f(r * pow(g_e, xi) * cos(theta), 4 * g_spectrums[2][g_fft_size / 2 - i].y - 5, r * pow(g_e, xi) * sin(theta));
			theta += 0.01;
		}
	}
	else {
		for (int i = 0; i < g_fft_size / 2; i++)
		{
			float r = 1.6;
			// plot
			glVertex3f(xr * r * cos(theta), 4 * g_spectrums[2][i].y - 5, xr * r * sin(theta));
			theta += 0.01;
		}
	}
	glEnd();

	glBegin(GL_LINE_STRIP);
	theta = 0.0f;

	// color
	glClear(GL_COLOR_BUFFER_BIT);
	if (g_randomize) {
		glColor4f(red, green, blue, 0.8);
	}
	else { glColor4f(1, 0.5, 0, 0.8); }
	if (g_spiral) {
		for (int i = 0; i < g_fft_size / 2; i++)
		{
			float r = 1.8;
			// plot
			float xi = theta / 5;
			glVertex3f(r * pow(g_e, xi) * cos(theta), 4 * g_spectrums[1][g_fft_size / 2 - i].y - 7, r * pow(g_e, xi) * sin(theta));
			theta += 0.01;
		}
	}
	else {
		for (int i = 0; i < g_fft_size / 2; i++)
		{
			float r = 1.8;
			// plot
			glVertex3f(xr * r * cos(theta), 4 * g_spectrums[1][g_fft_size / 2 - i].y - 7, xr * r * sin(theta));
			theta += 0.01;
		}
	}
	glEnd();

	glBegin(GL_LINE_STRIP);
	theta = 0.0f;

	// color
	glClear(GL_COLOR_BUFFER_BIT);

	if (g_randomize) {
		glColor4f(red, green, blue, 1);
	}
	else { glColor4f(1, 0.5, 0, 1); }

	if (g_spiral) {
		for (int i = 0; i < g_fft_size / 2; i++)
		{
			float r = 2.0;
			// plot
			float xi = theta / 5;
			glVertex3f(r * pow(g_e, xi) * cos(theta), 4 * g_spectrums[0][g_fft_size / 2 - i].y - 9, r * pow(g_e, xi) * sin(theta));
			theta += 0.01;
		}
	}
	else {
		for (int i = 0; i < g_fft_size / 2; i++)
		{
			float r = 2.0;
			// plot
			glColor4f(red, green, blue, 1);
			glVertex3f(xr * r * cos(theta), 4 * g_spectrums[0][i].y - 9, xr * r * sin(theta));
			theta += 0.01;
		}
	}
	glEnd();

	glPopMatrix();

	//----------------------------------------------------------------------------------------

	// flush!
	glFlush();
	// swap the double buffer
	glutSwapBuffers();
}