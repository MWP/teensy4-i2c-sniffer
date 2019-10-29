#include <Arduino.h>

#define I2C_SDA	(0)
#define I2C_SCL	(1)
#define I2C_INT (2)
#define SERIAL_BAUD	(460800)
#define BUFFER_ALL_LEN (50000)
#define CSV_DUMP
#define DUMP_PIN (3)
#define LED_PIN (13)

typedef enum
{
	NONE=0,
	SYS_START,
	SYS_STOP,
	I2C_DATA_ACK,
	I2C_DATA_NACK,
	I2C_STOP,
	I2C_START,
	INT_RISING,
	INT_FALLING,
} pFlags;

typedef struct 
{
	uint8_t		data;
	uint8_t		int_value;
	pFlags		flags;
	uint32_t	time;
} sData;

void setup()
{
	pinMode(LED_PIN, OUTPUT);
	pinMode(I2C_SDA, INPUT);
	pinMode(I2C_SCL, INPUT);
	pinMode(I2C_INT, INPUT);
	pinMode(DUMP_PIN, INPUT_PULLDOWN);
	Serial.begin(SERIAL_BAUD);
	Serial.println(__FUNCTION__);
}

#define pFALLING 0
#define pRISING 1
#define pSTEADY 2

#define DATABUF(ndata,nflags) dataBuf[dataBufLen].data = (ndata); dataBuf[dataBufLen].flags = (nflags); dataBuf[dataBufLen].time = micros(); dataBuf[dataBufLen].int_value = INT; dataBufLen++;

void loop()
{
	bool inData = false;
	uint8_t bit = 0;
	uint8_t byte = 0;

	sData dataBuf[BUFFER_ALL_LEN] = {0,};
	uint32_t dataBufLen = 0;
	uint16_t byteCount = 0;

	Serial.println(__FUNCTION__);

	register uint8_t oldSDA = digitalReadFast(I2C_SDA);
	register uint8_t oldSCL = digitalReadFast(I2C_SCL);
	register uint8_t oldINT = digitalReadFast(I2C_INT);
	register uint8_t xSDA, xSCL;
	register uint8_t SDA, SCL;
	register uint8_t INT = oldINT;

	DATABUF(0, SYS_START);
	while (1)
	{
		SDA = digitalReadFast(I2C_SDA);
		SCL = digitalReadFast(I2C_SCL);
		INT = digitalReadFast(I2C_INT);
		
		digitalWriteFast(LED_PIN, !digitalReadFast(LED_PIN));

		//check states
		if (SDA != oldSDA)
		{
			oldSDA = SDA;
			if (SDA)
				xSDA = pRISING;
			else
				xSDA = pFALLING;	
		}
		else
			xSDA = pSTEADY;
		
		if (SCL != oldSCL)
		{
			oldSCL = SCL;
			if (SCL)
				xSCL = pRISING;
			else
				xSCL = pFALLING;	
		}
		else
			xSCL = pSTEADY;

		if (INT != oldINT)
		{
			oldINT = INT;
			if (INT)
			{
				DATABUF(0, INT_RISING);
			}
			else
			{
				DATABUF(0, INT_FALLING);
			}
		}


		//
		if (xSCL == pRISING)
		{
			if (inData)
			{
				if (bit < 8)
				{
					//bit of data
					byte = (byte << 1) | SDA;
					bit++;
				}
				else
				{
					//got all 8 bits of byte
					if (SDA)
					{
						DATABUF(byte, I2C_DATA_NACK);
					}
					else
					{
						DATABUF(byte, I2C_DATA_ACK);
					}
					bit = 0;
					byte = 0;
					byteCount++;
				}
			}
		}
		else
		{

			if (xSCL == pSTEADY)
			{
				if (xSDA == pRISING)
					if (SCL)
					{
						//stopping
						inData = false;
						byte = 0;
						bit = 0;
						DATABUF(0, I2C_STOP);
					}
				if (xSDA == pFALLING)
					if (SCL)
					{
						//starting
						inData = true;
						byte = 0;
						bit = 0;
						DATABUF(0, I2C_START);
					}
			}
		}

		//dump?
		if (digitalReadFast(DUMP_PIN) || (dataBufLen == BUFFER_ALL_LEN-1) )
		{
			//do dump
			DATABUF(0, SYS_STOP);
			digitalWriteFast(LED_PIN, 1);
#ifdef CSV_DUMP
			Serial.println("");
			//Serial.println("REC;ABS_TIME;OFS_TIME;FLAG;DATA");
			//Serial.println("REC;OFS_TIME;FLAG;INT_VAL;DATA");
			Serial.println("OFS_TIME;FLAG;INT_VAL;DATA");
			for (uint32_t o = 0; o < dataBufLen; o++)
			{
				//Serial.print(o);
				//Serial.print(";");
				//Serial.printf("%d", dataBuf[o].time);
				//Serial.print(";");
				Serial.printf("%d", dataBuf[o].time - dataBuf[0].time);
				Serial.print(";");
				switch (dataBuf[o].flags)
				{
					case SYS_START: Serial.print("SYS_START"); break;
					case SYS_STOP: Serial.print("SYS_STOP"); break;
					case I2C_DATA_ACK: Serial.print("I2C_DATA_ACK"); break;
					case I2C_DATA_NACK: Serial.print("I2C_DATA_NACK"); break;
					case I2C_STOP: Serial.print("I2C_STOP"); break;
					case I2C_START: Serial.print("I2C_START"); break;
					case INT_RISING: Serial.print("INT_RISING"); break;
					case INT_FALLING: Serial.print("INT_FALLING"); break;
					default: Serial.print("XXXX"); break;
				}
				Serial.print(";");
				Serial.printf("%d", dataBuf[o].int_value);
				Serial.print(";");
				switch (dataBuf[o].flags)
				{
					case I2C_DATA_ACK:
					case I2C_DATA_NACK:
						Serial.printf("%02X", dataBuf[o].data);
						break;
					default:
						Serial.print("NA");
						break;
				}
				Serial.println("");
				delay(1);
			}
#else
			uint16_t len = 0;
			Serial.println("\nDUMP\n");
			for (uint16_t o = 0; o < outBufPos; o++)
			{
				uint16_t b = inBuf[o];
				if ((b >> 8) & 1)
				{
					//start
					len = 0;
					Serial.print("{");
				}
				else
				{
					if ((b >> 8) & 2)
					{
						//end
						Serial.printf("} /*len=%d*/\n", len);
					}
					else
					{
						//data
						len++;
						Serial.printf("0x%02X, ", b & 0xFF);
					}
				}
				delay(1);
			}

			//done
			Serial.println("\nDONE\n");
#endif			
			while(1){};
		}

	}
}



