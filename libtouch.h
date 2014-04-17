#ifndef _libtouch_H_
#define _libtouch_H_

#define TOUCHED 0x01
#define X_COORD 0x02
#define Y_COORD 0x04
#define LB_STAT 0x08  /* LB up / down */
#define RB_STAT 0x10  /* RB up / down (both needed for 3-btn emu) */

typedef enum touchState {
        PEN_TOUCHED   = 1,
        PEN_UNTOUCHED = 2,
        PEN_UNKNOWN   = 3
} LibTouchState_t;

typedef struct _libtouch {
        int cur_x;
        int cur_y;
        int ypos_changed;
	int xpos_changed;
        int old_x;
        int old_y;

        LibTouchState_t pen;

        OsTimerPtr tap_timer;
        int        tap_timeo;
        Bool       tap_timer_expired;

        OsTimerPtr longtouch_timer;
        int        longtouch_timeo;
        Bool       longtouch_timer_expired;

        int drag_timer;
        unsigned char pressed_btn_stat;

        int move_limit;

        int untouch_time;
        int touch_time;
        int touch_x;
        int touch_y;
        int last_touch_x;
        int last_touch_y;
        unsigned char touch_flags; /* 1 - touched, 2 - x-coord received
                                      4 - y-coord received */

        CARD32 past;
        CARD32 now;
        LocalDevicePtr local;
} LibTouchRec, *LibTouchRecPtr;

void libtouchSetDebugLevel(int level);
void libtouchSetTapTimeo(LibTouchRecPtr libtouch, int timeo);
void libtouchSetLongtouchTimeo(LibTouchRecPtr libtouch, int timeo);
void libtouchSetOneandahalftapTimeo(LibTouchRecPtr libtouch, int timeo);
void libtouchSetTime(LibTouchRecPtr libtouch, CARD32 now);
void libtouchSetMoveLimit(LibTouchRecPtr libtouch, int move_limit);

void libtouchInit(LibTouchRecPtr libtouch, LocalDevicePtr local);

void libtouchSetPos(LibTouchRecPtr libtouch, int x, int y);
void libtouchTriggerSM(LibTouchRecPtr libtouch, LibTouchState_t touch);

#endif
