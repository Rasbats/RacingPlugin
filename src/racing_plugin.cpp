// Copyright(C) 2018-2020 by Steven Adler
//
// This file is part of Racing plugin for OpenCPN.
//
// Racing plugin for OpenCPN is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Racing plugin for OpenCPN is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the Racing plugin for OpenCPN. If not, see <https://www.gnu.org/licenses/>.
//

//
// Project: Racing Plugin
// Description: Race Start display for OpenCPN
// Owner: twocanplugin@hotmail.com
// Date: 6/1/2020
// Version History: 
// 1.0 Initial Release
//

#include "racing_plugin.h"
#include "racing_icons.h"

// Globally accessible variables used by the plugin
wxFileConfig *configSettings;

double currentLatitude;
double currentLongitude;
double courseOverGround;
double speedOverGround;

// Toolbar State
bool racingWindowVisible;

// The class factories, used to create and destroy instances of the PlugIn
extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr) {
	return new RacingPlugin(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) {
	delete p;
}

// Constructor
RacingPlugin::RacingPlugin(void *ppimgr) : opencpn_plugin_18(ppimgr), wxEvtHandler() {
	
	// Load the plugin bitmaps/icons 
	initialize_images();
}

// Destructor
RacingPlugin::~RacingPlugin(void) {
}

int RacingPlugin::Init(void) {
	// Maintain a reference to the OpenCPN window to use as the parent for the Race Start Window
	parentWindow = GetOCPNCanvasWindow();

	// Maintain a reference to the OpenCPN configuration object 
	// Not used, however could have a preference for the display units (metres, yards) and count down time (5 minutes etc.)
	configSettings = GetOCPNConfigObject();

	// Load plugin icons
	wxString shareLocn = *GetpSharedDataLocation() +
	_T("plugins") + wxFileName::GetPathSeparator() +
	_T("racing_pi") + wxFileName::GetPathSeparator() +
	_T("data") + wxFileName::GetPathSeparator();
	wxString normalIcon = shareLocn + _T("racing_icon_normal.svg");
	wxString toggledIcon = shareLocn + _T("racing_icon_toggled.svg");
	wxString rolloverIcon = shareLocn + _T("racing_icon_rollover.svg");

	// BUG BUG Change for OpenCPN 5.0
	racingToolbar = InsertPlugInToolSVG(_T(""), normalIcon, rolloverIcon, toggledIcon, wxITEM_CHECK, _("Race Start Display"), _T(""), NULL, -1, 0, this);

	// Wire up the Dialog Close event
	// BUG BUG For some reason couldn't use wxAUI (Advanced User Interface), casting bug ??)
	Connect(wxEVT_RACE_DIALOG_EVENT, wxCommandEventHandler(RacingPlugin::OnDialogEvent));

	racingWindowVisible = false;

	// Notify OpenCPN what events we want to receive callbacks for
	return (WANTS_CONFIG | WANTS_TOOLBAR_CALLBACK | INSTALLS_TOOLBAR_TOOL | WANTS_NMEA_EVENTS);
}

// OpenCPN is either closing down, or we have been disabled from the Preferences Dialog
bool RacingPlugin::DeInit(void) {
	Disconnect(wxEVT_RACE_DIALOG_EVENT, wxCommandEventHandler(RacingPlugin::OnDialogEvent));
	return TRUE;
}

// Indicate what version of the OpenCPN Plugin API we support
int RacingPlugin::GetAPIVersionMajor() {
	return OPENCPN_API_VERSION_MAJOR;
}

int RacingPlugin::GetAPIVersionMinor() {
	return OPENCPN_API_VERSION_MINOR;
}

// The plugin version numbers. 
int RacingPlugin::GetPlugInVersionMajor() {
	return PLUGIN_VERSION_MAJOR;
}

int RacingPlugin::GetPlugInVersionMinor() {
	return PLUGIN_VERSION_MINOR;
}

// Return descriptions for the Plugin
wxString RacingPlugin::GetCommonName() {
	return _T("Race Start Display");
}

wxString RacingPlugin::GetShortDescription() {
	return _T("Race Start Display, Countdown timer, distance and time to start line");
}

wxString RacingPlugin::GetLongDescription() {
	return _T("Race Start Display, Countdown timer, distance and time to start line");
}

// 32x32 pixel PNG file, use pgn2wx.pl perl script
wxBitmap* RacingPlugin::GetPlugInBitmap() {
		return _img_racing_logo_32;
}

// Receive Position, Course & Speed from OpenCPN
void RacingPlugin::SetPositionFix(PlugIn_Position_Fix &pfix) {
	currentLatitude = pfix.Lat; 
	currentLongitude = pfix.Lon; 
	courseOverGround = pfix.Cog; 
	speedOverGround = pfix.Sog; 
}

// We only install a singe toolbar item
int RacingPlugin::GetToolbarToolCount(void) {
 return 1;
}

int RacingPlugin::GetToolbarItemId() { 
	return racingToolbar; 
}

void RacingPlugin::OnToolbarToolCallback(int id) {
	// Display modal Race Start Window
	//RacingDialog *racingDialog = new RacingDialog(parentWindow);
	//racingDialog->ShowModal();
	//delete racingDialog;
	//SetToolbarItemState(id, false);

	// Display a non-modal Race Start Window
	if (!racingWindowVisible) {
		racingWindow = new RacingWindow(parentWindow, this);
		racingWindowVisible = true;
		SetToolbarItemState(id, racingWindowVisible);
		racingWindow->Show(true);
	}
	else {
		racingWindow->Close();
		delete racingWindow;
		SetToolbarItemState(id, racingWindowVisible);
	}

	// BUG BUG Investigating.
	wxLogMessage(_T("*** Distance Unit: %s"), getUsrDistanceUnit_Plugin(-1));
	wxLogMessage(_T("*** Speed Unit: %s"), getUsrSpeedUnit_Plugin(-1));
}

// Handle events from the Race Start Dialog
void RacingPlugin::OnDialogEvent(wxCommandEvent &event) {
	switch (event.GetId()) {
		// Keep the toolbar & canvas in sync with the display of the race start dialog
		case RACE_DIALOG_CLOSED:
			if (!starboardMarkGuid.IsEmpty()) {
				DeleteSingleWaypoint(starboardMarkGuid);
			}
			if (!portMarkGuid.IsEmpty()) {
				DeleteSingleWaypoint(portMarkGuid);
			}
			SetToolbarItemState(racingToolbar, racingWindowVisible);
			break;
		// drop temporary waypoints to represent port & starboard ends of the start line
		case RACE_DIALOG_STBD: {
			PlugIn_Waypoint waypoint;
			waypoint.m_IsVisible = true;
			waypoint.m_MarkName = _T("Starboard");
			starboardMarkGuid = GetNewGUID();
			waypoint.m_GUID = starboardMarkGuid;
			waypoint.m_lat = 43.75847; // currentLatitude
			waypoint.m_lon = 7.49575; // currentLongitude
			AddSingleWaypoint(&waypoint, false);
			break;
		}
		case RACE_DIALOG_PORT: {
			PlugIn_Waypoint waypoint;
			waypoint.m_IsVisible = true;
			waypoint.m_MarkName = _T("Port");
			portMarkGuid = GetNewGUID();
			waypoint.m_GUID = portMarkGuid;
			waypoint.m_lat = 43.757188; // currentLatitude
			waypoint.m_lon = 7.497963; // currentLongitude
			AddSingleWaypoint(&waypoint, false);
			break;
		}
		default:
			event.Skip();
	}
}