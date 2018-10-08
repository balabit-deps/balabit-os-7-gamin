/* Gamin
 * Copyright (C) 2003 James Willcox, Corey Bowers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "gam_event.h"

/**
 * gam_event_to_string:
 * @event: an event
 *
 * Provide a string describing the event type
 *
 * Returns a constant string for the event.
 */
const char *
gam_event_to_string(GaminEventType event)
{
    switch (event) {
        case GAMIN_EVENT_CHANGED:
            return "Changed";
        case GAMIN_EVENT_CREATED:
            return "Created";
        case GAMIN_EVENT_DELETED:
            return "Deleted";
        case GAMIN_EVENT_MOVED:
            return "Moved";
        case GAMIN_EVENT_EXISTS:
            return "Exists";
        default:
            return "None";
    }

    return "Unknown";
}
