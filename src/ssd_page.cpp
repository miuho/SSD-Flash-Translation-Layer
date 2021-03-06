/* Copyright 2009, 2010 Brendan Tauras */

/* ssd_page.cpp is part of FlashSim. */

/* FlashSim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version. */

/* FlashSim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. */

/* You should have received a copy of the GNU General Public License
 * along with FlashSim.  If not, see <http://www.gnu.org/licenses/>. */

/****************************************************************************/

/* Page class
 * Brendan Tauras 2009-04-06
 *
 * The page is the lowest level data storage unit that is the size unit of
 * requests (events).  Pages maintain their state as events modify them. */

#include <assert.h>
#include <stdio.h>
#include "ssd.h"

using namespace ssd;

Page::Page(const Block &parent, double read_delay, double write_delay):
	state(EMPTY),
	parent(parent),
	read_delay(read_delay),
	write_delay(write_delay)
{
	state = EMPTY;

	if(read_delay < 0.0){
		fprintf(stderr, "Page warning: %s: constructor received negative read delay value\n\tsetting read delay to 0.0\n", __func__);
		this -> read_delay = 0.0;
	}

	if(write_delay < 0.0){
		fprintf(stderr, "Page warning: %s: constructor received negative write delay value\n\tsetting write delay to 0.0\n", __func__);
		this -> write_delay = 0.0;
	}
	return;
}

Page::~Page(void)
{
	return;
}

enum status Page::_read(Event &event)
{
	assert(read_delay >= 0.0);
	if(state == VALID){
		event.incr_time_taken(read_delay);
		return SUCCESS;
	} else {
    fprintf(stderr, "Trying to read invalid page\n");
		return FAILURE;
  }
}

enum status Page::_write(Event &event)
{
	assert(write_delay >= 0.0);
	if(state == EMPTY){
		event.incr_time_taken(write_delay);
		state = VALID;
		return SUCCESS;
	} else {
    fprintf(stderr, "Trying to write invalid page\n");
		return FAILURE;
  }
}

const Block &Page::get_parent(void) const
{
	return parent;
}

enum page_state Page::get_state(void) const
{
	return state;
}

void Page::set_state(enum page_state state)
{
	this -> state = state;
	return;
}
