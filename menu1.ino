#include <LiquidCrystal.h>

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

enum State { MAIN_MENU, PARAM_MENU, EDIT, CALIB_MENU, CALIB_EDIT };
State state = MAIN_MENU;

int mainIndex = 0;
int menuIndex = 0;
int calibIndex = 0;
float voltFactor = 1.0;
float currFactor = 1.0;

int getButton() {

  static int lastStable = 0;
  static unsigned long lastTime = 0;

  int val = analogRead(A0);
  int btn = 0;

  if (val < 150) btn = 2;
  else if (val < 350) btn = 3;
  else if (val < 800) btn = 5;
  else btn = 0;

  if (btn != lastStable) {
    if (millis() - lastTime > 50) {
      lastStable = btn;
      lastTime = millis();
      if (btn != 0) return btn;
    }
  }

  return 0;
}

//PARAMETER MENU 
struct Menu {
  const char* name;
  const char* options[5];
  int count;
  int index;
};

Menu menu[] = {
  {"Phase", {"1P","3P"}, 2, 0},
  {"Voltage", {"230","380","415"}, 3, 0},
  {"Freq", {"50","60"}, 2, 0},
  {"OverVolt", {"240","250","260"}, 3, 0},
  {"UnderVolt", {"170","180","190"}, 3, 0},
  {"OverCurr", {"10","20","30"}, 3, 0},
  {"Overload", {"1000","1500","2000"}, 3, 0},
  {"Timer", {"5","10","15"}, 3, 0},
  {"BattHigh", {"14","15","16"}, 3, 0},
  {"BattLow", {"10","11","12"}, 3, 0},
  {"DispTime", {"1","2","3","4","5"}, 5, 1},
  {"Priority", {"NORMAL","ALT"}, 2, 0},
  {"ChgTime", {"1","2","3","4","5"}, 5, 1}
};

#define TOTAL 13

// CALIB MENU 
const char* calibMenu[] = {"Battery Voltage", "Battery Current"};

int setValue = 0;
int Raw = 0;

void showMain() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("MAIN MENU");

  lcd.setCursor(0,1);
  if (mainIndex == 0) lcd.print(">PARAMETERS");
  else lcd.print(">CALIBRATION");
}

void showParamMenu() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("PARAM MENU");

  lcd.setCursor(0,1);
  lcd.print(menu[menuIndex].name);
}

void showCalibMenu() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("CALIB MENU");

  lcd.setCursor(0,1);
  lcd.print(calibMenu[calibIndex]);
}

void showCalibEdit() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Raw:");
  lcd.print(Raw);

  lcd.setCursor(0,1);
  lcd.print("Set:");
  lcd.print(setValue);
}

void setup() {
  lcd.begin(16,2);
  showMain();
}

void loop() {

  int btn = getButton();

  // MAIN MENU 
  if (state == MAIN_MENU) {

    if (btn == 2 || btn == 3) {
      mainIndex = 1 - mainIndex;
      showMain();
    }

    if (btn == 5) {
      if (mainIndex == 0) {
        state = PARAM_MENU;
        menuIndex = 0;
        showParamMenu();
      } else {
        state = CALIB_MENU;
        calibIndex = 0;
        showCalibMenu();
      }
    }
  }

  //PARAM MENU 
  else if (state == PARAM_MENU) {

    if (btn == 2) {
      menuIndex = (menuIndex - 1 + TOTAL) % TOTAL;
      showParamMenu();
    }

    if (btn == 3) {
      menuIndex = (menuIndex + 1) % TOTAL;
      showParamMenu();
    }

    if (btn == 5) {
      state = EDIT;

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(menu[menuIndex].name);

      lcd.setCursor(0,1);
      lcd.print(menu[menuIndex].options[menu[menuIndex].index]);
    }
  }

  // PARAM EDIT
  else if (state == EDIT) {

    if (btn == 2) {
      menu[menuIndex].index++;
      if (menu[menuIndex].index >= menu[menuIndex].count)
        menu[menuIndex].index = 0;

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(menu[menuIndex].name);
      lcd.setCursor(0,1);
      lcd.print(menu[menuIndex].options[menu[menuIndex].index]);
    }

    if (btn == 3) {
      menu[menuIndex].index--;
      if (menu[menuIndex].index < 0)
        menu[menuIndex].index = menu[menuIndex].count - 1;

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(menu[menuIndex].name);
      lcd.setCursor(0,1);
      lcd.print(menu[menuIndex].options[menu[menuIndex].index]);
    }

    if (btn == 5) {
      state = PARAM_MENU;
      showParamMenu();
    }
  }

  //  CALIB MENU 
  else if (state == CALIB_MENU) {

    if (btn == 2) {
      calibIndex = (calibIndex - 1 + 2) % 2;
      showCalibMenu();
    }

    if (btn == 3) {
      calibIndex = (calibIndex + 1) % 2;
      showCalibMenu();
    }

    if (btn == 5) {
      Raw = analogRead(calibIndex == 0 ? A1 : A2);
      setValue = 0;
      state = CALIB_EDIT;
      showCalibEdit();
    }
  }

  // CALIB EDIT
  else if (state == CALIB_EDIT) {

    if (btn == 2) {
      setValue++;
      showCalibEdit();
    }

    if (btn == 3) {
      setValue--;
      showCalibEdit();
    }

    if (btn == 5) {

      if (Raw != 0) {

        float factor = (float)setValue / Raw;

        if (calibIndex == 0) {
          voltFactor = factor;
        } else {
          currFactor = factor;
        }

        lcd.clear();
        lcd.print("Factor:");
        lcd.setCursor(0,1);
        lcd.print(factor, 4);

      } else {
        lcd.clear();
        lcd.print("Error Raw=0");
      }

      delay(1500);

      state = CALIB_MENU;
      showCalibMenu();
    }
  }
}
 
