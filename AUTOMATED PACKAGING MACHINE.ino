#include <Arduino.h>
#include "control.h"
#include <AccelStepper.h>
#include <EEPROMex.h>
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
const int NUM_SETTING_ITEMS = 5;
const int NUM_TESTMACHINE_ITEMS = 7;
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
	{"STEPPER", "SEC"},
	{"SEALER 1", "SEC"},
	{"SEALER 2", "SEC"},
	{"SPARE", "SPARE"},
	{"SAVE", "Test"}};

String testmachine_items[NUM_TESTMACHINE_ITEMS][2] = { // array with item names
	{"START", "SCALE"},
	{"DISCHARGE", "LINEAR"},
	{"STOP", "SCALE"},
	{"STEPPER", "MOVE"},
	{"SEALER 1", "LINEAR 1"},
	{"SEALER 2", "LINEAR 1"},
	{"EXIT"}};

int parametersTimer[NUM_SETTING_ITEMS] = {1, 1, 1, 1};
int parametersTimerMaxValue[NUM_SETTING_ITEMS] = {1200, 1200, 1200, 1200};

bool refreshScreen = true;
unsigned long previousMillis = 0;
const long interval = 1000;
int StepperTimeAdd = 20;
int Sealer1TimeAdd = 30;
int Sealer2TimeAdd = 40;

void saveSettings()
{
	EEPROM.writeDouble(StepperTimeAdd, parametersTimer[0]);
	EEPROM.writeDouble(Sealer1TimeAdd, parametersTimer[1]);
	EEPROM.writeDouble(Sealer2TimeAdd, parametersTimer[2]);
}

void loadSettings()
{
	parametersTimer[0] = EEPROM.readDouble(StepperTimeAdd);
	parametersTimer[1] = EEPROM.readDouble(Sealer1TimeAdd);
	parametersTimer[2] = EEPROM.readDouble(Sealer2TimeAdd);
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

Control Stepper(100, 100, 100);
Control Sealer1(35, 100, 100);
Control Sealer2(33, 100, 100);
Control Heater1(37, 100, 100);
Control Heater2(39, 100, 100);

Control TestLightA(41, 100, 100);
Control TestLightB(13, 100, 100);

// Auto Weighing Control
Control Start(47, 100, 100);
Control Stop(43, 100, 100);
Control Discharge(45, 100, 100);

Control AfterDispenseTimer(100, 100, 100);
Control AfterLinearTimer(100, 100, 100);

const int loadFeedback = A10;
const int dischargeFeedback = A11;
bool loadStatus, dischargeStatus = false;

void stopAll()
{
	DisableStepper();

	Stepper.stop();
	Stepper.relayOff();

	Sealer1.stop();
	Sealer1.relayOff();

	Sealer2.stop();
	Sealer2.relayOff();

	Start.stop();
	Start.relayOff();

	Stop.stop();
	Stop.relayOff();

	Discharge.stop();
	Discharge.relayOff();
	Heater1.stop();
	Heater2.stop();
	Heater1.relayOff();
	Heater2.relayOff();
}

void setTimers()
{
	Stepper.setTimer(secondsToHHMMSS(parametersTimer[0]));
	Sealer1.setTimer(secondsToHHMMSS(parametersTimer[1]));
	Sealer2.setTimer(secondsToHHMMSS(parametersTimer[2]));

	Start.setTimer(secondsToHHMMSS(1));
	Stop.setTimer(secondsToHHMMSS(1));
	Discharge.setTimer(secondsToHHMMSS(2));
	AfterDispenseTimer.setTimer(secondsToHHMMSS(3));
	AfterLinearTimer.setTimer(secondsToHHMMSS(2));
}

bool TestMachineFlag = false;
bool vibroTestFlag, linearTestFlag, stepperTestFlag, sealer1TestFlag, sealer2TestFLag, heater1TestFlag, heater2TestFlag = false;

void RunTestMachine()
{
	Start.run();
	Stop.run();
	Discharge.run();

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
}

bool RunAutoFlag = false;
int RunAutoSequence = 0;

void RunAuto()
{
	switch (RunAutoSequence)
	{
	case 0:
		RunAutoStart();
		break;
	case 1:
		RunAutoDispensing();
		break;
	case 2:
		RunAutoAfterDispenseTimer();
		break;
	case 3:
		RunAutoLinearDischarge();
		break;
	case 4:
		RunAutoDischarge();
		break;
	case 5:
		RunAutAfterDischargeTimer();
		break;
	case 6:
		RunAutoStepper();
		break;
	case 7:
		RunAutoUpperSeal();
		break;
	case 8:
		RunAutoLowerSeal();
		break;
	default:
		stopAll();
		RunAutoFlag = false;
		break;
	}
}

void RunAutoStart()
{
	Start.run();
	if (Start.isTimerCompleted() == true)
	{
		if (digitalRead(loadFeedback) == false)
		{
			RunAutoSequence = 1;
		}
	}
}

void RunAutoDispensing()
{
	if (digitalRead(loadFeedback) == false)
	{
		loadStatus = true;
	}
	else if (digitalRead(loadFeedback) == true && loadStatus == true)
	{
		AfterDispenseTimer.start();
		RunAutoSequence = 2;
	}
}

void RunAutoAfterDispenseTimer()
{
	AfterDispenseTimer.run();
	if (AfterDispenseTimer.isTimerCompleted() == true)
	{
		Discharge.start();
		RunAutoSequence = 3;
	}
}

void RunAutoLinearDischarge()
{
	Discharge.run();
	if (Discharge.isTimerCompleted() == true)
	{
		if (digitalRead(dischargeFeedback) == false)
		{
			RunAutoSequence = 4;
		}
	}
}

void RunAutoDischarge()
{
	if (digitalRead(dischargeFeedback) == false)
	{
		dischargeStatus = true;
	}
	else if (digitalRead(dischargeFeedback) == true && dischargeStatus == true)
	{
		AfterLinearTimer.start();
		RunAutoSequence = 5;
	}
}

void RunAutAfterDischargeTimer()
{
	AfterLinearTimer.run();
	if (AfterLinearTimer.isTimerCompleted() == true)
	{
		RunAutoSequence = 6;
		dischargeStatus = false;
		loadStatus = false;
		Stepper.start();
	}
}

void RunAutoStepper()
{
	Stepper.run();

	if (Stepper.isTimerCompleted() == true)
	{
		DisableStepper();
		RunAutoSequence = 7;
		Sealer1.start();
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

	if (Sealer1.isTimerCompleted() == true)
	{
		Sealer2.start();
		RunAutoSequence = 8;
	}
}
void RunAutoLowerSeal()
{
	Sealer2.run();

	if (Sealer2.isTimerCompleted() == true)
	{
		RunAutoSequence = 0;
		Start.start();
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
				else
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
					else
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
			Serial.println("test Fast");
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
			else
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
				Serial.println("test Slow");
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
				else
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
						saveSettings();
						loadSettings();
						currentSettingScreen = 0;
						setTimers();
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
						if (Start.isTimerCompleted() == true)
						{
							Start.start();
						}
						else
						{
							Start.stop();
						}
					}
					else if (currentTestMenuScreen == 1)
					{
						if (Discharge.isTimerCompleted() == true)
						{
							Discharge.start();
						}
						else
						{
							Discharge.stop();
						}
					}
					else if (currentTestMenuScreen == 2)
					{
						if (Stop.isTimerCompleted() == true)
						{
							Stop.start();
						}
						else
						{
							Stop.stop();
						}
					}
					else if (currentTestMenuScreen == 3)
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
					else if (currentTestMenuScreen == 4)
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
					else if (currentTestMenuScreen == 5)
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
						RunAutoSequence = 0;
						Start.start();
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
			printRunAutoScreen("Run Auto", "Start Dispensing", Start.getTimeRemaining());
			break;

		case 1:
			printRunAutoScreen("Run Auto", "Dispensing", "N/A");
			break;

		case 2:
			printRunAutoScreen("Run Auto", "Waiting", AfterDispenseTimer.getTimeRemaining());
			break;

		case 3:
			printRunAutoScreen("Run Auto", "Start Discharge", Discharge.getTimeRemaining());
			break;

		case 4:
			printRunAutoScreen("Run Auto", "Discharging", "N/A");
			break;

		case 5:
			printRunAutoScreen("Run Auto", "Waiting", AfterLinearTimer.getTimeRemaining());
			break;

		case 6:
			printRunAutoScreen("Run Auto", "Roll Down", Stepper.getTimeRemaining());
			break;

		case 7:
			printRunAutoScreen("Run Auto", "Sealing Side", Sealer1.getTimeRemaining());
			break;

		case 8:
			printRunAutoScreen("Run Auto", "Sealing Middle", Sealer2.getTimeRemaining());
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
			printTestScreen(testmachine_items[currentTestMenuScreen][0], Start.getTimeRemaining(), !Start.isTimerCompleted(), false);
			break;
		case 1:
			printTestScreen(testmachine_items[currentTestMenuScreen][0], Stop.getTimeRemaining(), !Stop.isTimerCompleted(), false);
			break;
		case 2:
			printTestScreen(testmachine_items[currentTestMenuScreen][0], Discharge.getTimeRemaining(), !Discharge.isTimerCompleted(), false);
			break;
		case 3:
			printTestScreen(testmachine_items[currentTestMenuScreen][0], Stepper.getTimeRemaining(), !Stepper.isTimerCompleted(), false);
			break;
		case 4:
			printTestScreen(testmachine_items[currentTestMenuScreen][0], Sealer1.getTimeRemaining(), !Sealer1.isTimerCompleted(), false);
			break;
		case 5:
			printTestScreen(testmachine_items[currentTestMenuScreen][0], Sealer2.getTimeRemaining(), !Sealer2.isTimerCompleted(), false);
			break;
		case 6:
			printTestScreen(testmachine_items[currentTestMenuScreen][0], " ", vibroTestFlag, true);
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

void printRunAutoScreen(String SettingTitle, String Process, String TimeRemaining)
{
	lcd.clear();
	lcd.print(SettingTitle);
	lcd.setCursor(0, 1);
	lcd.print(Process);

	lcd.setCursor(0, 2);
	lcd.print("Time Remaining:");
	lcd.setCursor(0, 3);
	lcd.print(TimeRemaining);
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

void printTestScreen(String TestMenuTitle, String Job, bool Status, bool ExitFlag)
{
	lcd.clear();
	lcd.print(TestMenuTitle);
	if (ExitFlag == false)
	{
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
	setTimers();
	setStepper();
	stopAll();
	refreshScreen = true;
	pinMode(buttonPin, INPUT_PULLUP);
	pinMode(buttonPin2, INPUT_PULLUP);
	pinMode(buttonPin3, INPUT_PULLUP);
	pinMode(loadFeedback, INPUT_PULLUP);
	pinMode(dischargeFeedback, INPUT_PULLUP);
}

void loop()
{
	InputReadandFeedback();

	if (refreshScreen == true)
	{
		printScreen();
		refreshScreen = false;
	}

	if (TestMachineFlag == true)
	{
		RunTestMachine();
		unsigned long currentMillis = millis();
		if (currentMillis - previousMillis >= interval)
		{
			previousMillis = currentMillis;
			refreshScreen = true;
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
				previousMillis = currentMillis;
				refreshScreen = true;
			}
		}
	}
}
