/*
 * Copyright (C) 2008-2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2008-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Julien "_FrnchFrgg_" RIVAUD <frnchfrgg@free.fr>
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

#include <algorithm>
#include <math.h>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef COMPILER_MSVC
# include <float.h>
/* isinf() & isnan() are C99 standards, which older MSVC doesn't provide */
# define ISINF(val) !((bool)_finite((double)val))
# define ISNAN(val) (bool)_isnan((double)val)
#else
# define ISINF(val) std::isinf((val))
# define ISNAN(val) std::isnan((val))
#endif

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>

#include "gtkmm2ext/utils.h"

#include "ardour/audio_buffer.h"
#include "ardour/data_type.h"
#include "ardour/chan_mapping.h"
#include "ardour/plugin_insert.h"
#include "ardour/session.h"

#include "plugin_test_gui.h"
#include "ardour_ui.h"
#include "gui_thread.h"
#include "LuaBridge/LuaBridge.h"
#include "pbd/i18n.h"
#include <iostream>
using namespace ARDOUR;

PluginTestGui::PluginTestGui (boost::shared_ptr<ARDOUR::PluginInsert> pluginInsert,luabridge::LuaRef* lua_render)
	 :
	 _plugin_insert (pluginInsert)
	, _pointer_in_area_xpos (-1)
	 , _lua_render (lua_render)

{
	// Setup analysis drawing area
	_analysis_scale_surface = 0;
	_analysis_area = new Gtk::DrawingArea();
	_analysis_width = 256.0;
	_analysis_height = 256.0;
	_analysis_area->set_size_request (_analysis_width, _analysis_height);

	_analysis_area->add_events (Gdk::POINTER_MOTION_MASK | Gdk::LEAVE_NOTIFY_MASK | Gdk::BUTTON_PRESS_MASK);

	_analysis_area->signal_expose_event().connect (sigc::mem_fun (*this, &PluginTestGui::expose_analysis_area));
	_analysis_area->signal_size_allocate().connect (sigc::mem_fun (*this, &PluginTestGui::resize_analysis_area));
	_analysis_area->signal_motion_notify_event().connect (sigc::mem_fun (*this, &PluginTestGui::analysis_area_mouseover));
	_analysis_area->signal_leave_notify_event().connect (sigc::mem_fun (*this, &PluginTestGui::analysis_area_mouseexit));

	// dB selection

	_live_signal_combo = new Gtk::ComboBoxText ();
	_live_signal_combo->append_text (_("Off"));
	_live_signal_combo->append_text (_("Output / Input"));
	_live_signal_combo->append_text (_("Input"));
	_live_signal_combo->append_text (_("Output"));
	_live_signal_combo->append_text (_("Input +40dB"));
	_live_signal_combo->append_text (_("Output +40dB"));
	_live_signal_combo->set_active (0);



	// Freq/dB info for mouse over
	_pointer_info = new Gtk::Label ("", 1, 0.5);
	_pointer_info->set_name ("PluginAnalysisInfoLabel");
	Gtkmm2ext::set_size_request_to_display_given_text (*_pointer_info, "10.0kHz_000.0dB_180.0\u00B0", 0, 0);

	// populate table
	attach (*manage(_analysis_area), 0, 4, 0, 1);
	attach (*manage(_pointer_info),  3, 4, 1, 2, Gtk::FILL,   Gtk::SHRINK);
}

PluginTestGui::~PluginTestGui ()
{
	stop_listening ();

	if (_analysis_scale_surface) {
		cairo_surface_destroy (_analysis_scale_surface);
	}

}

void
PluginTestGui::start_listening ()
{
	if (!_plugin) {
		_plugin = _plugin_insert->get_impulse_analysis_plugin ();
	}

	_plugin->activate ();
//	set_buffer_size (8192, 16384);
//	

	/* Connect the realtime signal collection callback */
//	_plugin_insert->AnalysisDataGathered.connect (analysis_connection, invalidator (*this), boost::bind (&PluginTestGui::signal_collect_callback, this, _1, _2), gui_context());
}

void
PluginTestGui::stop_listening ()
{
	analysis_connection.disconnect ();
	_plugin->deactivate ();
}

void
PluginTestGui::on_hide ()
{
	stop_updating ();
	Gtk::Table::on_hide ();
}

void
PluginTestGui::stop_updating ()
{
	if (_update_connection.connected ()) {
		_update_connection.disconnect ();
	}
}

void
PluginTestGui::start_updating ()
{
	if (!_update_connection.connected() && is_visible()) {
		_update_connection = Glib::signal_timeout().connect (sigc::mem_fun (this, &PluginTestGui::timeout_callback), 250, Glib::PRIORITY_DEFAULT_IDLE);
	}
}

void
PluginTestGui::on_show ()
{
	Gtk::Table::on_show ();

	start_updating ();

	Gtk::Widget *toplevel = get_toplevel ();
	if (toplevel) {
		if (!_window_unmap_connection.connected ()) {
			_window_unmap_connection = toplevel->signal_unmap().connect (sigc::mem_fun (this, &PluginTestGui::stop_updating));
		}

		if (!_window_map_connection.connected ()) {
			_window_map_connection = toplevel->signal_map().connect (sigc::mem_fun (this, &PluginTestGui::start_updating));
		}
	}
}


void
PluginTestGui::redraw_scales ()
{

	if (_analysis_scale_surface) {
		cairo_surface_destroy (_analysis_scale_surface);
		_analysis_scale_surface = 0;
	}

	_analysis_area->queue_draw ();

	// TODO: Add graph legend!
}


void
PluginTestGui::resize_analysis_area (Gtk::Allocation& size)
{
	_analysis_width  = (float)size.get_width();
	_analysis_height = (float)size.get_height();

	if (_analysis_scale_surface) {
		cairo_surface_destroy (_analysis_scale_surface);
		_analysis_scale_surface = 0;
	}
}

bool
PluginTestGui::timeout_callback ()
{

	return true;
}



void
PluginTestGui::update_pointer_info( float x)
{
	_pointer_info->set_text ("test");
}

bool
PluginTestGui::analysis_area_mouseover (GdkEventMotion *event)
{
	update_pointer_info (event->x);
	_pointer_in_area_xpos = event->x;
	_analysis_area->queue_draw ();
	return true;
}

bool
PluginTestGui::analysis_area_mouseexit (GdkEventCrossing *)
{
	_pointer_info->set_text ("");
	_pointer_in_area_xpos = -1;
	_analysis_area->queue_draw ();
	return true;
}

bool
PluginTestGui::expose_analysis_area (GdkEventExpose *)
{
	redraw_analysis_area ();
	return true;
}

void
PluginTestGui::draw_analysis_scales (cairo_t *ref_cr)
{
	// TODO: check whether we need rounding
	_analysis_scale_surface = cairo_surface_create_similar (cairo_get_target (ref_cr),
			CAIRO_CONTENT_COLOR,
			_analysis_width,
			_analysis_height);

	cairo_t *cr = cairo_create (_analysis_scale_surface);

	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
	cairo_rectangle (cr, 0.0, 0.0, _analysis_width, _analysis_height);
	cairo_fill (cr);



	cairo_destroy (cr);
}

void
PluginTestGui::redraw_analysis_area ()
{
	cairo_t *cr;
	

//	cr = gdk_cairo_create (GDK_DRAWABLE(_analysis_area->get_window()->gobj()));
cr =	gdk_cairo_create (_analysis_area->get_window()->gobj());

	//cairo_copy_page (cr);

//	cairo_set_source_surface (cr, _analysis_scale_surface, 0.0, 0.0);
	Cairo::Context ctx (cr);
        luabridge::LuaRef rv = (*_lua_render)((Cairo::Context *)&ctx, _analysis_width,_analysis_height);
//	cairo_paint (cr);

//	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);



	if (_pointer_in_area_xpos >= 0) {
		update_pointer_info (_pointer_in_area_xpos);
	}



//	cairo_destroy (cr);
}


