#ifdef PROCESS_PIXELS

#include <strings.h>

#include "pixels.h"
#include "errors.h"
#include "mqtt.h"
#include "controller.h"
#include "clock.h"

#include "Colour.h"
#include "Frame.h"
#include "Led.h"
#include "Leds.h"
#include "Sprite.h"
#include "boot.h"

// Some of the colours have been commented out because they don't render well
// on NeoPixels

Adafruit_NeoPixel *strip = NULL;

struct PixelSettings pixelSettings;

Leds *leds;
Frame *frame;

void setDefaultPixelName(void *dest)
{
	strcpy((char *)dest, "");
}

boolean validatePixelName(void *dest, const char *newValueStr)
{
	return (validateString((char *)dest, newValueStr, MAX_PIXEL_NAME_LENGTH));
}

struct SettingItem pixelNameSetting = {
	"Name in pixels text", "pixelname", pixelSettings.pixelName, MAX_PIXEL_NAME_LENGTH, text, setDefaultPixelName, validatePixelName};

void setDefaultPixelControlPinNo(void *dest)
{
	int *destInt = (int *)dest;
	*destInt = 0;
}

struct SettingItem pixelControlPinSetting = {"Pixel Control Pin",
											 "pixelcontrolpin",
											 &pixelSettings.pixelControlPinNo,
											 NUMBER_INPUT_LENGTH,
											 integerValue,
											 setDefaultPixelControlPinNo,
											 validateInt};

void setDefaultNoOfXPixels(void *dest)
{
	int *destInt = (int *)dest;
	*destInt = 12;
}

void setDefaultNoOfYPixels(void *dest)
{
	int *destInt = (int *)dest;
	*destInt = 1;
}

boolean validateNoOfXPixels(void *dest, const char *newValueStr)
{
	int value;

	if (!validateInt(&value, newValueStr))
	{
		return false;
	}

	if (value < 0)
	{
		return false;
	}

	*(int *)dest = value;
	return true;
}

boolean validateNoOfYPixels(void *dest, const char *newValueStr)
{
	int value;

	if (!validateInt(&value, newValueStr))
	{
		return false;
	}

	if (value < 1)
	{
		return false;
	}

	*(int *)dest = value;
	return true;
}

void setDefaultPanelWidth(void *dest)
{
	int *destInt = (int *)dest;
	*destInt = 8;
}

void setDefaultPanelHeight(void *dest)
{
	int *destInt = (int *)dest;
	*destInt = 8;
}

boolean validatePanelWidth(void *dest, const char *newValueStr)
{
	int value;

	if (!validateInt(&value, newValueStr))
	{
		return false;
	}

	if (value < 1)
	{
		return false;
	}

	*(int *)dest = value;
	return true;
}

boolean validatePanelHeight(void *dest, const char *newValueStr)
{
	int value;

	if (!validateInt(&value, newValueStr))
	{
		return false;
	}

	if (value < 1)
	{
		return false;
	}

	*(int *)dest = value;
	return true;
}

struct SettingItem pixelNoOfXPixelsSetting = {"Number of X pixels (0 for pixels not fitted)",
											  "noofxpixels",
											  &pixelSettings.noOfXPixels,
											  NUMBER_INPUT_LENGTH,
											  integerValue,
											  setDefaultNoOfXPixels,
											  validateNoOfXPixels};

struct SettingItem pixelNoOfYPixelsSetting = {"Number of Y pixels (at least 1 row)",
											  "noofypixels",
											  &pixelSettings.noOfYPixels,
											  NUMBER_INPUT_LENGTH,
											  integerValue,
											  setDefaultNoOfYPixels,
											  validateNoOfYPixels};

struct SettingItem pixelPanelWidth = {"Width of a pixel panel",
									  "pixelpanelwidth",
									  &pixelSettings.panelWidth,
									  NUMBER_INPUT_LENGTH,
									  integerValue,
									  setDefaultPanelWidth,
									  validatePanelWidth};

struct SettingItem pixelPanelHeight = {"Height of a pixel panel",
									   "pixelpanelheight",
									   &pixelSettings.panelHeight,
									   NUMBER_INPUT_LENGTH,
									   integerValue,
									   setDefaultPanelHeight,
									   validatePanelHeight};

void setDefaultPixelBrightness(void *dest)
{
	float *destFloat = (float *)dest;
	*destFloat = 1;
}

boolean validatePixelBrightness(void *dest, const char *newValueStr)
{
	float value;

	if (!validateFloat0to1(&value, newValueStr))
	{
		return false;
	}

	*(float *)dest = value;

	if (frame != NULL)
	{
		frame->fadeToBrightness(value, 10);
	}

	return true;
}

struct SettingItem pixelBrightnessSetting = {"Default pixel brightness",
											 "pixelbrightness",
											 &pixelSettings.brightness,
											 NUMBER_INPUT_LENGTH,
											 floatValue,
											 setDefaultPixelBrightness,
											 validatePixelBrightness};

void setDefaultPixelConfig(void *dest)
{
	int *destConfig = (int *)dest;
	*destConfig = 1;
}

boolean validatePixelConfig(void *dest, const char *newValueStr)
{
	int config;

	bool validConfig = validateInt(&config, newValueStr);

	if (!validConfig)
		return false;

	if (config < 1 || config > 4)
		return false;

	int *intDest = (int *)dest;

	*intDest = config;

	return true;
}

struct SettingItem pixelPixelConfig = {"Pixel config(1=ring[NEO_GRB + NEO_KHZ800] 2=strand[NEO_KHZ400 + NEO_RGB],3=single panel[NEO_GRB + NEO_KHZ800],4=multi-panel[NEO_GRB + NEO_KHZ800])",
									   "pixelconfig",
									   &pixelSettings.pixelConfig,
									   NUMBER_INPUT_LENGTH,
									   integerValue,
									   setDefaultPixelConfig,
									   validatePixelConfig};

struct SettingItem *pixelSettingItemPointers[] =
	{
		&pixelControlPinSetting,
		&pixelNoOfXPixelsSetting,
		&pixelNoOfYPixelsSetting,
		&pixelPanelWidth,
		&pixelPanelHeight,
		&pixelPixelConfig,
		&pixelBrightnessSetting,
		&pixelNameSetting};

struct SettingItemCollection pixelSettingItems = {
	"pixel",
	"Pixel hardware and display properties",
	pixelSettingItemPointers,
	sizeof(pixelSettingItemPointers) / sizeof(struct SettingItem *)};

unsigned long millisOfLastPixelUpdate;

int *rasterLookup = NULL;

int noOfPixels;

bool buildDecodeArray()
{
	int noOfPixels = pixelSettings.noOfXPixels * pixelSettings.noOfYPixels;
	int lookupDest;

	switch (pixelSettings.pixelConfig)
	// 1=ring 2=strand,3=single panel,4=multi-panel
	{
	case 1: // ring of pixels
	case 2: // line of pixels
		if (pixelSettings.noOfYPixels != 1)
		{
			displayMessage("Invalid pixel setup: a pixel ring or line must have a y height of 1\n");
			return false;
		}

		rasterLookup = new int[noOfPixels];

		for (int i = 0; i < noOfPixels; i++)
		{
			rasterLookup[i] = i;
		}

		return true;

	case 3: // Single panel
		if (pixelSettings.noOfYPixels == 1)
		{
			displayMessage("Invalid pixel setup: a pixel panel must have a y height of more than 1\n");
			return false;
		}

		rasterLookup = new int[noOfPixels];

		lookupDest = (pixelSettings.noOfYPixels * pixelSettings.noOfXPixels) - 1;

		for (int y = 0; y < pixelSettings.noOfYPixels; y++)
		{
			for (int x = 0; x < pixelSettings.noOfXPixels; x++)
			{
				int rowStart = y * pixelSettings.noOfXPixels;
				int pos;

				if ((y & 1))
				{
					// even row - ascending order
					pos = rowStart + x;
					rasterLookup[lookupDest] = pos;
				}
				else
				{
					// odd row - descending order
					pos = rowStart + (pixelSettings.noOfXPixels - x - 1);
				}
				rasterLookup[lookupDest] = pos;
				lookupDest--;
			}
		}

		return true;
		break;

	case 4: // Multiple panels
		if (pixelSettings.noOfYPixels < pixelSettings.panelHeight || pixelSettings.noOfXPixels < pixelSettings.panelWidth)
		{
			displayMessage("Invalid pixel setup: in a multiple panel configuration sizes must be larger than panel dimensions\n");
			return false;
		}

		int xPanels = int(pixelSettings.noOfXPixels / pixelSettings.panelWidth);
		int yPanels = int(pixelSettings.noOfYPixels / pixelSettings.panelHeight);

		int panelRowPixels = (pixelSettings.panelWidth * pixelSettings.panelHeight) * xPanels;
		int topLeft = panelRowPixels * (yPanels - 1) + pixelSettings.panelHeight - 1;

		rasterLookup = new int[noOfPixels];

		int pos = 0;

		for (int y = 0; y < pixelSettings.noOfYPixels; y++)
		{
			for (int x = 0; x < pixelSettings.noOfXPixels; x++)
			{
				int rowNo = y / pixelSettings.panelHeight;
				int p = topLeft - (rowNo * panelRowPixels);
				int py = y - (rowNo * pixelSettings.panelHeight);
				p = p + (pixelSettings.panelWidth * x) - y;
				rasterLookup[pos] = p;
				pos++;
			}
		}
		return true;
	}
	return false;
}

bool startPixelStrip()
{
	if (!buildDecodeArray())
	{
		displayMessage("********* Pixel dimensions invalid\n");
		return false;
	}

	int noOfPixels = pixelSettings.noOfXPixels * pixelSettings.noOfYPixels;

	switch (pixelSettings.pixelConfig)
	{
	// 1=ring 2=strand,3=single panel,4=multi-panel
	case 1:
		// ring using standard
	case 3:
		// panel
	case 4:
		// multi-panel
		strip = new Adafruit_NeoPixel(noOfPixels, pixelSettings.pixelControlPinNo,
									  NEO_GRB + NEO_KHZ800);

		break;
	case 2:
		// strand
		strip = new Adafruit_NeoPixel(noOfPixels, pixelSettings.pixelControlPinNo,
									  NEO_KHZ400 + NEO_RGB);
		break;
	default:
		strip = NULL;
	}

	if (strip == NULL)
	{
		displayMessage("********* Pixel config invalid");
		return false;
	}

	strip->begin();

	return true;
}

/// commands

boolean validatePixelCommandString(void *dest, const char *newValueStr)
{
	return (validateString((char *)dest, newValueStr, PIXEL_COMMAND_NAME_LENGTH));
}

// The first element of a process command block is always a floating point value called "value"
// This will be loaded with a normalised value to be used by the process when it runs

#define FLOAT_VALUE_OFFSET 0
#define RED_PIXEL_COMMAND_OFFSET (FLOAT_VALUE_OFFSET + sizeof(float))
#define BLUE_PIXEL_COMMAND_OFFSET (RED_PIXEL_COMMAND_OFFSET + sizeof(float))
#define GREEN_PIXEL_COMMAND_OFFSET (BLUE_PIXEL_COMMAND_OFFSET + sizeof(float))
#define SPEED_PIXEL_COMMAND_OFFSET (GREEN_PIXEL_COMMAND_OFFSET + sizeof(float))
#define COMMAND_PIXEL_COMMAND_OFFSET (SPEED_PIXEL_COMMAND_OFFSET + sizeof(int))
#define COLOURNAME_PIXEL_COMMAND_OFFSET (COMMAND_PIXEL_COMMAND_OFFSET + PIXEL_COMMAND_NAME_LENGTH)
#define COMMAND_PIXEL_OPTION_OFFSET (COLOURNAME_PIXEL_COMMAND_OFFSET + PIXEL_COMMAND_NAME_LENGTH)
#define COMMAND_PIXEL_PATTERN_OFFSET (COMMAND_PIXEL_OPTION_OFFSET + PIXEL_COMMAND_NAME_LENGTH)
#define COMMAND_PIXEL_TIMEOUT_OFFSET (COMMAND_PIXEL_PATTERN_OFFSET + sizeof(int))

struct CommandItem redCommandItem = {
	"red",
	"amount of red (0-1)",
	RED_PIXEL_COMMAND_OFFSET,
	floatCommand,
	validateFloat0to1,
	noDefaultAvailable};

struct CommandItem blueCommandItem = {
	"blue",
	"amount of blue (0-1)",
	BLUE_PIXEL_COMMAND_OFFSET,
	floatCommand,
	validateFloat0to1,
	noDefaultAvailable};

struct CommandItem greenCommandItem = {
	"green",
	"amount of green (0-1)",
	GREEN_PIXEL_COMMAND_OFFSET,
	floatCommand,
	validateFloat0to1,
	noDefaultAvailable};

boolean setDefaultPixelTimeout(void *dest)
{
	int *destInt = (int *)dest;
	*destInt = 0;
	return true;
}

boolean validatePixelTimeout(void *dest, const char *newValueStr)
{
	int value;

	if (!validateInt(&value, newValueStr))
	{
		return false;
	}

	if (value < 0 || value > 300)
	{
		return false;
	}

	*(int *)dest = value;
	return true;
}

struct CommandItem pixelTimeoutCommandItem = {
	"timeout",
	"command timeout (0-300 minutes)",
	COMMAND_PIXEL_TIMEOUT_OFFSET,
	integerCommand,
	validatePixelTimeout,
	setDefaultPixelTimeout};

boolean setDefaultPixelChangeSteps(void *dest)
{
	int *destInt = (int *)dest;
	*destInt = 20;
	return true;
}

struct CommandItem pixelChangeStepsCommandItem = {
	"steps",
	"no of 50Hz steps to complete the change",
	SPEED_PIXEL_COMMAND_OFFSET,
	integerCommand,
	validateInt,
	setDefaultPixelChangeSteps};

struct CommandItem pixelCommandName = {
	"pixelCommand",
	"pixel settting command",
	COMMAND_PIXEL_COMMAND_OFFSET,
	textCommand,
	validatePixelCommandString,
	noDefaultAvailable};

struct CommandItem colourCommandName = {
	"colourname",
	"name of the colour to set",
	COLOURNAME_PIXEL_COMMAND_OFFSET,
	textCommand,
	validatePixelCommandString,
	noDefaultAvailable};

struct CommandItem colourCommandMask = {
	"colourmask",
	"mask of colour characters",
	COLOURNAME_PIXEL_COMMAND_OFFSET,
	textCommand,
	validatePixelCommandString,
	noDefaultAvailable};

struct CommandItem colourCommandOptionItem = {
	"options",
	"options for the colour command(timed)",
	COMMAND_PIXEL_OPTION_OFFSET,
	textCommand,
	validatePixelCommandString,
	setDefaultEmptyString};

struct CommandItem floatValueItem = {
	"value",
	"value (0-1)",
	FLOAT_VALUE_OFFSET,
	floatCommand,
	validateFloat0to1,
	noDefaultAvailable};

char *pixelDisplaySelections[] = {"walking", "mask"};

boolean validateSelectionCommandString(void *dest, const char *newValueStr)
{
	char buffer[PIXEL_COMMAND_NAME_LENGTH];

	bool commandOK = validateString(buffer, newValueStr, PIXEL_COMMAND_NAME_LENGTH);

	if (!commandOK)
		return false;

	commandOK = false;

	for (unsigned int i = 0; i < sizeof(pixelDisplaySelections) / sizeof(char *); i++)
	{
		if (strcasecmp(buffer, pixelDisplaySelections[i]) == 0)
		{
			commandOK = true;
		}
	}

	if (commandOK)
	{
		strcpy((char *)dest, buffer);
	}

	return commandOK;
}

struct CommandItem pixelPatternSelectionItem = {
	"pattern",
	"chosen pattern (walking,mask)",
	COMMAND_PIXEL_PATTERN_OFFSET,
	textCommand,
	validateSelectionCommandString,
	noDefaultAvailable};

struct CommandItem *setColourItems[] =
	{
		&redCommandItem,
		&blueCommandItem,
		&greenCommandItem,
		&pixelChangeStepsCommandItem,
		&pixelTimeoutCommandItem};

int doSetPixelColor(char *destination, unsigned char *settingBase);

struct Command setPixelColourCommand{
	"setcolour",
	"Sets the colour of the pixels",
	setColourItems,
	sizeof(setColourItems) / sizeof(struct CommandItem *),
	doSetPixelColor};

int doSetPixelColor(char *destination, unsigned char *settingBase)
{
	if (*destination != 0)
	{
		// we have a destination for the command. Build the string
		char buffer[JSON_BUFFER_SIZE];
		createJSONfromSettings("pixels", &setPixelColourCommand, destination, settingBase, buffer, JSON_BUFFER_SIZE);
		return publishCommandToRemoteDevice(buffer, destination);
	}

	if (pixelProcess.status != PIXEL_OK)
	{
		return JSON_MESSAGE_PIXELS_NOT_ENABLED;
	}

	// sets the pixel colour - caller has set the r,g and b values

	float red = (float)getUnalignedFloat(settingBase + RED_PIXEL_COMMAND_OFFSET);
	float blue = (float)getUnalignedFloat(settingBase + BLUE_PIXEL_COMMAND_OFFSET);
	float green = (float)getUnalignedFloat(settingBase + GREEN_PIXEL_COMMAND_OFFSET);
	int steps = (int)getUnalignedInt(settingBase + SPEED_PIXEL_COMMAND_OFFSET);
	int time = (int)getUnalignedInt(settingBase + COMMAND_PIXEL_TIMEOUT_OFFSET);

	if (time == 0)
	{
		frame->fadeToColour({red, green, blue}, steps);
	}
	else
	{
		frame->overlayColour({red, green, blue}, time);
	}

	return WORKED_OK;
}

struct CommandItem *setBackgroundColourItems[] =
	{
		&redCommandItem,
		&blueCommandItem,
		&greenCommandItem,
		&pixelChangeStepsCommandItem};

int doSetBackColor(char *destination, unsigned char *settingBase);

struct Command setBackColourCommand{
	"setbackground",
	"Sets the colour of the background",
	setBackgroundColourItems,
	sizeof(setBackgroundColourItems) / sizeof(struct CommandItem *),
	doSetBackColor};

int doSetBackColor(char *destination, unsigned char *settingBase)
{
	if (*destination != 0)
	{
		// we have a destination for the command. Build the string
		char buffer[JSON_BUFFER_SIZE];
		createJSONfromSettings("pixels", &setBackColourCommand, destination, settingBase, buffer, JSON_BUFFER_SIZE);
		return publishCommandToRemoteDevice(buffer, destination);
	}

	if (pixelProcess.status != PIXEL_OK)
	{
		return JSON_MESSAGE_PIXELS_NOT_ENABLED;
	}

	// sets the pixel colour - caller has set the r,g and b values

	float red = (float)getUnalignedFloat(settingBase + RED_PIXEL_COMMAND_OFFSET);
	float blue = (float)getUnalignedFloat(settingBase + BLUE_PIXEL_COMMAND_OFFSET);
	float green = (float)getUnalignedFloat(settingBase + GREEN_PIXEL_COMMAND_OFFSET);
	int steps = (int)getUnalignedInt(settingBase + SPEED_PIXEL_COMMAND_OFFSET);

	frame->fadeBackToColour({red, green, blue}, steps);
	return WORKED_OK;
}

struct CommandItem *setNamedPixelColourItems[] =
	{
		&colourCommandName,
		&pixelChangeStepsCommandItem,
		&pixelTimeoutCommandItem};

int doSetNamedColour(char *destination, unsigned char *settingBase);

struct Command setPixelsToNamedColour{
	"setnamedcolour",
	"Sets the pixels to a named colour",
	setNamedPixelColourItems,
	sizeof(setNamedPixelColourItems) / sizeof(struct CommandItem *),
	doSetNamedColour};

int doSetNamedColour(char *destination, unsigned char *settingBase)
{
	if (*destination != 0)
	{
		// we have a destination for the command. Build the string
		char buffer[JSON_BUFFER_SIZE];
		createJSONfromSettings("pixels", &setPixelsToNamedColour, destination, settingBase, buffer, JSON_BUFFER_SIZE);
		return publishCommandToRemoteDevice(buffer, destination);
	}

	if (pixelProcess.status != PIXEL_OK)
	{
		return JSON_MESSAGE_PIXELS_NOT_ENABLED;
	}

	struct colourNameLookup *col;

	char *colourName = (char *)(settingBase + COLOURNAME_PIXEL_COMMAND_OFFSET);

	int steps = getUnalignedInt(settingBase + SPEED_PIXEL_COMMAND_OFFSET);

	col = findColourByName(colourName);

	if (col != NULL)
	{
		int time = (int)getUnalignedInt(settingBase + COMMAND_PIXEL_TIMEOUT_OFFSET);

		if (time == 0)
		{
			frame->fadeToColour(col->col, steps);
		}
		else
		{
			frame->overlayColour(col->col, time);
		}
		return WORKED_OK;
	}
	else
	{
		return JSON_MESSAGE_INVALID_COLOUR_NAME;
	}
}

struct CommandItem *setNamedBackgroundColourItems[] =
	{
		&colourCommandName,
		&pixelChangeStepsCommandItem};

int doSetNamedBackgroundColour(char *destination, unsigned char *settingBase);

struct Command setBackgroundToNamedColour{
	"setnamedbackground",
	"Sets the background to a named colour",
	setNamedBackgroundColourItems,
	sizeof(setNamedBackgroundColourItems) / sizeof(struct CommandItem *),
	doSetNamedBackgroundColour};

int doSetNamedBackgroundColour(char *destination, unsigned char *settingBase)
{
	if (*destination != 0)
	{
		// we have a destination for the command. Build the string
		char buffer[JSON_BUFFER_SIZE];
		createJSONfromSettings("pixels", &setBackgroundToNamedColour, destination, settingBase, buffer, JSON_BUFFER_SIZE);
		return publishCommandToRemoteDevice(buffer, destination);
	}

	if (pixelProcess.status != PIXEL_OK)
	{
		return JSON_MESSAGE_PIXELS_NOT_ENABLED;
	}

	struct colourNameLookup *col;

	char *colourName = (char *)(settingBase + COLOURNAME_PIXEL_COMMAND_OFFSET);

	int steps = getUnalignedInt(settingBase + SPEED_PIXEL_COMMAND_OFFSET);

	col = findColourByName(colourName);

	if (col != NULL)
	{
		frame->fadeBackToColour(col->col, steps);
		return WORKED_OK;
	}
	else
	{
		return JSON_MESSAGE_INVALID_COLOUR_NAME;
	}
}

struct CommandItem *setRandomPixelColourItems[] =
	{
		&pixelChangeStepsCommandItem,
		&colourCommandOptionItem};

int doSetRandomColour(char *destination, unsigned char *settingBase);

struct Command setPixelsToRandomColour{
	"setrandomcolour",
	"Sets all the pixels to a random colour",
	setRandomPixelColourItems,
	sizeof(setRandomPixelColourItems) / sizeof(struct CommandItem *),
	doSetRandomColour};

void seedRandomFromClock()
{
	if (clockSensor.status != SENSOR_OK)
	{
		// no clock - just pick a random colour
		return;
	}

	// get a seed for the random number generator
	struct clockReading *clockActiveReading;

	clockActiveReading =
		(struct clockReading *)clockSensor.activeReading;

	unsigned long seedValue = (clockActiveReading->hour * 60) + clockActiveReading->minute;

	// seed the random number generator with this value - it will only be for one number
	// but it shold ensure that we get a random range each time

	localSrand(seedValue);
}

int doSetRandomColour(char *destination, unsigned char *settingBase)
{
	if (*destination != 0)
	{
		// we have a destination for the command. Build the string
		char buffer[JSON_BUFFER_SIZE];
		createJSONfromSettings("pixels", &setPixelsToRandomColour, destination, settingBase, buffer, JSON_BUFFER_SIZE);
		return publishCommandToRemoteDevice(buffer, destination);
	}

	if (pixelProcess.status != PIXEL_OK)
	{
		return JSON_MESSAGE_PIXELS_NOT_ENABLED;
	}

	char *option = (char *)(settingBase + COMMAND_PIXEL_OPTION_OFFSET);

	if (strcasecmp(option, "timed") == 0)
	{
		seedRandomFromClock();
	}

	struct colourNameLookup *randomColour = findRandomColour();

	int steps = getUnalignedInt(settingBase + SPEED_PIXEL_COMMAND_OFFSET);

	frame->fadeToColour(randomColour->col, steps);

	return WORKED_OK;
}

struct CommandItem *setPixelTwinkleItems[] =
	{
		&pixelChangeStepsCommandItem,
		&colourCommandOptionItem};

int doSetTwinkle(char *destination, unsigned char *settingBase);

struct Command setPixelsToTwinkle{
	"twinkle",
	"Sets the pixels to twinkle random colours",
	setPixelTwinkleItems,
	sizeof(setPixelTwinkleItems) / sizeof(struct CommandItem *),
	doSetTwinkle};

int doSetTwinkle(char *destination, unsigned char *settingBase)
{
	if (*destination != 0)
	{
		// we have a destination for the command. Build the string
		char buffer[JSON_BUFFER_SIZE];
		createJSONfromSettings("pixels", &setPixelsToTwinkle, destination, settingBase, buffer, JSON_BUFFER_SIZE);
		return publishCommandToRemoteDevice(buffer, destination);
	}

	if (pixelProcess.status != PIXEL_OK)
	{
		return JSON_MESSAGE_PIXELS_NOT_ENABLED;
	}

	char *option = (char *)(settingBase + COMMAND_PIXEL_OPTION_OFFSET);

	if (strcasecmp(option, "timed") == 0)
	{
		seedRandomFromClock();
	}

	int steps = getUnalignedInt(settingBase + SPEED_PIXEL_COMMAND_OFFSET);

	frame->fadeSpritesToTwinkle(steps);

	return WORKED_OK;
}

struct CommandItem *setPixelBrightnessItems[] =
	{
		&floatValueItem,
		&pixelChangeStepsCommandItem};

int doSetBrightness(char *destination, unsigned char *settingBase);

struct Command setPixelBrightness{
	"brightness",
	"Sets the pixel brightness",
	setPixelBrightnessItems,
	sizeof(setPixelBrightnessItems) / sizeof(struct CommandItem *),
	doSetBrightness};

int doSetBrightness(char *destination, unsigned char *settingBase)
{
	if (*destination != 0)
	{
		// we have a destination for the command. Build the string
		char buffer[JSON_BUFFER_SIZE];
		createJSONfromSettings("pixels", &setPixelBrightness, destination, settingBase, buffer, JSON_BUFFER_SIZE);
		return publishCommandToRemoteDevice(buffer, destination);
	}

	if (pixelProcess.status != PIXEL_OK)
	{
		return JSON_MESSAGE_PIXELS_NOT_ENABLED;
	}

	float brightness = getUnalignedFloat(settingBase + FLOAT_VALUE_OFFSET);

	int steps = getUnalignedInt(settingBase + SPEED_PIXEL_COMMAND_OFFSET);

	frame->fadeToBrightness(brightness, steps);

	return WORKED_OK;
}

struct CommandItem *setPixelPatternItems[] =
	{
		&pixelPatternSelectionItem,
		&pixelChangeStepsCommandItem,
		&colourCommandMask};

int doSetPattern(char *destination, unsigned char *settingBase);

struct Command setPixelPattern{
	"pattern",
	"Sets the pixel pattern",
	setPixelPatternItems,
	sizeof(setPixelPatternItems) / sizeof(struct CommandItem *),
	doSetPattern};

int doSetPattern(char *destination, unsigned char *settingBase)
{
	if (*destination != 0)
	{
		// we have a destination for the command. Build the string
		char buffer[JSON_BUFFER_SIZE];
		createJSONfromSettings("pixels", &setPixelPattern, destination, settingBase, buffer, JSON_BUFFER_SIZE);
		return publishCommandToRemoteDevice(buffer, destination);
	}

	if (pixelProcess.status != PIXEL_OK)
	{
		return JSON_MESSAGE_PIXELS_NOT_ENABLED;
	}

	char *colourMask = (char *)(settingBase + COLOURNAME_PIXEL_COMMAND_OFFSET);
	char *pattern = (char *)(settingBase + COMMAND_PIXEL_PATTERN_OFFSET);
	int steps = getUnalignedInt(settingBase + SPEED_PIXEL_COMMAND_OFFSET);

	TRACELOG("Got new pattern:");
	TRACELOG(pattern);

	if (strcasecmp(pattern, "walking") == 0)
	{
		frame->fadeSpritesToWalkingColours(colourMask, steps);
	}

	if (strcasecmp(pattern, "mask") == 0)
	{
		frame->fadeSpritesToColourCharMask(colourMask, steps);
	}
	return WORKED_OK;
}

struct CommandItem *setPixelValueInColourMapItems[] =
	{
		&floatValueItem,
		&colourCommandOptionItem,
		&pixelChangeStepsCommandItem,
		&colourCommandMask};

int doPixelMapValue(char *destination, unsigned char *settingBase);

struct Command setPixelValueInColourMap{
	"map",
	"Maps a value into a colour map",
	setPixelValueInColourMapItems,
	sizeof(setPixelValueInColourMapItems) / sizeof(struct CommandItem *),
	doPixelMapValue};

int doPixelMapValue(char *destination, unsigned char *settingBase)
{
	TRACELOGLN("Mapping a pixel value to a mask");

	if (*destination != 0)
	{
		// we have a destination for the command. Build the string
		char buffer[JSON_BUFFER_SIZE];
		createJSONfromSettings("pixels", &setPixelValueInColourMap, destination, settingBase, buffer, JSON_BUFFER_SIZE);
		return publishCommandToRemoteDevice(buffer, destination);
	}

	if (pixelProcess.status != PIXEL_OK)
	{
		return JSON_MESSAGE_PIXELS_NOT_ENABLED;
	}

	float value = getUnalignedFloat(settingBase + FLOAT_VALUE_OFFSET);

	TRACELOG("Value:");
	TRACELOG(value);

	char *colourMask = (char *)(settingBase + COLOURNAME_PIXEL_COMMAND_OFFSET);

	TRACELOG(" Mask:");
	TRACELOG(colourMask);

	int steps = getUnalignedInt(settingBase + SPEED_PIXEL_COMMAND_OFFSET);
	TRACELOG(" Steps:");
	TRACELOG(steps);

	char *option = (char *)(settingBase + COMMAND_PIXEL_OPTION_OFFSET);
	TRACELOG(" Option:");
	TRACELOG(option);

	int maskLength = strlen(colourMask);
	TRACELOG(" Mask:");
	TRACELOG(colourMask);

	if (value < 0)
		value = 0;
	if (value > 1)
		value = 1;

	struct Colour colour;

	if (value == 0)
	{
		colour = findColourByChar(colourMask[0])->col;
	}
	else
	{
		if (value == 1)
		{
			colour = findColourByChar(colourMask[maskLength - 1])->col;
		}
		else
		{
			float colourPos = (maskLength - 1) * value;
			TRACELOG(" ColourPos");
			TRACELOG(colourPos);

			if (strcasecmp(option, "mix") == 0)
			{
				int lowCol = (int)colourPos;
				int highCol = lowCol + 1;
				TRACELOG(" LowCol:");
				TRACELOG(lowCol);
				TRACELOG(" HighCol:");
				TRACELOG(highCol);
				getColourInbetweenMask(colourMask[lowCol], colourMask[highCol], colourPos - lowCol, &colour);
			}
			else
			{
				int intPos = int(colourPos + 0.5);
				TRACELOG(" IntPos:");
				TRACELOG(intPos);
				colour = findColourByChar(colourMask[intPos])->col;
			}
		}
	}
	TRACELOGLN();

	frame->fadeToColour(colour, steps);
	return WORKED_OK;
}

struct Command *pixelCommandList[] = {
	&setPixelColourCommand,
	&setBackColourCommand,
	&setPixelsToNamedColour,
	&setBackgroundToNamedColour,
	&setPixelsToRandomColour,
	&setPixelsToTwinkle,
	&setPixelBrightness,
	&setPixelPattern,
	&setPixelValueInColourMap};

struct CommandItemCollection pixelCommands =
	{
		"Control the pixels on the device",
		pixelCommandList,
		sizeof(pixelCommandList) / sizeof(struct Command *)};

//////////////////////////////////////////////////////////////////////////
//// BUSY PIXEL
//////////////////////////////////////////////////////////////////////////

bool busyPixelActive = false;
byte busyPixelPos = 0;
byte busyRed, busyGreen, busyBlue;

void updateBusyPixel()
{
	if (!busyPixelActive)
	{
		return;
	}

	busyPixelPos++;

	if (busyPixelPos == noOfPixels)
		busyPixelPos = 0;
}

void setBusyPixelColour(byte red, byte green, byte blue)
{
	busyRed = red;
	busyBlue = blue;
	busyGreen = green;
}

void startBusyPixel(byte red, byte green, byte blue)
{
	busyPixelActive = true;
	setBusyPixelColour(red, green, blue);
	busyPixelPos = 0;
}

void stopBusyPixel()
{
	busyPixelActive = false;
}

void renderBusyPixel()
{
	if (!busyPixelActive)
	{
		return;
	}
	strip->setPixelColor(rasterLookup[busyPixelPos], busyRed, busyGreen, busyBlue);
}

// Called by the the leds in the Frame to draw the display
// We draw the busy pixel on top

void show()
{
	renderBusyPixel();
	strip->show();
}

void setPixel(int no, float r, float g, float b)
{
	if (rasterLookup == NULL)
	{
		return;
	}
	unsigned char rs = (unsigned char)round(r * 255);
	unsigned char gs = (unsigned char)round(g * 255);
	unsigned char bs = (unsigned char)round(b * 255);
	if (rs > 255 || gs > 255 || bs > 255)
		displayMessage("setPixel no:%d raster:%d r:%f g:%f b:%f\n", no, rasterLookup[no], r, g, b);
	strip->setPixelColor(rasterLookup[no], rs, gs, bs);
}

void setAllLightsOff()
{
	frame->fadeSpritesToWalkingColours("K", 10);
}

void initPixel()
{
	pixelProcess.status = PIXEL_OFF;

	noOfPixels = pixelSettings.noOfXPixels * pixelSettings.noOfYPixels;

	if (noOfPixels == 0)
	{
		pixelProcess.status = PIXEL_NO_PIXELS;
		return;
	}

	if(!startPixelStrip()){
		pixelProcess.status = PIXEL_NO_PIXELS;
	}
}

void startPixel()
{
	if(pixelProcess.status == PIXEL_NO_PIXELS){
		return;
	}

	leds = new Leds(pixelSettings.noOfXPixels, pixelSettings.noOfYPixels, show, setPixel);

	frame = new Frame(leds, BLACK_COLOUR);
	frame->fadeUp(1000);

	millisOfLastPixelUpdate = millis();
	pixelProcess.status = PIXEL_OK;

	frame->fadeSpritesToWalkingColours("RGBYMC", 10);
	frame->fadeToBrightness(pixelSettings.brightness, 10);
}

void flickeringColouredLights(byte r, byte g, byte b, int steps)
{
	frame->fadeToColour({(float)r / 256, (float)g / 256, (float)b / 256}, steps);
}

void setFlickerUpdateSpeed(int speed)
{
	frame->setSpriteSpeed(speed / 10);
}

void transitionToColor(byte speed, byte r, byte g, byte b)
{
}

void setLightColor(byte r, byte g, byte b)
{
}

void randomiseLights()
{
	frame->fadeSpritesToTwinkle(10);
}

void flickerOn()
{
}

void flickerOff()
{
}

void showDeviceStatus();	   // declared in control.h
boolean getInputSwitchValue(); // declared in inputswitch.h

void updateFrame()
{
	unsigned long currentMillis = millis();
	unsigned long millisSinceLastUpdate = ulongDiff(currentMillis, millisOfLastPixelUpdate);

	if (millisSinceLastUpdate >= MILLIS_BETWEEN_UPDATES)
	{
		frame->update();
		frame->render();
		millisOfLastPixelUpdate = currentMillis;
	}
}

void updatePixel()
{
	switch (pixelProcess.status)
	{
	case PIXEL_NO_PIXELS:
		return;
	case PIXEL_OK:
		updateFrame();
		break;
	case PIXEL_OFF:
		return;
	default:
		break;
	}
}

void stopPixel()
{
	pixelProcess.status = PIXEL_OFF;
}

bool pixelStatusOK()
{
	return pixelProcess.status == PIXEL_OK;
}

void pixelStatusMessage(char *buffer, int bufferLength)
{
	switch (pixelProcess.status)
	{
	case PIXEL_NO_PIXELS:
		snprintf(buffer, bufferLength, "No pixels connected");
		break;
	case PIXEL_OK:
		snprintf(buffer, bufferLength, "PIXEL OK");
		break;
	case PIXEL_OFF:
		snprintf(buffer, bufferLength, "PIXEL OFF");
		break;
	default:
		snprintf(buffer, bufferLength, "Pixel status invalid");
		break;
	}
}

struct process pixelProcess = {
	"pixels",
	initPixel,
	startPixel,
	updatePixel,
	stopPixel,
	pixelStatusOK,
	pixelStatusMessage,
	false,
	0,
	0,
	0,
	NULL,
	(unsigned char *)&pixelSettings, sizeof(PixelSettings), &pixelSettingItems,
	&pixelCommands,
	BOOT_PROCESS + ACTIVE_PROCESS + CONFIG_PROCESS + WIFI_CONFIG_PROCESS,
	NULL,
	NULL,
	NULL};

#endif