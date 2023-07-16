#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BMP3XX.h"
#include <CayenneLPP.h>
#define SEALEVELPRESSURE_HPA (1013.25)

/*
 * set LoraWan_RGB to Active,the RGB active in loraWan
 * RGB red means sending;
 * RGB purple means joined done;
 * RGB blue means RxWindow1;
 * RGB yellow means RxWindow2;
 * RGB green means received done;
 */

/*OTAA params*/
uint8_t devEui[] = { <Your Device EUI>};
uint8_t appEui[] = { <Your App EUI>};
uint8_t appKey[] = { <Your App Key>};
/* ABP params*/
uint8_t nwkSKey[] = { <Your nwk key>};
uint8_t appSKey[] = { <Your app key>};
uint32_t devAddr =  ( uint32_t ) <your device address>;

/*LoraWan channelsmask, default channels 0-7*/ 
uint16_t userChannelsMask[6]={ 0xFF00,0x0000,0x0000,0x0000,0x0000,0x0000 };

/*LoraWan region, select in arduino IDE tools*/
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;

/*LoraWan Class, Class A and Class C are supported*/
DeviceClass_t  loraWanClass = LORAWAN_CLASS;

/*the application data transmission duty cycle.  value in [ms].*/
uint32_t appTxDutyCycle = 15000;

/*OTAA or ABP*/
bool overTheAirActivation = LORAWAN_NETMODE;

/*ADR enable*/
bool loraWanAdr = LORAWAN_ADR;

/* set LORAWAN_Net_Reserve ON, the node could save the network info to flash, when node reset not need to join again */
bool keepNet = LORAWAN_NET_RESERVE;

/* Indicates if the node is sending confirmed or unconfirmed messages */
bool isTxConfirmed = LORAWAN_UPLINKMODE;

/* Application port */
uint8_t appPort = 2;

/*Store each air pressure reading. If the pressure increase between readings is higher than the 5 min threshold , 
then decrease the cycle/sleep time to 5 minute (from the normal 10 mins). If the pressure increase b/w readings is higher than
the 1 min threshold, then decrease the cycle/sleep time to 1 minute. Rapid increase in pressure indicates rapid altiture drop.*/
float lastBarometricPressureHPA = 0;
float currentBarometricPressureHPA = 0;
float barometricPressureDeltaThreshold_5Min = 4;
float barometricPressureDeltaThreshold_1Min = 7;

enum ReceiveType{
	NoRx = 0,
	ResetBoard = 1,
	Unkwn = 2
	
};
// Store Receive Type, sends confirmation of receive next transmission, after which the desired command is fulfilled.
ReceiveType CurrentReceiveType = NoRx;
// Requires one last cycle before received request is fulfilled.
bool LastCycleBeforeReceiveFulfill = false;
// For the first 12 sends (first hour), send data every 5 mins regardless. 
uint sendCount = 0;
/*!
* Number of trials to transmit the frame, if the LoRaMAC layer did not
* receive an acknowledgment. The MAC performs a datarate adaptation,
* according to the LoRaWAN Specification V1.0.2, chapter 18.4, according
* to the following table:
*
* Transmission nb | Data Rate
* ----------------|-----------
* 1 (first)       | DR
* 2               | DR
* 3               | max(DR-1,0)
* 4               | max(DR-1,0)
* 5               | max(DR-2,0)
* 6               | max(DR-2,0)
* 7               | max(DR-3,0)
* 8               | max(DR-3,0)
*
* Note, that if NbTrials is set to 1 or 2, the MAC will not decrease
* the datarate, in case the LoRaMAC layer did not receive an acknowledgment
*/
uint8_t confirmedNbTrials = 4;

Adafruit_BMP3XX bmp;
// Receiving 

//downlink data handle function example
void downLinkDataHandle(McpsIndication_t *mcpsIndication)
{
  Serial.printf("+REV DATA:%s,RXSIZE %d,PORT %d\r\n",mcpsIndication->RxSlot?"RXWIN2":"RXWIN1",mcpsIndication->BufferSize,mcpsIndication->Port);
  Serial.print("+REV DATA:");
  // Buffer size should ONLY BE 1.
  uint8_t rxValue = 0;
  for(uint8_t i=0;i<mcpsIndication->BufferSize;i++)
  {
    Serial.printf("%02X",mcpsIndication->Buffer[i]);
	rxValue = mcpsIndication->Buffer[i];
  }
  Serial.println();
  if(rxValue == 0x52){
	Serial.println("Rx RESET REQUEST");
	CurrentReceiveType = ResetBoard;
  }else{
	Serial.println("Rx UNKNOWN/ UNSUPPORTED");
	CurrentReceiveType = Unkwn;
  }
	

}

/* Prepares the payload of the frame */
static void prepareTxFrame(uint8_t appPort, float baro, float temp, int rxType)
{
	/*appData size is LORAWAN_APP_DATA_MAX_SIZE which is defined in "commissioning.h".
	*appDataSize max value is LORAWAN_APP_DATA_MAX_SIZE.
	*if enabled AT, don't modify LORAWAN_APP_DATA_MAX_SIZE, it may cause system hanging or failure.
	*if disabled AT, LORAWAN_APP_DATA_MAX_SIZE can be modified, the max value is reference to lorawan region and SF.
	*for example, if use REGION_CN470, 
	*the max value for different DR can be found in MaxPayloadOfDatarateCN470 refer to DataratesCN470 and BandwidthsCN470 in "RegionCN470.h".
	*/
	CayenneLPP lpp(LORAWAN_APP_DATA_MAX_SIZE);
	lpp.addBarometricPressure(1,baro);
	lpp.addTemperature(1, temp);
    lpp.getBuffer();
    appDataSize = lpp.getSize();
    memcpy(appData,lpp.getBuffer(),appDataSize);
    
}

void setup() {
	boardInitMcu();
	Serial.begin(9600);
	CurrentReceiveType = NoRx;
#if(AT_SUPPORT)
	enableAt();
#endif
	deviceState = DEVICE_STATE_INIT;
	LoRaWAN.ifskipjoin();
	
	// Setup barometer/thermometer
	if (!bmp.begin_I2C()) {   
    Serial.println("Could not find a valid BMP3 sensor, check wiring!");
  }else{
	
	bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  	bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  	bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  	bmp.setOutputDataRate(BMP3_ODR_50_HZ);
  }
}

void loop()
{
	

 
	switch( deviceState )
	{
		case DEVICE_STATE_INIT:
		{
#if(AT_SUPPORT)
			getDevParam();
#endif
			printDevParam();
			LoRaWAN.init(loraWanClass,loraWanRegion);
			deviceState = DEVICE_STATE_JOIN;
			break;
		}
		case DEVICE_STATE_JOIN:
		{
			LoRaWAN.join();
			break;
		}
		case DEVICE_STATE_SEND:
		{
			Serial.write("Sending data!");
			if (! bmp.performReading()) {
    			Serial.println("Failed to perform reading :(");
				Serial.print("CurrentReceiveType: ");
				Serial.println(CurrentReceiveType);
				prepareTxFrame( appPort, (float) -1, (float) -1, CurrentReceiveType);
				LoRaWAN.send();
  			}else{
				// Send it!
				Serial.print("Temperature = ");
  				Serial.print(bmp.temperature);
  				Serial.println(" *C");

  				Serial.print("Pressure = ");
  					Serial.print(bmp.pressure / 100.0);
  				Serial.println(" hPa");
				Serial.print("Rx Type = ");
				Serial.println(CurrentReceiveType);
  				lastBarometricPressureHPA = currentBarometricPressureHPA;
				currentBarometricPressureHPA = bmp.pressure / 100.0; 
				
				prepareTxFrame( appPort, (float) currentBarometricPressureHPA, (float) bmp.temperature, CurrentReceiveType);
				LoRaWAN.send();
			}
			
			deviceState = DEVICE_STATE_ACT;
			
			break;
		}
		case DEVICE_STATE_ACT:
		{
			// Act on Rx commands.
			deviceState = DEVICE_STATE_CYCLE;
			switch(CurrentReceiveType){
				case NoRx:{
					break;
				}
				case ResetBoard:
				{
					if(!LastCycleBeforeReceiveFulfill){
						LastCycleBeforeReceiveFulfill = true;
					}else
						CySoftwareReset();
				
				break;
				}
					
				
					
				
			}
			break;
		}
		case DEVICE_STATE_CYCLE:
		{
            // If first 12 sends, sleep for only 5 mins. 
            if(sendCount < 12){
                // Five minutes!
				Serial.println("Going to sleep for 5 minutes");
				txDutyCycleTime = appTxDutyCycle + 300000; 
                sendCount++;
            }else{
                // Schedule next packet transmission
                // If the difference between current pressure and last pressure is above 7hPa, 
                // schedule sleep to be one minute. If the difference is above 4 hPa, schedule 5 mins.
                // Else, schedule 10 mins.  
                if(currentBarometricPressureHPA - lastBarometricPressureHPA >= barometricPressureDeltaThreshold_1Min ){
                    
                    // One min!
                    Serial.println("Going to sleep for 1 minute");
                    txDutyCycleTime = appTxDutyCycle + 60000; 
                }else if(currentBarometricPressureHPA - lastBarometricPressureHPA >= barometricPressureDeltaThreshold_5Min ){
                    // Five minutes!
                    Serial.println("Going to sleep for 5 minutes");
                    txDutyCycleTime = appTxDutyCycle + 300000; 
                    
                }else{
                    // Ten minutes.
                    Serial.println("Going to sleep for 10 minutes");
                    txDutyCycleTime = appTxDutyCycle + 600000; 
                }
            }
			
			LoRaWAN.cycle(txDutyCycleTime);
			deviceState = DEVICE_STATE_SLEEP;
			break;
		}
		case DEVICE_STATE_SLEEP:
		{
			LoRaWAN.sleep();
			break;
		}
		default:
		{
			deviceState = DEVICE_STATE_INIT;
			break;
		}
	}
}