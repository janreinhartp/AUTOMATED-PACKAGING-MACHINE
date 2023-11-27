#include <Arduino.h>
#include "control.h"
#include <AccelStepper.h>
#include <EEPROMex.h>
#include <HX711.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);


#define DOUT A0
#define CLK A1

HX711 scale;

float CALIBRATION_FACTOR = 0;
float KNOWN_WEIGHT = 0;

int currentWeight = 0;

void setScale()
{
	scale.begin(DOUT, CLK);
	scale.set_scale(CALIBRATION_FACTOR);
	scale.tare();
}

void setCalibrationToZero()
{
	scale.set_scale();
	scale.tare();
}

float getCalibration(float weight)
{
	float reading = scale.get_units(10);
	return reading / weight;
}



// Declaration of LCD Variables
const int NUM_MAIN_ITEMS = 3;
const int NUM_SETTING_ITEMS = 8;
const int NUM_TESTMACHINE_ITEMS = 8;
const int MAX_ITEM_LENGTH = 20; // maximum characters for the item name

char menu_items[NUM_MAIN_ITEMS][MAX_ITEM_LENGTH] = { // array with item names
	{"SETTING"},
	{"RUN AUTO"},
	{"TEST MACHINE"}};

char setting_items[NUM_SETTING_ITEMS][MAX_ITEM_LENGTH] = { // array with item names
	{"SCALE"},
	{"LINEAR"},
	{"STEPPER"},
	{"SEALER 1"},
	{"SEALER 2"},
	{"HEATER 1"},
	{"HEATER 2"},
	{"SAVE"}};

int parametersTimer[NUM_SETTING_ITEMS] = {1, 1, 1, 1, 1, 1, 1, 1};
int parametersTimerMaxValue[NUM_SETTING_ITEMS] = {100000, 1200, 1200, 1200, 1200, 1200, 1200, 1200};

int CalibrationAdd = 10;
int LinearAdd = 20;
int StepperTimeAdd = 30;
int Sealer1TimeAdd = 40;
int Sealer2TimeAdd = 50;
int Heater1TimeAdd = 60;
int Heater2TimeAdd = 70;

void saveSettings()
{
	EEPROM.writeDouble(CalibrationAdd, parametersTimer[0]);
	EEPROM.writeDouble(LinearAdd, parametersTimer[1]);
	EEPROM.writeDouble(StepperTimeAdd, parametersTimer[2]);
	EEPROM.writeDouble(Sealer1TimeAdd, parametersTimer[3]);
	EEPROM.writeDouble(Sealer2TimeAdd, parametersTimer[4]);
	EEPROM.writeDouble(Heater1TimeAdd, parametersTimer[5]);
	EEPROM.writeDouble(Heater2TimeAdd, parametersTimer[6]);
}

void loadSettings()
{
	parametersTimer[0] = EEPROM.readDouble(CalibrationAdd);
	parametersTimer[1] = EEPROM.readDouble(LinearAdd);
	parametersTimer[2] = EEPROM.readDouble(StepperTimeAdd);
	parametersTimer[3] = EEPROM.readDouble(Sealer1TimeAdd);
	parametersTimer[4] = EEPROM.readDouble(Sealer2TimeAdd);
	parametersTimer[5] = EEPROM.readDouble(Heater1TimeAdd);
	parametersTimer[6] = EEPROM.readDouble(Heater2TimeAdd);
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

char testmachine_items[NUM_TESTMACHINE_ITEMS][MAX_ITEM_LENGTH] = { // array with item names
	{"VIBRATOR"},
	{"LINEAR"},
	{"STEPPER"},
	{"SEALER 1"},
	{"SEALER 2"},
	{"HEATER 1"},
	{"HEATER 2"},
	{"EXIT"}};

#define BUTTON_UP_PIN 4
#define BUTTON_SELECT_PIN 3
#define BUTTON_DOWN_PIN 2

int button_up_clicked = 0;
int button_select_clicked = 0;
int button_down_clicked = 0;

int item_selected = 0;
int setting_item_selected = 0;
int testmachine_item_selected = 0;

int item_sel_previous;
int item_sel_next;

int setting_item_sel_previous;
int setting_item_sel_next;

int testmachine_item_sel_previous;
int testmachine_item_sel_next;

bool settingEditFlag = false;
bool calibrateFlag = false;

int current_screen = 0; // 0 = menu, 1 = screenshot, 2 = qr
int calibrate_screen = 0;

int ena1 = 25;
int dir1 = 26;
int step1 = 27;
long currentPos1 = 0;
long lastPos1 = 0;
long speedStep1 = 8000;
long moveStep1 = 10000;

AccelStepper Roller(AccelStepper::FULL2WIRE, dir1, step1);

void setRollerStepper()
{
	Roller.setEnablePin(ena1);
	Roller.setPinsInverted(false, false, false);
	Roller.setMaxSpeed(speedStep1);
	Roller.setSpeed(speedStep1);
	Roller.setAcceleration(speedStep1 * 200);
	Roller.enableOutputs();
	lastPos1 = Roller.currentPosition();
}

void DisableStepper()
{
	Roller.disableOutputs();
}
void EnableStepper()
{
	Roller.enableOutputs();
}

void runRollerStepper()
{
	if (Roller.distanceToGo() == 0)
	{
		Roller.setCurrentPosition(0);
		Roller.move(moveStep1);
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
	Linear.setTimer(secondsToHHMMSS(parametersTimer[0]));
	Stepper.setTimer(secondsToHHMMSS(parametersTimer[1]));
	Sealer1.setTimer(secondsToHHMMSS(parametersTimer[2]));
	Sealer2.setTimer(secondsToHHMMSS(parametersTimer[3]));
	Heater1.setTimer(secondsToHHMMSS(parametersTimer[4]));
	Heater2.setTimer(secondsToHHMMSS(parametersTimer[5]));
}

bool TestMachineFlag = false;
bool vibroTestFlag, linearTestFlag, stepperTestFlag, sealer1TestFlag, sealer2TestFLag, heater1TestFlag, heater2TestFlag = false;

void RunTestMachine()
{
	if (vibroTestFlag == true)
	{
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
	if (Stepper.isTimerCompleted() == false)
	{
		EnableStepper();
		runRollerStepper();
	}
	else
	{
		DisableStepper();
	}

	Sealer1.run();
	Sealer2.run();
	Heater1.run();
	Heater2.run();
}

bool RunAutoFlag = false;

void RunAuto()
{
	switch (RunAutoFlag)
	{

	case 1:

		break;
	case 2:

		break;
	default:
		stopAll();
		RunAutoFlag = 0;
		break;
	}
}

void setupScreen()
{
	u8g2.setColorIndex(1); // set the color to white
	u8g2.begin();
	u8g2.setBitmapMode(1);

	// define pins for buttons
	// INPUT_PULLUP means the button is HIGH when not pressed, and LOW when pressed
	// since it´s connected between some pin and GND
	pinMode(BUTTON_UP_PIN, INPUT_PULLUP);	  // up button
	pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP); // select button
	pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);	  // down button
}

void selectPrevNextItem()
{
	// set correct values for the previous and next items
	item_sel_previous = item_selected - 1;
	if (item_sel_previous < 0)
	{
		item_sel_previous = NUM_MAIN_ITEMS - 1;
	} // previous item would be below first = make it the last
	item_sel_next = item_selected + 1;
	if (item_sel_next >= NUM_MAIN_ITEMS)
	{
		item_sel_next = 0;
	} // next item would be after last = make it the first

	// set correct values for the previous and next items
	setting_item_sel_previous = setting_item_selected - 1;
	if (setting_item_sel_previous < 0)
	{
		setting_item_sel_previous = NUM_SETTING_ITEMS - 1;
	} // previous item would be below first = make it the last
	setting_item_sel_next = setting_item_selected + 1;
	if (setting_item_sel_next >= NUM_SETTING_ITEMS)
	{
		setting_item_sel_next = 0;
	} // next item would be after last = make it the first

	// set correct values for the previous and next items
	testmachine_item_sel_previous = testmachine_item_selected - 1;
	if (testmachine_item_sel_previous < 0)
	{
		testmachine_item_sel_previous = NUM_TESTMACHINE_ITEMS - 1;
	} // previous item would be below first = make it the last
	testmachine_item_sel_next = testmachine_item_selected + 1;
	if (testmachine_item_sel_next >= NUM_TESTMACHINE_ITEMS)
	{
		testmachine_item_sel_next = 0;
	} // next item would be after last = make it the first
}

// 'scrollbar_background', 8x64px
const unsigned char bitmap_scrollbar_background[] PROGMEM = {
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x40,
	0x00,
	0x00,
};

// 'item_sel_outline', 128x21px
const unsigned char bitmap_item_sel_outline[] PROGMEM = {
	0xF8,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0x03,
	0x04,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x04,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0x02,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x0C,
	0xFC,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0x07,
	0xF8,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0x03,
};

const unsigned char epd_bitmap_gram_icon[] PROGMEM = {
	0x00, 0x3f, 0x00, 0x00, 0x21, 0x00, 0xc0, 0xff, 0x00, 0xe0, 0xff, 0x01, 0xf0, 0xff, 0x03, 0x70,
	0x80, 0x03, 0x38, 0x00, 0x07, 0x38, 0xff, 0x07, 0x3c, 0xff, 0x0f, 0x3c, 0xff, 0x0f, 0x3e, 0x0f,
	0x1f, 0x3e, 0x0f, 0x1f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x00, 0x3f, 0x7e, 0x80, 0x1f, 0xfc, 0xff, 0x0f, 0xf8,
	0xff, 0x07};

// ------------------ end generated bitmaps from image2cpp ---------------------------------
U8X8_SH1106_128X64_NONAME_HW_I2C u8x8(/* reset=*/U8X8_PIN_NONE);

void controls()
{
	if (RunAutoFlag == false)
	{
		if (current_screen == 0)
		{
			if ((digitalRead(BUTTON_UP_PIN) == LOW) && (button_up_clicked == 0))
			{
				item_selected = item_selected + 1;
				button_up_clicked = 1;
				if (item_selected >= NUM_MAIN_ITEMS)
				{
					item_selected = 0;
				}
			}
			else if ((digitalRead(BUTTON_DOWN_PIN) == LOW) && (button_down_clicked == 0))
			{
				item_selected = item_selected - 1;
				button_down_clicked = 1;
				if (item_selected < 0)
				{
					item_selected = NUM_MAIN_ITEMS - 1;
				}
			}
			if ((digitalRead(BUTTON_UP_PIN) == HIGH) && (button_up_clicked == 1))
			{
				button_up_clicked = 0;
			}
			if ((digitalRead(BUTTON_DOWN_PIN) == HIGH) && (button_down_clicked == 1))
			{
				button_down_clicked = 0;
			}
		}
		else if (current_screen == 1)
		{
			// SETTING SELECT
			if ((digitalRead(BUTTON_UP_PIN) == LOW) && (button_up_clicked == 0))
			{
				if (settingEditFlag == true)
				{
					button_up_clicked = 1;
					if (parametersTimer[setting_item_selected] >= parametersTimerMaxValue[setting_item_selected] - 1)
					{
						parametersTimer[setting_item_selected] = parametersTimerMaxValue[setting_item_selected];
					}
					else
					{
						parametersTimer[setting_item_selected] += 1;
					}
				}
				else
				{
					setting_item_selected = setting_item_selected + 1;
					button_up_clicked = 1;
					if (setting_item_selected >= NUM_SETTING_ITEMS)
					{
						setting_item_selected = 0;
					}
				}
			}
			else if ((digitalRead(BUTTON_DOWN_PIN) == LOW) && (button_down_clicked == 0))
			{
				if (settingEditFlag == true)
				{
					button_down_clicked = 1;
					if (parametersTimer[setting_item_selected] <= 0)
					{
						parametersTimer[setting_item_selected] = 0;
					}
					else
					{
						parametersTimer[setting_item_selected] -= 1;
					}
				}
				else
				{
					setting_item_selected = setting_item_selected - 1;
					button_down_clicked = 1;
					if (setting_item_selected < 0)
					{
						setting_item_selected = NUM_SETTING_ITEMS - 1;
					}
				}
			}
			if ((digitalRead(BUTTON_UP_PIN) == HIGH) && (button_up_clicked == 1))
			{
				button_up_clicked = 0;
			}
			if ((digitalRead(BUTTON_DOWN_PIN) == HIGH) && (button_down_clicked == 1))
			{
				button_down_clicked = 0;
			}
		}
		else if (current_screen == 2)
		{
			// TEST SELECT
			if ((digitalRead(BUTTON_UP_PIN) == LOW) && (button_up_clicked == 0))
			{
				testmachine_item_selected = testmachine_item_selected + 1;
				button_up_clicked = 1;
				if (testmachine_item_selected >= NUM_TESTMACHINE_ITEMS)
				{
					testmachine_item_selected = 0;
				}
			}
			else if ((digitalRead(BUTTON_DOWN_PIN) == LOW) && (button_down_clicked == 0))
			{
				testmachine_item_selected = testmachine_item_selected - 1;
				button_down_clicked = 1;
				if (testmachine_item_selected < 0)
				{
					testmachine_item_selected = NUM_TESTMACHINE_ITEMS - 1;
				}
			}
			if ((digitalRead(BUTTON_UP_PIN) == HIGH) && (button_up_clicked == 1))
			{
				button_up_clicked = 0;
			}
			if ((digitalRead(BUTTON_DOWN_PIN) == HIGH) && (button_down_clicked == 1))
			{
				button_down_clicked = 0;
			}
		}
	}

	if ((digitalRead(BUTTON_SELECT_PIN) == LOW) && (button_select_clicked == 0))
	{							   // select button clicked, jump between screens
		button_select_clicked = 1; // set button to clicked to only perform the action once
		if (current_screen == 0)
		{
			if (item_selected == 0)
			{
				current_screen = 1;
			}
			else if (item_selected == 1)
			{
				if (RunAutoFlag == true)
				{
					RunAutoFlag = false;
				}
				else
				{
					RunAutoFlag = true;
				}
			}
			else
			{
				current_screen = 2;
				TestMachineFlag = true;
			}
		}
		else if (current_screen == 1)
		{
			if (setting_item_selected == NUM_SETTING_ITEMS - 1)
			{
				current_screen = 0;
				setting_item_selected = 0;
				item_selected = 0;
				saveSettings();
				setTimers();
			}
			else if (setting_item_selected == 0)
			{
				if (setting_item_selected == 0 && calibrateFlag == false)
				{
					calibrateFlag = true;
				}
				else
				{
					if (setting_item_selected == 0 && calibrateFlag == true && calibrate_screen == 0)
					{
					}
					else if (setting_item_selected == 0 && calibrateFlag == true && calibrate_screen == 1)
					{
					}
				}
			}
			else
			{
				if (settingEditFlag == false)
				{
					settingEditFlag = true;
				}
				else
				{
					settingEditFlag = false;
				}
			}
		}
		else
		{
			if (testmachine_item_selected == NUM_TESTMACHINE_ITEMS - 1)
			{
				current_screen = 0;
				testmachine_item_selected = 0;
				item_selected = 0;
				TestMachineFlag = false;
				stopAll();
			}
			else if (testmachine_item_selected == 0)
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
			else if (testmachine_item_selected == 1)
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
			else if (testmachine_item_selected == 2)
			{
				if (Stepper.isTimerCompleted() == true)
				{
					Stepper.start();
				}
				else
				{
					Stepper.stop();
				}
			}
			else if (testmachine_item_selected == 3)
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
			else if (testmachine_item_selected == 4)
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
			else if (testmachine_item_selected == 5)
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
			else if (testmachine_item_selected == 6)
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
	}

	if ((digitalRead(BUTTON_SELECT_PIN) == HIGH) && (button_select_clicked == 1))
	{ // unclick
		button_select_clicked = 0;
	}

	selectPrevNextItem();
}

void printScreen()
{
	u8g2.clearBuffer(); // clear buffer for storing display content in RAM

	if (current_screen == 0)
	{ // MENU SCREEN
		if (current_screen == 0 && RunAutoFlag == true)
		{
			printRunAuto("Test", currentWeight);
		}
		else
		{
			// selected item background
			u8g2.drawXBMP(0, 22, 128, 21, bitmap_item_sel_outline);

			// draw previous item as icon + label
			u8g2.setFont(u8g_font_7x14);
			u8g2.drawStr(4, 15, menu_items[item_sel_previous]);

			// draw selected item as icon + label in bold font
			u8g2.setFont(u8g_font_7x14B);
			u8g2.drawStr(4, 15 + 20 + 2, menu_items[item_selected]);
			//   u8g2.drawXBMP( 4, 24, 16, 16, bitmap_icons[item_selected]);

			// draw next item as icon + label
			u8g2.setFont(u8g_font_7x14);
			u8g2.drawStr(4, 15 + 20 + 20 + 2 + 2, menu_items[item_sel_next]);
			//   u8g2.drawXBMP( 4, 46, 16, 16, bitmap_icons[item_sel_next]);

			// draw scrollbar background
			u8g2.drawXBMP(128 - 8, 0, 8, 64, bitmap_scrollbar_background);

			// draw scrollbar handle
			u8g2.drawBox(125, 64 / NUM_MAIN_ITEMS * item_selected, 3, 64 / NUM_MAIN_ITEMS);
		}
	}
	else if (current_screen == 1)
	{ // SETTING
		if (current_screen == 1 && settingEditFlag == true)
		{
			u8g2.setFont(u8g_font_7x14B);
			u8g2.drawStr(4, 15, setting_items[setting_item_selected]);
			u8g2.setFont(u8g_font_7x14);
			u8g2.drawStr(4, 15 + 20 + 2, "Time:");
			char timeval[10];
			itoa(parametersTimer[setting_item_selected], timeval, 10);
			char secChar[5] = " SEC";
			strcat(timeval, secChar);
			u8g2.drawStr(4, 15 + 20 + 20 + 2 + 2, timeval);
		}
		else
		{
			// selected item background
			u8g2.drawXBMP(0, 22, 128, 21, bitmap_item_sel_outline);

			// draw previous item as icon + label
			u8g2.setFont(u8g_font_7x14);
			u8g2.drawStr(4, 15, setting_items[setting_item_sel_previous]);

			// draw selected item as icon + label in bold font
			u8g2.setFont(u8g_font_7x14B);
			u8g2.drawStr(4, 15 + 20 + 2, setting_items[setting_item_selected]);
			//   u8g2.drawXBMP( 4, 24, 16, 16, bitmap_icons[item_selected]);

			// draw next item as icon + label
			u8g2.setFont(u8g_font_7x14);
			u8g2.drawStr(4, 15 + 20 + 20 + 2 + 2, setting_items[setting_item_sel_next]);
			//   u8g2.drawXBMP( 4, 46, 16, 16, bitmap_icons[item_sel_next]);

			// draw scrollbar background
			u8g2.drawXBMP(128 - 8, 0, 8, 64, bitmap_scrollbar_background);

			// draw scrollbar handle
			u8g2.drawBox(125, 64 / NUM_SETTING_ITEMS * setting_item_selected, 3, 64 / NUM_SETTING_ITEMS);
		}
	}
	else if (current_screen == 2)
	{ // TEST MACHINE

		// selected item background
		u8g2.drawXBMP(0, 22, 128, 21, bitmap_item_sel_outline);

		// draw previous item as icon + label
		u8g2.setFont(u8g_font_7x14);
		u8g2.drawStr(4, 15, testmachine_items[testmachine_item_sel_previous]);
		u8g2.setFont(u8g_font_7x14);
		switch (testmachine_item_sel_previous)
		{
		case 0:
			if (vibroTestFlag == true)
			{
				u8g2.drawStr(90, 15, "ON");
			}
			else
			{
				u8g2.drawStr(90, 15, "OFF");
			}
			break;
		case 1:
			if (linearTestFlag == true)
			{
				u8g2.drawStr(90, 15, "ON");
			}
			else
			{
				u8g2.drawStr(90, 15, "OFF");
			}
			break;
		case 2:
			if (Stepper.isTimerCompleted() == true)
			{
				u8g2.drawStr(90, 15, "OFF");
			}
			else
			{
				u8g2.drawStr(90, 15, "ON");
			}
			break;
		case 3:
			if (Sealer1.isTimerCompleted() == true)
			{
				u8g2.drawStr(90, 15, "OFF");
			}
			else
			{
				u8g2.drawStr(90, 15, "ON");
			}
			break;
		case 4:
			if (Sealer2.isTimerCompleted() == true)
			{
				u8g2.drawStr(90, 15, "OFF");
			}
			else
			{
				u8g2.drawStr(90, 15, "ON");
			}
			break;
		case 5:
			if (Heater1.isTimerCompleted() == true)
			{
				u8g2.drawStr(90, 15, "OFF");
			}
			else
			{
				u8g2.drawStr(90, 15, "ON");
			}
			break;
		case 6:
			if (Heater2.isTimerCompleted() == true)
			{
				u8g2.drawStr(90, 15, "OFF");
			}
			else
			{
				u8g2.drawStr(90, 15, "ON");
			}
			break;
		default:
			break;
		}

		// draw selected item as icon + label in bold font
		u8g2.setFont(u8g_font_7x14B);
		u8g2.drawStr(4, 15 + 20 + 2, testmachine_items[testmachine_item_selected]);

		u8g2.setFont(u8g_font_7x14B);
		switch (testmachine_item_selected)
		{
		case 0:
			if (vibroTestFlag == true)
			{
				u8g2.drawStr(90, 15 + 20 + 2, "ON");
			}
			else
			{
				u8g2.drawStr(90, 15 + 20 + 2, "OFF");
			}
			break;
		case 1:
			if (linearTestFlag == true)
			{
				u8g2.drawStr(90, 15 + 20 + 2, "ON");
			}
			else
			{
				u8g2.drawStr(90, 15 + 20 + 2, "OFF");
			}
			break;
		case 2:
			if (Stepper.isTimerCompleted() == true)
			{
				u8g2.drawStr(90, 15 + 20 + 2, "OFF");
			}
			else
			{
				u8g2.drawStr(90, 15 + 20 + 2, "ON");
			}
			break;
		case 3:
			if (Sealer1.isTimerCompleted() == true)
			{
				u8g2.drawStr(90, 15 + 20 + 2, "OFF");
			}
			else
			{
				u8g2.drawStr(90, 15 + 20 + 2, "ON");
			}
			break;
		case 4:
			if (Sealer2.isTimerCompleted() == true)
			{
				u8g2.drawStr(90, 15 + 20 + 2, "OFF");
			}
			else
			{
				u8g2.drawStr(90, 15 + 20 + 2, "ON");
			}
			break;
		case 5:
			if (Heater1.isTimerCompleted() == true)
			{
				u8g2.drawStr(90, 15 + 20 + 2, "OFF");
			}
			else
			{
				u8g2.drawStr(90, 15 + 20 + 2, "ON");
			}
			break;
		case 6:
			if (Heater2.isTimerCompleted() == true)
			{
				u8g2.drawStr(90, 15 + 20 + 2, "OFF");
			}
			else
			{
				u8g2.drawStr(90, 15 + 20 + 2, "ON");
			}
			break;
		default:
			break;
		}
		//   u8g2.drawXBMP( 4, 24, 16, 16, bitmap_icons[item_selected]);

		// draw next item as icon + label
		u8g2.setFont(u8g_font_7x14);
		u8g2.drawStr(4, 15 + 20 + 20 + 2 + 2, testmachine_items[testmachine_item_sel_next]);
		u8g2.setFont(u8g_font_7x14);
		switch (testmachine_item_sel_next)
		{
		case 0:
			if (vibroTestFlag == true)
			{
				u8g2.drawStr(90, 15 + 20 + 20 + 2 + 2, "ON");
			}
			else
			{
				u8g2.drawStr(90, 15 + 20 + 20 + 2 + 2, "OFF");
			}
			break;
		case 1:
			if (linearTestFlag == true)
			{
				u8g2.drawStr(90, 15 + 20 + 20 + 2 + 2, "ON");
			}
			else
			{
				u8g2.drawStr(90, 15 + 20 + 20 + 2 + 2, "OFF");
			}
			break;
		case 2:
			if (Stepper.isTimerCompleted() == true)
			{
				u8g2.drawStr(90, 15 + 20 + 20 + 2 + 2, "OFF");
			}
			else
			{
				u8g2.drawStr(90, 15 + 20 + 20 + 2 + 2, "ON");
			}
			break;
		case 3:
			if (Sealer1.isTimerCompleted() == true)
			{
				u8g2.drawStr(90, 15 + 20 + 20 + 2 + 2, "OFF");
			}
			else
			{
				u8g2.drawStr(90, 15 + 20 + 20 + 2 + 2, "ON");
			}
			break;
		case 4:
			if (Sealer2.isTimerCompleted() == true)
			{
				u8g2.drawStr(90, 15 + 20 + 20 + 2 + 2, "OFF");
			}
			else
			{
				u8g2.drawStr(90, 15 + 20 + 20 + 2 + 2, "ON");
			}
			break;
		case 5:
			if (Heater1.isTimerCompleted() == true)
			{
				u8g2.drawStr(90, 15 + 20 + 20 + 2 + 2, "OFF");
			}
			else
			{
				u8g2.drawStr(90, 15 + 20 + 20 + 2 + 2, "ON");
			}
			break;
		case 6:
			if (Heater2.isTimerCompleted() == true)
			{
				u8g2.drawStr(90, 15 + 20 + 20 + 2 + 2, "OFF");
			}
			else
			{
				u8g2.drawStr(90, 15 + 20 + 20 + 2 + 2, "ON");
			}
			break;
		default:
			break;
		}
		//   u8g2.drawXBMP( 4, 46, 16, 16, bitmap_icons[item_sel_next]);

		// draw scrollbar background
		u8g2.drawXBMP(128 - 8, 0, 8, 64, bitmap_scrollbar_background);

		// draw scrollbar handle
		u8g2.drawBox(125, 64 / NUM_TESTMACHINE_ITEMS * testmachine_item_selected, 3, 64 / NUM_TESTMACHINE_ITEMS);
	}

	u8g2.sendBuffer(); // send buffer from RAM to display controller
}

void setup()
{
	setupScreen();
	// saveSettings();
	loadSettings();
	setTimers();
	setRollerStepper();
	stopAll();
}

void loop()
{
	printScreen();
	controls();
	if (TestMachineFlag == true)
	{
		RunTestMachine();
	}
	currentWeight = scale.get_units(10);

}

void printRunAuto(String job, int weight)
{
	u8g2.clearBuffer(); // clear buffer for storing display content in RAM
	u8g2.drawXBMP(4, 38, 22, 22, epd_bitmap_gram_icon);

	u8g2.setFont(u8g2_font_t0_14b_tf);
	u8g2.drawStr(40, 15, "Status:");

	char *jobArr = new char[job.length() + 1];
	strcpy(jobArr, job.c_str());
	u8g2.setFont(u8g2_font_t0_14b_tf);
	u8g2.drawStr(getXCoordinateCenter(job), 15 + 15, jobArr);

	char cstr[10];
	itoa(weight, cstr, 10);
	String g = " G";
	strcat(cstr, g.c_str());
	u8g2.setFont(u8g2_font_logisoso22_tr);
	u8g2.drawStr(4 + 22 + 4 + 10, 38 + 22, cstr);
	u8g2.sendBuffer(); // send buffer from RAM to display controller
}

int getXCoordinateCenter(String text)
{
	char *char_array = new char[text.length() + 1];
	strcpy(char_array, text.c_str());
	return (128 - u8g2.getStrWidth(char_array)) / 2;
}