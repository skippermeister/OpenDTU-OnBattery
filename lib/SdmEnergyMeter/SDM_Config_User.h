/* Library for reading SDM 72/120/220/230/630 Modbus Energy meters.
*  Reading via Hardware or Software Serial library & rs232<->rs485 converter
*  2016-2022 Reaper7 (tested on wemos d1 mini->ESP8266 with Arduino 1.8.10 & 2.5.2 esp8266 core)
*  crc calculation by Jaime García (https://github.com/peninquen/Modbus-Energy-Monitor-Arduino/)
*/

/*
*  define user WAITING_TURNAROUND_DELAY time in ms to wait for process current request
*/
//#define WAITING_TURNAROUND_DELAY            200

//------------------------------------------------------------------------------

/*
*  define user RESPONSE_TIMEOUT time in ms to wait for return response from all devices before next request
*/
//#define RESPONSE_TIMEOUT                    500

//------------------------------------------------------------------------------
