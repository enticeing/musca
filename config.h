/*
  This file is part of Musca.

  Musca is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
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

// general settings.
// field 1 is a name usable with the 'set' command.
// field 2 is mst_str=char, mst_ucell=unsigned int, mst_dcell = double
// field 3 is set according to field 2.  mst_str = .s, mst_ucell = .u, mst_dcell = .d
// field 4 is a posix extended case insensitive regex to validate any changes made with the 'set' command
setting settings[] = {
	// these colours must be strings that X11 will recognize
	{ "border_focus",            mst_str,   { .s = "Blue"       }, ".+" },
	{ "border_unfocus",          mst_str,   { .s = "Dim Gray"   }, ".+" },
	{ "border_dedicate_focus",   mst_str,   { .s = "Red"        }, ".+" },
	{ "border_dedicate_unfocus", mst_str,   { .s = "Dark Red"   }, ".+" },
	{ "border_catchall_focus",   mst_str,   { .s = "Green"      }, ".+" },
	{ "border_catchall_unfocus", mst_str,   { .s = "Dark Green" }, ".+" },
	{ "border_width",            mst_ucell, { .u = 1            }, "[0-9]+" },
	// frame size limits in pixels
	{ "frame_min_wh",            mst_ucell, { .u = 100 }, "[0-9]+" },
	{ "frame_resize",            mst_ucell, { .u = 20  }, "[0-9]+" },
	// optional startup file of musca commands, one per line
	{ "startup",                 mst_str,   { .s = ".musca_start" }, ".+" },
	// customize the dmenu command.
	{ "dmenu",                   mst_str,   { .s = "sort | dmenu -i"  }, ".+" },
	// customize the actions of dmenu driven window/group/command menus.  by default we
	// just spit commands back to musca, but you can wrap or redirect stuff.  the $MUSCA
	// environment variable is set to argv[0] in setup().  -i means execute stdin.
	{ "switch_window",           mst_str,   { .s = "sed 's/^/raise /' | $MUSCA -i" }, ".+" },
	{ "switch_group",            mst_str,   { .s = "sed 's/^/use /'   | $MUSCA -i" }, ".+" },
	{ "run_musca_command",       mst_str,   { .s =                     "$MUSCA -i" }, ".+" },
	{ "run_shell_command",       mst_str,   { .s = "sed 's/^/exec /'  | $MUSCA -i" }, ".+" },
	// your preferred method of being send a message, eg:
	// print to stdout with line break "echo `cat`"
	// popup xmessage "xmessage -timeout 3 -file -"
	// popup dzen2 (needs a line break) "echo `cat` | dzen2 -p 3 -w 300"
	// popup notify-send (doesn't accept stdin) "notify-send -t 3000 Musca \"`cat`\""
	{ "notify",                  mst_str,   { .s = "echo `cat`" }, ".+" },
	// this keyboard modifier is used in stacking mode, along with mouse buttons 1 and 3, to
	// move and resize windows respectively.
	{ "stack_mouse_modifier",    mst_str,   { .s = "Mod4"    }, ".+" },
	{ "focus_follow_mouse",      mst_ucell, { .u = 0         }, ".+" },
	// whether new windows should open in the current focused frame or try to find an empty frame.
	{ "window_open_frame",       mst_str,   { .s = "current" }, "^(current|empty)$" },
	// whether new windows should automatically take the focus
	{ "window_open_focus",       mst_ucell, { .u = 1 }, "[0-9]+" },
	// in bytes
	{ "command_buffer_size",     mst_ucell, { .u = 4096 }, "[0-9]+" },
	{ "notify_buffer_size",      mst_ucell, { .u = 4096 }, "[0-9]+" },
	// whether empty frames should automatically find and display a hidden window
	{ "frame_display_hidden",    mst_ucell, { .u = 1 }, "[0-9]+" },
	// when spliting a frame, should focus should go to the current frame or the new one
	{ "frame_split_focus",       mst_str,   { .s = "current" }, "^(current|new)$" },
	// auto close empty groups when they are hidden
	{ "group_close_empty",       mst_ucell, { .u = 0 }, "[0-9]+" },
	// should the clients honor the size hints?
	{ "window_size_hints",       mst_ucell, { .u = 1 }, "[0-9]+" },
};

// default list of window *classes* to ignore.  use xprop WM_CLASS to find them.  either
// make additions here, or use the 'manage' command in your startup file.
char *unmanaged_windows[] = { "trayer", "Xmessage", "Conky" };

// when binding keys, these are the (case insensitive) modifier names we will recognize.  note
// that LockMask is explicitly ignored in grab_stuff(), so adding it here will have precisely
// zero effect :)
modmask modmasks[] = {
	{ "Mod1", Mod1Mask },
	{ "Mod2", Mod2Mask },
	{ "Mod3", Mod3Mask },
	{ "Mod4", Mod4Mask },
	{ "Mod5", Mod5Mask },
	{ "Control", ControlMask },
	{ "Shift",   ShiftMask   },
};

// we simply map key bindings to any musca command.  either add custom stuff here, or use
// the 'bind' command in your startup file.
keymap keymaps[] = {
	{ "Mod4+h",             "hsplit 1/2"    },
	{ "Mod4+v",             "vsplit 1/2"    },
	{ "Mod4+r",             "remove"        },
	{ "Mod4+o",             "only"          },
	{ "Mod4+k",             "kill"          },
	{ "Mod4+c",             "cycle"         },
	{ "Mod4+Left",          "focus left"    },
	{ "Mod4+Right",         "focus right"   },
	{ "Mod4+Up",            "focus up"      },
	{ "Mod4+Down",          "focus down"    },
	{ "Mod4+Next",          "use (next)"    },
	{ "Mod4+Prior",         "use (prev)"    },
	{ "Mod4+Tab",           "screen (next)" },
	{ "Mod4+w",             "switch window" },
	{ "Mod4+g",             "switch group"  },
	{ "Mod4+x",             "shell"         },
	{ "Mod4+m",             "command"       },
	{ "Mod4+d",             "dedicate flip" },
	{ "Mod4+a",             "catchall flip" },
	{ "Mod4+u",             "undo"          },
	{ "Mod4+s",             "stack flip"    },
	{ "Mod4+Control+Left",  "resize left"   },
	{ "Mod4+Control+Right", "resize right"  },
	{ "Mod4+Control+Up",    "resize up"     },
	{ "Mod4+Control+Down",  "resize down"   },
	{ "Mod4+Shift+Left",    "swap left"     },
	{ "Mod4+Shift+Right",   "swap right"    },
	{ "Mod4+Shift+Up",      "swap up"       },
	{ "Mod4+Shift+Down",    "swap down"     },
	{ "Mod4+t",             "exec urxvt +sb -fn 'xft:Terminus:pixelsize=10'"    },
};

// be careful if you modify these.  each callback function expects the correct number
// of matched sub-strings.
command commands[] = {
	// frames
	{ "hsplit,vsplit", "^(hsplit|vsplit)[[:space:]]+([0-9]+%?)$\n^(hsplit|vsplit)[[:space:]]+([0-9]+)/([0-9]+)$",
		com_frame_split,     GF_TILING },
	{ "width,height", "^(width|height)[[:space:]]+([0-9]+%?)$\n^(width|height)[[:space:]]+([0-9]+)/([0-9]+)$",
		com_frame_size,      GF_TILING },
	{ "remove", "^remove$",
		com_frame_remove,    GF_TILING },
	{ "kill", "^kill[[:space:]]*(.+)?$",
		com_frame_kill,      GF_TILING|GF_STACKING },
	{ "cycle", "^cycle[[:space:]]*(local|next|prev)?[[:space:]]*(next|prev)?$",
		com_frame_cycle,     GF_TILING|GF_STACKING },
	{ "only", "^only$",
		com_frame_only,      GF_TILING },
	{ "focus", "^focus[[:space:]]+(left|right|up|down)$",
		com_frame_focus,     GF_TILING },
	{ "lfocus,rfocus,ufocus,dfocus", "^(lfocus|rfocus|ufocus|dfocus)$",
		com_frame_focus,     GF_TILING },
	{ "dedicate", "^dedicate[[:space:]]+(on|off|flip)$",
		com_frame_dedicate,  GF_TILING },
	{ "catchall", "^catchall[[:space:]]+(on|off|flip)$",
		com_frame_catchall,  GF_TILING },
	{ "undo", "^undo$",
		com_group_undo,      GF_TILING },
	{ "resize", "^resize[[:space:]]+(.+)$",
		com_frame_resize,    GF_TILING },

	// groups
	{ "pad", "^pad[[:space:]]+([0-9]+)[[:space:]]+([0-9]+)[[:space:]]+([0-9]+)[[:space:]]+([0-9]+)$",
		com_group_pad,       GF_TILING|GF_STACKING },
	{ "add", "^add[[:space:]]+([^[:space:]]+)$",
		com_group_add,       GF_TILING|GF_STACKING },
	{ "drop", "^drop[[:space:]]+([^[:space:]]+)$",
		com_group_drop,      GF_TILING|GF_STACKING },
	{ "name", "^name[[:space:]]+([^[:space:]]+)$",
		com_group_name,      GF_TILING|GF_STACKING },
	{ "dump", "^dump[[:space:]]+([^[:space:]]+)$",
		com_group_dump,      GF_TILING },
	{ "load", "^load[[:space:]]+([^[:space:]]+)$",
		com_group_load,      GF_TILING },
	{ "use", "^use[[:space:]]+(.+)$",
		com_group_use,       GF_TILING|GF_STACKING },
	{ "stack", "^stack[[:space:]]+(on|off|flip)$",
		com_group_stack,     GF_TILING|GF_STACKING },

	// windows
	{ "uswap,dswap,lswap,rswap", "^(uswap|dswap|lswap|rswap)$",
		com_frame_swap,      GF_TILING },
	{ "swap", "^swap[[:space:]]+(up|down|left|right)$",
		com_frame_swap,      GF_TILING },
	{ "uslide,dslide,lslide,rslide", "^(uslide|dslide|lslide|rslide)$",
		com_frame_slide,     GF_TILING },
	{ "slide", "^slide[[:space:]]+(up|down|left|right)$",
		com_frame_slide,     GF_TILING },
	{ "move", "^move[[:space:]]+([^[:space:]]+)$",
		com_window_to_group, GF_TILING|GF_STACKING },
	{ "manage", "^manage[[:space:]]+(on|off)[[:space:]]+([^[:space:]]+)$",
		com_manage,          GF_TILING|GF_STACKING },
	{ "raise", "^raise[[:space:]]+(.+)$",
		com_window_raise,    GF_TILING|GF_STACKING },
	{ "shrink", "^shrink[[:space:]]*(.+)?$",
		com_window_shrink,   GF_TILING|GF_STACKING },
	{ "border", "^border[[:space:]]+(on|off|flip)$",
		com_border,          GF_TILING },
	{ "refresh", "^refresh$",
		com_refresh,         GF_TILING|GF_STACKING },
	{ "place", "^place[[:space:]]+([^[:space:]]+)[[:space:]]+(on|off)[[:space:]]*([^[:space:]]*)?",
		com_place,           GF_TILING|GF_STACKING },

	// misc
	{ "screen", "^screen[[:space:]]+(.+)$",
		com_screen_switch,   GF_TILING|GF_STACKING },
	{ "exec", "^exec[[:space:]]+(.+)$",
		com_exec,            GF_TILING|GF_STACKING },
	{ "set", "^set[[:space:]]+([a-z0-9_]+)[[:space:]]+(.+)$",
		com_set,             GF_TILING|GF_STACKING },
	{ "bind", "^bind[[:space:]]+(on|off)[[:space:]]+([a-z0-9_+]+)(.+)?$",
		com_bind,            GF_TILING|GF_STACKING },
	{ "switch", "^switch[[:space:]]+(window|group)$",
		com_switch,          GF_TILING|GF_STACKING },
	{ "command", "^command$",
		com_command,         GF_TILING|GF_STACKING },
	{ "shell", "^shell$",
		com_shell,           GF_TILING|GF_STACKING },
	{ "alias", "^alias[[:space:]]+([^[:space:]]+)[[:space:]]+(.+)$",
		com_alias,           GF_TILING|GF_STACKING },
	{ "say", "^say[[:space:]]+(.+)$",
		com_say,             GF_TILING|GF_STACKING },
	{ "run", "^run[[:space:]]+(.+)$",
		com_run,             GF_TILING|GF_STACKING },
	{ "show", "^show[[:space:]]+(unmanaged|bindings|settings|hooks|groups|frames|windows|aliases)$",
		com_show,            GF_TILING|GF_STACKING },
	{ "hook", "^hook[[:space:]]+(on|off)[[:space:]]+([^[:space:]]+)[[:space:]]*(.+)?$",
		com_hook,            GF_TILING|GF_STACKING },
	{ "client", "^client[[:space:]]+(hints)[[:space:]]+(on|off)$",
		com_client,          GF_TILING|GF_STACKING },
	{ "debug", "^debug[[:space:]]+(sanity)$",
		com_debug,           GF_TILING|GF_STACKING },
	{ "quit", "^quit$",
		com_quit,            GF_TILING|GF_STACKING },
};

// clientmessage triggers commands, so these format strings need to
// match a command above.
char *command_formats[cmd_last] = {
	[cmd_raise] = "raise %d",
	[cmd_use]   = "use %d",
	[cmd_kill]  = "kill %d",
	[cmd_drop]  = "drop %d",
	[cmd_add]   = "add %s",
	[cmd_shrink] = "shrink %d",
};
