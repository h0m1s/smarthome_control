#include <Arduino.h>
#include "OneButton.h"
#include <RotaryEncoder.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <images.h>

#include <WiFi.h>
#include <PubSubClient.h>

#define button_pin 2
#define rotary_A 0
#define rotary_B 1
#define ring_PIN 3


#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64
#define OLED_RESET -1


// функции

void singleClick();
void doubleClick();
void pressStart();


void setup_wifi();
void setup_mqtt();
void send_mqtclient(int pos);
void send_mqtt_request(const char *sensor);
void checkPosition();
void accept_reply(char* topic, byte* payload, unsigned int length);
void print_message(char message[]);
void start_screen();
void encoder_read();
void after_encoder(int pos, int dir);
void ring_screen(int value);
void dimmer_screen(char message[]);
void regular_mqtt_sensor();
void startTimer();
void stopTimer();
void dimmer_encoder(int dir);
void send_new_value(int value);
void initial_screen();
void error_sensor_screen();
void reconnect();
void reject_func();

void stopTimer_request();
void startTimer_request();


void blinds_screen(char message[]);
void open_blinds();
void close_blinds();
void accept_reply_blinds(char message[]);
void blinds_encoder(int dir);
void startTimer_blinds();
void stopTimer_blinds();