#include <Arduino.h>
#include "control.h"
#include <AccelStepper.h>
#include <EEPROMex.h>
#include <HX711.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);
byte enterChar[] = {
	B10000,
	B10000,
	B10100,
	B10110,
	B11111,
	B00110,
	B00100,
	B00000};

byte fastChar[] = {
	B00110,
	B01110,
	B00110,
	B00110,
	B01111,
	B00000,
	B00100,
	B01110};
byte slowChar[] = {
	B00011,
	B00111,
	B00011,
	B11011,
	B11011,
	B00000,
	B00100,
	B01110};

#define DOUT A0
#define CLK A1

HX711 scale;

struct Calibration
{
	float CALIBRATION_FACTOR;
	long CALIBRATION_OFFSET;
};

Calibration calibration = {0, 0};

int CalibrationFactorAdd = 120;
int CalibrationOffsetAdd = 130;

void SaveCalibration()
{
	EEPROM.writeFloat(CalibrationFactorAdd, calibration.CALIBRATION_FACTOR);
	EEPROM.writeLong(CalibrationOffsetAdd, calibration.CALIBRATION_OFFSET);
}

void loadCalibration()
{
	calibration.CALIBRATION_FACTOR = EEPROM.readFloat(CalibrationFactorAdd);
	calibration.CALIBRATION_OFFSET = EEPROM.readLong(CalibrationOffsetAdd);
}

float KNOWN_WEIGHT = 500;
int currentWeight = 0;
int prevWeight = 0;

bool calibrateFlag = false;
int calibrate_screen = 0;

void setScale()
{
	scale.begin(DOUT, CLK);
	scale.set_scale(calibration.CALIBRATION_FACTOR);
	scale.set_offset(calibration.CALIBRATION_OFFSET);
}

static const int buttonPin = 2;
int buttonStatePrevious = HIGH;

static const int buttonPin2 = 3;
int buttonStatePrevious2 = HIGH;

static const int buttonPin3 = 4;
int buttonStatePrevious3 = HIGH;

unsigned long minButtonLongPressDuration = 2000;
unsigned long buttonLongPressUpMillis;
unsigned long buttonLongPressDownMillis;
unsigned long buttonLongPressEnterMillis;
bool buttonStateLongPressUp = false;
bool buttonStateLongPressDown = false;
bool buttonStateLongPressEnter = false;

const int intervalButton = 50;
unsigned long previousButtonMillis;
unsigned long buttonPressDuration;
unsigned long currentMillis;

const int intervalButton2 = 50;
unsigned long previousButtonMillis2;
unsigned long buttonPressDuration2;
unsigned long currentMillis2;

const int intervalButton3 = 50;
unsigned long previousButtonMillis3;
unsigned long buttonPressDuration3;
unsigned long currentMillis3;

// Declaration of LCD Variables
const int NUM_MAIN_ITEMS = 3;
const int NUM_SETTING_ITEMS = 9;
const int NUM_TESTMACHINE_ITEMS = 8;
const int MAX_ITEM_LENGTH = 20; // maximum characters for the item name

int currentMainScreen;
int currentSettingScreen;
int currentTestMenuScreen;
bool settingFlag, settingEditFlag, testMenuFlag = false;

String menu_items[NUM_MAIN_ITEMS][2] = { // array with item names
	{"SETTING", "ENTER TO EDIT"},
	{"RUN AUTO", "ENTER TO RUN AUTO"},
	{"TEST MACHINE", "ENTER TO TEST"}};

String setting_items[NUM_SETTING_ITEMS][2] = { // array with item names
	{"SCALE CALIBRATE"},
	{"WEIGHT", "G"},
	{"LINEAR", "SEC"},
	{"STEPPER", "SEC"},
	{"SEALER 1", "SEC"},
	{"SEALER 2", "SEC"},
	{"HEATER 1", "SEC"},
	{"HEATER 2", "SEC"},
	{"SAVE"}};

String testmachine_items[NUM_TESTMACHINE_ITEMS][2] = { // array with item names
	{"VIBRATOR", "DISPENSE"},
	{"LINEAR", "DROP"},
	{"STEPPER", "MOVE"},
	{"SEALER 1", "LINEAR 1"},
	{"SEALER 2", "LINEAR 1"},
	{"HEATER 1", "SEAL 1"},
	{"HEATER 2", "SEAL 1"},
	{"EXIT"}};

int parametersTimer[NUM_SETTING_ITEMS] = {1, 1, 1, 1, 1, 1, 1, 1};
int parametersTimerMaxValue[NUM_SETTING_ITEMS] = {100000, 1200, 1200, 1200, 1200, 1200, 1200, 1200};

bool refreshScreen = true;
unsigned long previousMillis = 0;
const long interval = 1000;

int CalibrationAdd = 10;
int vibroAdd = 20;
int LinearAdd = 30;
int StepperTimeAdd = 40;
int Sealer1TimeAdd = 50;
int Sealer2TimeAdd = 60;
int Heater1TimeAdd = 70;
int Heater2TimeAdd = 80;

void saveSettings()
{
	EEPROM.writeDouble(vibroAdd, parametersTimer[1]);
	EEPROM.writeDouble(LinearAdd, parametersTimer[2]);
	EEPROM.writeDouble(StepperTimeAdd, parametersTimer[3]);
	EEPROM.writeDouble(Sealer1TimeAdd, parametersTimer[4]);
	EEPROM.writeDouble(Sealer2TimeAdd, parametersTimer[5]);
	EEPROM.writeDouble(Heater1TimeAdd, parametersTimer[6]);
	EEPROM.writeDouble(Heater2TimeAdd, parametersTimer[7]);
}

void loadSettings()
{
	parametersTimer[1] = EEPROM.readDouble(vibroAdd);
	parametersTimer[2] = EEPROM.readDouble(LinearAdd);
	parametersTimer[3] = EEPROM.readDouble(StepperTimeAdd);
	parametersTimer[4] = EEPROM.readDouble(Sealer1TimeAdd);
	parametersTimer[5] = EEPROM.readDouble(Sealer2TimeAdd);
	parametersTimer[6] = EEPROM.readDouble(Heater1TimeAdd);
	parametersTimer[7] = EEPROM.readDouble(Heater2TimeAdd);
}

char *secondsToHHMMSS(int total_seconds)
{
	int hours, minutes, seconds;

	hours = total_seconds / 3600;		  // Divide by number of seconds in an hour
	total_seconds = total_seconds % 3600; // Get the remaining seconds
	minutes = total_seconds / 60;		  // Divide by number of seconds in a minute
	seconds = total_seconds % 60;		  // Get the remaining seconds

	// Format the output string
	static char hhmmss_str[7]; // 6 characters for HHMMSS + 1 for null terminator
	sprintf(hhmmss_str, "%02d%02d%02d", hours, minutes, seconds);
	return hhmmss_str;
}

int ena = 25;
int dir = 26;
int step = 27;
AccelStepper stepper(AccelStepper::FULL2WIRE, dir, step);
long currentPos = 0;
long lastPos = 0;
int speedStep = 10000;
int moveStep = -10000;

void setStepper()
{
	stepper.setEnablePin(ena);
	stepper.setPinsInverted(false, false, false);
	stepper.setMaxSpeed(speedStep);
	stepper.setSpeed(speedStep);
	stepper.setAcceleration(speedStep * 2000);
	stepper.enableOutputs();
	lastPos = stepper.currentPosition();
}
void DisableStepper()
{
	stepper.disableOutputs();
}
void EnableStepper()
{
	stepper.enableOutputs();
}

void runRollerStepper()
{
	if (stepper.distanceToGo() == 0)
	{
		stepper.setCurrentPosition(0);
		stepper.move(moveStep);
	}
}

Control Vibro(47, 100, 100);
Control Linear(45, 100, 100);
Control Stepper(100, 100, 100);
Control Sealer1(33, 100, 100);
Control Sealer2(35, 100, 100);
Control Heater1(43, 100, 100);
Control Heater2(41, 100, 100);

void stopAll()
{
	DisableStepper();
	Vibro.stop();
	Vibro.relayOff();

	Linear.stop();
	Linear.relayOff();

	Stepper.stop();
	Stepper.relayOff();

	Sealer1.stop();
	Sealer1.relayOff();

	Sealer2.stop();
	Sealer2.relayOff();

	Heater1.stop();
	Heater1.relayOff();

	Heater2.stop();
	Heater2.relayOff();
}

void setTimers()
{
	Linear.setTimer(secondsToHHMMSS(parametersTimer[2]));
	Stepper.setTimer(secondsToHHMMSS(parametersTimer[3]));
	Sealer1.setTimer(secondsToHHMMSS(parametersTimer[4]));
	Sealer2.setTimer(secondsToHHMMSS(parametersTimer[5]));
	Heater1.setTimer(secondsToHHMMSS(parametersTimer[6]));
	Heater2.setTimer(secondsToHHMMSS(parametersTimer[7]));
}

bool TestMachineFlag = false;
bool vibroTestFlag, linearTestFlag, stepperTestFlag, sealer1TestFlag, sealer2TestFLag, heater1TestFlag, heater2TestFlag = false;

void RunTestMachine()
{
	if (vibroTestFlag == true)
	{
		if (scale.is_ready())
		{
			currentWeight = scale.get_units(3);
		}
		Vibro.relayOn();
	}
	else
	{
		Vibro.relayOff();
	}

	if (linearTestFlag == true)
	{
		Linear.relayOn();
	}
	else
	{
		Linear.relayOff();
	}

	Stepper.run();
	if (Stepper.isTimerCompleted() == true)
	{
		DisableStepper();
	}
	else
	{
		stepper.run();
		EnableStepper();
		runRollerStepper();
	}
	Sealer1.run();
	Sealer2.run();
	Heater1.run();
	Heater2.run();
}

bool RunAutoFlag = false;
int RunAutoSequence = 0;

void RunAuto()
{
	switch (RunAutoSequence)
	{
	case 0:
		RunAutoWeighingDispenser();
		break;
	case 1:
		RunAutoOpenDispenser();
		break;
	case 2:
		RunAutoStepper();
		break;
	case 3:
		RunAutoUpperSeal();
		break;
	case 4:
		RunAutoLowerSeal();
		break;
	default:
		stopAll();
		RunAutoFlag = false;
		break;
	}
}

void RunAutoWeighingDispenser()
{

	if (scale.is_ready())
	{
		currentWeight = scale.get_units(5);
		if (prevWeight != currentWeight)
		{
			prevWeight = currentWeight;
			refreshScreen = true;
		}
	}

	if (currentWeight >= parametersTimer[1])
	{
		Vibro.relayOff();
		RunAutoSequence = 1;
		Linear.start();
	}
	else
	{
		Vibro.relayOn();
	}
}
void RunAutoOpenDispenser()
{
	Linear.run();
	if (Linear.isTimerCompleted() == true)
	{
		RunAutoSequence = 2;
		Stepper.start();
	}
}
void RunAutoStepper()
{
	Stepper.run();

	if (Stepper.isTimerCompleted() == true)
	{
		DisableStepper();
		RunAutoSequence = 3;
		Sealer1.start();
		Heater1.start();
	}
	else
	{
		stepper.run();
		EnableStepper();
		runRollerStepper();
	}
}
void RunAutoUpperSeal()
{
	Sealer1.run();
	Heater1.run();
	if (Sealer1.isTimerCompleted() == true && Heater1.isTimerCompleted() == true)
	{
		Sealer2.start();
		Heater2.start();
		RunAutoSequence = 4;
	}
}
void RunAutoLowerSeal()
{
	Sealer2.run();
	Heater2.run();
	if (Sealer2.isTimerCompleted() == true && Heater2.isTimerCompleted() == true)
	{
		scale.tare(5);
		RunAutoSequence = 0;
	}
}

// Function for reading the button state
void readButtonUpState()
{
	if (currentMillis - previousButtonMillis > intervalButton)
	{
		int buttonState = digitalRead(buttonPin);
		if (buttonState == LOW && buttonStatePrevious == HIGH && !buttonStateLongPressUp)
		{
			buttonLongPressUpMillis = currentMillis;
			buttonStatePrevious = LOW;
		}
		buttonPressDuration = currentMillis - buttonLongPressUpMillis;
		if (buttonState == LOW && !buttonStateLongPressUp && buttonPressDuration >= minButtonLongPressDuration)
		{
			buttonStateLongPressUp = true;
		}
		if (buttonStateLongPressUp == true)
		{
			// Insert Fast Scroll Up
			if (settingFlag == true)
			{
				if (settingEditFlag == true)
				{
					if (parametersTimer[currentSettingScreen] >= parametersTimerMaxValue[currentSettingScreen] - 1)
					{
						parametersTimer[currentSettingScreen] = parametersTimerMaxValue[currentSettingScreen];
					}
					else
					{
						parametersTimer[currentSettingScreen] += 1;
					}
				}
				else if (currentSettingScreen == 0 && calibrateFlag == true && calibrate_screen == 2)
				{
					refreshScreen = true;
					KNOWN_WEIGHT += 1;
				}
				else if (calibrateFlag == false)
				{
					if (currentSettingScreen == NUM_SETTING_ITEMS - 1)
					{
						currentSettingScreen = 0;
					}
					else
					{
						currentSettingScreen++;
					}
				}
			}
			else if (testMenuFlag == true)
			{
				if (currentTestMenuScreen == NUM_TESTMACHINE_ITEMS - 1)
				{
					currentTestMenuScreen = 0;
				}
				else
				{
					currentTestMenuScreen++;
				}
			}
			else
			{
				if (currentMainScreen == NUM_MAIN_ITEMS - 1)
				{
					currentMainScreen = 0;
				}
				else
				{
					currentMainScreen++;
				}
			}
			refreshScreen = true;
		}
		if (buttonState == HIGH && buttonStatePrevious == LOW)
		{
			buttonStatePrevious = HIGH;
			buttonStateLongPressUp = false;
			if (buttonPressDuration < minButtonLongPressDuration)
			{
				// Short Scroll Up
				if (settingFlag == true)
				{
					if (settingEditFlag == true)
					{
						if (parametersTimer[currentSettingScreen] >= parametersTimerMaxValue[currentSettingScreen] - 1)
						{
							parametersTimer[currentSettingScreen] = parametersTimerMaxValue[currentSettingScreen];
						}
						else
						{
							parametersTimer[currentSettingScreen] += 1;
						}
					}
					else if (currentSettingScreen == 0 && calibrateFlag == true && calibrate_screen == 2)
					{
						refreshScreen = true;
						KNOWN_WEIGHT += 1;
					}
					else if (calibrateFlag == false)
					{
						if (currentSettingScreen == NUM_SETTING_ITEMS - 1)
						{
							currentSettingScreen = 0;
						}
						else
						{
							currentSettingScreen++;
						}
					}
				}
				else if (testMenuFlag == true)
				{
					if (currentTestMenuScreen == NUM_TESTMACHINE_ITEMS - 1)
					{
						currentTestMenuScreen = 0;
					}
					else
					{
						currentTestMenuScreen++;
					}
				}
				else
				{
					if (currentMainScreen == NUM_MAIN_ITEMS - 1)
					{
						currentMainScreen = 0;
					}
					else
					{
						currentMainScreen++;
					}
				}
				refreshScreen = true;
			}
		}
		previousButtonMillis = currentMillis;
	}
}

void readButtonDownState()
{
	if (currentMillis2 - previousButtonMillis2 > intervalButton2)
	{
		int buttonState2 = digitalRead(buttonPin2);
		if (buttonState2 == LOW && buttonStatePrevious2 == HIGH && !buttonStateLongPressDown)
		{
			buttonLongPressDownMillis = currentMillis2;
			buttonStatePrevious2 = LOW;
		}
		buttonPressDuration2 = currentMillis2 - buttonLongPressDownMillis;
		if (buttonState2 == LOW && !buttonStateLongPressDown && buttonPressDuration2 >= minButtonLongPressDuration)
		{
			buttonStateLongPressDown = true;
		}
		if (buttonStateLongPressDown == true)
		{
			// Insert Fast Scroll Down
			if (settingFlag == true)
			{
				if (settingEditFlag == true)
				{
					if (parametersTimer[currentSettingScreen] <= 0)
					{
						parametersTimer[currentSettingScreen] = 0;
					}
					else
					{
						parametersTimer[currentSettingScreen] -= 1;
					}
				}
				else if (currentSettingScreen == 0 && calibrateFlag == true && calibrate_screen == 2)
				{
					refreshScreen = true;
					if (KNOWN_WEIGHT < 1)
					{
						KNOWN_WEIGHT = 0;
					}
					else
					{
						KNOWN_WEIGHT -= 1;
					}
				}
				else
				{
					if (currentSettingScreen == 0)
					{
						currentSettingScreen = NUM_SETTING_ITEMS - 1;
					}
					else
					{
						currentSettingScreen--;
					}
				}
			}
			else if (testMenuFlag == true)
			{
				if (currentTestMenuScreen == 0)
				{
					currentTestMenuScreen = NUM_TESTMACHINE_ITEMS - 1;
				}
				else
				{
					currentTestMenuScreen--;
				}
			}
			else if (calibrateFlag == false)
			{
				if (currentMainScreen == 0)
				{
					currentMainScreen = NUM_MAIN_ITEMS - 1;
				}
				else
				{
					currentMainScreen--;
				}
			}
			refreshScreen = true;
		}
		if (buttonState2 == HIGH && buttonStatePrevious2 == LOW)
		{
			buttonStatePrevious2 = HIGH;
			buttonStateLongPressDown = false;
			if (buttonPressDuration2 < minButtonLongPressDuration)
			{
				// Short Scroll Down
				if (settingFlag == true)
				{
					if (settingEditFlag == true)
					{
						if (parametersTimer[currentSettingScreen] <= 0)
						{
							parametersTimer[currentSettingScreen] = 0;
						}
						else
						{
							parametersTimer[currentSettingScreen] -= 1;
						}
					}
					else if (currentSettingScreen == 0 && calibrateFlag == true && calibrate_screen == 2)
					{
						refreshScreen = true;
						if (KNOWN_WEIGHT < 1)
						{
							KNOWN_WEIGHT = 0;
						}
						else
						{
							KNOWN_WEIGHT -= 1;
						}
					}
					else
					{
						if (currentSettingScreen == 0)
						{
							currentSettingScreen = NUM_SETTING_ITEMS - 1;
						}
						else
						{
							currentSettingScreen--;
						}
					}
				}
				else if (testMenuFlag == true)
				{
					if (currentTestMenuScreen == 0)
					{
						currentTestMenuScreen = NUM_TESTMACHINE_ITEMS - 1;
					}
					else
					{
						currentTestMenuScreen--;
					}
				}
				else if (calibrateFlag == false)
				{
					if (currentMainScreen == 0)
					{
						currentMainScreen = NUM_MAIN_ITEMS - 1;
					}
					else
					{
						currentMainScreen--;
					}
				}
				refreshScreen = true;
			}
		}
		previousButtonMillis2 = currentMillis2;
	}
}

void readButtonEnterState()
{
	if (currentMillis3 - previousButtonMillis3 > intervalButton3)
	{
		int buttonState3 = digitalRead(buttonPin3);
		if (buttonState3 == LOW && buttonStatePrevious3 == HIGH && !buttonStateLongPressEnter)
		{
			buttonLongPressEnterMillis = currentMillis3;
			buttonStatePrevious3 = LOW;
		}
		buttonPressDuration3 = currentMillis3 - buttonLongPressEnterMillis;
		if (buttonState3 == LOW && !buttonStateLongPressEnter && buttonPressDuration3 >= minButtonLongPressDuration)
		{
			buttonStateLongPressEnter = true;
		}
		if (buttonStateLongPressEnter == true)
		{
			// Insert Fast Scroll Enter
			Serial.println("Long Press Enter");
		}
		if (buttonState3 == HIGH && buttonStatePrevious3 == LOW)
		{
			buttonStatePrevious3 = HIGH;
			buttonStateLongPressEnter = false;
			if (buttonPressDuration3 < minButtonLongPressDuration)
			{
				refreshScreen = true;
				// Short Scroll Enter
				if (currentMainScreen == 0 && settingFlag == true)
				{
					if (currentSettingScreen == NUM_SETTING_ITEMS - 1)
					{
						settingFlag = false;
					}
					else if (currentSettingScreen == 0)
					{

						if (currentSettingScreen == 0 && calibrateFlag == false)
						{
							calibrateFlag = true;
							calibrate_screen = 1;
							
						}
						else
						{
							if (currentSettingScreen == 0 && calibrateFlag == true && calibrate_screen == 1)
							{
								scale.tare();
								calibrate_screen = 2;
								
							}
							else if (currentSettingScreen == 0 && calibrateFlag == true && calibrate_screen == 2)
							{
								scale.calibrate_scale(KNOWN_WEIGHT, 20);
								long scaleOffset = scale.get_offset();
								float scaleFactor = scale.get_scale();
								calibration.CALIBRATION_FACTOR = scaleFactor;
								calibration.CALIBRATION_OFFSET = scaleOffset;
								SaveCalibration();
								calibrate_screen = 3;
								
							}
							else if (currentSettingScreen == 0 && calibrateFlag == true && calibrate_screen == 3)
							{
								calibrateFlag = false;
								calibrate_screen = 0;
								
							}
						}
					}
					else
					{
						if (settingEditFlag == true)
						{
							settingEditFlag = false;
						}
						else
						{
							settingEditFlag = true;
						}
					}
				}
				else if (currentMainScreen == 1 && RunAutoFlag == true)
				{
					RunAutoFlag = false;
					stopAll();
				}
				else if (currentMainScreen == 2 && testMenuFlag == true)
				{
					if (currentTestMenuScreen == NUM_TESTMACHINE_ITEMS - 1)
					{
						currentMainScreen = 0;
						currentTestMenuScreen = 0;
						TestMachineFlag = false;
						testMenuFlag = false;
						stopAll();
					}
					else if (currentTestMenuScreen == 0)
					{
						if (vibroTestFlag == true)
						{
							vibroTestFlag = false;
						}
						else
						{
							vibroTestFlag = true;
						}
					}
					else if (currentTestMenuScreen == 1)
					{
						if (linearTestFlag == true)
						{
							linearTestFlag = false;
						}
						else
						{
							linearTestFlag = true;
						}
					}
					else if (currentTestMenuScreen == 2)
					{
						if (Stepper.isTimerCompleted() == true)
						{
							Stepper.start();
							EnableStepper();
						}
						else
						{
							Stepper.stop();
							DisableStepper();
						}
					}
					else if (currentTestMenuScreen == 3)
					{
						if (Sealer1.isTimerCompleted() == true)
						{
							Sealer1.start();
						}
						else
						{
							Sealer1.stop();
						}
					}
					else if (currentTestMenuScreen == 4)
					{
						if (Sealer2.isTimerCompleted() == true)
						{
							Sealer2.start();
						}
						else
						{
							Sealer2.stop();
						}
					}
					else if (currentTestMenuScreen == 5)
					{
						if (Heater1.isTimerCompleted() == true)
						{
							Heater1.start();
						}
						else
						{
							Heater1.stop();
						}
					}
					else if (currentTestMenuScreen == 6)
					{
						if (Heater2.isTimerCompleted() == true)
						{
							Heater2.start();
						}
						else
						{
							Heater2.stop();
						}
					}
				}
				else
				{
					if (currentMainScreen == 0)
					{
						settingFlag = true;
					}
					else if (currentMainScreen == 1)
					{
						RunAutoFlag = true;
						scale.tare();
						RunAutoSequence = 0;
					}
					else if (currentMainScreen == 2)
					{
						testMenuFlag = true;
						TestMachineFlag = true;
					}
				}
			}
		}
		previousButtonMillis3 = currentMillis3;
	}
}

void InputReadandFeedback()
{
	currentMillis = millis();
	currentMillis2 = millis();
	currentMillis3 = millis();
	readButtonEnterState();
	readButtonUpState();
	readButtonDownState();
}

void printScreen()
{
	if (settingFlag == true)
	{
		if (currentSettingScreen == NUM_SETTING_ITEMS - 1)
		{
			printSettingScreen(setting_items[currentSettingScreen][0], setting_items[currentSettingScreen][1], parametersTimer[currentSettingScreen], settingEditFlag, true);
		}
		else if (currentSettingScreen == 0 && calibrateFlag == true)
		{
			Serial.println(currentSettingScreen);
			Serial.println(calibrate_screen);
			Serial.println(calibrateFlag);
			switch (calibrate_screen)
			{
			case 1:
				printCalibrateScreen("Calibrate", "Empty the Bin", currentWeight, "Enter To Tare", calibrate_screen);
				break;
			case 2:
				printCalibrateScreen("Enter Known Weight", "Empty the Bin", currentWeight, "Enter To Calibrate", calibrate_screen);
				break;
			case 3:
				printCalibrateScreen("Calibration", "Calibrated", currentWeight, "Enter To Exit", calibrate_screen);
				break;
			default:
				break;
			}
		}
		else
		{
			printSettingScreen(setting_items[currentSettingScreen][0], setting_items[currentSettingScreen][1], parametersTimer[currentSettingScreen], settingEditFlag, false);
		}
	}
	else if (RunAutoFlag == true)
	{
		switch (RunAutoSequence)
		{
		case 0:
			printRunAutoScreen("Run Auto", "Weighing", "", true, currentWeight);
			break;

		case 1:
			printRunAutoScreen("Run Auto", "Dispensing", Linear.getTimeRemaining(), false, 0);
			break;

		case 2:
			printRunAutoScreen("Run Auto", "Roll Down", Stepper.getTimeRemaining(), false, 0);
			break;

		case 3:
			printRunAutoScreen("Run Auto", "Seal Side", Sealer1.getTimeRemaining(), false, 0);
			break;

		case 4:
			printRunAutoScreen("Run Auto", "Seal Bottom", Sealer2.getTimeRemaining(), false, 0);
			break;

		default:
			break;
		}
	}
	else if (testMenuFlag == true)
	{
		switch (currentTestMenuScreen)
		{
		case 0:
			printTestScreen("Dispenser", testmachine_items[currentTestMenuScreen][1], vibroTestFlag, currentWeight, false);
			break;
		case 1:
			printTestScreen(testmachine_items[currentTestMenuScreen][0], testmachine_items[currentTestMenuScreen][1], linearTestFlag, currentWeight, false);
			break;
		case 2:
			printTestScreen(testmachine_items[currentTestMenuScreen][0], Stepper.getTimeRemaining(), !Stepper.isTimerCompleted(), currentWeight, false);
			break;
		case 3:
			printTestScreen(testmachine_items[currentTestMenuScreen][0], Sealer1.getTimeRemaining(), !Sealer1.isTimerCompleted(), currentWeight, false);
			break;
		case 4:
			printTestScreen(testmachine_items[currentTestMenuScreen][0], Sealer2.getTimeRemaining(), !Sealer2.isTimerCompleted(), currentWeight, false);
			break;
		case 5:
			printTestScreen(testmachine_items[currentTestMenuScreen][0], Heater1.getTimeRemaining(), !Heater1.isTimerCompleted(), currentWeight, false);
			break;
		case 6:
			printTestScreen(testmachine_items[currentTestMenuScreen][0], Heater2.getTimeRemaining(), !Heater2.isTimerCompleted(), currentWeight, false);
			break;
		case 7:
			printTestScreen(testmachine_items[currentTestMenuScreen][0], " ", vibroTestFlag, currentWeight, true);
			break;

		default:
			break;
		}
	}
	else
	{
		printMainMenu(menu_items[currentMainScreen][0], menu_items[currentMainScreen][1]);
	}
}

void printMainMenu(String MenuItem, String Action)
{
	lcd.clear();
	lcd.print(MenuItem);
	lcd.setCursor(0, 3);
	lcd.write(0);
	lcd.setCursor(2, 3);
	lcd.print(Action);
	refreshScreen = false;
}

void printRunAutoScreen(String SettingTitle, String Process, String TimeRemaining, bool Dispensing, int Weight)
{
	lcd.clear();
	lcd.print(SettingTitle);
	lcd.setCursor(0, 1);
	lcd.print(Process);
	if (Dispensing == true)
	{
		lcd.setCursor(0, 2);
		lcd.print("Weight:");
		lcd.setCursor(0, 3);
		lcd.print(Weight);
	}
	else
	{
		lcd.setCursor(0, 2);
		lcd.print("Time Remaining:");
		lcd.setCursor(0, 3);
		lcd.print(TimeRemaining);
	}

	refreshScreen = false;
}

void printSettingScreen(String SettingTitle, String Unit, int Value, bool EditFlag, bool SaveFlag)
{
	lcd.clear();
	lcd.print(SettingTitle);
	lcd.setCursor(0, 1);

	if (SaveFlag == true)
	{
		lcd.setCursor(0, 3);
		lcd.write(0);
		lcd.setCursor(2, 3);
		lcd.print("ENTER TO SAVE ALL");
	}
	else
	{
		lcd.print(Value);
		lcd.print(" ");
		lcd.print(Unit);
		lcd.setCursor(0, 3);
		lcd.write(0);
		lcd.setCursor(2, 3);
		if (EditFlag == false)
		{
			lcd.print("ENTER TO EDIT");
		}
		else
		{
			lcd.print("ENTER TO SAVE");
		}
	}
	refreshScreen = false;
}

void printTestScreen(String TestMenuTitle, String Job, bool Status, int Weight, bool ExitFlag)
{
	lcd.clear();
	lcd.print(TestMenuTitle);
	if (ExitFlag == false)
	{
		lcd.setCursor(0, 1);
		lcd.print("Weight: ");
		lcd.print(Weight);
		lcd.print(" G");
		lcd.setCursor(0, 2);
		lcd.print(Job);
		lcd.print(" : ");
		if (Status == true)
		{
			lcd.print("ON");
		}
		else
		{
			lcd.print("OFF");
		}
	}

	if (ExitFlag == true)
	{
		lcd.setCursor(0, 3);
		lcd.print("Click to Exit Test");
	}
	else
	{
		lcd.setCursor(0, 3);
		lcd.print("Click to Run Test");
	}
	refreshScreen = false;
}

void printCalibrateScreen(String TestMenuTitle, String Job, int Weight, String Action, int Screen)
{
	lcd.clear();
	lcd.print(TestMenuTitle);
	switch (Screen)
	{
	case 1:
		lcd.setCursor(0, 1);
		lcd.print(Job);
		lcd.setCursor(0, 2);
		lcd.print("Weight: ");
		lcd.print(Weight);
		lcd.print(" G");
		lcd.setCursor(0, 3);
		lcd.print(Action);
		break;
	case 2:
		lcd.setCursor(0, 1);
		lcd.print(KNOWN_WEIGHT, 0);
		lcd.setCursor(0, 2);
		lcd.print("Weight: ");
		lcd.print(Weight);
		lcd.print(" G");
		lcd.setCursor(0, 3);
		lcd.print(Action);
		break;
	case 3:
		lcd.setCursor(0, 1);
		lcd.print(Job);
		lcd.setCursor(0, 2);
		lcd.print("Weight: ");
		lcd.print(Weight);
		lcd.print(" G");
		lcd.setCursor(0, 3);
		lcd.print(Action);
		break;
	}

	refreshScreen = false;
}

void setupLCD()
{
	lcd.init();
	lcd.clear();
	lcd.createChar(0, enterChar);
	lcd.createChar(1, fastChar);
	lcd.createChar(2, slowChar);
	lcd.backlight();
	refreshScreen = true;
}

void setup()
{
	// saveSettings();
	Serial.begin(9600);
	setupLCD();
	loadSettings();
	loadCalibration();
	setTimers();
	setStepper();
	stopAll();
	setScale();
	refreshScreen = true;
	pinMode(buttonPin, INPUT_PULLUP);
	pinMode(buttonPin2, INPUT_PULLUP);
	pinMode(buttonPin3, INPUT_PULLUP);
}

void loop()
{
	InputReadandFeedback();
	if (refreshScreen == true)
	{
		printScreen();
	}

	if (TestMachineFlag == true)
	{
		RunTestMachine();
		unsigned long currentMillis = millis();
		if (currentMillis - previousMillis >= interval)
		{
			// save the last time you blinked the LED
			previousMillis = currentMillis;
			refreshScreen = true;
			if (scale.is_ready())
			{
				currentWeight = scale.get_units(5);
				if (prevWeight != currentWeight)
				{
					prevWeight = currentWeight;
					refreshScreen = true;
				}
			}
		}
	}
	if (settingFlag == true && currentSettingScreen == 0 && calibrateFlag == true)
	{
		if (scale.is_ready())
		{
			currentWeight = scale.get_units(5);
			if (prevWeight != currentWeight)
			{
				prevWeight = currentWeight;
				refreshScreen = true;
			}
		}
	}
	if (RunAutoFlag == true)
	{
		RunAuto();
		if (RunAutoSequence != 0)
		{
			unsigned long currentMillis = millis();
			if (currentMillis - previousMillis >= interval)
			{
				// save the last time you blinked the LED
				previousMillis = currentMillis;
				refreshScreen = true;
			}
		}
	}
}
