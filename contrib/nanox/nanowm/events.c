/*
 * NanoWM - Window Manager for Nano-X
 *
 * Copyright (C) 2000, 2003 Greg Haerr <greg@censoft.com>
 * Copyright (C) 2000 Alex Holden <alex@linuxhacker.org>
 */
#include <runtime/lib.h>

#include "nanox/include/nano-X.h"

/* Uncomment this if you want debugging output from this file */
/*#define DEBUG*/

#include "nanowm.h"

void Wm_do_exposure(GR_EVENT_EXPOSURE *event)
{
	win *window;

	Dprintf("do_exposure: wid %d, x %d, y %d, width %d, height %d\n",
		event->wid, event->x, event->y, event->width, event->height);

	if(!(window = Wm_find_window(event->wid))) return;

	switch(window->type) {
		case WINDOW_TYPE_CONTAINER:
			Wm_container_exposure(window, event);
			break;
		default:
			debug_printf ("Unhandled exposure on window %d "
				"(type %d)\n", window->wid, window->type);
			break;
	}

}

void Wm_do_button_down(GR_EVENT_BUTTON *event)
{
	win *window;

	Dprintf("do_button_down: wid %d, subwid %d, rootx %d, rooty %d, x %d, "
		"y %d, buttons %d, changebuttons %d, modifiers %d\n",
		event->wid, event->subwid, event->rootx, event->rooty, event->x,
		event->y, event->buttons, event->changebuttons,
		event->modifiers);

	if(!(window = Wm_find_window(event->wid))) return;

	switch(window->type) {
		case WINDOW_TYPE_CONTAINER:
			Wm_container_buttondown(window, event);
			break;
		default:
			debug_printf ("Unhandled button down on window %d "
				"(type %d)\n", window->wid, window->type);
			break;
	}
}

void Wm_do_button_up(GR_EVENT_BUTTON *event)
{
	win *window;

	Dprintf("do_button_up: wid %d, subwid %d, rootx %d, rooty %d, x %d, "
		"y %d, buttons %d, changebuttons %d, modifiers %d\n",
		event->wid, event->subwid, event->rootx, event->rooty, event->x,
		event->y, event->buttons, event->changebuttons,
		event->modifiers);

	if(!(window = Wm_find_window(event->wid))) return;

	switch(window->type) {
		case WINDOW_TYPE_CONTAINER:
			Wm_container_buttonup(window, event);
			break;
		default:
			debug_printf ("Unhandled button up on window %d "
				"(type %d)\n", window->wid, window->type);
			break;
	}
}

void Wm_do_mouse_enter(GR_EVENT_GENERAL *event)
{
	win *window;

	Dprintf("do_mouse_enter: wid %d\n", event->wid);

	if(!(window = Wm_find_window(event->wid)))
		return;

	switch(window->type) {
		default:
			debug_printf ("Unhandled mouse enter from window %d "
				"(type %d)\n", window->wid, window->type);
			break;
	}
}

void Wm_do_mouse_exit(GR_EVENT_GENERAL *event)
{
	win *window;

	Dprintf("do_mouse_exit: wid %d\n", event->wid);

	if(!(window = Wm_find_window(event->wid))) return;

	switch(window->type) {
		default:
			debug_printf ("Unhandled mouse exit from window %d "
				"(type %d)\n", window->wid, window->type);
			break;
	}
}

void Wm_do_mouse_moved(GR_EVENT_MOUSE *event)
{
	win *window;

	/* Dprintf("do_mouse_moved: wid %d, subwid %d, rootx %d, rooty %d, x %d, "
		"y %d, buttons %d, modifiers %d\n", event->wid, event->subwid,
		event->rootx, event->rooty, event->x, event->y, event->buttons,
		event->modifiers); */

	if(!(window = Wm_find_window(event->wid))) return;

	switch(window->type) {
		case WINDOW_TYPE_CONTAINER:
			Wm_container_mousemoved(window, event);
			break;
		default:
			debug_printf ("Unhandled mouse movement in window %d "
				"(type %d)\n", window->wid, window->type);
			break;
	}
}

void Wm_do_focus_in(GR_EVENT_GENERAL *event)
{
	win *window;

	Dprintf("do_focus_in: wid %d\n", event->wid);

	if(!(window = Wm_find_window(event->wid)))
		return;

	switch(window->type) {
		default:
			debug_printf ("Unhandled focus in from window %d "
				"(type %d)\n", window->wid, window->type);
			break;
	}
}

void Wm_do_key_down(GR_EVENT_KEYSTROKE *event)
{
	Dprintf("do_key_down: wid %d, subwid %d, rootx %d, rooty %d, x %d, "
		"y %d, buttons %d, modifiers %d, ch %u, scancode %d\n",
		event->wid, event->subwid, event->rootx,
		event->rooty, event->x, event->y, event->buttons,
		event->modifiers, event->ch, event->scancode);

	/* FIXME: Implement keyboard shortcuts */
}

void Wm_do_key_up(GR_EVENT_KEYSTROKE *event)
{
	Dprintf("do_key_up: wid %d, subwid %d, rootx %d, rooty %d, x %d, "
		"y %d, buttons %d, modifiers %d, ch %u, scancode %d\n",
		event->wid, event->subwid, event->rootx,
		event->rooty, event->x, event->y, event->buttons,
		event->modifiers, event->ch, event->scancode);
}

void Wm_do_update(GR_EVENT_UPDATE *event)
{
	win *window;

	Dprintf("do_update: wid %d, subwid %d, x %d, y %d, width %d, height %d, "
	       "utype %d\n", event->wid, event->subwid, event->x, event->y, event->width,
	       event->height, event->utype);

	if(!(window = Wm_find_window(event->subwid))) {
	  if (event->utype == GR_UPDATE_MAP) Wm_new_client_window(event->subwid);
	  return;
	}

	if(window->type == WINDOW_TYPE_CONTAINER) {
		if (event->utype == GR_UPDATE_ACTIVATE)
			Wm_redraw_ncarea(window);
		return;
	}

	if (window->type == WINDOW_TYPE_CLIENT) {
	  if(event->utype == GR_UPDATE_MAP) Wm_client_window_remap(window);
	  if(event->utype == GR_UPDATE_DESTROY) Wm_client_window_destroy(window);
	  if(event->utype == GR_UPDATE_UNMAP) Wm_client_window_unmap(window);
	  if(event->utype == GR_UPDATE_SIZE) Wm_client_window_resize(window);
	}
}
