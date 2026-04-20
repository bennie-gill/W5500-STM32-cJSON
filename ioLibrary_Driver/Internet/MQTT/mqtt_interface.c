//*****************************************************************************
//! \file mqtt_interface.c
//! \brief Paho MQTT to WIZnet Chip interface implement file.
//! \details The process of porting an interface to use paho MQTT.
//! \version 1.0.0
//! \date 2016/12/06
//! \par  Revision history
//!       <2016/12/06> 1st Release
//!
//! \author Peter Bang & Justin Kim
//! \copyright
//!
//! Copyright (c)  2016, WIZnet Co., LTD.
//! All rights reserved.
//!
//! Redistribution and use in source and binary forms, with or without
//! modification, are permitted provided that the following conditions
//! are met:
//!
//!     * Redistributions of source code must retain the above copyright
//! notice, this list of conditions and the following disclaimer.
//!     * Redistributions in binary form must reproduce the above copyright
//! notice, this list of conditions and the following disclaimer in the
//! documentation and/or other materials provided with the distribution.
//!     * Neither the name of the <ORGANIZATION> nor the names of its
//! contributors may be used to endorse or promote products derived
//! from this software without specific prior written permission.
//!
//! THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//! AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//! IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//! ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
//! LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//! CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//! SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//! INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//! CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//! ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
//! THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

#include "mqtt_interface.h"
#include "main.h"
#include "socket.h"
#include "wizchip_conf.h"

/*
    @brief Timer Initialize
    @param  timer : pointer to a Timer structure
           that contains the configuration information for the Timer.
*/
void TimerInit(Timer *timer) { timer->end_time = 0; }

/*
    @brief expired Timer
    @param  timer : pointer to a Timer structure
           that contains the configuration information for the Timer.
*/
char TimerIsExpired(Timer *timer) {
  uint32_t now = HAL_GetTick();
  return (now >= timer->end_time);
}

/*
    @brief Countdown millisecond Timer
    @param  timer : pointer to a Timer structure
           that contains the configuration information for the Timer.
           timeout : setting timeout millisecond.
*/
void TimerCountdownMS(Timer *timer, unsigned int timeout) {
  timer->end_time = HAL_GetTick() + timeout;
}

/*
    @brief Countdown second Timer
    @param  timer : pointer to a Timer structure
           that contains the configuration information for the Timer.
           timeout : setting timeout millisecond.
*/
void TimerCountdown(Timer *timer, unsigned int timeout) {
  timer->end_time = HAL_GetTick() + (timeout * 1000);
}

/*
    @brief left millisecond Timer
    @param  timer : pointer to a Timer structure
           that contains the configuration information for the Timer.
*/
int TimerLeftMS(Timer *timer) {
  uint32_t now = HAL_GetTick();
  if (now >= timer->end_time)
    return 0;
  return timer->end_time - now;
}

/*
    @brief New network setting
    @param  n : pointer to a Network structure
           that contains the configuration information for the Network.
           sn : socket number where x can be (0..7).
    @retval None
*/
void NewNetwork(Network *n, int sn) {
  n->my_socket = sn;
  n->mqttread = w5x00_read;
  n->mqttwrite = w5x00_write;
  n->disconnect = w5x00_disconnect;
}

/*
    @brief read function
    @param  n : pointer to a Network structure
           that contains the configuration information for the Network.
           buffer : pointer to a read buffer.
           len : buffer length.
           timeout : timeout in ms
    @retval received data length or SOCKERR code
*/
int w5x00_read(Network *n, unsigned char *buffer, int len, int timeout) {
  uint32_t start_tick = HAL_GetTick();
  int recv_len = 0;
  while (recv_len < len) {
    int ret = recv(n->my_socket, buffer + recv_len, len - recv_len);
    if (ret > 0) {
      recv_len += ret;
    } else if (ret != SOCK_BUSY && ret < 0) {
      return ret; // Error
    }
    if ((HAL_GetTick() - start_tick) > timeout)
      break;
  }
  return recv_len;
}

/*
    @brief write function
    @param  n : pointer to a Network structure
           that contains the configuration information for the Network.
           buffer : pointer to a read buffer.
           len : buffer length.
           timeout : timeout in ms
    @retval length of data sent or SOCKERR code
*/
int w5x00_write(Network *n, unsigned char *buffer, int len, int timeout) {
  uint32_t start_tick = HAL_GetTick();
  int sent_len = 0;
  while (sent_len < len) {
    int ret = send(n->my_socket, buffer + sent_len, len - sent_len);
    if (ret > 0) {
      sent_len += ret;
    } else if (ret != SOCK_BUSY && ret < 0) {
      return ret; // Error
    }
    if ((HAL_GetTick() - start_tick) > timeout)
      break;
  }
  return sent_len;
}

/*
    @brief disconnect function
    @param  n : pointer to a Network structure
           that contains the configuration information for the Network.
*/
void w5x00_disconnect(Network *n) { disconnect(n->my_socket); }

/*
    @brief connect network function
    @param  n : pointer to a Network structure
           that contains the configuration information for the Network.
           ip : server iP.
           port : server port.
    @retval SOCKOK code or SOCKERR code
*/
int ConnectNetwork(Network *n, uint8_t *ip, uint16_t port) {
  uint16_t myport = 12345;

  if (socket(n->my_socket, Sn_MR_TCP, myport, 0) != n->my_socket) {
    return SOCK_ERROR;
  }

#if 1
  // 20231016 taylor//teddy 240122
#if ((_WIZCHIP_ == 6100) || (_WIZCHIP_ == 6300))
  if (connect(n->my_socket, ip, port, 4) != SOCK_OK)
#else
  if (connect(n->my_socket, ip, port) != SOCK_OK)
#endif
#else
  if (connect(n->my_socket, ip, port) != SOCK_OK)
#endif
    return SOCK_ERROR;

  return SOCK_OK;
}
