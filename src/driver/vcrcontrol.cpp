/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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
#include <config.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <driver/vcrcontrol.h>

#include <gui/movieinfo.h>

#include <driver/encoding.h>
#include <driver/stream2file.h>

#include <gui/widget/messagebox.h>
#include <global.h>
#include <neutrino.h>
#include <gui/widget/hintbox.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#include <errno.h>
#include <math.h>

#include <daemonc/remotecontrol.h>
#include <zapit/client/zapittools.h>

#include <zapit/client/zapittypes.h>
#include <zapit/frontend_c.h>
#include <zapit/channel.h>


extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */
int was_record = 0;
extern bool autoshift;
extern bool autoshift_delete;

#define SA struct sockaddr
#define SAI struct sockaddr_in
extern "C" {
#include <driver/genpsi.h>
}


CMovieInfo * g_cMovieInfo;
MI_MOVIE_INFO * g_movieInfo;
t_channel_id rec_channel_id;
int safe_mkdir(char * path);

extern int FrontendCount;
CFrontend * getFE(int index);
CZapitChannel * find_channel_tozap(const t_channel_id channel_id, bool in_nvod);
extern CFrontend * live_fe;

static CVCRControl vcrControl;

CVCRControl * CVCRControl::getInstance()
{
	return &vcrControl;
}

CVCRControl::CVCRControl()
{
	Device = NULL;
}

CVCRControl::~CVCRControl()
{
	unregisterDevice();
}

void CVCRControl::unregisterDevice()
{
	if (Device)
	{
		delete Device;
		Device = NULL;
	}
}

void CVCRControl::registerDevice(CDevice * const device)
{
	unregisterDevice();
	
	Device = device;
}

bool CVCRControl::Record(const CTimerd::RecordingInfo * const eventinfo)
{
	int mode = g_Zapit->isChannelTVChannel(eventinfo->channel_id) ? NeutrinoMessages::mode_tv : NeutrinoMessages::mode_radio;

	return Device->Record(eventinfo->channel_id, mode, eventinfo->epgID, eventinfo->epgTitle, eventinfo->apids, eventinfo->epg_starttime); 
}

void CVCRControl::CDevice::getAPIDs(const unsigned char ap, APIDList & apid_list)
{
	//(strstr(g_RemoteControl->current_PIDs.APIDs[i].desc, "(AC3)") == NULL))
	
        unsigned char apids = ap;

        if (apids == TIMERD_APIDS_CONF)
                apids = g_settings.recording_audio_pids_default;

        apid_list.clear();
        CZapitClient::responseGetRecordPIDs allpids;
	
        g_Zapit->getRecordPIDS(allpids);

        // assume smallest apid ist std apid
        if (apids & TIMERD_APIDS_STD)
        {
                uint32_t apid_min=UINT_MAX;
                uint32_t apid_min_idx=0;
                for(unsigned int i = 0; i < allpids.APIDs.size(); i++)
                {
                        if (allpids.APIDs[i].pid < apid_min && !allpids.APIDs[i].is_ac3)
                        {
                                apid_min = allpids.APIDs[i].pid;
                                apid_min_idx = i;
                        }
                }
                
                if (apid_min != UINT_MAX)
                {
                        APIDDesc a = {apid_min, apid_min_idx, false};
                        apid_list.push_back(a);
                }
        }

        if (apids & TIMERD_APIDS_ALT)
        {
                uint32_t apid_min=UINT_MAX;
                uint32_t apid_min_idx=0;
                for(unsigned int i = 0; i < allpids.APIDs.size(); i++)
                {
                        if (allpids.APIDs[i].pid < apid_min && !allpids.APIDs[i].is_ac3)
                        {
                                apid_min = allpids.APIDs[i].pid;
                                apid_min_idx = i;
                        }
                }
                for(unsigned int i = 0; i < allpids.APIDs.size(); i++)
                {
                        if (allpids.APIDs[i].pid != apid_min && !allpids.APIDs[i].is_ac3)
                        {
                                APIDDesc a = {allpids.APIDs[i].pid, i, false};
                                apid_list.push_back(a);
                        }
                }
        }

        if (apids & TIMERD_APIDS_AC3)
        {
                bool ac3_found=false;
                for(unsigned int i = 0; i < allpids.APIDs.size(); i++)
                {
                        if (allpids.APIDs[i].is_ac3)
                        {
                                APIDDesc a = {allpids.APIDs[i].pid, i, true};
                                apid_list.push_back(a);
                                ac3_found=true;
                        }
                }
                
                // add non ac3 apid if ac3 not found
                if (!(apids & TIMERD_APIDS_STD) && !ac3_found)
                {
                        uint32_t apid_min=UINT_MAX;
                        uint32_t apid_min_idx=0;
                        for(unsigned int i = 0; i < allpids.APIDs.size(); i++)
                        {
                                if (allpids.APIDs[i].pid < apid_min && !allpids.APIDs[i].is_ac3)
                                {
                                        apid_min = allpids.APIDs[i].pid;
                                        apid_min_idx = i;
                                }
                        }

                        if (apid_min != UINT_MAX)
                        {
                                APIDDesc a = {apid_min, apid_min_idx, false};
                                apid_list.push_back(a);
                        }
                }
        }

        // no apid selected use standard
        if (apid_list.empty() && !allpids.APIDs.empty())
        {
                uint32_t apid_min=UINT_MAX;
                uint32_t apid_min_idx=0;
                for(unsigned int i = 0; i < allpids.APIDs.size(); i++)
                {
                        if (allpids.APIDs[i].pid < apid_min && !allpids.APIDs[i].is_ac3)
                        {
                                apid_min = allpids.APIDs[i].pid;
                                apid_min_idx = i;
                        }
                }
                
                if (apid_min != UINT_MAX)
                {
                        APIDDesc a = {apid_min, apid_min_idx, false};
                        apid_list.push_back(a);
                }
                
                for(APIDList::iterator it = apid_list.begin(); it != apid_list.end(); it++)
                        printf("[CVCRControl] Record APID 0x%X %d\n",it->apid, it->ac3);
        }
}

bool CVCRControl::CVCRDevice::Stop()
{
	deviceState = CMD_VCR_STOP;

	if(last_mode != NeutrinoMessages::mode_scart)
	{
		g_RCInput->postMsg( NeutrinoMessages::VCR_OFF, 0 );
		g_RCInput->postMsg( NeutrinoMessages::CHANGEMODE , last_mode);
	}

	return true;
}

bool CVCRControl::CVCRDevice::Record(const t_channel_id channel_id, int mode, const event_id_t epgid, const std::string& epgTitle, unsigned char apids, const time_t epg_time)
{
	printf("[CVCRControl] Record channel_id: " PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS " epg: %llx, apids %x mode\n", channel_id, epgid, apids);
	
	// leave menu (if in any)
	g_RCInput->postMsg( CRCInput::RC_timeout, 0 );
	
	last_mode = CNeutrinoApp::getInstance()->getMode();

	if(mode != last_mode) 
	{
		CNeutrinoApp::getInstance()->handleMsg( NeutrinoMessages::CHANGEMODE , mode | NeutrinoMessages::norezap );
	}
	
	// check for twin
	#if 0
	bool twin = false;
	
	if(FrontendCount > 1)
	{
		for (int i = 1; i < FrontendCount; i++)
		{
			// twin
			if(getFE(0)->getInfo()->type == getFE(i)->getInfo()->type)
				twin = true;
		}
	}
	
	// check for multi
	bool multi = false;
	CZapitChannel * record_channel = find_channel_tozap(channel_id, false);
	
	if( record_channel->getFeIndex() != live_fe->getFeIndex() )
		multi = true;
	#endif
	
	// zapit
	if(channel_id != 0)	// wenn ein channel angegeben ist
	{
		// zap for live stream
		//if( (g_Zapit->getCurrentServiceID() != channel_id) && !SAME_TRANSPONDER(g_Zapit->getCurrentServiceID(), channel_id) /*&& !twin && !multi*/ )	// eventually not tuned
		//	g_Zapit->zapTo_serviceID(channel_id);		// for live stream

		// zap for record
		g_Zapit->zapTo_record(channel_id);			// for recording
	}

	// apids
        if(! (apids & TIMERD_APIDS_STD)) // nicht std apid
        {
                APIDList apid_list;
                getAPIDs(apids, apid_list);

                if(!apid_list.empty())
                {
                        if(!apid_list.begin()->ac3)
                                g_Zapit->setAudioChannel(apid_list.begin()->index);
                        else
                                g_Zapit->setAudioChannel(0); //sonst apid 0, also auf jeden fall ac3 aus !
                }
                else
                        g_Zapit->setAudioChannel(0); //sonst apid 0, also auf jeden fall ac3 aus !
        }
        else
                g_Zapit->setAudioChannel(0); //sonst apid 0, also auf jeden fall ac3 aus !

	// switch to scart
	if(SwitchToScart)
	{
		// Auf Scart schalten
		CNeutrinoApp::getInstance()->handleMsg( NeutrinoMessages::VCR_ON, 0 );
		// Das ganze nochmal in die queue, da obiges RC_timeout erst in der naechsten ev. loop ausgef�hrt wird
		// und dann das menu widget das display falsch r�cksetzt
		g_RCInput->postMsg( NeutrinoMessages::VCR_ON, 0 );
	}

	deviceState = CMD_VCR_RECORD;

	// Send IR
	return true;
}

bool CVCRControl::CVCRDevice::Pause()
{
	return true;
}

bool CVCRControl::CVCRDevice::Resume()
{
	return true;
}

void CVCRControl::CFileAndServerDevice::RestoreNeutrino(void)
{
	//printf("RestoreNeutrino\n");fflush(stdout);
	
	// unset record mode
	// after this zapit send EVT_RECORDMODE_DEACTIVATED, so neutrino getting NeutrinoMessages::EVT_RECORDMODE
	g_Zapit->setRecordMode( false );

	// start playback
	if (!g_Zapit->isPlayBackActive() && (CNeutrinoApp::getInstance()->getMode() != NeutrinoMessages::mode_standby))
		g_Zapit->startPlayBack();

	// alten mode wieder herstellen (ausser wen zwischenzeitlich auf oder aus sb geschalten wurde)
	if(CNeutrinoApp::getInstance()->getMode() != last_mode && CNeutrinoApp::getInstance()->getMode() != NeutrinoMessages::mode_standby && last_mode != NeutrinoMessages::mode_standby)
	{
		if(!autoshift) 
			g_RCInput->postMsg( NeutrinoMessages::CHANGEMODE , last_mode);
	}

	if(last_mode == NeutrinoMessages::mode_standby && CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_standby )
	{
		//Wenn vorher und jetzt standby, dann die zapit wieder auf sb schalten
		//g_Zapit->setStandby(true);
		//was_record = 1;
	}

	// restart sectionsd if stopped
#ifndef __sh__	
	if((last_mode != NeutrinoMessages::mode_standby) && StopSectionsd)
		g_Sectionsd->setPauseScanning(false);
#endif	
}

void CVCRControl::CFileAndServerDevice::CutBackNeutrino(const t_channel_id channel_id, const int mode)
{
	rec_channel_id = channel_id;
	
	//printf("CutBackNeutrino\n");fflush(stdout);
	
	last_mode = CNeutrinoApp::getInstance()->getMode();

	if(last_mode == NeutrinoMessages::mode_standby)
	{
		g_Zapit->setStandby(false);
		was_record = 1;
	}
	
	if (channel_id != 0) 
	{
		if (mode != last_mode && (last_mode != NeutrinoMessages::mode_standby || mode != CNeutrinoApp::getInstance()->getLastMode())) 
		{
			CNeutrinoApp::getInstance()->handleMsg( NeutrinoMessages::CHANGEMODE , mode | NeutrinoMessages::norezap );
			// Wenn wir im Standby waren, dann brauchen wir f�rs streamen nicht aufwachen...
			if(last_mode == NeutrinoMessages::mode_standby)
				CNeutrinoApp::getInstance()->handleMsg( NeutrinoMessages::CHANGEMODE , NeutrinoMessages::mode_standby);
		}
		
		#if 0
		bool twin = false;
	
		for (int i = 1; i < FrontendCount; i++)
		{
			// twin
			if(getFE(0)->getInfo()->type == getFE(i)->getInfo()->type)
				twin = true;
		}
		
		// check for multi
		bool multi = false;
		CZapitChannel * record_channel = find_channel_tozap(channel_id, false);
			
		if( record_channel->getFeIndex() != live_fe->getFeIndex() )
			multi = true;
		#endif
	
		// zap for live stream
		//if( (g_Zapit->getCurrentServiceID() != channel_id) && !SAME_TRANSPONDER(g_Zapit->getCurrentServiceID(), channel_id) /*&& !twin && !multi*/ )	// eventually not tuned
		//	g_Zapit->zapTo_serviceID(channel_id);
		
		// zap to record
		g_Zapit->zapTo_record(channel_id);
	}

#ifndef __sh__
	if(StopSectionsd)		// wenn sectionsd gestoppt werden soll
		g_Sectionsd->setPauseScanning(true);		// sectionsd stoppen
#endif


	// after this zapit send EVT_RECORDMODE_ACTIVATED, so neutrino getting NeutrinoMessages::EVT_RECORDMODE
	g_Zapit->setRecordMode( true );

//test
#ifndef __sh__
	//if( (last_mode == NeutrinoMessages::mode_standby) || (StopPlayBack && g_Zapit->isPlayBackActive()) )
#else
	if( last_mode == NeutrinoMessages::mode_standby )
		g_Zapit->stopPlayBack();
#endif
}

bool sectionsd_getEPGidShort(event_id_t epgID, CShortEPGData * epgdata);
bool sectionsd_getEPGid(const event_id_t epgID, const time_t startzeit, CEPGData * epgdata);

std::string CVCRControl::CFileAndServerDevice::getCommandString(const CVCRCommand command, const t_channel_id channel_id, const event_id_t epgid, const std::string& epgTitle, unsigned char apids)
{
	char tmp[40];
	std::string apids_selected;
	const char * extCommand;
	std::string info1, info2;

	std::string extMessage = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n<neutrino commandversion=\"1\">\n\t<record command=\"";
	switch(command)
	{
		case CMD_VCR_RECORD:
			extCommand = "record";
			break;
		case CMD_VCR_STOP:
			extCommand = "stop";
			break;
		case CMD_VCR_PAUSE:
			extCommand = "pause";
			break;
		case CMD_VCR_RESUME:
			extCommand = "resume";
			break;
		case CMD_VCR_AVAILABLE:
			extCommand = "available";
			break;
		case CMD_VCR_UNKNOWN:
		default:
			extCommand = "unknown";
			printf("[CVCRControl] Unknown Command\n");
	}

	extMessage += extCommand;
	extMessage += 
		"\">\n"
		"\t\t<channelname>";
	
	CZapitClient::responseGetRecordPIDs pids;
	g_Zapit->getRecordPIDS (pids);
	CZapitClient::CRecordServiceInfo si = g_Zapit->getRecordServiceInfo ();

        APIDList apid_list;
        getAPIDs(apids,apid_list);
        apids_selected="";
	
        for(APIDList::iterator it = apid_list.begin(); it != apid_list.end(); it++)
        {
                if(it != apid_list.begin())
                        apids_selected += " ";
                sprintf(tmp, "%u", it->apid);
                apids_selected += tmp;
        }

	std::string tmpstring = g_Zapit->getChannelName(channel_id);
	if (tmpstring.empty())
		extMessage += "unknown";
	else
		extMessage += ZapitTools::UTF8_to_UTF8XML(tmpstring.c_str());
	
	extMessage += "</channelname>\n\t\t<epgtitle>";
	
	tmpstring = "not available";
	if (epgid != 0)
	{
		CShortEPGData epgdata;
		//if (g_Sectionsd->getEPGidShort(epgid, &epgdata)) {
		if(sectionsd_getEPGidShort(epgid, &epgdata)) 
		{
			//#warning fixme sectionsd should deliver data in UTF-8 format
			tmpstring = epgdata.title;
			info1 = epgdata.info1;
			info2 = epgdata.info2;
		}
	} 
	else if (!epgTitle.empty()) 
	{
		tmpstring = epgTitle;
	}
	extMessage += ZapitTools::UTF8_to_UTF8XML(tmpstring.c_str());
	
	extMessage += "</epgtitle>\n\t\t<id>";
	
	sprintf(tmp, PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id);
	extMessage += tmp;
	
	extMessage += "</id>\n\t\t<info1>";
	extMessage += ZapitTools::UTF8_to_UTF8XML(info1.c_str());
	extMessage += "</info1>\n\t\t<info2>";
	extMessage += ZapitTools::UTF8_to_UTF8XML(info2.c_str());
	extMessage += "</info2>\n\t\t<epgid>";
	sprintf(tmp, "%llu", epgid);
	extMessage += tmp;
	extMessage += "</epgid>\n\t\t<mode>";
	sprintf(tmp, "%d", g_Zapit->getMode());
	extMessage += tmp;
	extMessage += "</mode>\n\t\t<videopid>";
	sprintf(tmp, "%u", si.vpid);
	extMessage += tmp;
	extMessage += "</videopid>\n\t\t<audiopids selected=\"";
	extMessage += apids_selected;
	extMessage += "\">\n";
	// super hack :-), der einfachste weg an die apid descriptions ranzukommen
	//g_RemoteControl->current_PIDs = pids;
	//g_RemoteControl->processAPIDnames();

	for(unsigned int i= 0; i< pids.APIDs.size(); i++)
	{
		extMessage += "\t\t\t<audio pid=\"";
		sprintf(tmp, "%u", pids.APIDs[i].pid);
		extMessage += tmp;
		extMessage += "\" name=\"";
		extMessage += ZapitTools::UTF8_to_UTF8XML(g_RemoteControl->current_PIDs.APIDs[i].desc);
		extMessage += "\"/>\n";
	}
	extMessage += 
		"\t\t</audiopids>\n"
		"\t\t<vtxtpid>";
	sprintf(tmp, "%u", si.vtxtpid);
	extMessage += tmp;
	extMessage +=
		"</vtxtpid>\n"
		"\t</record>\n"
		"</neutrino>\n";

	return extMessage;
}

bool CVCRControl::CFileDevice::Stop()
{
	std::string extMessage = " ";
	time_t end_time = time(0);
	
	//printf("[direct] Stop recording, g_movieInfo %lx\n", g_movieInfo); fflush(stdout);
	//FIXME why not save info if shift ?
	//if(!autoshift || autoshift_delete) 
	{
		// g_movieInfo->length = (end_time - start_time) / 60;
		g_movieInfo->length = (int) round((double) (end_time - start_time) / (double) 60);
		//printf("[direct] stop recording 1\n"); fflush(stdout);
		g_cMovieInfo->encodeMovieInfoXml(&extMessage, g_movieInfo);
		//printf("[direct] stop recording 2\n"); fflush(stdout);
	}

	bool return_value = (::stop_recording(extMessage.c_str()) == STREAM2FILE_OK);

	//test
	//bool return_value = (::stop_recording() == STREAM2FILE_OK);
	//printf("[direct] stop recording 3\n"); fflush(stdout);

	RestoreNeutrino();

	deviceState = CMD_VCR_STOP;

	g_movieInfo->audioPids.clear();
	delete g_movieInfo;
	g_movieInfo = NULL;
	delete g_cMovieInfo;
	g_cMovieInfo = NULL;

	return return_value;
}

std::string ext_channel_name;
bool CVCRControl::CFileDevice::Record(const t_channel_id channel_id, int mode, const event_id_t epgid, const std::string& epgTitle, unsigned char apids, const time_t epg_time) 
{
	//printf("[CVCRControl] Record channel_id: " PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS " epg: %llx, apids %x mode %d\n", channel_id, epgid, apids, mode);

	// cut neutrino
	CutBackNeutrino(channel_id, mode);

#define MAXPIDS		64
	unsigned short pids[MAXPIDS];
	unsigned int numpids;
	unsigned int pos;

	//CZapitClient::CCurrentServiceInfo si = g_Zapit->getCurrentServiceInfo();
	CZapitClient::CRecordServiceInfo si = g_Zapit->getRecordServiceInfo();
	numpids = 0;

	// vpid
	if (si.vpid != 0)
		transfer_pids(si.vpid, si.vtype ? EN_TYPE_AVC : EN_TYPE_VIDEO, 0);

	// apids
        APIDList apid_list;
        getAPIDs(apids, apid_list);

        for(APIDList::iterator it = apid_list.begin(); it != apid_list.end(); it++) 
	{
                pids[numpids++] = it->apid;
		transfer_pids(it->apid, EN_TYPE_AUDIO, it->ac3 ? 1 : 0);
        }
        
#if 0 // FIXME : why this needed ?
        if(!apid_list.empty())
                g_Zapit->setAudioChannel(apid_list.begin()->index);
#endif
        CZapitClient::responseGetRecordPIDs allpids;
        g_Zapit->getRecordPIDS(allpids);

	//record file name format
	char filename[512]; // UTF-8

	// Create filename for recording
	pos = Directory.size();
	strcpy(filename, Directory.c_str());
	
	if ((pos == 0) || (filename[pos - 1] != '/')) 
	{
		filename[pos] = '/';
		pos++;
		filename[pos] = '\0';
	}

	pos = strlen(filename);
	
#if 0
	time_t t = time(NULL);
	strftime(&(filename[pos]), sizeof(filename) - pos - 1, "%Y%m%d_%H%M%S", localtime(&t));
	strcat(filename, "_");
	pos = strlen(filename);
#endif

	ext_channel_name = g_Zapit->getChannelName(channel_id);
	if (!(ext_channel_name.empty()))
	{
		strcpy(&(filename[pos]), UTF8_TO_FILESYSTEM_ENCODING(ext_channel_name.c_str()));
		char * p_act = &(filename[pos]);
		do {
			p_act += strcspn(p_act, "/ \"%&-\t`'�!,:;");
			if (*p_act) 
			{
				*p_act++ = '_';
			}
		} while (*p_act);

		// save channel name dir
		if (!autoshift && g_settings.recording_save_in_channeldir)
		{
			struct stat statInfo;
			int res = stat(filename, &statInfo);

			if (res == -1) 
			{
				if (errno == ENOENT) 
				{
					res = safe_mkdir(filename);

					if (res == 0) 
					{
						strcat(filename,"/");
					} 
					else 
					{
						perror("[vcrcontrol] mkdir");
					}
						
				} 
				else 
				{
					perror("[vcrcontrol] stat");
				}
			} 
			else 
			{
				// directory exists
				strcat(filename,"/");
			}	
					
		} 
		else
			strcat(filename, "_");
	}

	pos = strlen(filename);

	// epg for filename
	if (g_settings.recording_epg_for_filename) 
	{ 
		if(epgid != 0) 
		{
			CShortEPGData epgdata;
			//if (g_Sectionsd->getEPGidShort(epgid, &epgdata))
			if(sectionsd_getEPGidShort(epgid, &epgdata))
			{
				if (!(epgdata.title.empty()))
				{
					strcpy(&(filename[pos]), epgdata.title.c_str());
					char * p_act = &(filename[pos]);
					do {
						p_act +=  strcspn(p_act, "/ \"%&-\t`'~<>!,:;?^�$\\=*#@�|");

						if (*p_act) 
						{
							*p_act++ = '_';
						}
					} while (*p_act);
				}
			}
		} 
		else if (!epgTitle.empty()) 
		{
			strcpy(&(filename[pos]), epgTitle.c_str());
			char * p_act = &(filename[pos]);
			do {
				p_act +=  strcspn(p_act, "/ \"%&-\t`'~<>!,:;?^�$\\=*#@�|");
				if (*p_act) 
				{
					*p_act++ = '_';
				}
			} while (*p_act);
		}
	}

#if 1
	pos = strlen(filename);
	time_t t = time(NULL);
	strftime(&(filename[pos]), sizeof(filename) - pos - 1, "%Y%m%d_%H%M%S", localtime(&t));
#endif

	start_time = time(0);

	stream2file_error_msg_t error_msg = ::start_recording(filename,
			      getMovieInfoString(CMD_VCR_RECORD, channel_id, epgid, epgTitle, apid_list, epg_time).c_str(), 
			      si.vpid, 
			      pids, 
			      numpids);

	if (error_msg == STREAM2FILE_OK) 
	{
		deviceState = CMD_VCR_RECORD;
		return true;
	}
	else 
	{
		RestoreNeutrino();

		printf("[CVCRControl] stream2file error code: %d\n", error_msg);
#warning FIXME: Use better error message
		DisplayErrorMessage(g_Locale->getText(
				      error_msg == STREAM2FILE_BUSY ? LOCALE_STREAMING_BUSY :
				      error_msg == STREAM2FILE_INVALID_DIRECTORY ? LOCALE_STREAMING_DIR_NOT_WRITABLE :
				      LOCALE_STREAMINGSERVER_NOCONNECT
				      )); // UTF-8

		return false;
	}
}

bool sectionsd_getActualEPGServiceKey(const t_channel_id uniqueServiceKey, CEPGData * epgdata);

void strReplace(std::string & orig, const char *fstr, const std::string rstr);

bool CVCRControl::Screenshot(const t_channel_id channel_id, char * fname, int spos) 
{
	char filename[512]; // UTF-8
	char cmd[512];
	std::string channel_name;
	CEPGData                epgData;
	event_id_t epgid = 0;
	unsigned int pos;

	if(!fname) // live stream
	{
		std::string str = "sda2";
		
		struct statfs s;
		
		if (::statfs(g_settings.network_nfs_recordingdir, &s) == 0) 
		{
			//std::string str1 = g_settings.network_nfs_recordingdir;
			//str = str1.substr(7, 4);
			//str = str1.substr(str1.length() - 4, str1.length());
		}
		char str2[100];	
		sprintf(str2, "/media/%s/screenshots/", str.c_str() );
		
		if(safe_mkdir(str2))
			return false;

		strcpy(filename, str2);
	
		pos = strlen(filename);
		channel_name = g_Zapit->getChannelName(channel_id);

		if (!(channel_name.empty())) 
		{
			strcpy(&(filename[pos]), UTF8_TO_FILESYSTEM_ENCODING(channel_name.c_str()));
			char * p_act = &(filename[pos]);

			do {
				p_act += strcspn(p_act, "/ \"%&-\t`'�!,:;");
				if (*p_act) {
					*p_act++ = '_';
				}
			} while (*p_act);
			strcat(filename, "_");
		}

		pos = strlen(filename);

		//if (g_Sectionsd->getActualEPGServiceKey(channel_id&0xFFFFFFFFFFFFULL, &epgData))
		if(sectionsd_getActualEPGServiceKey(channel_id&0xFFFFFFFFFFFFULL, &epgData));
			epgid = epgData.eventID;

		if(epgid != 0) 
		{
			CShortEPGData epgdata;
			//if (g_Sectionsd->getEPGidShort(epgid, &epgdata)) {
			if(sectionsd_getEPGidShort(epgid, &epgdata)) 
			{
				if (!(epgdata.title.empty())) 
				{
					strcpy(&(filename[pos]), epgdata.title.c_str());
					char * p_act = &(filename[pos]);
					do {
						p_act +=  strcspn(p_act, "/ \"%&-\t`'~<>!,:;?^�$\\=*#@�|");
						if (*p_act) {
							*p_act++ = '_';
						}
					} while (*p_act);
				}
			}
		}
		
		pos = strlen(filename);
		time_t t = time(NULL);
		strftime(&(filename[pos]), sizeof(filename) - pos - 1, "%Y%m%d_%H%M%S", localtime(&t));
		
		strcat(filename, ".jpg");
		
#ifdef __sh__		
		sprintf(cmd, "ffmpeg -itsoffset -4 -i `wget -q -O - \"http://127.0.0.1/control/build_live_url?vlc_link=true\"` -y -f image2 -sn -an -vframes 1 -s 320*240 %s", filename);
#else
		sprintf(cmd, "grab -v %s", filename);
#endif
	} 
	else
	{
		//from tsbrowser(MP)
		strcpy(filename, fname);

		std::string file_name = fname; // UTF-8
		
		strReplace(file_name, ".ts", ".jpg");
	
#ifdef __sh__		
		sprintf(cmd, "ffmpeg -y -i %s -y -f image2 -ss %d -vframes 1 -s 320*240 %s", fname, spos, (char *)file_name.c_str());
#else
		//sprintf(cmd, "grab -v %s", filename);
		sprintf(cmd, "grab -v %s", (char *)file_name.c_str());
#endif
	}
	
	printf("Executing %s\n", cmd);
	
	CHintBox * hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_SCREENSHOT_CREATING) );

	hintBox->paint();
	
	if(system(cmd))
		return false;
	
	hintBox->hide();
	delete hintBox;
	
	return true;
}

extern unsigned short g_vpid;
extern unsigned short g_vtype;
extern unsigned short g_apids[10];
extern unsigned short g_ac3flags[10];
extern unsigned short g_numpida;
extern unsigned int g_currentapid, g_currentac3;

std::string CVCRControl::CFileAndServerDevice::getMovieInfoString(const CVCRCommand command, const t_channel_id channel_id, const event_id_t epgid, const std::string& epgTitle, APIDList apid_list, const time_t epg_time)
{
	std::string extMessage;
	std::string apids10;
	std::string info1, info2;
	if(!g_cMovieInfo)
		g_cMovieInfo = new CMovieInfo();
	if(!g_movieInfo)
		g_movieInfo = new MI_MOVIE_INFO();

	g_cMovieInfo->clearMovieInfo(g_movieInfo);

	//CZapitClient::responseGetRecordPIDs pids;
	g_Zapit->getRecordPIDS(pids);
	CZapitClient::CRecordServiceInfo si = g_Zapit->getRecordServiceInfo();
	//CZapitClient::responseGetPIDs pids;
	//g_Zapit->getPIDS(pids);
	//CZapitClient::CRecordServiceInfo si = g_Zapit->getRecordServiceInfo();

	std::string tmpstring = g_Zapit->getChannelName(channel_id);
	if (tmpstring.empty())
		g_movieInfo->epgChannel = "unknown";
	else
		g_movieInfo->epgChannel = ZapitTools::UTF8_to_UTF8XML(tmpstring.c_str());

	tmpstring = "not available";
	if (epgid != 0) 
	{
		CEPGData epgdata;
		//if (g_Sectionsd->getEPGid(epgid, epg_time, &epgdata)) {
		if (sectionsd_getEPGid(epgid, epg_time, &epgdata)) 
		{
			tmpstring = epgdata.title;
			info1 = epgdata.info1;
			info2 = epgdata.info2;
			
			g_movieInfo->parentalLockAge = epgdata.fsk;
			
			if(epgdata.contentClassification.size() > 0 )
				g_movieInfo->genreMajor = epgdata.contentClassification[0];
				
			g_movieInfo->length = epgdata.epg_times.dauer	/ 60;
				
			printf("[CVCRControl] fsk:%d, Genre:%d, Dauer: %d min\r\n",g_movieInfo->parentalLockAge,g_movieInfo->genreMajor,g_movieInfo->length);	
		}
	} 
	else if (!epgTitle.empty()) 
	{
		tmpstring = epgTitle;
	}
	g_movieInfo->epgTitle		= ZapitTools::UTF8_to_UTF8XML(tmpstring.c_str());
	g_movieInfo->epgId		= channel_id;
	g_movieInfo->epgInfo1		= ZapitTools::UTF8_to_UTF8XML(info1.c_str());
	g_movieInfo->epgInfo2		= ZapitTools::UTF8_to_UTF8XML(info2.c_str());
	g_movieInfo->epgEpgId		= epgid ;
	g_movieInfo->epgMode		= g_Zapit->getMode();
	g_movieInfo->epgVideoPid	= si.vpid;
	g_movieInfo->VideoType		= si.vtype;

	g_vpid = si.vpid;
	g_vtype = si.vtype;
	g_currentapid = si.apid;
	memset(g_apids, 0, sizeof(unsigned short)*10);
	memset(g_ac3flags, 0, sizeof(unsigned short)*10);
	g_numpida = 0;

	EPG_AUDIO_PIDS audio_pids;
	
	// get apids desc
	processAPIDnames();

	APIDList::iterator it;
	for(unsigned int i= 0; i< pids.APIDs.size(); i++) 
	{
		for(it = apid_list.begin(); it != apid_list.end(); it++) 
		{
			if(pids.APIDs[i].pid == it->apid) 
			{
				audio_pids.epgAudioPid = pids.APIDs[i].pid;
				audio_pids.epgAudioPidName = ZapitTools::UTF8_to_UTF8XML(g_RemoteControl->current_PIDs.APIDs[i].desc);
				audio_pids.atype = pids.APIDs[i].is_ac3;
				audio_pids.selected = (audio_pids.epgAudioPid == (int) g_currentapid) ? 1 : 0;
				g_movieInfo->audioPids.push_back(audio_pids);

				if(pids.APIDs[i].is_ac3)
					g_ac3flags[i] = 1;

				g_apids[i] = pids.APIDs[i].pid;
				if(g_apids[i] == g_currentapid)
					g_currentac3 = pids.APIDs[i].is_ac3;
				g_numpida++;
			}
		}
	}

	//FIXME sometimes no apid in xml ??
	if(g_movieInfo->audioPids.empty() && pids.APIDs.size()) 
	{
		int i = 0;
		audio_pids.epgAudioPid = pids.APIDs[i].pid;
		audio_pids.epgAudioPidName = ZapitTools::UTF8_to_UTF8XML(g_RemoteControl->current_PIDs.APIDs[i].desc);
		audio_pids.atype = pids.APIDs[i].is_ac3;
		audio_pids.selected = 1;
		g_movieInfo->audioPids.push_back(audio_pids);
	}
	g_movieInfo->epgVTXPID = si.vtxtpid;

	g_cMovieInfo->encodeMovieInfoXml(&extMessage, g_movieInfo);
	
	//g_movieInfo->audioPids.clear();

	return extMessage;
}

bool sectionsd_getComponentTagsUniqueKey(const event_id_t uniqueKey, CSectionsdClient::ComponentTagList& tags);
void CVCRControl::CFileAndServerDevice::processAPIDnames()
{
	bool has_unresolved_ctags= false;
	bool has_ac3 = false; //FIXME what this variable suppoused to do ?? seems unused
	int pref_found = -1;
	int pref_ac3_found = -1;
	int pref_idx = -1;
	int pref_ac3_idx = -1;
	int ac3_found = -1;
	const char *desc;
	char lang[4];

	#if 0
	if(g_settings.auto_lang) 
	{
		// first we check prefs to find pid according to pref index
		for(int i = 0; i < 3; i++) 
		{
			for(int j = 0; j < (int) current_PIDs.APIDs.size(); j++) 
			{
				desc = current_PIDs.APIDs[j].desc;
				// In some cases AAC is the only audio system used
				// so accept it here as a 'normal' sound track
				if(strstr(desc, "(AAC)")) 
				{
					strncpy(lang, desc, 3);
					lang[3] = 0;
					desc = lang;
				}
				/* processAPIDnames called 2 times, TODO find better way to detect second call */
				if(strlen( desc ) != 3)
					continue;
				
				if(strlen(g_settings.pref_lang[i]) == 0)
					continue;

				std::string temp(g_settings.pref_lang[i]);
				std::map<std::string, std::string>::const_iterator it;
				for(it = iso639.begin(); it != iso639.end(); it++) 
				{
					if(temp == it->second && strcasecmp(desc, it->first.c_str()) == 0) 
					{
						/* remember first pref found index and pid*/
						if(pref_found < 0) 
						{
							pref_found = j;
							pref_idx = i;
						}
						
						if(current_PIDs.APIDs[j].is_ac3 && g_settings.audio_DolbyDigital && (pref_ac3_found < 0)) {
							pref_ac3_found = j;
							pref_ac3_idx = i;
						}
						break;
					}
				}
			} /* for all pids */
		} /*for all prefs*/

		/* reset pref ac3, if it have lower priority */
		if((pref_idx >= 0) && (pref_idx < pref_ac3_idx))
			pref_ac3_found = -1;
	}
	#endif

	for(unsigned int count=0; count < pids.APIDs.size(); count++)
	{
		//printf("CRemoteControl::processAPIDnames: apid name= %s (%s) pid= 0x%x\n", current_PIDs.APIDs[count].desc, getISO639Description( current_PIDs.APIDs[count].desc ), current_PIDs.APIDs[count].pid);
		if ( pids.APIDs[count].component_tag != 0xFF )
		{
			has_unresolved_ctags= true;
		}
		
		if ( strlen( pids.APIDs[count].desc ) == 3 )
		{
			// unaufgeloeste Sprache...
			strcpy( pids.APIDs[count].desc, getISO639Description( pids.APIDs[count].desc ) );
		}

		if ( pids.APIDs[count].is_ac3 )
		{
			strncat(pids.APIDs[count].desc, " (AC3)", 25);
			has_ac3 = true;
			if((strlen( pids.APIDs[count].desc ) == 3) && g_settings.audio_DolbyDigital && (ac3_found < 0))
				ac3_found = count;
		}
	}

	if ( has_unresolved_ctags )
	{
		if ( record_EPGid != 0 )
		{
			CSectionsdClient::ComponentTagList tags;
			//if ( g_Sectionsd->getComponentTagsUniqueKey( current_EPGid, tags ) )
			if ( sectionsd_getComponentTagsUniqueKey( record_EPGid, tags ) )
			{
				has_unresolved_ctags = false;

				for (unsigned int i=0; i< tags.size(); i++)
				{
					for (unsigned int j=0; j< pids.APIDs.size(); j++)
					{
						if ( pids.APIDs[j].component_tag == tags[i].componentTag )
						{
							// workaround for buggy ZDF ctags / or buggy sectionsd/drivers , who knows...
							if(!tags[i].component.empty())
							{
								strncpy(pids.APIDs[j].desc, tags[i].component.c_str(), 25);
								if ( pids.APIDs[j].is_ac3)
									strncat( pids.APIDs[j].desc, " (AC3)", 25);
								
								else if ( pids.APIDs[j].is_aac)
									strncat( pids.APIDs[j].desc, " (AAC)", 25);
							}
							pids.APIDs[j].component_tag = -1;
							break;
						}
					}
				}
			}
		}
	}
	
	//printf("CRemoteControl::processAPIDnames: pref_found %d pref_ac3_found %d ac3_found %d\n", pref_found, pref_ac3_found, ac3_found);
	
	#if 0
	if(pref_ac3_found >= 0) 
	{
		printf("CRemoteControl::processAPIDnames: set apid name= %s pid= 0x%x\n", current_PIDs.APIDs[pref_ac3_found].desc, current_PIDs.APIDs[pref_ac3_found].pid);
		setAPID(pref_ac3_found);
	} 
	else if(pref_found >= 0) 
	{
		printf("CRemoteControl::processAPIDnames: set apid name= %s pid= 0x%x\n", current_PIDs.APIDs[pref_found].desc, current_PIDs.APIDs[pref_found].pid);
		setAPID(pref_found);
	}
	else if(ac3_found >= 0) 
	{
		printf("CRemoteControl::processAPIDnames: set apid name= %s pid= 0x%x\n", current_PIDs.APIDs[ac3_found].desc, current_PIDs.APIDs[ac3_found].pid);
		setAPID(ac3_found);
	}
	else if ( current_PIDs.PIDs.selected_apid >= current_PIDs.APIDs.size() )
	{
		setAPID( 0 );
	}
	#endif

	//t_channel_id * p = new t_channel_id;
	//*p = current_channel_id;
	//g_RCInput->postMsg(NeutrinoMessages::EVT_ZAP_GOTAPIDS, (const neutrino_msg_data_t)p, false); // data is pointer to allocated memory
}


