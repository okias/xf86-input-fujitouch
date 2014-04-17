/*
 * Copyright 2004 by Kenan Esau <kenan.esau@conan.de>, Baltmannsweiler, 
 * Germany.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of copyright holders not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  The copyright holders
 * make no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <misc.h>
#include <os.h>

#include <xf86.h>
#ifndef NEED_XF86_TYPES
#define NEED_XF86_TYPES
#endif
#include <xf86_OSproc.h>
#include <xf86Xinput.h>
#include <exevents.h>

#include "libtouch.h"

#ifdef DBG
#undef DBG
#endif

static int debug_level = 0;
#define DBG(lvl, f) {if ((lvl) <= debug_level) f;}

typedef struct state {
        void (*enter_state)(LibTouchRecPtr priv);
        int  (*handle_state)(LibTouchRecPtr priv);
        void (*action)(LibTouchRecPtr priv, int btn, int x, int y);
        int btn;
} state_t;


typedef enum states {
  S_UNTOUCHED = 0,
  S_TOUCHED = 1,
  S_LONGTOUCHED = 2,
  S_MOVING = 3,
  S_MAYBETAPPED = 4,
  S_ONEANDAHALFTAP = 5
} state_name_t;


static void btn_down_action(LibTouchRecPtr priv, int btn, int x, int y)
{
        DBG(4, ErrorF("LibTouch: Issuing Button %d down\n", btn));
        xf86PostButtonEvent(priv->local->dev, TRUE,
                            btn, 1, 0, 2, x, y);
        priv->pressed_btn_stat |= 1 << btn;
}

static void btn_up_action(LibTouchRecPtr priv, int btn, int x, int y)
{
        DBG(4, ErrorF("LibTouch: Issuing Button %d up\n", btn));
        xf86PostButtonEvent(priv->local->dev, TRUE,
                            btn, 0, 0, 2, x, y);
        priv->pressed_btn_stat &= ~(1 << btn);
}

static void btn_click_action(LibTouchRecPtr priv, int btn, int x, int y)
{
        btn_down_action(priv, btn, x, y);
        btn_up_action(priv, btn, x, y);
}

static void enter_untouched(LibTouchRecPtr priv);
static int handle_untouched(LibTouchRecPtr priv);
static void enter_touched(LibTouchRecPtr priv);
static int handle_touched(LibTouchRecPtr priv);
static void enter_longtouched(LibTouchRecPtr priv);
static int handle_longtouched(LibTouchRecPtr priv);
static void enter_moving(LibTouchRecPtr priv);
static int handle_moving(LibTouchRecPtr priv);
static void enter_maybetap(LibTouchRecPtr priv);
static int handle_maybetap(LibTouchRecPtr priv);
static void enter_oneandahalftap(LibTouchRecPtr priv);
static int handle_oneandahalftap(LibTouchRecPtr priv);

static void dump_configuration();

state_t state_ar[] = {
        {enter_untouched, handle_untouched, NULL, 0},
        {enter_touched, handle_touched, NULL, 0},
        {enter_longtouched, handle_longtouched, btn_down_action, 1},
        {enter_moving, handle_moving, NULL, 0},
        {enter_maybetap, handle_maybetap, btn_click_action, 1},
        {enter_oneandahalftap, handle_oneandahalftap, btn_down_action, 3},
        {NULL, NULL, NULL, -1},
};

char *state_str[] = {
          "S_UNTOUCHED",
          "S_TOUCHED",
          "S_LONGTOUCHED",
          "S_MOVING",
          "S_MAYBETAPPED",
          "S_ONEANDAHALFTAP",
          NULL,
};

char *state_action_str[] = {
          "untouched_action",
          "touched_action",
          "longtouched_action",
          "moving_action",
          "maybetapped_action",
          "oneandahalftap_action",
          NULL,
};

char *state_button_str[] = {
          "untouched_button",
          "touched_button",
          "longtouched_button",
          "moving_button",
          "maybetapped_button",
          "oneandahalftap_button",
          NULL,
};

char *action_str[] = {
        "down",
        "up",
        "click",
        NULL,
};

void (*action_handler[])(LibTouchRecPtr, int, int, int) = {
        btn_down_action,
        btn_up_action,
        btn_click_action,
        NULL,
};

void libtouchSetDebugLevel(int level) {debug_level = level;}
 

void libtouchSetTapTimeo(LibTouchRecPtr libtouch, int timeo)
{
        libtouch->tap_timeo = timeo;
}


void libtouchSetLongtouchTimeo(LibTouchRecPtr libtouch, int timeo)
{
        libtouch->longtouch_timeo = timeo;
}


void libtouchSetMoveLimit(LibTouchRecPtr libtouch, int move_limit)
{
        libtouch->move_limit = move_limit;
}


void libtouchInit(LibTouchRecPtr libtouch, LocalDevicePtr local)
{
        int state_action_idx = 0;
        int state_button_idx = 0;
        int action_idx = 0;
        int btn;

        char *str;

        memset(libtouch, 0, sizeof(LibTouchRec));

        libtouch->now = GetTimeInMillis();
        libtouch->past = libtouch->now;
        libtouch->local = local;
        libtouch->move_limit = 30;

        /*
          Actions: up, down, click

          Example(s):
          longtouch_action "down"
          longtouch_button 1

          tap_action "click"
          tap_button 2

          oneandahalftap_action "down"
          oneandahalftap_button 3
        */

        /* parse buttons */
        for (state_button_idx = 0; state_button_str[state_button_idx] != NULL; state_button_idx++) {
                btn = xf86SetIntOption(local->options, state_button_str[state_button_idx], -1);
                if (btn != -1) state_ar[state_button_idx].btn = btn;
        }

        /* parse actions for the states */
        for (state_action_idx = 0; state_action_str[state_action_idx] != NULL; state_action_idx++) {
                DBG(4, ErrorF("LibTouch: Finding Option %s\n",  state_action_str[state_action_idx]));
                str = xf86FindOptionValue(local->options, state_action_str[state_action_idx]);
                if (str == NULL)
                        continue;

                for (action_idx = 0; action_str[action_idx] != NULL; action_idx++) {
                        if (xf86NameCmp(str, action_str[action_idx]) == 0) {
                                state_ar[state_action_idx].action = action_handler[action_idx];
                                break;
                        }
                }
        }

        dump_configuration();
}

void libtouchSetYPos(LibTouchRecPtr libtouch, int y)
{
        libtouch->old_y = libtouch->cur_y;
        libtouch->cur_y = y;
        libtouch->ypos_changed = 1;
}

void libtouchSetXPos(LibTouchRecPtr libtouch, int x)
{
        libtouch->old_x = libtouch->cur_x;
        libtouch->cur_x = x;
        libtouch->xpos_changed = 1;
}

void libtouchSetPos(LibTouchRecPtr libtouch, int x, int y)
{
	libtouchSetXPos(libtouch, x);
	libtouchSetYPos(libtouch, y);
}


void libtouchSetTime(LibTouchRecPtr libtouch, CARD32 now)
{
        libtouch->now = now;
}

static
void dump_configuration()
{
        int i = 0;
        int n = 0;
        char *str = NULL;
        
        for(i = 0; state_ar[i].enter_state != NULL; i++) {
                ErrorF("State: %s\t", state_str[i]);
               
                if (state_ar[i].action == NULL)
                        str = "No Action";
                else {
                        for(n = 0; action_handler[n] != NULL; n++) {
                                if (action_handler[n] == state_ar[i].action) {
                                        str = action_str[n];
                                        break;
                                }
                        }
                }
                
                ErrorF("Action: %s\t\tButton: %d\n", str, state_ar[i].btn);
        }
}

static
void issue_btn_event(LibTouchRecPtr priv, int state, int x, int y)
{
        if (state_ar[state].action != NULL)
                state_ar[state].action(priv, state_ar[state].btn, x, y);
}


static 
int delta(int x1, int x2)
{
        return (x1 > x2) ? x1 - x2 : x2 - x1;
}


static void
disable_timers(LibTouchRecPtr priv)
{
        int sigstate;

        sigstate = xf86BlockSIGIO();
        if (priv->tap_timer) 
                TimerFree(priv->tap_timer);
        priv->tap_timer = NULL;
        priv->tap_timer_expired = FALSE;
   
        if (priv->longtouch_timer) 
                TimerFree(priv->longtouch_timer);
        priv->longtouch_timer = NULL;
        priv->longtouch_timer_expired = FALSE;
        xf86UnblockSIGIO(sigstate);
}


static CARD32
tap_timer_func(OsTimerPtr timer, CARD32 now, pointer _priv)
{
        int sigstate;
        LibTouchRecPtr priv = (LibTouchRecPtr)_priv;
        
        sigstate = xf86BlockSIGIO();
        libtouchSetTime(priv, now);
        priv->tap_timer_expired = TRUE;
        libtouchTriggerSM(priv, PEN_UNKNOWN);
        xf86UnblockSIGIO(sigstate);
       return 0;
}


static CARD32
longtouch_timer_func(OsTimerPtr timer, CARD32 now, pointer _priv)
{
        int sigstate;
        LibTouchRecPtr priv = (LibTouchRecPtr)_priv;

        sigstate = xf86BlockSIGIO();
        libtouchSetTime(priv, now);
        priv->longtouch_timer_expired = TRUE;
        libtouchTriggerSM(priv, PEN_UNKNOWN);
        xf86UnblockSIGIO(sigstate);
        return 0;
}


static void enter_untouched(LibTouchRecPtr priv)
{
        int i = 0;
        int bit_size = sizeof(priv->pressed_btn_stat) * 8;
        
        priv->touch_flags = 0;
        disable_timers(priv);

        /* do an untouch for all pressed buttons */
        for (i = 0; i < bit_size; i++)
                if (priv->pressed_btn_stat & (1 << i)) {
                        DBG(4, ErrorF("LibTouch: Issuing Button-release %d\n", i));
                        xf86PostButtonEvent(priv->local->dev, TRUE,
                                            i, 0, 0, 2,
                                            priv->cur_x, 
                                            priv->cur_y);
                }

        priv->pressed_btn_stat = 0;
}


static int handle_untouched(LibTouchRecPtr priv)
{
        static int rc = S_UNTOUCHED;
        int tmp = 0;

        DBG(4, ErrorF("LibTouch: %s\n", __FUNCTION__));
        if (priv->pen == PEN_TOUCHED) {
                priv->touch_flags |= TOUCHED;
                priv->touch_time = priv->now;
                rc = S_TOUCHED;
                DBG(4, ErrorF("LibTouch: untouched: rc = S_TOUCHED\n"));
        }

        if (priv->xpos_changed) {
                if ( !(priv->touch_flags & X_COORD)) {
                        priv->touch_x = priv->cur_x;
                        DBG(4, ErrorF("LibTouch: untouched: touch_x = %d\n", 
                               priv->touch_x));
                        priv->touch_flags |= X_COORD;
                }
	}

        if (priv->ypos_changed) {
		if ( !(priv->touch_flags & Y_COORD)) {
                        priv->touch_y = priv->cur_y;
                        DBG(4, ErrorF("LibTouch: untouched: touch_y = %d\n", 
                               priv->touch_y));
                        priv->touch_flags |= Y_COORD;
                }
        }

        if ( (priv->touch_flags & TOUCHED) &&
             (priv->touch_flags & X_COORD) &&
             (priv->touch_flags & Y_COORD) ) {
                tmp = rc;
                DBG(4, ErrorF("LibTouch: untouched: rc = %d\n", rc));
                rc = S_UNTOUCHED;
                return tmp;
        }

	DBG(4, ErrorF("LibTouch: untouched: rc = S_UNTOUCHED\n"));
        return S_UNTOUCHED;
}


static void enter_touched(LibTouchRecPtr priv)
{
        disable_timers(priv);
        priv->longtouch_timer = TimerSet(priv->longtouch_timer, 0,
                                         priv->longtouch_timeo,
                                         longtouch_timer_func, priv);
}


static int handle_touched(LibTouchRecPtr priv)
{
        int dx = 0;
        int dy = 0;

        if (priv->pen == PEN_UNTOUCHED) {
                priv->untouch_time = priv->now;
                priv->touch_flags &= ~(TOUCHED | X_COORD | Y_COORD);
                return S_MAYBETAPPED;
        }

        if (priv->longtouch_timer_expired) {
                TimerFree(priv->longtouch_timer);
                priv->longtouch_timer = NULL;
                priv->longtouch_timer_expired = FALSE;
                return S_LONGTOUCHED;
        }

        if (priv->xpos_changed) {
                if (priv->cur_x != priv->old_x) {
                        dx = delta(priv->touch_x, priv->cur_x);
                        if (dx > priv->move_limit) {
                                return S_MOVING;
                        }
                }
	}

	if (priv->ypos_changed) {
                if (priv->cur_y != priv->old_y) {
                        dy = delta(priv->touch_y, priv->cur_y);
                        if (dy > priv->move_limit) {
                                return S_MOVING;
                        }
                }
        }

        return S_TOUCHED;
}


static void enter_moving(LibTouchRecPtr priv)
{
        disable_timers(priv);
}


static int handle_moving(LibTouchRecPtr priv)
{
        if (priv->pen == PEN_UNTOUCHED)
                return S_UNTOUCHED;

        return S_MOVING;
}


static void enter_longtouched(LibTouchRecPtr priv)
{
        disable_timers(priv);
        DBG(4, ErrorF("LibTouch: Issuing Button-press 1\n"));
        issue_btn_event(priv, S_LONGTOUCHED, priv->cur_x, priv->cur_y);
}


static int handle_longtouched(LibTouchRecPtr priv)
{
        static int rc = S_LONGTOUCHED;
        int tmp = 0;
        int dx = 0;
        int dy = 0;

        if (priv->pen == PEN_UNTOUCHED) {
                priv->untouch_time = priv->now;
                priv->touch_flags &= ~(TOUCHED | X_COORD | Y_COORD);
                rc = S_UNTOUCHED;
        } else  {
                if (priv->cur_x != priv->old_x) {
                        dx = delta(priv->touch_x, priv->cur_x);
                        if (dx > priv->move_limit) 
                                rc = S_MOVING;
                }

                if (priv->cur_y != priv->old_y) {
                        dy = delta(priv->touch_y, priv->cur_y);
                        if (dy > priv->move_limit)
                                rc = S_MOVING;
                }
        }
        
        tmp = rc;
        rc = S_LONGTOUCHED;
        return tmp;
}


static void enter_maybetap(LibTouchRecPtr priv)
{
        disable_timers(priv);
        priv->tap_timer = TimerSet(priv->tap_timer, 0,
                                   priv->tap_timeo,
                                   tap_timer_func, priv);
}


static int handle_maybetap(LibTouchRecPtr priv)
{
        int dx = 0;
        int dy = 0;

        if (priv->tap_timer_expired) {
                TimerFree(priv->tap_timer);
                priv->tap_timer = NULL;
                priv->tap_timer_expired = FALSE;

                issue_btn_event(priv, S_MAYBETAPPED, priv->touch_x, priv->touch_y);                                               

                return S_UNTOUCHED;
        }

        if (priv->pen == PEN_TOUCHED) {
                /*FIXME: touched again -> clear all current timers
                  and set up a new taptimer AND longtouch-timer and
                  switch to ONEANDAHALFTAP-state
                */
                disable_timers(priv);
                priv->touch_flags |= TOUCHED;
                priv->touch_time = priv->now;
        }

        if (priv->xpos_changed) {
		dx = delta(priv->touch_x, priv->cur_x);
		if (dx > priv->move_limit) {
			DBG(4, ErrorF("LibTouch: touch_x = %d cur_x = %d\n",
				      priv->touch_x, priv->cur_x));
		}
		priv->last_touch_x = priv->touch_x;
		priv->touch_x = priv->cur_x;
		priv->touch_flags |= X_COORD;
	}

	if (priv->ypos_changed) {
		dy = delta(priv->touch_y, priv->cur_y);
		if (dy > priv->move_limit) {
			DBG(4, ErrorF("LibTouch: touch_y = %d cur_y = %d\n",
				      priv->touch_y, priv->cur_y));
		}
                        priv->last_touch_y = priv->touch_y;
                        priv->touch_y = priv->cur_y;
                        priv->touch_flags |= Y_COORD;
	}
        
        if ( (priv->touch_flags & TOUCHED) &&
             (priv->touch_flags & X_COORD) && (priv->touch_flags & Y_COORD) ) {
                dx = delta(priv->touch_x, priv->last_touch_x);
                dy = delta(priv->touch_y, priv->last_touch_y);

                if ( (dx > priv->move_limit) || (dy > priv->move_limit) ) {
                        issue_btn_event(priv, S_MAYBETAPPED, 
                                        priv->last_touch_x,
                                        priv->last_touch_y);
                        
                        return S_TOUCHED; /* touch on another place */
                }
                else 
                        return S_ONEANDAHALFTAP;
        }

        return S_MAYBETAPPED;
}


static void enter_oneandahalftap(LibTouchRecPtr priv)
{
        disable_timers(priv);
        priv->longtouch_timer = TimerSet( priv->longtouch_timer, 0,
                                          priv->longtouch_timeo,
                                          longtouch_timer_func, priv);
}


static int handle_oneandahalftap(LibTouchRecPtr priv)
{
        int dx = 0;
        int dy = 0;
        static int event_issued = 0;

        if (priv->pen == PEN_UNTOUCHED) {
                priv->touch_flags = 0;
                if ( (event_issued == 0) &&
                     (priv->longtouch_timer_expired == FALSE) ) {
                        disable_timers(priv);
                        issue_btn_event(priv, S_MAYBETAPPED, 
                                        priv->last_touch_x,
                                        priv->last_touch_y);
                        issue_btn_event(priv, S_MAYBETAPPED, 
                                        priv->last_touch_x,
                                        priv->last_touch_y);
                }
                
                event_issued = 0;
                return S_UNTOUCHED;
        }

        if ((priv->xpos_changed) || (priv->ypos_changed)) {
                dx = delta(priv->touch_x, priv->cur_x);
                dy = delta(priv->touch_y, priv->cur_y);

                if ( (dx > priv->move_limit) ||
                     (dy > priv->move_limit) ) {
                        event_issued = 0;
                        return S_MOVING;
                }
                else 
                        return S_ONEANDAHALFTAP;

        }

        if ( (event_issued == 0) &&
             (priv->longtouch_timer_expired) ) {
                TimerFree(priv->longtouch_timer);
                priv->longtouch_timer = NULL;
                priv->longtouch_timer_expired = FALSE;
                event_issued = 1;

                issue_btn_event(priv, S_ONEANDAHALFTAP, priv->cur_x, priv->cur_y);

                return S_ONEANDAHALFTAP;
        }

        return S_ONEANDAHALFTAP;
}


void libtouchTriggerSM(LibTouchRecPtr libtouch, LibTouchState_t pen)
{
        static int state_idx = S_UNTOUCHED;
        int next_state_idx;

        if (pen != PEN_UNKNOWN)
                libtouch->pen = pen;

	DBG(4, ErrorF("LibTouch: Triggering SM pen = 0x%02x\n", pen));

        next_state_idx = state_ar[state_idx].handle_state(libtouch);
        if( (next_state_idx != state_idx) && 
            (state_ar[next_state_idx].enter_state != NULL) ) {
                state_ar[next_state_idx].enter_state(libtouch);
        }

	DBG(4, ErrorF("LibTouch: Next State %d = %s\n", next_state_idx,
		      state_str[next_state_idx]));

        state_idx = next_state_idx;
        libtouch->past = libtouch->now;
        libtouch->xpos_changed = 0;
        libtouch->ypos_changed = 0;
}
