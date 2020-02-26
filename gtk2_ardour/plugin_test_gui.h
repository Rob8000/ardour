/*
 * Copyright (C) 2008-2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2018-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_plugin_test_gui_h
#define __ardour_plugin_test_gui_h

#include "pbd/signals.h"

#include "ardour/buffer_set.h"
#include "LuaBridge/LuaBridge.h"
#include <gtkmm/table.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/combobox.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/liststore.h>

namespace ARDOUR {
	class Plugin;
	class PluginInsert;
}

class PluginTestGui : public Gtk::Table
{
public:
	PluginTestGui (boost::shared_ptr<ARDOUR::PluginInsert>, luabridge::LuaRef* lua_render);
	~PluginTestGui ();
	luabridge::LuaRef* _lua_render;
	void start_listening ();
	void stop_listening ();

private:
	// Setup
	void set_buffer_size (uint32_t, uint32_t);

	// Analysis

	// Drawing
	virtual void on_hide ();
	virtual void on_show ();

	void stop_updating ();
	void start_updating ();

	void resize_analysis_area (Gtk::Allocation&);
	void redraw_analysis_area ();

	void draw_analysis_scales (cairo_t *);
	bool expose_analysis_area (GdkEventExpose *);


	void update_pointer_info(float);
	bool analysis_area_mouseover(GdkEventMotion *);
	bool analysis_area_mouseexit(GdkEventCrossing *);

	// Helpers
	bool timeout_callback ();
	void redraw_scales ();

	// Fields:



	// buffers
	ARDOUR::BufferSet _bufferset;
	ARDOUR::BufferSet _collect_bufferset;

	// dimensions
	float _analysis_width;
	float _analysis_height;

	// My objects
	boost::shared_ptr<ARDOUR::Plugin> _plugin;
	boost::shared_ptr<ARDOUR::PluginInsert> _plugin_insert;

	// gui objects
	Gtk::DrawingArea *_analysis_area;
	cairo_surface_t *_analysis_scale_surface;
	Gtk::Label *_pointer_info;
	int _pointer_in_area_xpos;
	int _pointer_in_area_freq;




	Gtk::ComboBoxText* _live_signal_combo;


	// signals and connections
	sigc::connection _update_connection;
	sigc::connection _window_unmap_connection;
	sigc::connection _window_map_connection;

	PBD::ScopedConnection analysis_connection;
};

#endif
