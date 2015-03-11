/*
   Copyright (C) 2014 Lindsay Mathieson (lindsay.mathieson@gmail.com).

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "qspicehelper.h"
#include "qspicemainchannel.h"

void QSpiceMainChannel::initCallbacks()
{
}

void QSpiceMainChannel::mainSetDisplay(int id, int x, int y, int width, int height)
{
    spice_main_set_display((SpiceMainChannel *) gobject, id, x, y, width, height);
}

void QSpiceMainChannel::mainSetDisplayEnabled(int id, bool enabled)
{
    spice_main_set_display_enabled((SpiceMainChannel *) gobject, id, enabled);
}

void QSpiceMainChannel::mainSendMonitorConfig()
{
    spice_main_send_monitor_config((SpiceMainChannel *) gobject);
}
