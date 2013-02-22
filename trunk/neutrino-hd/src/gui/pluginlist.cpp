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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <plugin.h>

#include <gui/pluginlist.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/icons.h>

#include <sstream>
#include <fstream>
#include <iostream>

#include <dirent.h>
#include <dlfcn.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <global.h>
#include <neutrino.h>
#include <plugins.h>
#include <driver/encoding.h>

#include <zapit/client/zapittools.h>

#include <daemonc/remotecontrol.h>
extern CPlugins       * g_PluginList;    /* neutrino.cpp */
extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */

CPluginList::CPluginList(const neutrino_locale_t Name, const uint32_t listtype)
{
	frameBuffer = CFrameBuffer::getInstance();
	
	name = Name;
	pluginlisttype = listtype;
	
	selected = 0;
	width = 500;
	if(width>(g_settings.screen_EndX-g_settings.screen_StartX))
		width=(g_settings.screen_EndX - g_settings.screen_StartX);
	
	height = 526;
	if((height+50)>(g_settings.screen_EndY-g_settings.screen_StartY))
		height=(g_settings.screen_EndY-g_settings.screen_StartY) - 50; // 2*25 pixel frei
	theight  = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	//
	fheight1 = g_Font[SNeutrinoSettings::FONT_TYPE_GAMELIST_ITEMLARGE]->getHeight();
	fheight2 = g_Font[SNeutrinoSettings::FONT_TYPE_GAMELIST_ITEMSMALL]->getHeight();
	fheight = fheight1 + fheight2 + 2;
	//
	listmaxshow = (height-theight-0)/fheight;
	height = theight+0+listmaxshow*fheight; // recalc height
	x = (((g_settings.screen_EndX- g_settings.screen_StartX)-width) / 2) + g_settings.screen_StartX;
	y = (((g_settings.screen_EndY- g_settings.screen_StartY)-height) / 2) + g_settings.screen_StartY;
	liststart = 0;
}

CPluginList::~CPluginList()
{
	for(unsigned int count=0;count<pluginlist.size();count++)
	{
		delete pluginlist[count];
	}
	pluginlist.clear();
}

int CPluginList::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;

	if (parent)
	{
		parent->hide();
	}

	//scan4plugins here!
	for(unsigned int count=0; count<pluginlist.size(); count++)
	{
		delete pluginlist[count];
	}
	pluginlist.clear();

	pluginitem * tmp = new pluginitem();
	tmp->name = g_Locale->getText(LOCALE_MENU_BACK);
	pluginlist.push_back(tmp);

	for(unsigned int count=0; count < (unsigned int)g_PluginList->getNumberOfPlugins(); count++)
	{
		if ((g_PluginList->getType(count) & pluginlisttype) && !g_PluginList->isHidden(count))
		{
			tmp = new pluginitem();
			tmp->number = count;
			tmp->name = g_PluginList->getName(count);
			tmp->desc = g_PluginList->getDescription(count);
			tmp->icon = g_PluginList->getIcon(count);
			pluginlist.push_back(tmp);
		}
	}

	paint();
	
#ifdef FB_BLIT
	frameBuffer->blit();
#endif	

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

	bool loop=true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );
		neutrino_msg_t msg_repeatok = msg & ~CRCInput::RC_Repeat;

		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

		if ( ( msg == CRCInput::RC_timeout ) || ( msg == (neutrino_msg_t)g_settings.key_channelList_cancel ) )
		{
			loop=false;
		}
		else if ( msg == (neutrino_msg_t)g_settings.key_channelList_pageup )
		{
			if ((int(selected)-int(listmaxshow))<0)
				selected=0;
			else
				selected -= listmaxshow;
			liststart = (selected/listmaxshow)*listmaxshow;
			paintItems();
		}
		else if ( msg == (neutrino_msg_t)g_settings.key_channelList_pagedown )
		{
			selected+=listmaxshow;
			if (selected>pluginlist.size()-1)
				selected=pluginlist.size()-1;
			liststart = (selected/listmaxshow)*listmaxshow;
			paintItems();
		}
		else if ( msg == CRCInput::RC_up )
		{
			int prevselected=selected;
			if(selected==0)
			{
				selected = pluginlist.size()-1;
			}
			else
				selected--;
			paintItem(prevselected - liststart);
			unsigned int oldliststart = liststart;
			liststart = (selected/listmaxshow)*listmaxshow;
			if(oldliststart!=liststart)
			{
				paintItems();
			}
			else
			{
				paintItem(selected - liststart);
			}
		}
		else if ( msg == CRCInput::RC_down )
		{
			int prevselected=selected;
			selected = (selected+1)%pluginlist.size();
			paintItem(prevselected - liststart);
			unsigned int oldliststart = liststart;
			liststart = (selected/listmaxshow)*listmaxshow;
			if(oldliststart!=liststart)
			{
				paintItems();
			}
			else
			{
				paintItem(selected - liststart);
			}
		}
		else if ( msg == CRCInput::RC_ok )
		{
			if(selected==0)
			{
				loop=false;
			}
			else
			{
				//exec the plugin :))
				if (pluginSelected() == close)
				{
					loop=false;
				}
			}
		}
		else if( (msg== CRCInput::RC_red) || (msg==CRCInput::RC_green) || (msg==CRCInput::RC_yellow) || (msg==CRCInput::RC_blue)  || (CRCInput::isNumeric(msg)) )
		{
			g_RCInput->postMsg(msg, data);
			loop=false;
		}
		else if ( CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all )
		{
			loop = false;
			res = menu_return::RETURN_EXIT_ALL;
		}
#ifdef FB_BLIT
		frameBuffer->blit();
#endif		
	}
	
	hide();
	
	return res;
}

void CPluginList::hide()
{
	frameBuffer->paintBackgroundBoxRel(x, y, width + 15, height + ((RADIUS_MID * 2) + 1));
#ifdef FB_BLIT
	frameBuffer->blit();
#endif	
}

void CPluginList::paintItem(int pos)
{
	int ypos = (y+theight) + pos*fheight;
	int itemheight = fheight;

	uint8_t    color   = COL_MENUCONTENT;
	fb_pixel_t bgcolor = COL_MENUCONTENT_PLUS_0;

	if (liststart + pos == selected)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}

	if(liststart+pos == 0)
	{	//back is half-height...
		itemheight = (fheight / 2) + 3;
		frameBuffer->paintBoxRel(x     , ypos + itemheight    , width     , 15, COL_MENUCONTENT_PLUS_0);
		frameBuffer->paintBoxRel(x + 10, ypos + itemheight + 5, width - 20,  1, COL_MENUCONTENT_PLUS_5);
		frameBuffer->paintBoxRel(x + 10, ypos + itemheight + 6, width - 20,  1, COL_MENUCONTENT_PLUS_2);
	}
	else if(liststart==0)
	{
		ypos -= (fheight / 2) - 15;
		if(pos==(int)listmaxshow-1)
			frameBuffer->paintBoxRel(x,ypos+itemheight, width, (fheight / 2)-15, COL_MENUCONTENT_PLUS_0);

	}
	
	frameBuffer->paintBoxRel(x, ypos, width, itemheight, bgcolor );
	
	// name + desc + icon???
	if(liststart + pos <pluginlist.size())
	{
		pluginitem * actplugin = pluginlist[liststart + pos];
		
		bool isback_menu  = !strcmp((const char *)actplugin->name.c_str(), (const char *)g_Locale->getText(LOCALE_MENU_BACK) );
		
		// icon
		int icon_w = 0;
		int icon_h = 0;
		if(!isback_menu)
		{
			std::string IconName = PLUGINDIR "/" + actplugin->icon;
			
			frameBuffer->getIconSize(NEUTRINO_ICON_PLUGIN, &icon_w, &icon_h);
			
			if (!(actplugin->icon.empty()))
				frameBuffer->paintIcon(IconName.c_str(), x + 8, ypos + 5 );
			else
				frameBuffer->paintIcon( NEUTRINO_ICON_PLUGIN, x + 8, ypos + 5 );
		}
		
		// name
		g_Font[SNeutrinoSettings::FONT_TYPE_GAMELIST_ITEMLARGE]->RenderString(x + 10 + icon_w, ypos+fheight1+3, width-20, actplugin->name, color, 0, true); // UTF-8
		
		// desc
		g_Font[SNeutrinoSettings::FONT_TYPE_GAMELIST_ITEMSMALL]->RenderString(x + 20 + icon_w, ypos+fheight,    width-20, actplugin->desc, color, 0, true); // UTF-8
	}
}

void CPluginList::paintHead()
{
	int sb_width = 0;
	if(listmaxshow <= pluginlist.size()+1)
		sb_width=15;

	frameBuffer->paintBoxRel(x, y + height - ((RADIUS_MID * 2) + 1) + (RADIUS_MID / 3 * 2), width + sb_width, ((RADIUS_MID * 2) + 1), COL_MENUCONTENT_PLUS_0, RADIUS_MID, CORNER_BOTTOM);
	frameBuffer->paintBoxRel(x, y, width + sb_width, theight, COL_MENUHEAD_PLUS_0, RADIUS_MID, CORNER_TOP);
	frameBuffer->paintBoxRel(x, y + theight, width, height- theight - ((RADIUS_MID * 2) + 1) + (RADIUS_MID / 3 * 2), COL_MENUCONTENT_PLUS_0);

	// title + icon
	if(pluginlisttype == CPlugins::P_TYPE_GAME)
	{
		frameBuffer->paintIcon(NEUTRINO_ICON_GAMES, x + 8, y + 5);

		int neededWidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(g_Locale->getText(name), true); // UTF-8
		int stringstartposX = x +(width >> 1) - (neededWidth >> 1);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(stringstartposX, y + theight, width - (stringstartposX- x) , g_Locale->getText(name), COL_MENUHEAD, 0, true); // UTF-8
	} 
	else
	{
		frameBuffer->paintIcon(NEUTRINO_ICON_SHELL, x + 8, y + 5);

		int neededWidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(g_Locale->getText(name), true); // UTF-8
		int stringstartposX = x +(width >> 1) - (neededWidth >> 1);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(stringstartposX, y + theight, width - (stringstartposX- x) , g_Locale->getText(name), COL_MENUHEAD, 0, true); // UTF-8
	}
}

void CPluginList::paint()
{
	hide();
	width = 500;
	if(width>(g_settings.screen_EndX-g_settings.screen_StartX))
		width=(g_settings.screen_EndX-g_settings.screen_StartX);
	height = 526;
	if((height+50)>(g_settings.screen_EndY-g_settings.screen_StartY))
		height=(g_settings.screen_EndY-g_settings.screen_StartY) - 50; // 2*25 pixel frei
	listmaxshow = (height-theight-0)/fheight;
	height = theight+0+listmaxshow*fheight; // recalc height
	x=(((g_settings.screen_EndX- g_settings.screen_StartX)-width) / 2) + g_settings.screen_StartX;
	y=(((g_settings.screen_EndY- g_settings.screen_StartY)-height) / 2) + g_settings.screen_StartY;
	
	liststart = (selected/listmaxshow)*listmaxshow;

	// head
	paintHead();
	
	// items
	paintItems();
}

void CPluginList::paintItems()
{
	if(listmaxshow <= pluginlist.size()+1)
	{
		// Scrollbar
		int nrOfPages = ((pluginlist.size()-1) / listmaxshow)+1; 
		int currPage  = (liststart/listmaxshow) +1;
		frameBuffer->paintBoxRel(x+width, y+theight, 15, height-theight,  COL_MENUCONTENT_PLUS_1);
		frameBuffer->paintBoxRel(x+ width +2, y+theight+2+(currPage-1)*(height-theight-4)/nrOfPages, 11, (height-theight-4)/nrOfPages, COL_MENUCONTENT_PLUS_3 );
	}
	
	for(unsigned int count=0;count<listmaxshow;count++)
	{
		paintItem(count);
	}
}

CPluginList::result_ CPluginList::pluginSelected()
{
	g_PluginList->startPlugin(pluginlist[selected]->number, 0);
	
	paint();
	
#ifdef FB_BLIT
	frameBuffer->blit();
#endif	
	
	return resume;
}

CPluginChooser::CPluginChooser(const neutrino_locale_t Name, const uint32_t listtype, char* pluginname)
	: CPluginList(Name, listtype), selected_plugin(pluginname)
{
}

CPluginList::result_ CPluginChooser::pluginSelected()
{
	strcpy(selected_plugin,g_PluginList->getFileName(pluginlist[selected]->number));
	return CPluginList::close;
}