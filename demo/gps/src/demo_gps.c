
#include <string.h>
#include <stdio.h>
#include <api_os.h>
#include <api_gps.h>
#include <api_event.h>
#include <api_hal_uart.h>
#include <api_debug.h>
#include "buffer.h"
#include "gps_parse.h"
#include "math.h"

#define MAIN_TASK_STACK_SIZE    (2048 * 2)
#define MAIN_TASK_PRIORITY      0
#define MAIN_TASK_NAME          "GPS Test Task"

static HANDLE gpsTaskHandle = NULL;


#define GPS_DATA_BUFFER_MAX_LENGTH 2048
#define GPS_NMEA_FRAME_BUFFER_LENGTH 1024

Buffer_t gpsNmeaBuffer;
uint8_t  gpsDataBuffer[GPS_DATA_BUFFER_MAX_LENGTH];
uint8_t tmp[GPS_NMEA_FRAME_BUFFER_LENGTH+1];


void GpsUpdate()
{
    int32_t index = Buffer_Query(&gpsNmeaBuffer,"$GNVTG",strlen("$GNVTG"),Buffer_StartPostion(&gpsNmeaBuffer));
    if(index >= 0)
    {
        // Trace(1,"find $GNVTG");
        index = Buffer_Query(&gpsNmeaBuffer,"\r\n",strlen("\r\n"),index);
        if(index >= 0)
        {
            Trace(1,"find complete GPS frame");
            
            memset(tmp,0,sizeof(tmp));
            uint32_t len = Buffer_Size2(&gpsNmeaBuffer,index)+1;
            Trace(1,"frame len:%d",len);
            if(!Buffer_Gets(&gpsNmeaBuffer,tmp,len>GPS_NMEA_FRAME_BUFFER_LENGTH?GPS_NMEA_FRAME_BUFFER_LENGTH:len))
            {
                Trace(1,"get data from buffer fail");
                return;
            }
            GPS_Parse(tmp);
        }
    }
}


// const uint8_t nmea[]="$GNGGA,000021.263,2228.7216,N,11345.5625,E,0,0,,153.3,M,-3.3,M,,*4E\r\n$GPGSA,A,1,,,,,,,,,,,,,,,*1E\r\n$BDGSA,A,1,,,,,,,,,,,,,,,*0F\r\n$GPGSV,1,1,00*79\r\n$BDGSV,1,1,00*68\r\n$GNRMC,000021.263,V,2228.7216,N,11345.5625,E,0.000,0.00,060180,,,N*5D\r\n$GNVTG,0.00,T,,M,0.000,N,0.000,K,N*2C\r\n";

void EventDispatch(API_Event_t* pEvent)
{
    switch(pEvent->id)
    {
        case API_EVENT_ID_GPS_UART_RECEIVED:
            // Trace(1,"received GPS data,length:%d, data:%s,flag:%d",pEvent->param1,pEvent->pParam1,flag);
            Buffer_Puts(&gpsNmeaBuffer,pEvent->pParam1,pEvent->param1);
            GpsUpdate();
            break;
        case API_EVENT_ID_UART_RECEIVED:
            if(pEvent->param1 == UART1)
            {
                uint8_t data[pEvent->param2+1];
                data[pEvent->param2] = 0;
                memcpy(data,pEvent->pParam1,pEvent->param2);
                Trace(1,"uart received data,length:%d,data:%s",pEvent->param2,data);
                if(strcmp(data,"close") == 0)
                {
                    Trace(1,"close gps");
                    GPS_Close();
                }
                else if(strcmp(data,"open") == 0)
                {
                    Trace(1,"open gps");
                    GPS_Open(NULL);
                }
            }
            break;
        case API_EVENT_ID_NETWORK_REGISTERED_HOME:
        case API_EVENT_ID_NETWORK_REGISTERED_ROAMING:
            Trace(1,"register success");
            break;
        default:
            break;
    }
}

void gps_testTask(void *pData)
{
    GPS_Info_t* gpsInfo = Gps_GetInfo();
    uint8_t buffer[150];

    while(1)
    {
        //send NMEA raw message to UART1 every 5 seconds
        UART_Write(UART1,tmp,strlen(tmp));
        UART_Write(UART1,"\r\n\r\n",4);

        //show fix info
        uint8_t isFixed = gpsInfo->gsa[0].fix_type > gpsInfo->gsa[1].fix_type ?gpsInfo->gsa[0].fix_type:gpsInfo->gsa[1].fix_type;
        char* isFixedStr;            
        if(isFixed == 2)
            isFixedStr = "2D fix";
        else if(isFixed == 3)
            isFixedStr = "3D fix";
        else
            isFixedStr = "no fix";
        snprintf(buffer,sizeof(buffer),"GPS fix mode:%d, BDS fix mode:%d, is fixed:%s, Latitude:%d/%d, Longitude:%d/%d",gpsInfo->gsa[0].fix_type, gpsInfo->gsa[1].fix_type,
                                                 isFixedStr,gpsInfo->rmc.latitude.value,gpsInfo->rmc.latitude.scale, 
                                                            gpsInfo->rmc.longitude.value,gpsInfo->rmc.longitude.scale);
        //show in tracer
        Trace(2,buffer);
        //send to UART1
        UART_Write(UART1,buffer,strlen(buffer));
        UART_Write(UART1,"\r\n\r\n",4);

        OS_Sleep(5000);
    }
}


void gps_MainTask(void *pData)
{
    API_Event_t* event=NULL;
    
    //open GPS hardware(UART2 open either)
    GPS_Open(NULL);

    //open UART1 to print NMEA infomation
    UART_Config_t config = {
        .baudRate = UART_BAUD_RATE_115200,
        .dataBits = UART_DATA_BITS_8,
        .stopBits = UART_STOP_BITS_1,
        .parity   = UART_PARITY_NONE,
        .rxCallback = NULL,
        .useEvent   = true
    };
    UART_Init(UART1,config);

    //Initialize buffer to cache nmea message
    Buffer_Init(&gpsNmeaBuffer,gpsDataBuffer,GPS_DATA_BUFFER_MAX_LENGTH);

    //Create UART1 send task and location print task
    OS_CreateTask(gps_testTask,
            NULL, NULL, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, 0, 0, MAIN_TASK_NAME);

    //Wait event
    while(1)
    {
        if(OS_WaitEvent(gpsTaskHandle, (void**)&event, OS_TIME_OUT_WAIT_FOREVER))
        {
            EventDispatch(event);
            OS_Free(event->pParam1);
            OS_Free(event->pParam2);
            OS_Free(event);
        }
    }
}


void gps_Main(void)
{
    gpsTaskHandle = OS_CreateTask(gps_MainTask,
        NULL, NULL, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, 0, 0, MAIN_TASK_NAME);
    OS_SetUserMainHandle(&gpsTaskHandle);
}

