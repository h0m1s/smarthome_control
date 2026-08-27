#include "main.h"

OneButton button(button_pin, true);
RotaryEncoder *encoder = nullptr;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_NeoPixel ring(16, ring_PIN, NEO_GRB + NEO_KHZ800);

//wifi and mqtt
WiFiClient espClient;
PubSubClient client(espClient);


// Для ассинзронного вызова
TimerHandle_t timer_mqtt_sensor;
TimerHandle_t timer_mqtt_request;
TimerHandle_t timer_blinds_encoder;

//wifi
const char* ssid = "";
const char* password = "";
const char* hostname = "";

//mqtt
const char* mqtt_server = ""; 
const char* mqtt_user = "mqtt_test"; 
const char* mqtt_password = "mqtt_test";
const char* topic = "home/esp32/";

// для штор
const char* command_topic   = "curtains/set";
const char* position_topic  = "curtains/state";
const char* availability_topic = "curtains/availability";
boolean enable_blinds = false;

//сенсоры и объекты умного дома 
const char *objects_home[] = {
    "Sensor",
    "Blinds",
    "Dimmer"
};

// На какокм экране сейчас находимся главный/диммер
static int flag_screen = 0; 

// Какой элемент сейчас на главном экране от 0 до 2 
static int current_object = 1;

long lastMsg = 0;
int counter = 0;

// Значение яркосить диммера
static int brightness = -1;

static int blinds_position = -1;



void timer_callback_mqtt_sensor(TimerHandle_t xTimer) {
  regular_mqtt_sensor();
}

void timer_callback_mqtt_request(TimerHandle_t xTimer) {
  reject_func();
}
void timer_callback_blinds(TimerHandle_t xTimer) {
  char value[20];
  sprintf(value, "%d", blinds_position);
  client.publish(command_topic, value);
}

void setup() {
  ring.begin();
  ring.show();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  initial_screen();
  Serial.begin(115200);

  pinMode(button_pin, INPUT_PULLUP);
  button.attachClick(singleClick);
  button.attachDoubleClick(doubleClick);
  button.setPressMs(1000);
  button.attachLongPressStart(pressStart);

  encoder = new RotaryEncoder(rotary_A, rotary_B, RotaryEncoder::LatchMode::TWO03);
  attachInterrupt(digitalPinToInterrupt(rotary_A), checkPosition, CHANGE);
  attachInterrupt(digitalPinToInterrupt(rotary_B), checkPosition, CHANGE);

  client.setCallback(accept_reply);

  timer_mqtt_sensor = xTimerCreate(
    "Timer1",
    pdMS_TO_TICKS(5 * 60 * 1000), // 5 минут
    pdTRUE,   // авто-повтор
    (void*)0,
    timer_callback_mqtt_sensor
  );
  timer_mqtt_request  = xTimerCreate(
    "MyTimer",
    pdMS_TO_TICKS(5 * 1000), // 5 секунд
    pdFALSE,   // без авто-повтора
    (void*)0,
    timer_callback_mqtt_request
  );

  timer_blinds_encoder  = xTimerCreate(
    "MyTimer",
    pdMS_TO_TICKS(1 * 1000), // 5 секунд
    pdFALSE,   // без авто-повтора
    (void*)0,
    timer_callback_blinds
  );

  delay(5000);
  setup_wifi();
  setup_mqtt();
  start_screen();
}

void loop() {
  encoder_read();
  button.tick();
  client.loop();
}

void singleClick(){
  if (flag_screen == 0){
    if (current_object != 1) send_mqtt_request(objects_home[current_object]);
    else if (current_object == 1){
      client.subscribe(position_topic);
      client.subscribe(availability_topic);
      startTimer_request();
    }
    flag_screen = 1;
    brightness = -1;
  } else {
    if (current_object == 1) open_blinds();
  }
}
void doubleClick() {
  if (current_object == 1 && flag_screen == 1) close_blinds();
} 

void pressStart() {
  if (flag_screen == 1){
    start_screen();
    flag_screen = 0;
    client.disconnect();
    client.connect(hostname, mqtt_user, mqtt_password);
    ring.clear();
    ring.show();
    if (current_object == 0) stopTimer();
    stopTimer_request();
    enable_blinds = false;
  }
}


void setup_wifi() {

  delay(500);
  WiFi.begin(ssid, password);
  WiFi.setHostname(hostname);
  Serial.println("Connecting to WiFi...");

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    attempts++;

    if (attempts > 20) {
      Serial.println("\nWiFi failed!");
      Serial.print("Status: ");
      Serial.println(WiFi.status());
      return;
    }
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void setup_mqtt(){
  Serial.print("Connecting to MQTT... ");
  client.setServer(mqtt_server, 1883);
  while(!client.connect(hostname, mqtt_user, mqtt_password)) {
    Serial.print("-");
    delay(1000);
  }
}

void send_mqtclient(int pos){
  char poss[10];
  snprintf(poss, sizeof(poss), "%d", pos);
  client.publish(topic, poss);
}


void send_mqtt_request(const char *sensor){
  char pub_topic[100];
  char sub_topic[100];
  strcpy(pub_topic, topic);
  strcat(pub_topic, sensor);
  strcpy(sub_topic, pub_topic);

  strcat(pub_topic, "/pub");
  strcat(sub_topic, "/sub");

  client.subscribe(sub_topic); 
  client.publish(pub_topic, "Request"); // Делаем сообщения запроса на топик
  if (current_object == 0) startTimer();
  startTimer_request();
}


void checkPosition() {
  encoder->tick();
}


void accept_reply(char* topic, byte* payload, unsigned int length){
  stopTimer_request();
  char msg[length + 1];
  memcpy(msg, payload, length);
  msg[length] = '\0';
  if (current_object == 0){
    print_message(msg);
  }
  if (current_object == 2){
    dimmer_screen(msg);
  }
  if (current_object == 1){
    accept_reply_blinds(msg);
  }
}

void print_message(char message[]){

  char *temp = strtok(message, ";");
  char *humid = strtok(NULL, ";");

  if (strcmp(temp, "unknown") == 0 || strcmp(humid, "unknown") == 0) {
    error_sensor_screen();
  } else{
    display.clearDisplay();
    display.setTextSize(2); 
    display.setTextColor(WHITE);

    display.setCursor(0, 16);
    display.print("Tem: ");
    display.println(temp);

    display.setCursor(0, 38);
    display.print("Hum: ");
    display.println(humid);

    display.display();
  }
}


void start_screen(){
  flag_screen = 0;
  display.clearDisplay();
  display.drawBitmap(32, 0, images[current_object], 64, 64, WHITE);
  display.display();
}

void encoder_read(){
  static int pos = 0;
  int newPos = encoder->getPosition();
  if (pos != newPos) {
    int dir = (int)(encoder->getDirection());
    pos = newPos;
    after_encoder(pos, dir);
  }
}

void after_encoder(int pos, int dir){
  Serial.println(enable_blinds);
  if (flag_screen == 0){ // главный экран
    if (dir == -1 && (current_object == 0)) current_object = 2;
    else if (dir == 1 && (current_object == 2)) current_object = 0;
    else current_object+= dir;
    start_screen();
  } else { // экран датчиков
    if (current_object == 2 && brightness != -1){
      dimmer_encoder(dir);
    } else if (current_object == 1 && enable_blinds == true) blinds_encoder(dir);
  }
}



void dimmer_screen(char message[]){

  char *ptr = strchr(message, ':');

  if (ptr != NULL) {
     ptr++;
     brightness = (int)strtol(ptr, NULL, 10);
  }

  ring_screen(brightness);
  display.clearDisplay();
  display.setTextSize(3); 
  display.setTextColor(WHITE);

  display.setCursor(40, 30);
  display.println(ptr);
  display.display();
}


void ring_screen(int value){
  int pixels = value / 6;
  ring.clear();
  for (int i = 0; i < pixels; i++){
    ring.setPixelColor(i, ring.Color(0, 255, 0));
  }
  ring.show();
}

void dimmer_encoder(int dir){
  brightness +=  dir * 5;
  if (brightness < 0){
    brightness = 0;
  } else if (brightness > 100){
    brightness = 100;
  }
  send_new_value(brightness);
}


void send_new_value(int value){
  char pub_topic[100];
  strcpy(pub_topic, topic);
  strcat(pub_topic, objects_home[current_object]);
  strcat(pub_topic, "/pub");

  char svalue[20];
  sprintf(svalue, "%d", value);

  char str_value[100];
  strcpy(str_value, "Value:");
  strcat(str_value, svalue);

  client.publish(pub_topic, str_value); // Отправляем значение на топик
  if (current_object == 2) dimmer_screen(str_value);
  if (current_object == 1) blinds_screen(str_value);
}

void regular_mqtt_sensor(){
  char full_topic[100];
  strcpy(full_topic, topic);
  strcat(full_topic, objects_home[current_object]);
  client.publish(full_topic, "Request"); 
}


void stopTimer() {
  xTimerStop(timer_mqtt_sensor, 0);
}

void startTimer() {
  xTimerStart(timer_mqtt_sensor, 0);
}

void stopTimer_request() {
  xTimerStop(timer_mqtt_request, 0);
}

void startTimer_request() {
  xTimerStart(timer_mqtt_request, 0);
}


void stopTimer_blinds() {
  xTimerStop(timer_blinds_encoder, 0);
}

void startTimer_blinds() {
  xTimerStart(timer_blinds_encoder, 0);
}

void initial_screen(){
  display.clearDisplay();
  display.drawBitmap(32, 0, start_img, 64, 64, WHITE);
  display.display();
}

void error_sensor_screen(){
  client.disconnect();
  client.connect(hostname, mqtt_user, mqtt_password);
  stopTimer();

  display.clearDisplay();
  display.setTextSize(2); 
  display.setTextColor(WHITE);
  display.setCursor(30, 22);
  display.print("Sensor");
  display.setCursor(36, 38);
  display.print("error");
  display.display();
  delay(1500);
  start_screen();
}

void reject_func(){

  if (current_object == 0) stopTimer();

  client.disconnect();
  client.connect(hostname, mqtt_user, mqtt_password);

  display.clearDisplay();
  display.setTextSize(2); 
  display.setTextColor(WHITE);
  display.setCursor(30, 22);
  display.print(objects_home[current_object]);
  display.setCursor(20, 38);
  display.print("no answer");
  display.display();

  delay(1000);
  start_screen();
}

void blinds_screen(char message[]){
  blinds_position = strtol(message, NULL, 10);
  ring_screen(blinds_position);
  display.clearDisplay();
  display.setTextSize(3); 
  display.setTextColor(WHITE);

  display.setCursor(40, 30);
  display.println(message);
  display.display();
}

void open_blinds(){
  client.publish(command_topic, "100");
}

void close_blinds(){
  client.publish(command_topic, "0");
}


void accept_reply_blinds(char message[]) {
  static char last_message[50] = "";
  if (strcmp(message, "online") == 0) {
    enable_blinds = true;
    if (enable_blinds && strlen(last_message) > 0) {
      blinds_screen(last_message);
    }
  } else {
    strcpy(last_message, message);
    if (enable_blinds) {
      blinds_screen(message);
    }
  }
}

void blinds_encoder(int dir){
  stopTimer_blinds();
  blinds_position += dir;
  if (blinds_position < 0) blinds_position = 0;
  if (blinds_position > 100) blinds_position = 100;
  char value[20];
  sprintf(value, "%d", blinds_position);
  blinds_screen(value);
  startTimer_blinds();
}
