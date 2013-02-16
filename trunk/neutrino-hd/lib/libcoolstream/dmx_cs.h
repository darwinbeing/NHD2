/*
	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#ifndef __DEMUX_CS_H
#define __DEMUX_CS_H

#include <string>

#include <linux/dvb/dmx.h>
#include <linux/dvb/version.h>


#define DEMUX_POLL_TIMEOUT 0  			// timeout in ms
//#define MAX_FILTER_LENGTH 16    		// maximum number of filters
//#ifndef DMX_FILTER_SIZE
//#define DMX_FILTER_SIZE MAX_FILTER_LENGTH
//#endif

/* e2 */
#define DMX_BUFFER_SIZE 		8192  	//65536
#define AUDIO_STREAM_BUFFER_SIZE     	64*1024 	//65536
#define VIDEO_STREAM_BUFFER_SIZE     	64*1024 	//65536
#define RECORD_STREAM_BUFFER_SIZE	256*1024 	//262144)

#define MAX_SECTION_LENGTH (0x0fff + 3)

//NeutrinoHD
//define RECORD_STREAM_BUFFER_SIZE	2 * 3008 * 62

// player2 token from backend.h
#if 0
#define TRANSPORT_PACKET_SIZE           188

#define DMX_BUFFER_SIZE			(512*TRANSPORT_PACKET_SIZE) 	//96256
#define AUDIO_STREAM_BUFFER_SIZE        (200*TRANSPORT_PACKET_SIZE) 	//37600
#define VIDEO_STREAM_BUFFER_SIZE        (200*TRANSPORT_PACKET_SIZE) 	//37600
#endif


typedef enum
{
	DMX_VIDEO_CHANNEL=1,
	DMX_AUDIO_CHANNEL,
	DMX_PES_CHANNEL,
	DMX_PSI_CHANNEL,
	DMX_PIP_CHANNEL,
	DMX_TP_CHANNEL,
	DMX_PCR_ONLY_CHANNEL
} DMX_CHANNEL_TYPE;


class cDemux
{
	private:
		int demux_num;
		int demux_fd;
		int adapter_num;
		int fe_num;
		
		DMX_CHANNEL_TYPE type;
		unsigned short pid;

	public:
		bool Open(DMX_CHANNEL_TYPE Type, int uBufferSize = DMX_BUFFER_SIZE, int feindex = 0);
		void Close(void);
		bool Start(void);
		bool Stop(void);
		int Read(unsigned char * const buff, /*const size_t*/ int len, int Timeout = 0);
		bool sectionFilter(unsigned short Pid, const unsigned char * const Tid, const unsigned char * const Mask, int len, int Timeout = DEMUX_POLL_TIMEOUT, const unsigned char * const nMask = 0);
		bool pesFilter(const unsigned short Pid, const dmx_input_t Input = DMX_IN_FRONTEND);
		void addPid(unsigned short Pid);
		void removePid(unsigned short Pid);
		void getSTC(int64_t * STC);
		
		cDemux(int num = 0);
		~cDemux();
};

#endif //__DEMUX_H