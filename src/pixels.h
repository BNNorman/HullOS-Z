#pragma once

#include "utils.h"
#include "settings.h"
#include "controller.h"
#include "processes.h"
#include "Colour.h"
#include "Leds.h"
#include "Frame.h"

#define PIXEL_OK 100
#define PIXEL_OFF 101
#define PIXEL_NO_PIXELS 102

#include <Adafruit_NeoPixel.h>

#define PIXEL_RING_CONFIG NEO_GRB+NEO_KHZ800
#define PIXEL_STRING_CONFIG NEO_KHZ400+NEO_RGB

#define MAX_NO_OF_PIXELS 200
#define MAX_PIXEL_NAME_LENGTH 32

#define MILLIS_BETWEEN_UPDATES 20

#define MAX_BRIGHTNESS 255

#define PIXEL_COMMAND_NAME_LENGTH 20
#define PIXEL_COLOUR_NAME_LENGTH 15

void setupWalkingColour(Colour colour);
void setAllLightsOff();

void pixelStatusMessage(struct process * pixelProcess, char * buffer, int bufferLength);

struct PixelSettings
{
	int pixelControlPinNo;
	int noOfXPixels;
	int noOfYPixels;
	int pixelConfig;
	float brightness;
	char pixelName[MAX_PIXEL_NAME_LENGTH];
};

extern Leds *leds;
extern Frame *frame;

extern struct PixelSettings pixelSettings;
extern struct colourNameLookup colourNames[];
extern int noOfColours;

extern struct SettingItemCollection pixelSettingItems;

extern struct process pixelProcess;

void fadeWalkingColour(Colour newColour, int noOfSteps);
struct colourNameLookup * findColourByName(const char * name);

void updateBusyPixel();

void startBusyPixel(byte red, byte green, byte blue);

void setBusyPixelColour(byte red, byte green, byte blue);

void stopBusyPixel();

void flickeringColouredLights(byte r, byte g, byte b,int steps);

void setFlickerUpdateSpeed(int speed);

void transitionToColor(byte speed, byte r, byte g, byte b);

void setLightColor(byte r, byte g, byte b);

void randomiseLights();

void flickerOn();

void flickerOff();


