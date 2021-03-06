/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Ilya Juhnowski <juhnowski@gmail.com>                                #
#                         add ximu sensor                                       #
# This program is free software; you can redistribute it and/or modify          #
# it under the terms of the GNU General Public License as published by          #
# the Free Software Foundation; either version 2 of the License, or             #
# (at your option) any later version.                                           #
#                                                                               #
# This program is distributed in the hope that it will be useful,               #
# but WITHOUT ANY WARRANTY; without even the implied warranty of                #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 #
# GNU General Public License for more details.                                  #
#                                                                               #
# You should have received a copy of the GNU General Public License                                                   #
# along with this program; if not, write to the Free Software                                                               #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA                                      #
#                                                                                                                                             #
********************************************************************************/
#include "meta.h"
#include "defs.h"
#include "ximu.h"
#include "globals.h"
#include "guvcview.h"

#include <math.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#include <glib.h>
#include <time.h>

#define __XIMU_MUTEX &meta->ximu_mutex

typedef enum {
    QCalibratedBattery = 12,
    QCalibratedThermometer = 8,
    QCalibratedGyroscope = 4,
    QCalibratedAccelerometer = 11,
    QCalibratedMagnetometer = 11,
	QQuaternion = 15,
    QBatterySensitivity = 5,
    QBatteryBias = 8,
    QThermometerSensitivity = 6,
    QThermometerBias = 0,
    QGyroscopeSensitivity = 7,
	QGyroscopeSampled200dps = 0,
    QGyroscopeBiasAt25degC = 3,
	QGyroscopeBiasTempSensitivity = 11,
	QGyroscopeSampledBias = 3,
    QAccelerometerSensitivity = 4,
    QAccelerometerBias = 8,
	QAccelerometerSampled1g = 4,
    QMagnetometerSensitivity = 4,
    QMagnetometerBias = 8,
	QMagnetometerHardIronBias = 11,
    QAlgorithmKp = 11,
    QAlgorithmKi = 15,
	QAlgorithmInitKp = 11,
	QAlgorithmInitPeriod = 11,
	QCalibratedAnalogueInput = 12,
	QAnalogueInputSensitivity = 4,
	QAnalogueInputBias = 8,
    QCalibratedADXL345 = 10,
	QADXL345busSensitivity = 6,
	QADXL345busBias = 8,
} FixedQ;

//----------------------------------------------------------------------------------------------------
// Variables

float gyroscope[3] = { 0, 0, 0 };
float accelerometer[3] = { 0, 0, 0 };
float magnetometer[3] = { 0, 0, 0 };
float quaternion[4] = { 0, 0, 0, 0};
float eulerAngles[3] = { 0, 0, 0 };

unsigned char shiftReg[256];
/*
//XimuStatesCount
void* init_tbl[][] = 
    {
      ximu_error,               // XIMU_STATE_INIT
      ximu_do_enable,           // XIMU_STATE_ENABLE
      ximu_error,               // XIMU_STATE_WAIT
      ximu_error,               // XIMU_STATE_CAPTURE_START
      ximu_error,               // XIMU_STATE_RUN
      ximu_error,               // XIMU_STATE_CAPTURE_STOP
      ximu_error,               // XIMU_STATE_CLOSE
      ximu_error,               // XIMU_STATE_DISABLE
      ximu_do_error_init,       // XIMU_STATE_ERROR_INIT
      ximu_error,               // XIMU_STATE_ERROR_WAITING
      ximu_error,               // XIMU_STATE_ERROR_RUNNING
      ximu_error,               // XIMU_STATE_REINIT
      ximu_error,               // XIMU_STATE_CALIBRATE
      ximu_error,               // XIMU_STATE_START_CALIBRATE
      ximu_error,               // XIMU_STATE_STOP_CALIBRATE
      ximu_error                // XIMU_STATE_STOP
    };

// transition functions XIMU_STATE_INIT
void ximu_do_enable(XIMU *ximu, int state)
{
    g_print("XIMU: ximu_do_enable");
//    ximu_change_state(XIMU * ximu, XIMU_STATE_WAIT);
}

void ximu_do_error_init(XIMU *ximu, int state)
{
    g_print("XIMU: ximu_do_error_init");
//    ximu_change_state(XIMU * ximu, XIMU_STATE_DISABLE);
}

// transition functions XIMU_STATE_ENABLE
void* enable_tbl[] = 
    {
      ximu_error,                // XIMU_STATE_INIT
      ximu_error,                // XIMU_STATE_ENABLE
      ximu_do_enable_wait,// XIMU_STATE_WAIT
      ximu_error,               // XIMU_STATE_CAPTURE_START
      ximu_error,               // XIMU_STATE_RUN
      ximu_error,               // XIMU_STATE_CAPTURE_STOP
      ximu_error,               // XIMU_STATE_CLOSE
      ximu_error,               // XIMU_STATE_DISABLE
      ximu_error,               // XIMU_STATE_ERROR_INIT
      ximu_error,               // XIMU_STATE_ERROR_WAITING
      ximu_error,               // XIMU_STATE_ERROR_RUNNING
      ximu_error,               // XIMU_STATE_REINIT
      ximu_error,               // XIMU_STATE_CALIBRATE
      ximu_error,               // XIMU_STATE_START_CALIBRATE
      ximu_error,               // XIMU_STATE_STOP_CALIBRATE
      ximu_error               // XIMU_STATE_STOP
    };


void ximu_do_enable_wait(XIMU *ximu, int state)
{

}
*/

//----------------------------------------------------------------------------------------------------
// Function declarations

static int DecodePacket(const unsigned char* const rxBuf, unsigned char* const decodedPacket, const int rxBufSize);
static void LeftShift(void);
float Fixed16ToFloat(const short fixed, const FixedQ q);
unsigned short Concat(const unsigned char msb, const unsigned char lsb);

//----------------------------------------------------------------------------------------------------
// Functions

int xIMUReceiverIsFramingChar(const unsigned char c) {
	return (c & 0x80);
}

ErrorCode xIMUReceiverProcess(const unsigned char* const rxBuf, const unsigned char rxBufSize, PacketHeader* const packetHeader) {
	unsigned char decodedPacket[256];
	int decodedPacketSize;
	unsigned char checksum;

	// Decode receive buffer
	decodedPacketSize = DecodePacket(rxBuf, decodedPacket, rxBufSize);

	// Verify checksum
	checksum = 0;
	int i;
	for(i = 0; i < decodedPacketSize - 1; i++) {
		checksum += decodedPacket[i];
	}
	if(checksum != decodedPacket[decodedPacketSize - 1]) {
		return ErrInvalidChecksum;
	}

	// Interpret packet according to packet header
	switch(decodedPacket[0]) {
		case(PktCalInertialAndMagneticData):
			*packetHeader = PktCalInertialAndMagneticData;
			if(decodedPacketSize != 20) {
				return ErrInvalidNumBytesForPacketHeader;
			}
			gyroscope[0] = Fixed16ToFloat(Concat(decodedPacket[1], decodedPacket[2]), QCalibratedGyroscope);
			gyroscope[1] = Fixed16ToFloat(Concat(decodedPacket[3], decodedPacket[4]), QCalibratedGyroscope);
			gyroscope[2] = Fixed16ToFloat(Concat(decodedPacket[5], decodedPacket[6]), QCalibratedGyroscope);
			accelerometer[0] = Fixed16ToFloat(Concat(decodedPacket[7], decodedPacket[8]), QCalibratedAccelerometer);
			accelerometer[1] = Fixed16ToFloat(Concat(decodedPacket[9], decodedPacket[10]), QCalibratedAccelerometer);
			accelerometer[2] = Fixed16ToFloat(Concat(decodedPacket[11], decodedPacket[12]), QCalibratedAccelerometer);
			magnetometer[0] = Fixed16ToFloat(Concat(decodedPacket[13], decodedPacket[14]), QCalibratedMagnetometer);
			magnetometer[1] = Fixed16ToFloat(Concat(decodedPacket[15], decodedPacket[16]), QCalibratedMagnetometer);
			magnetometer[2] = Fixed16ToFloat(Concat(decodedPacket[17], decodedPacket[18]), QCalibratedMagnetometer);
			break;
		case(PktQuaternionData):
			*packetHeader = PktQuaternionData;
			if(decodedPacketSize != 10) {
				return ErrInvalidNumBytesForPacketHeader;
			}
			quaternion[0] = Fixed16ToFloat(Concat(decodedPacket[1], decodedPacket[2]), QQuaternion);
			quaternion[1] = Fixed16ToFloat(Concat(decodedPacket[3], decodedPacket[4]), QQuaternion);
			quaternion[2] = Fixed16ToFloat(Concat(decodedPacket[5], decodedPacket[6]), QQuaternion);
			quaternion[3] = Fixed16ToFloat(Concat(decodedPacket[7], decodedPacket[8]), QQuaternion);
			eulerAngles[0] = 57.296f * atan2(2.0f * (quaternion[2] * quaternion[3] - quaternion[0] * quaternion[1]), 2.0f * quaternion[0] * quaternion[0] - 1.0f + 2.0f * quaternion[3] * quaternion[3]);
			eulerAngles[1] = 57.296f * atan((2.0f * (quaternion[1] * quaternion[3] + quaternion[0] * quaternion[2])) / sqrt(1.0f - pow((2.0 * quaternion[1] * quaternion[3] + 2.0f * quaternion[0] * quaternion[2]), 2.0f)));
			eulerAngles[2] = 57.296f * atan2(2.0f * quaternion[0] * quaternion[0] - 1.0f + 2.0f * quaternion[1] * quaternion[1], 2.0f * (quaternion[1] * quaternion[2] - quaternion[0] * quaternion[3]));
			break;
		default:
			return ErrUnknownHeader;
	}

	// Return successfully
	return ErrNoError;
}

static int DecodePacket(const unsigned char* const rxBuf, unsigned char* const decodedPacket, const int rxBufSize) {
	int decodedPacketSize = rxBufSize - 1 - ((rxBufSize - 1) >> 3);
	int i;
	for (i = rxBufSize - 1; i >= 0; i--) {
        shiftReg[i] = rxBuf[i];
        LeftShift();
    }
	
	for (i = 0; i < decodedPacketSize; i++) {
        decodedPacket[i] = shiftReg[i];
    }
    return decodedPacketSize;
}

static void LeftShift(void) {
    shiftReg[0] <<= 1;
    int i;
    for (i = 1; i < sizeof(shiftReg); i++) {
        if (shiftReg[i] & 0x80) {
        	shiftReg[i - 1] |= 0x01;
        }
        shiftReg[i] <<= 1;
    }
}

float Fixed16ToFloat(const short fixed, const FixedQ q) {
	return (float)fixed / (float)(1 << q);
}

unsigned short Concat(const unsigned char msb, const unsigned char lsb) {
	return ((unsigned short)msb << 8) | (unsigned short)lsb;
}


void ximu_print(struct ALL_DATA *all_data)
{
    struct metaData *meta = all_data->mdata;
    XIMU *ximu = meta->xdata;
    struct GLOBAL *global = all_data->global;
    
    if(global->debug) g_print("Time: %f, Gyr: %8.2f, %8.2f, %8.2f, Acc: %8.2f, %8.2f, %8.2f, Mag: %8.2f, %8.2f, %8.2f\n", ximu->data->time_stamp , ximu->data->frame->gyroscope[0], ximu->data->frame->gyroscope[1], ximu->data->frame->gyroscope[2], ximu->data->frame->accelerometer[0], ximu->data->frame->accelerometer[1], ximu->data->frame->accelerometer[2], ximu->data->frame->magnetometer[0], ximu->data->frame->magnetometer[1], ximu->data->frame->magnetometer[2]);
}

void ximu_print_1(struct ALL_DATA *all_data)
{
    struct metaData *meta = all_data->mdata;
    XIMU *ximu = meta->xdata;
    struct GLOBAL *global = all_data->global;
    
    if(global->debug) g_print("Time: %f, Pitch = %6.1f, \tRoll = %6.1f, \tYaw = %6.1f\n", ximu->data->time_stamp, ximu->data->frame->eulerAngles[0], ximu->data->frame->eulerAngles[1], ximu->data->frame->eulerAngles[2]);
}

void ximu_change_state(XIMU *ximu, int state)
{
    ximu->ximu_state = state;
}


void ximu_error(XIMU *ximu, int state)
{
    g_print("[XIMU] ximu_error: something wrog");
    ximu_change_state(ximu, XIMU_STATE_DISABLE);
}

void ximu_check_state(XIMU *ximu, int state)
{

}

void *ximu_loop(void *data)
{
    struct ALL_DATA *all_data = (struct ALL_DATA *) data;
    struct metaData *meta = all_data->mdata;
    XIMU *ximu = meta->xdata;
    
    clock_t t;
    float f;
    gboolean signalquit = FALSE;
    
    
    	// Serial port variables
	int serialHandle;
	struct termios options;
	unsigned char numBytes;
	unsigned char serialBuf[256];

	// Packet decoding variables
	unsigned char rxBuf[256];
	unsigned char rxBufSize = 0;
	ErrorCode ErrorCode = ErrNoError;
	PacketHeader packetHeader;

	// Open serial port
	serialHandle = open( "/dev/ttyUSB0", O_RDWR | O_NOCTTY );
	if (serialHandle == -1) {
		printf("xIMU: Could not open serial port\n");
	}
	else {
		fcntl(serialHandle, F_SETFL, 0);
		printf("xIMU: Opened serial port successfully!\n");
	}
	bzero(&options, sizeof(options)); 	// set all bits to zero
	options.c_cflag = B115200 | CS8 | CLOCAL | CREAD; //| CRTSCTS
	tcflush(serialHandle, TCIFLUSH); 				// clean the serial line
	tcsetattr(serialHandle, TCSANOW, &options); 	//set the new option for the port
	g_print("xIMU: Starting ximu_loop signalquit=%d\n",signalquit);
	
while(1)
{
    while (signalquit == FALSE) 
    {
        __LOCK_MUTEX(__XIMU_MUTEX);
            signalquit = meta->xdata->ximu_state;
        __UNLOCK_MUTEX(__XIMU_MUTEX);
	
		// Fetch data from serial port
		numBytes = read(serialHandle, &serialBuf, sizeof(serialBuf));

		// Process each byte
		int i;
		for(i = 0; i < numBytes; i++) {

			// Add data to receive buffer
			rxBuf[rxBufSize++] = serialBuf[i];

			// Process receive buffer if framing char received
			if(xIMUReceiverIsFramingChar(serialBuf[i]))	{
				ErrorCode = xIMUReceiverProcess(rxBuf, rxBufSize, &packetHeader);
				rxBufSize = 0;	// clear receive buffer
				t = clock();
				f = ((float)t)/CLOCKS_PER_SEC;
				// Print data if decoded successfully else print Error code
				if(ErrorCode == ErrNoError) {
					switch(packetHeader) {
						case(PktCalInertialAndMagneticData):
							ximu->data->time_stamp = f;
							ximu->data->frame->gyroscope[0] = gyroscope[0];
							ximu->data->frame->gyroscope[1] = gyroscope[1];
							ximu->data->frame->gyroscope[2] = gyroscope[2];
							ximu->data->frame->accelerometer[0] = accelerometer[0];
							ximu->data->frame->accelerometer[1] = accelerometer[1];
							ximu->data->frame->accelerometer[2] = accelerometer[2];
							ximu->data->frame->magnetometer[0] = magnetometer[0];
							ximu->data->frame->magnetometer[1] = magnetometer[1];
							ximu->data->frame->magnetometer[2] = magnetometer[2];
							ximu_print(all_data);
							//printf("Time: %f, Gyr: %8.2f, %8.2f, %8.2f, Acc: %8.2f, %8.2f, %8.2f, Mag: %8.2f, %8.2f, %8.2f\n", f , gyroscope[0], gyroscope[1], gyroscope[2], accelerometer[0], accelerometer[1], accelerometer[2], magnetometer[0], magnetometer[1], magnetometer[2]);
							break;
						case(PktQuaternionData):
							ximu->data->time_stamp = f;
							ximu->data->frame->eulerAngles[0] = eulerAngles[0]; 
							ximu->data->frame->eulerAngles[1] = eulerAngles[1];
							ximu->data->frame->eulerAngles[2] = eulerAngles[2];
							ximu_print_1(all_data);
							break;
						default:
                            printf("Something else");
							break;
					}
				}
				else{
					//printf("Error Code: %d\n", ErrorCode);
				}
			}
		}	
    }
    sleep(1);
    __LOCK_MUTEX(__XIMU_MUTEX);
    signalquit = meta->xdata->ximu_state;
    __UNLOCK_MUTEX(__XIMU_MUTEX);

}  
  //finalize ximu process and variables
}
