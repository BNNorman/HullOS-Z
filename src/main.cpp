#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "LittleFS.h"

#include "debug.h"
#include "utils.h"

#if defined(PROCESS_PIXELS)
#include "pixels.h"
#endif

#include "settings.h"
#include "processes.h"
#include "sensors.h"
#include "pirSensor.h"
#include "buttonsensor.h"
#include "inputswitch.h"
#include "controller.h"
#include "statusled.h"
#include "messages.h"
#include "console.h"
#include "connectwifi.h"
#include "mqtt.h"
#include "servoproc.h"
#include "clock.h"
#include "registration.h"
#include "rotarySensor.h"
#include "potSensor.h"
#include "settingsWebServer.h"
#include "MAX7219Messages.h"
#include "printer.h"
#include "BME280Sensor.h"
#include "HullOS.h"
#include "boot.h"
#include "otaupdate.h"
#include "outpin.h"
#include "RFID.h"
#include "Motors.h"
#include "distance.h"
#include "codeEditorProcess.h"

void setup1()
{
}

void loop1()
{
  updateMotorsCore1();
}

// This function will be different for each build of the device.

void populateProcessList()
{
#if defined(PROCESS_CONSOLE)
  addProcessToAllProcessList(&consoleProcessDescriptor);
#endif
#if defined(PROCESS_CONTROLLER)
  addProcessToAllProcessList(&controllerProcess);
#endif
#ifdef PROCESS_HULLOS
  addProcessToAllProcessList(&hullosProcess);
#endif
#if defined(PROCESS_INPUT_SWITCH)
  addProcessToAllProcessList(&inputSwitchProcess);
#endif
#ifdef PROCESS_MAX7219
  addProcessToAllProcessList(&max7219MessagesProcess);
#endif
#if defined(PROCESS_MESSAGES)
  addProcessToAllProcessList(&messagesProcess);
#endif
#ifdef PROCESS_OUTPIN
  addProcessToAllProcessList(&outPinProcess);
#endif
#if defined(PROCESS_MQTT)
  addProcessToAllProcessList(&MQTTProcessDescriptor);
#endif
#if defined(PROCESS_PIXELS)
  addProcessToAllProcessList(&pixelProcess);
#endif
#ifdef PROCESS_PRINTER
  addProcessToAllProcessList(&printerProcess);
#endif
#if defined(PROCESS_REGISTRATION)
  addProcessToAllProcessList(&RegistrationProcess);
#endif
#ifdef PROCESS_SERVO
  addProcessToAllProcessList(&ServoProcess);
#endif
#if defined(PROCESS_STATUS_LED)
  addProcessToAllProcessList(&statusLedProcess);
#endif
#if defined(PROCESS_WIFI)
  addProcessToAllProcessList(&WiFiProcessDescriptor);
#endif
#if defined(PROCESS_MOTOR)
  addProcessToAllProcessList(&motorProcessDescriptor);
#endif
#if defined(PROCESS_CODE_EDITOR)
  addProcessToAllProcessList(&codeEditorProcess);
#endif
}

void populateSensorList()
{
#ifdef SENSOR_BME280
  addSensorToAllSensorsList(&bme280Sensor);
  addSensorToActiveSensorsList(&bme280Sensor);
#endif
#ifdef SENSOR_BUTTON
  addSensorToAllSensorsList(&buttonSensor);
  addSensorToActiveSensorsList(&buttonSensor);
#endif
#ifdef SENSOR_CLOCK
  addSensorToAllSensorsList(&clockSensor);
  addSensorToActiveSensorsList(&clockSensor);
#endif
#ifdef SENSOR_POT
  addSensorToAllSensorsList(&potSensor);
  addSensorToActiveSensorsList(&potSensor);
#endif
#ifdef SENSOR_PIR
  addSensorToAllSensorsList(&pirSensor);
  addSensorToActiveSensorsList(&pirSensor);
#endif
#ifdef SENSOR_RFID
  addSensorToAllSensorsList(&RFIDSensor);
  addSensorToActiveSensorsList(&RFIDSensor);
#endif
#ifdef SENSOR_ROTARY
  addSensorToAllSensorsList(&rotarySensor);
  addSensorToActiveSensorsList(&rotarySensor);
#endif
#ifdef SENSOR_DISTANCE
  addSensorToAllSensorsList(&Distance);
  addSensorToActiveSensorsList(&Distance);
#endif
}

void displayControlMessage(int messageNumber, ledFlashBehaviour severity, char *messageText)
{
  char buffer[20];

  ledFlashBehaviourToString(severity, buffer, 20);

  displayMessage("%s: %d %s\n", buffer, messageNumber, messageText);
}

unsigned long heapPrintTime = 0;

void startDevice()
{

#if defined(PICO_USE_UART)
  Serial1.begin(115200);
  delay(100);
  while (1)
  {
    Serial1.print(".");
    delay(200);
  }
#else
  Serial.begin(115200);
#endif

  for (int i = 0; i < 10; i++)
  {
    delay(1000);
    Serial.println(10 - i);
    if(Serial.available()){
      break;
    }
  }

  alwaysDisplayMessage("\n\n\n\n");

  char deviceNameBuffer[DEVICE_NAME_LENGTH];
  PrintSystemDetails(deviceNameBuffer, DEVICE_NAME_LENGTH);
  alwaysDisplayMessage("%s\n", deviceNameBuffer);
  alwaysDisplayMessage("Connected Little Boxes Device\n");
  alwaysDisplayMessage("Powered by HULLOS-X\n");
  alwaysDisplayMessage("www.connectedlittleboxes.com\n");
  alwaysDisplayMessage("Version %s build date: %s %s\n", Version, __DATE__, __TIME__);

#ifdef DEBUG
  messageLogf("**** Debug output enabled");
#endif

  START_MEMORY_MONITOR();

  populateProcessList();

  DISPLAY_MEMORY_MONITOR("Populate process list");

  populateSensorList();

  DISPLAY_MEMORY_MONITOR("Populate sensor list");

  SettingsSetupStatus status = setupSettings();

  switch (status)
  {
  case SETTINGS_SETUP_OK:
    displayMessage("Settings loaded OK\n");
    break;
  case SETTINGS_RESET_TO_DEFAULTS:
    displayMessage("Settings reset to defaults\n");
    break;
  case SETTINGS_FILE_SYSTEM_FAIL:
    displayMessage("Settings file system fail\n");
    break;
  default:
    displayMessage("Invalid setupSettings return\n");
  }

  // get the boot mode
  getBootMode();

  if (bootMode == COLD_BOOT_MODE)
  {

#if defined(SETTINGS_WEB_SERVER)
    // first power up
    if (needWifiConfigBootMode())
    {
      // start hosting the config website and request a timeout
      startHostingConfigWebsite(true);
    }
  }
  else
  {

    if (bootMode == CONFIG_HOST_BOOT_NO_TIMEOUT_MODE)
    {
      startHostingConfigWebsite(false);
    }

    if (bootMode == CONFIG_HOST_TIMEOUT_BOOT_MODE)
    {
      startHostingConfigWebsite(true);
    }
#endif

#if defined(WEMOSD1MINI) || defined(ESP32DOIT)
    if (bootMode == OTA_UPDATE_BOOT_MODE)
    {
      performOTAUpdate();
    }
#endif
  }

  startstatusLedFlash(1000);

  initialiseAllProcesses();

  DISPLAY_MEMORY_MONITOR("Initialise all processes\n");
  
  buildActiveProcessListFromMask(BOOT_PROCESS);

  startProcesses();

  DISPLAY_MEMORY_MONITOR("Initialise all processes\n");

  bindMessageHandler(displayControlMessage);

  startSensors();

  DISPLAY_MEMORY_MONITOR("Start all sensors\n");

  delay(1000); // show the status for a while
  if (messagesSettings.messagesEnabled)
  {
    displayMessage("Start complete\n\nType help and press enter for help\n\n");
  }
}

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
uint32_t oldHeap = 0;

#define HEAP_PRINT_INTERVAL 500

void heapMonitor()
{
  unsigned long m = millis();
  if ((m - heapPrintTime) > HEAP_PRINT_INTERVAL)
  {
    uint32_t newHeap = ESP.getFreeHeap();
    if (newHeap != oldHeap)
    {
      Serial.print("Heap: ");
      displayMessage("%lu", newHeap);
      oldHeap = newHeap;
    }
    heapPrintTime = millis();
  }
}
#endif

void setup()
{
  startDevice();
}

void loop()
{
  updateSensors();
  updateProcesses();
  delay(5);
  //  DISPLAY_MEMORY_MONITOR("System");
}