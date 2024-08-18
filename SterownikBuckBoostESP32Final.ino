#include <Arduino.h>
#include <FS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include "SPIFFS.h"
#include "driver/mcpwm.h"
#include "driver/adc.h"

//const char* ssid     = "lab32";
//const char* password = "J4jeczk0";

//const char* ssid     = "Michal";
//const char* password = "E8W7WTEM79NKG";

const char* ssid     = "KatedraMetrologii";
const char* password = "123456789";

//pwm config data
#define GPIO_PWM0A_OUT 13  //Q1
#define GPIO_PWM0B_OUT 12  //Q2
#define GPIO_PWM1B_OUT 25  //Q3
#define GPIO_PWM1A_OUT 26  //Q4

#define GPIO_INTR 14
#define POWER3_3V 27
#define ANALOG1 34

mcpwm_config_t pwm_config;  // initialize "pwm_config" structure

//Variables declaration
int frequency = 5000;
int deadTime = 10;
int D = 0;
int mode = 0;
bool status = false;
bool statusBuck = true;
float potRead2 = 0;
float IGiv = 0;
float IAct = 0;
double currentLimitVal = 0; 
bool emergencyStop = false;

//Intervals variables declaration
unsigned long previousMillis = 0;
const long interval1000 = 1000;
const long interval10 = 10;

// Create AsyncWebServer instance on port 80
AsyncWebServer server(80);

//TEZ interrupt function (when MCPWM Timer equals 0)
void IRAM_ATTR MCPWM_ISR(void*) {
  WRITE_PERI_REG(0x3FF5E11C, BIT(3));    //clear TEZ interrupt
  digitalWrite(GPIO_INTR, !digitalRead(GPIO_INTR));
  if (status == true) {
    potRead2 = analogRead(ANALOG1);
    IAct = potRead2*3.3/4095;
  }
  digitalWrite(GPIO_INTR, !digitalRead(GPIO_INTR));
}

void setup() {
  Serial.begin(115200);         // Start the Serial communication to send messages to the computer
  Serial.setTimeout(500);
  Serial.println('\n');

  pinMode(GPIO_INTR, OUTPUT);          // sets the digital pin 17 as output
  pinMode(POWER3_3V, OUTPUT);          // sets the digital pin 17 as output
  digitalWrite(POWER3_3V, HIGH);       //temperary for potentiometer power source

  WifiSetup();     // Connecting to WiFi

  SPIFFS.begin();  // SPI file system startup

  delay(1000);

  PWMSetup();  // Initialize PWM signals on GPIO4, GPIO16

  //ADC config setup
  //adc1_config_width(ADC_WIDTH_BIT_12);
  //adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_6); (max 1,5 V on adc input!!!)

  //Routes for send homepage
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    //request->send_P(200, "text/html", PAGE_MAIN);
    request->send(SPIFFS, "/HomePage.htm");
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/script.js", "text/javascript");
  });

  server.on("/img/schemat.png", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/img/schemat.png");
  });

  // Routes for data request handling
  server.on("/GETSTATE", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument doc(1024);
    doc["status"] = status;
    doc["statusBuck"] = statusBuck;
    doc["D"] = D;
    doc["IGiv"] = IGiv;
    doc["IAct"] = IAct;
    doc["emergencyStop"] = emergencyStop;
    serializeJson(doc, *response);
    request->send(response);
  });

  server.on("/START", HTTP_GET, [](AsyncWebServerRequest * request) {
    status = true;

    char param[4];
    AsyncWebParameter* val = request->getParam("mode");
    strcpy(param, val->value().c_str());
    mode = atoi(param);

    if (mode == 1 && emergencyStop == false) {  //PWM duty cycle
      PWMDutyCycleMode();
    }
    else if (mode == 0 && emergencyStop == false ) { //PID mode
      CurrentPIDMode();
    }
    request->send(200, "text/plain", "Start");
  });

  server.on("/STOP", HTTP_GET, [](AsyncWebServerRequest * request) {
    status = false;
    StopAllFunc();
    request->send(200, "text/plain", "Stop");
  });

  server.on("/RESET", HTTP_GET, [](AsyncWebServerRequest * request) {
    emergencyStop = false;
    request->send(200, "text/plain", "Reset");
  });

  server.on("/SETD", HTTP_GET, [](AsyncWebServerRequest * request) {
    char param[4];
    AsyncWebParameter* val = request->getParam("val");
    strcpy(param, val->value().c_str());
    D = atoi(param);
    request->send(200, "text/plain", "D Set");
  });

    server.on("/APPLYD", HTTP_GET, [](AsyncWebServerRequest * request) {
    char param[4];
    AsyncWebParameter* val = request->getParam("val");
    strcpy(param, val->value().c_str());
    D = atoi(param);
    if (D > 0 && D <=100) {
      statusBuck = true;
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, 0);  //Q4 set signal low
      if(D!=100) {
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, D);  //Q1 set signal duty
      }
      else {
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 99);  //Q1 set signal high
      }
    }
    else if (D > 100 && D <= 150) {
    statusBuck = false;
      mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 99);  //Q1 set signal high
      mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, D-99);  //Q4 set signal duty
    }
    request->send(200, "text/plain", "D Set");
  });


  server.on("/SETDATA", HTTP_GET, [](AsyncWebServerRequest * request) {
    char param[4];
    AsyncWebParameter* val1 = request->getParam("DeadTime");
    strcpy(param, val1->value().c_str());
    deadTime = atoi(param);
    AsyncWebParameter* val2 = request->getParam("I");
    strcpy(param, val2->value().c_str());
    IGiv = atoi(param);
    AsyncWebParameter* val3 = request->getParam("CurrentLimit");
    strcpy(param, val3->value().c_str());
    currentLimitVal = atof(param);
    Serial.print(currentLimitVal);
    request->send(200, "text/plain", "dead time set");
  });

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();
}

void WifiSetup() {
  WiFi.begin(ssid, password);             // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.println(" ...");

  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(".");
    Serial.print(' ');
  }

  Serial.println('\n');
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer
}

void PWMSetup() {
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_PWM0A_OUT);  // initializes gpio "GPIO_PWM0A_OUT" for MCPWM
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, GPIO_PWM0B_OUT);  // initializes gpio "GPIO_PWM0B_OUT" for MCPWM
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, GPIO_PWM1A_OUT);  // initializes gpio "GPIO_PWM1A_OUT" for MCPWM
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1B, GPIO_PWM1B_OUT);  // initializes gpio "GPIO_PWM1B_OUT" for MCPWM

  //PWM CONFIG
  pwm_config.frequency = frequency * 2;
  pwm_config.counter_mode = MCPWM_UP_DOWN_COUNTER;       // creates symetrical vaweforms
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
  pwm_config.cmpr_a = 0;          // Duty cycle of PWMxA (LOW level)
  pwm_config.cmpr_b = 0;          // Duty cycle of PWMxB (HIGH level)
  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);  // Configure PWM0A & PWM0B with settings
  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &pwm_config);  // Configure PWM1A & PWM1B with settings

  // set all low
  mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A);
  mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_B);
  mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_A);
  mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_B);
}

void PWMDutyCycleMode() {

  mcpwm_isr_register(MCPWM_UNIT_0, MCPWM_ISR, NULL, ESP_INTR_FLAG_IRAM, NULL);
  WRITE_PERI_REG(0x3FF5E110, BIT(3));

    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0); // High duty type on PWM0A
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, MCPWM_DUTY_MODE_0); // High duty type on PWM0B
    mcpwm_deadtime_enable(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, deadTime, deadTime);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, MCPWM_DUTY_MODE_0); // High duty type on PWM0A
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_B, MCPWM_DUTY_MODE_0); // High duty type on PWM0B
    mcpwm_deadtime_enable(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, deadTime, deadTime);

  if (D > 0 && D <=100) {
    statusBuck = true;
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, 0);  //Q4 set signal low
    if(D!=100) {
      mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, D);  //Q1 set signal duty
    }
    else {
      mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 99);  //Q1 set signal high
    }
  }
  else if (D > 100 && D <= 150) {
    statusBuck = false;
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 99);  //Q1 set signal high
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, D-99);  //Q4 set signal duty
  }
}

void CurrentPIDMode() {

}

void StopAllFunc() {
    mcpwm_deadtime_disable(MCPWM_UNIT_0, MCPWM_TIMER_0);
    mcpwm_deadtime_disable(MCPWM_UNIT_0, MCPWM_TIMER_1);
    mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A); //zmienić na ustawienie 0
    mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_B);
    mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_A);
    mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_B);
}

void loop() {
  //update values and print to serial
  unsigned long currentMillis = millis();

  //Wifi reconnect
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval1000)) {
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }

  if (currentMillis - previousMillis >= interval1000) {
    if(status == true) {
      if(IAct > currentLimitVal) {
       emergencyStop = true;
       status = false;
       StopAllFunc();
      }
     }
    previousMillis = currentMillis;
  }

}
