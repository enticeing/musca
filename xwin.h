/*
  This file is part of Musca.

  Musca is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as publishead by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Musca is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Musca.  If not, see <http://www.gnu.org/licenses/>.

  Sean Pringle
  sean dot pringle at gmail dot com
  https://launchpad.net/musca
*/

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#define WINLIST 32
typedef struct {
	Window *items;
	ucell depth;
	ucell limit;
} winlist;

static int (*xerrorxlib)(Display *, XErrorEvent *);

enum {
WMProtocols, WMDelete, WMState, WMClass, WMName, WMChangeState, WMWindowRole, UTF8String,
NetSupported, NetWMName, NetClientList, NetClientListStacking, NetActiveWindow, NetNumberOfDesktops,
NetDesktopNames, NetCurrentDesktop, NetDesktopGeometry, NetDesktopViewport, NetCloseWindow,
NetSupportingWMCheck, NetMoveResizeWindow, NetWorkarea, NetWMStrut, NetWMStrutPartial,
NetWMWindowType, NetWMWindowTypeDesktop, NetWMWindowTypeDock, NetWMWindowTypeToolbar, NetWMWindowTypeMenu,
NetWMWindowTypeUtility, NetWMWindowTypeSplash, NetWMWindowTypeDialog, NetWMWindowTypeDropdownMenu,
NetWMWindowTypePopupMenu, NetWMWindowTypeTooltip, NetWMWindowTypeNotification, NetWMWindowTypeCombo,
NetWMWindowTypeDND, NetWMWindowTypeNormal,
NetWMActionMove, NetWMActionResize, NetWMActionMinimize, NetWMActionShade, NetWMActionStick,
NetWMActionMaximizedHorz, NetWMActionMaximizedVert, NetWMActionFullscreen, NetWMActionChangeDesktop,
NetWMActionClose, NetWMActionAbove, NetWMActionBelow,
NetWMState, NetWMStateModal, NetWMStateSticky, NetWMStateMaximizedVert, NetWMStateMaximizedHorz,
NetWMStateShaded, NetWMStateSkipTaskbar, NetWMStateSkipPager, NetWMStateHidden, NetWMStateFullscreen,
NetWMStateAbove, NetWMStateBelow, NetWMStateDemandsAttention,
NetWMPid, NetWMDesktop, NetWMUserTime, NetWMUserTimeWindow,
ApisFlags, ApisReady, ApisCommandCode, ApisCommandIn, ApisResultCode, ApisResultOut, ApisResultError,
ApisSession, ApisTag,
AtomLast };

static Atom atoms[AtomLast];

char *atom_names[AtomLast] = {
	[WMProtocols] = "WM_PROTOCOLS",
	[WMDelete] = "WM_DELETE_WINDOW",
	[WMState] = "WM_STATE",
	[WMClass] = "WM_CLASS",
	[WMName] = "WM_NAME",
	[WMChangeState] = "WM_CHANGE_STATE",
	[WMWindowRole] = "WM_WINDOW_ROLE",
	[UTF8String] = "UTF8_STRING",
	[NetSupported] = "_NET_SUPPORTED",
	[NetWMName] = "_NET_WM_NAME",
	[NetClientList] = "_NET_CLIENT_LIST",
	[NetClientListStacking] = "_NET_CLIENT_LIST_STACKING",
	[NetActiveWindow] = "_NET_ACTIVE_WINDOW",
	[NetNumberOfDesktops] = "_NET_NUMBER_OF_DESKTOPS",
	[NetDesktopNames] = "_NET_DESKTOP_NAMES",
	[NetCurrentDesktop] = "_NET_CURRENT_DESKTOP",
	[NetDesktopGeometry] = "_NET_DESKTOP_GEOMETRY",
	[NetDesktopViewport] = "_NET_DESKTOP_VIEWPORT",
	[NetCloseWindow] = "_NET_CLOSE_WINDOW",
	[NetSupportingWMCheck] = "_NET_SUPPORTING_WM_CHECK",
	[NetMoveResizeWindow] = "_NET_MOVERESIZE_WINDOW",
	[NetWorkarea] = "_NET_WORKAREA",
	[NetWMStrut] = "_NET_WM_STRUT",
	[NetWMStrutPartial] = "_NET_WM_STRUT_PARTIAL",
	[NetWMWindowType] = "_NET_WM_WINDOW_TYPE",
	[NetWMWindowTypeDesktop] = "_NET_WM_WINDOW_TYPE_DESKTOP",
	[NetWMWindowTypeDock] = "_NET_WM_WINDOW_TYPE_DOCK",
	[NetWMWindowTypeToolbar] = "_NET_WM_WINDOW_TYPE_TOOLBAR",
	[NetWMWindowTypeMenu] = "_NET_WM_WINDOW_TYPE_MENU",
	[NetWMWindowTypeUtility] = "_NET_WM_WINDOW_TYPE_UTILITY",
	[NetWMWindowTypeSplash] = "_NET_WM_WINDOW_TYPE_SPLASH",
	[NetWMWindowTypeDialog] = "_NET_WM_WINDOW_TYPE_DIALOG",
	[NetWMWindowTypeDropdownMenu] = "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
	[NetWMWindowTypePopupMenu] = "_NET_WM_WINDOW_TYPE_POPUP_MENU",
	[NetWMWindowTypeTooltip] = "_NET_WM_WINDOW_TYPE_TOOLTIP",
	[NetWMWindowTypeNotification] = "_NET_WM_WINDOW_TYPE_NOTIFICATION",
	[NetWMWindowTypeCombo] = "_NET_WM_WINDOW_TYPE_COMBO",
	[NetWMWindowTypeDND] = "_NET_WM_WINDOW_TYPE_DND",
	[NetWMWindowTypeNormal] = "_NET_WM_WINDOW_TYPE_NORMAL",
	[NetWMActionMove] = "_NET_WM_ACTION_MOVE",
	[NetWMActionResize] = "_NET_WM_ACTION_RESIZE",
	[NetWMActionMinimize] = "_NET_WM_ACTION_MINIMIZE",
	[NetWMActionShade] = "_NET_WM_ACTION_SHADE",
	[NetWMActionStick] = "_NET_WM_ACTION_STICK",
	[NetWMActionMaximizedHorz] = "_NET_WM_ACTION_MAXIMIZE_HORZ",
	[NetWMActionMaximizedVert] = "_NET_WM_ACTION_MAXIMIZE_VERT",
	[NetWMActionFullscreen] = "_NET_WM_ACTION_FULLSCREEN",
	[NetWMActionChangeDesktop] = "_NET_WM_ACTION_CHANGE_DESKTOP",
	[NetWMActionClose] = "_NET_WM_ACTION_CLOSE",
	[NetWMActionAbove] = "_NET_WM_ACTION_ABOVE",
	[NetWMActionBelow] = "_NET_WM_ACTION_BELOW",
	[NetWMState] = "_NET_WM_STATE",
	[NetWMStateModal] = "_NET_WM_STATE_MODAL",
	[NetWMStateSticky] = "_NET_WM_STATE_STICKY",
	[NetWMStateMaximizedVert] = "_NET_WM_STATE_MAXIMIZED_VERT",
	[NetWMStateMaximizedHorz] = "_NET_WM_STATE_MAXIMIZED_HORZ",
	[NetWMStateShaded] = "_NET_WM_STATE_SHADED",
	[NetWMStateSkipTaskbar] = "_NET_WM_STATE_SKIP_TASKBAR",
	[NetWMStateSkipPager] = "_NET_WM_STATE_SKIP_PAGER",
	[NetWMStateHidden] = "_NET_WM_STATE_HIDDEN",
	[NetWMStateFullscreen] = "_NET_WM_STATE_FULLSCREEN",
	[NetWMStateAbove] = "_NET_WM_STATE_ABOVE",
	[NetWMStateBelow] = "_NET_WM_STATE_BELOW",
	[NetWMStateDemandsAttention] = "_NET_WM_STATE_DEMANDS_ATTENTION",
	[NetWMPid] = "_NET_WM_PID",
	[NetWMDesktop] = "_NET_WM_DESKTOP",
	[NetWMUserTime] = "_NET_WM_USER_TIME",
	[NetWMUserTimeWindow] = "_NET_WM_USER_TIME_WINDOW",
	[ApisFlags] = "APIS_FLAGS",
	[ApisReady] = "APIS_READY",
	[ApisCommandCode] = "APIS_COMMAND_CODE",
	[ApisCommandIn] = "APIS_COMMAND_IN",
	[ApisResultCode] = "APIS_RESULT_CODE",
	[ApisResultOut] = "APIS_RESULT_OUT",
	[ApisSession] = "APIS_SESSION",
	[ApisTag] = "APIS_TAG",
};
static Atom atoms[AtomLast];

typedef ubyte (*XIterator)(Window, void*);
