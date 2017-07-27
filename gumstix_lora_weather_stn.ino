#include <LowPower.h>
#include "settings.h"
#include <Wire.h>
#include <MS5611.h>
#include "i2c.h"
#include "i2c_SI7021.h"
#include "SparkFunTMP102.h"
#include <TheThingsNetwork.h>
#include <CayenneLPP.h>

struct WeatherPacket {
  int32_t ms5611_temp;
  int32_t ms5611_pres;
  int32_t si_temp;
  int32_t si_humi;
  int32_t tmp_temp;
};

extern const char *appEui;
extern const char *appKey;
#define loraSerial Serial1
#define debugSerial Serial
#define freqPlan TTN_FP_US915
const int LORA_RESET_PIN = 4;
TheThingsNetwork ttn(loraSerial, debugSerial, freqPlan);
CayenneLPP lpp(51);

WeatherPacket packet;
MS5611 ms5611(&Wire);
SI7021 si7021;
TMP102 tmp102(0x48);
const int ALERT_PIN = A3;

void sleep_x_mins(int mins)
{
  int wdt_count = 0;
  Serial.print("Sleeping for "); Serial.print(mins, DEC);Serial.println(" minutes");
  ttn.sleep(mins*59000);
  tmp102.sleep();
  delay(160);
  while(wdt_count < mins * 15)
  {
    Serial.flush();
    // LowPower.idle(SLEEP_4S, ADC_OFF, TIMER4_OFF, TIMER3_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART1_OFF, TWI_OFF, USB_OFF);
    LowPower.powerStandby(SLEEP_4S, ADC_OFF, BOD_OFF);
    wdt_count++;
  }
  Serial.println("Wakeup");
  tmp102.wakeup();
}

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,LOW);
  // setup barometer
  if(ms5611.connect()>0)
  {
    Serial.println("Error connecting to MS5611...");
  }

  // setup humidity sensor
  if (!si7021.initialize())
  {
        Serial.println("SI7021 missing");
  }

  // setup tmp102
  pinMode(ALERT_PIN,INPUT);  // Declare alertPin as an input
  tmp102.begin();  // Join I2C bus
  // set the number of consecutive faults before triggering alarm.
  // 0-3: 0:1 fault, 1:2 faults, 2:4 faults, 3:6 faults.
  tmp102.setFault(0);  // Trigger alarm immediately
  // set the polarity of the Alarm. (0:Active LOW, 1:Active HIGH).
  tmp102.setAlertPolarity(1); // Active HIGH
  // set the sensor in Comparator Mode (0) or Interrupt Mode (1).
  tmp102.setAlertMode(0); // Comparator Mode.
  // set the Conversion Rate (how quickly the sensor gets a new reading)
  //0-3: 0:0.25Hz, 1:1Hz, 2:4Hz, 3:8Hz
  tmp102.setConversionRate(2);
  //set Extended Mode.
  //0:12-bit Temperature(-55C to +128C) 1:13-bit Temperature(-55C to +150C)
  tmp102.setExtendedMode(0);
  //set T_HIGH, the upper limit to trigger the alert on
  tmp102.setHighTempC(29.4); // set T_HIGH in C
  //set T_LOW, the lower limit to shut turn off the alert
  tmp102.setLowTempC(26.67); // set T_LOW in C}

  Serial.println("Starting Lora stuff ...");
  pinMode(LORA_RESET_PIN,OUTPUT);
  digitalWrite(LORA_RESET_PIN,HIGH);
  // setup Lora
  loraSerial.begin(57600);
  debugSerial.println("-- STATUS");
  ttn.showStatus();

  debugSerial.println("-- JOIN");
  ttn.join(appEui, appKey);
}

void loop() {
  // read barometer
  ms5611.ReadProm();
  ms5611.Readout();
  float ms5611_temp, ms5611_pres;
  packet.ms5611_temp = (int32_t)(ms5611.GetTemp()*1000);
  packet.ms5611_pres = (int32_t)(ms5611.GetPres()*1000);
  ms5611_temp = ms5611.GetTemp()/100.0;
  ms5611_pres = ms5611.GetPres()/100.0; // ms5611_pres is in hPa

  // read humidity
  float humi, temp;
  si7021.getHumidity(humi);
  si7021.getTemperature(temp);
  si7021.triggerMeasurement();
  packet.si_temp = (int32_t)(temp*1000);
  packet.si_humi = (int32_t)(humi*1000);

  // read temperature
  tmp102.wakeup();
  float tmp_temp;
  packet.tmp_temp = (int32_t)(tmp102.readTempC()*1000);
  tmp_temp = tmp102.readTempC();
  tmp102.sleep();

  lpp.reset();
  lpp.addTemperature(1, ms5611_temp);
  
  // send packet
  if (ttn.sendBytes(lpp.getBuffer(), lpp.getSize(), 1, true, 0) == TTN_SUCCESSFUL_TRANSMISSION)
  {
    digitalWrite(LED_BUILTIN,HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN,LOW);
    delay(100);
    digitalWrite(LED_BUILTIN,HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN,LOW);
  }

  sleep_x_mins(1);

  // reset lpp, add data
  lpp.reset();
  lpp.addBarometricPressure(2, ms5611_pres);
  if (ttn.sendBytes(lpp.getBuffer(), lpp.getSize(), 2, true, 0) == TTN_SUCCESSFUL_TRANSMISSION)
  {
    digitalWrite(LED_BUILTIN,HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN,LOW);
    delay(100);
    digitalWrite(LED_BUILTIN,HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN,LOW);
  }

  sleep_x_mins(1);

    // reset lpp, add data
  lpp.reset();
  lpp.addAnalogInput(4, humi);
  if (ttn.sendBytes(lpp.getBuffer(), lpp.getSize(), 3, true, 0) == TTN_SUCCESSFUL_TRANSMISSION)
  {
    digitalWrite(LED_BUILTIN,HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN,LOW);
    delay(100);
    digitalWrite(LED_BUILTIN,HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN,LOW);
  }
  
  sleep_x_mins(5);
}
