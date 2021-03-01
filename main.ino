/* Václav Kuboň
 * 
 * Board: ESP32 Wrover Module
 * Upload Speed: 115200
 * Flash Frequency: 40Hz
 * Partition Scheme: Huge APP
 */

#include <WiFi.h>
#include "AzureIotHub.h"
#include "Esp32MQTTClient.h"

#include "esp32-hal-adc.h" // potřebné pro resetování pinů
#include "soc/sens_reg.h" // potřebné pro resetování pinů

uint64_t reg_b; // registry pinu

#define sensorPin 14 //pin pro připojení senzoru

//CREDENTIALS
const char* ssid = ""; // název wifi
const char* password = ""; 

// primary key získaný v Azure Iot Hub
static const char* connectionString = "HostName=iot-people-counter.azure-devices.net;DeviceId=ESP32-CAM;SharedAccessKey=LybGyFjR1HIdjDvLC3+9/ntRdpc2QnKHhX+MXYCy7/s=";

// JSON template pro odesílání dat
const char *messageData = "{\"deviceId\":\"%s\", \"messageId\":%d, \"waterLevel\":%d}";

int waterLevel; // hodnota senzoru


//-------------------------------------------------------------------------------------------------
// Definice potřebných funkcí a proměnných pro Azure

#define DEVICE_ID "ESP32-CAM" 
#define MESSAGE_MAX_LEN 256

int messageCount = 1;
static bool hasWifi = false;
static bool messageSending = true;

static void InitWifi() //inicialize a připojení
{
  reg_b = READ_PERI_REG(SENS_SAR_READ_CTRL2_REG);
  
  Serial.println("Connecting...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  hasWifi = true;
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //pinMode(sensorPin, INPUT);
}


static void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
  if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
  {
    Serial.println("Send Confirmation Callback finished.");
  }
}


static void MessageCallback(const char* payLoad, int size)
{
  Serial.println("Message callback:");
  Serial.println(payLoad);
}


static void DeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size)
{
  char *temp = (char *)malloc(size + 1);
  if (temp == NULL)
  {
    return;
  }
  memcpy(temp, payLoad, size);
  temp[size] = '\0';
  // Display Twin message.
  Serial.println(temp);
  free(temp);
}


static int  DeviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size)
{
  LogInfo("Try to invoke method %s", methodName);
  const char *responseMessage = "\"Successfully invoke device method\"";
  int result = 200;

  if (strcmp(methodName, "start") == 0)
  {
    LogInfo("Start sending temperature and humidity data");
    messageSending = true;
  }
  else if (strcmp(methodName, "stop") == 0)
  {
    LogInfo("Stop sending temperature and humidity data");
    messageSending = false;
  }
  else
  {
    LogInfo("No method %s found", methodName);
    responseMessage = "\"No method found\"";
    result = 404;
  }


  *response_size = strlen(responseMessage) + 1;
  *response = (unsigned char *)strdup(responseMessage);

  return result;
}
//-------------------------------------------------------------------------------------------------

void setup(){
  Serial.begin(115200);

  // Připojení k WiFi
  Serial.println(" > WiFi");
  hasWifi = false;
  InitWifi();
  if (!hasWifi)
  {
    return;
  }

  // Připojení k IoT Hubu
  Serial.println(" > IoT Hub");
  Esp32MQTTClient_SetOption(OPTION_MINI_SOLUTION_NAME, "GetStarted");
  Esp32MQTTClient_Init((const uint8_t*)connectionString, true);

  Esp32MQTTClient_SetSendConfirmationCallback(SendConfirmationCallback);
  Esp32MQTTClient_SetMessageCallback(MessageCallback);
  Esp32MQTTClient_SetDeviceTwinCallback(DeviceTwinCallback);
  Esp32MQTTClient_SetDeviceMethodCallback(DeviceMethodCallback);
  
}

void loop(){
  
  WRITE_PERI_REG(SENS_SAR_READ_CTRL2_REG, reg_b); // reset pinu kvůi problémům s WiFi knihovnou
  
  waterLevel = analogRead(sensorPin); // čtení analogového signálu
  waterLevel = 4095 - waterLevel; 
  
  
  char messagePayload[MESSAGE_MAX_LEN];
  snprintf(messagePayload, MESSAGE_MAX_LEN, messageData, DEVICE_ID, messageCount++, waterLevel); // vytvoření stringu pro odeslání
  
  Serial.println(messagePayload);
  
  EVENT_INSTANCE* message = Esp32MQTTClient_Event_Generate(messagePayload, MESSAGE);
  Esp32MQTTClient_Event_AddProp(message, "SendingPhoto...", "true");
  Esp32MQTTClient_SendEventInstance(message);
  
  delay(1000); // prodleva 1s
}
