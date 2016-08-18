#include <GSM.h>
#include <RunningMedian.h>
#include <LowPower.h>

#define GPRS_APN       "internet"
#define GPRS_LOGIN     ""
#define GPRS_PASSWORD  ""

// network
GSMClient client;
GPRS gprs;
GSM gsmAccess;
GSMModem modem;
int gsmStatus;
bool connected;

// batcave
char backend[] = "batman-test.aradolabs.com";
char backendApiPath[] = "/api/battery/testbattery/state";
int backendPort = 8080;
char batteryStateMessage[128];

// voltage measurement
int analogInput = A0;
float vout = 0.0;
float vin = 0.0;
float R1 = 30000.0;
float R2 = 7500.0;
int value = 0;
int centivolts = 0;
RunningMedian measurements = RunningMedian(7);

// device
int led = 13;
int sleepWait = 9999; // start sending/measuring immediately when arduino is powered on

void connectGSM()
{
  digitalWrite(3, HIGH); // battery save off

  while (gsmStatus != GPRS_READY)
  {
    gsmStatus = gsmAccess.begin();
    if (gsmStatus == GSM_READY) {
      gsmStatus = gprs.attachGPRS(GPRS_APN, GPRS_LOGIN, GPRS_PASSWORD);
      if (gsmStatus == CONNECTING) {
        // for some strange reason GPRS status can be connecting
        // even if already connected.
        gsmStatus = GPRS_READY;
      }
    }
    if (gsmStatus != GPRS_READY) {
      delay(1000);
    }
  } // connect loop
}

void shutdownGSM()
{
  gsmAccess.shutdown();  
  digitalWrite(3, LOW); // battery save on
  gsmStatus = 0;
}

void measureVoltage()
{
  // read voltage 7 times and then take the median voltage
  measurements.clear();
  for (int i = 0; i < 7; i++)
  {
    value = analogRead(analogInput);
    vout = (value * 5.0) / 1024.0;
    vin = vout / (R2 / (R1 + R2)); // R1 and R2 are resistances in the voltage sensor
    measurements.add(vin);
    delay(500); //make 0,5 second distance between the measurements to make the measurements more accurate
  }
  centivolts = (int)(measurements.getMedian() * 100.0f);
}

void createBatteryStateMessage()
{
  sprintf(batteryStateMessage, "{ \"batteryState\": %d }", centivolts);
}

void writeBatteryStateUpdateRequest()
{
  client.print("PUT ");
  client.print(backendApiPath);
  client.print(" HTTP/1.1");
  client.println();
  client.print("Host: ");
  client.print(backend);
  client.println();
  client.println("Content-Type: application/json; charset=utf-8");
  client.print("Content-Length: ");
  client.print(strlen(batteryStateMessage));
  client.println();
  client.println();
  client.println(batteryStateMessage);
  client.println();
  client.println();
}

void sendMeasuredVoltageToBackend()
{
  if (client.connect(backend, backendPort)) {
    createBatteryStateMessage();
    writeBatteryStateUpdateRequest();    
    client.stop();
  } // client.connect(backend, backendPort)
}

void setup()
{
  pinMode(analogInput, INPUT);
  pinMode(led, OUTPUT);
  shutdownGSM();
  modem.begin();
} // setup()

void loop()
{
  // 450 == hour, one sleepWait means 8 second
  if (sleepWait > 450)
  {
    measureVoltage();
    digitalWrite(led, HIGH);
    connectGSM();
    sendMeasuredVoltageToBackend();
    shutdownGSM();
    digitalWrite(led, LOW);
    sleepWait = 0;
  }
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  sleepWait++;
} // loop

