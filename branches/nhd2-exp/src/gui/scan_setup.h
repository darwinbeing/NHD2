/*
	$Id: scan_setup.h,v 1.4 2009/12/15 09:44:33 dbt Exp $

	Copyright (C) 2009 Thilo Graf (dbt)
	http://www.dbox2-tuning.de

	Neutrino-GUI  -   DBoxII-Project

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

#ifndef __scan_setup__
#define __scan_setup__

#include <gui/widget/menue.h>

#include <driver/framebuffer.h>

#include <settings.h>  /*zapit/settings.h*/

#include <string>



class CTPSelectHandler : public CMenuTarget
{
	private:
		int feindex;
	
	public:
		CTPSelectHandler(int num = 0);
		int exec(CMenuTarget* parent,  const std::string &actionkey);
};

class CScanSettings
{
	private:
		int feindex;
		
		uint32_t	getConfigValue(int num, const char * name, uint32_t defval);
		void		setConfigValue(int num, const char * name, uint32_t val);
		
	public:
		CConfigFile	configfile;
		
		int		bouquetMode;
		int		scanType;
		char            satNameNoDiseqc[50];
		
		int		scanSectionsd;
		int		scan_mode;
		int		TP_fec;
		int		TP_pol;
		int		TP_mod;
		char		TP_freq[10];
		char		TP_rate[9];
	
		//DVB-T
		int		TP_band;
		int 		TP_HP;
		int 		TP_LP;
		int		TP_const;
		int		TP_trans;
		int		TP_guard;
		int		TP_hierarchy;
	
		CScanSettings(int num = 0);
	
		bool loadSettings(const char * const fileName, int index = 0);
		bool saveSettings(const char * const fileName, int index = 0);
};

class CScanSetup : public CMenuTarget
{
	private:
		int feindex;

		void hide();
		void showScanService();
		
	public:
		CScanSetup(int num = 0);
		~CScanSetup();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

// sat setup notifuer
class CSatelliteSetupNotifier : public CChangeObserver
{
	private:
		std::vector<CMenuItem*> items1;
		std::vector<CMenuItem*> items2;
		std::vector<CMenuItem*> items3;
		
		int feindex;
	public:
		CSatelliteSetupNotifier(int num = 0);
		void addItem(int list, CMenuItem* item);
		bool changeNotify(const neutrino_locale_t, void * Data);
};

// scan setup notifier
class CScanSetupNotifier : public CChangeObserver
{
	private:
		std::vector<CMenuItem*> items1;
		std::vector<CMenuItem*> items2;
		std::vector<CMenuItem*> items3;
		
		int feindex;
	public:
		CScanSetupNotifier(int num = 0);
		void addItem(int list , CMenuItem *item);
		bool changeNotify(const neutrino_locale_t, void * Data);
};

#endif
