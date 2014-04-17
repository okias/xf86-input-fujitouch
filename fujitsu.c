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

#define _fujitsu_C_

#include "xorgVersion.h"


#ifndef XFree86LOADER
#include <unistd.h>
#include <errno.h>
#endif
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "misc.h"
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Xinput.h"
#include "exevents.h"
#include "xisb.h"

#ifdef XFree86LOADER
#include "xf86Module.h"
#endif


/*****************************************************************************
 *        Local Headers
 ****************************************************************************/
#include "libtouch.h"
#include "fujitsu.h"

/*****************************************************************************
 *        Variables without includable headers
 ****************************************************************************/

/*****************************************************************************
 *        Local Variables
 ****************************************************************************/


#ifdef DBG
#undef DBG
#endif
static int debug_level = 0;

#ifdef FUJIDBG
#define DBGOUT(lvl, ...) {if (lvl <= debug_level) xf86ErrorFVerb(lvl, __VA_ARGS__);}
#define DBG(lvl, f) {if ((lvl) <= debug_level) f;}
#else 
#define DBGOUT(lvl, ...)
#define DBG(lvl, f)
#endif

static InputInfoPtr
FujiPreInit(InputDriverPtr drv, IDevPtr dev, int flags);

_X_EXPORT InputDriverRec FUJITSU = {
        1,
        "fujitsu",
        NULL,
        FujiPreInit,
        NULL,
        NULL,
        0
};

#ifdef XFree86LOADER

static XF86ModuleVersionInfo VersionRec =
{
        "fujitsu",
        "Kenan Esau",
        MODINFOSTRING1,
        MODINFOSTRING2,
        XORG_VERSION_CURRENT,
        0,6,7,
        ABI_CLASS_XINPUT,
        ABI_XINPUT_VERSION,
        MOD_CLASS_XINPUT,
        {0, 0, 0, 0}           /* signature, to be patched into the file by
                                * a tool */
};


static pointer
Plug( pointer module,
      pointer options,
      int *errmaj,
      int *errmin )
{
        xf86AddInputDriver(&FUJITSU, module, 0);
        return module;
}


static void
Unplug(pointer p)
{
        DBGOUT(1, "Unplug\n");
}


_X_EXPORT XF86ModuleData fujitsuModuleData = {&VersionRec, Plug, Unplug };

#endif /* XFree86LOADER */


static const char *default_options[] =
{
        "BaudRate", "9600",
        "StopBits", "1",
        "DataBits", "8",
        "Parity", "None",
        "Vmin",  "1",
        "Vtime", "0",
        "FlowControl", "None"
};

static void
ControlProc(DeviceIntPtr device, PtrCtrl *ctrl);
static Bool
FujiInitHW(LocalDevicePtr local);


/*****************************************************************************
 *        Function Definitions
 ****************************************************************************/


static Bool
DeviceOn (DeviceIntPtr dev)
{
        LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
        FujiPrivatePtr priv = (FujiPrivatePtr) (local->private);
        
        local->fd = xf86OpenSerial(local->options);

        DBGOUT(2, "Device ON\n");

        if (local->fd == -1)
        {
                xf86Msg(X_WARNING, "%s: cannot open input device\n", local->name);
                return (!Success);
        }

        priv->buffer = XisbNew(local->fd, 64);
        if (!priv->buffer) 
        {
                xf86CloseSerial(local->fd);
                local->fd = -1;
                return (!Success);
        }

        xf86FlushInput(local->fd);
        AddEnabledDevice(local->fd);
        dev->public.on = TRUE;

        FujiInitHW(local);
        DBGOUT(2, "Device ON 2\n");
        return (Success);
}




static Bool
DeviceOff (DeviceIntPtr dev)
{
        LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
        FujiPrivatePtr priv = (FujiPrivatePtr) (local->private);

        DBGOUT(2, "Device OFF\n");

        if (local->fd != -1)
        { 
                xf86RemoveEnabledDevice (local);
                if (priv->buffer)
                {
                        XisbFree(priv->buffer);
                        priv->buffer = NULL;
                }
                xf86CloseSerial(local->fd);
        }

        dev->public.on = FALSE;

        if ( (priv->calibrate) && (priv->fifo>0) ){
                SYSCALL(close (priv->fifo));
        }
        DBGOUT(2, "Device OFF 2\n");

        return Success;
}




static Bool
DeviceInit (DeviceIntPtr dev)
{
        LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
        FujiPrivatePtr priv = (FujiPrivatePtr) (local->private);
        unsigned char map[] = {0, 1, 2, 3};
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
        Atom btn_labels[1] = {0};
        Atom axis_labels[1] = {0};
#endif

   
        /* 
         * these have to be here instead of in the SetupProc, because when the
         * SetupProc is run at server startup, screenInfo is not setup yet
         */

        ScrnInfoPtr   pScrn = xf86Screens[priv->screen_num];

        priv->phys_width = pScrn->currentMode->HDisplay;  /* physical screen resolution */
        priv->phys_height = pScrn->currentMode->VDisplay;
        priv->screen_width  = pScrn->virtualX;     /* It's the virtual screen size ! */
        priv->screen_height = pScrn->virtualY;
        priv->pViewPort_X0  = &(pScrn->frameX0);   /* initialize the pointers to the viewport coords */
        if ( (priv->screen_width != priv->phys_width) ||
             (priv->screen_height != priv->phys_height) ) 
              priv->virtual = 1;
        else  
                priv->virtual = 0;

        priv->pViewPort_Y0  = &(pScrn->frameY0);
        priv->pViewPort_X1  = &(pScrn->frameX1);
        priv->pViewPort_Y1  = &(pScrn->frameY1);

        DBGOUT(2, "DeviceInit\n");
        DBGOUT(2, "Display X,Y: %d %d\n", priv->phys_width, priv->phys_height);
        DBGOUT(2, "Virtual X,Y: %d %d\n", priv->screen_width, priv->screen_height);
        DBGOUT(2, "DriverName, Rev.: %s %d\n", pScrn->driverName, pScrn->driverVersion);
        DBGOUT(2, "Viewport X0,Y0: %d %d\n", *priv->pViewPort_X0, *priv->pViewPort_Y0);
        DBGOUT(2, "Viewport X1,Y1: %d %d\n", *priv->pViewPort_X1, *priv->pViewPort_Y1);
        DBGOUT(2, "MaxValue H,V: %d %d\n", pScrn->maxHValue, pScrn->maxVValue);



        priv->screen_width = screenInfo.screens[priv->screen_num]->width;
        priv->screen_height = screenInfo.screens[priv->screen_num]->height;


        /* 
         * Device reports button press for 3 buttons.
         */
        if (InitButtonClassDeviceStruct (dev, 3, 
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
					btn_labels,
#endif
					 map) == FALSE)
        {
                ErrorF("Unable to allocate Fuji touchscreen ButtonClassDeviceStruct\n");
                return !Success;
        }

        if (InitFocusClassDeviceStruct(dev) == FALSE) {
                ErrorF("Unable to allocate Fuji touchscreen FocusClassDeviceStruct\n");
                return !Success;
        }

        /*
         * Device reports motions on 2 axes in absolute coordinates.
         * Axes min and max values are reported in raw coordinates.
         */
        if (InitValuatorClassDeviceStruct(dev, 2,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
					  axis_labels,
#endif
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 3
                                          xf86GetMotionEvents,
#endif
                                          local->history_size, Absolute) == FALSE)
        {
                ErrorF ("Unable to allocate Fuji touchscreen ValuatorClassDeviceStruct\n");
                return !Success;
        }

	InitValuatorAxisStruct(dev, 0,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
			       0,
#endif
0, priv->screen_width,
			       FUJI_AXIS_MAX_RES,
			       FUJI_AXIS_MIN_RES /* min_res */ ,
			       FUJI_AXIS_MAX_RES /* max_res */ );

         /* xf86InitValuatorAxisStruct(dev, 0, 0, priv->screen_width, */
         /*                            FUJI_AXIS_MAX_RES, */
         /*                            FUJI_AXIS_MIN_RES /\* min_res *\/ , */
         /*                            FUJI_AXIS_MAX_RES /\* max_res *\/ ); */
         /* xf86InitValuatorDefaults(dev, 0); */

	InitValuatorAxisStruct(dev, 1,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
			       0,
#endif
			       0, priv->screen_height,
			       FUJI_AXIS_MAX_RES,
			       FUJI_AXIS_MIN_RES /* min_res */ ,
			       FUJI_AXIS_MAX_RES /* max_res */ );

         
         /* xf86InitValuatorAxisStruct(dev, 1, 0, priv->screen_height, */
         /*                            FUJI_AXIS_MAX_RES, */
         /*                            FUJI_AXIS_MIN_RES /\* min_res *\/ , */
         /*                            FUJI_AXIS_MAX_RES /\* max_res *\/ ); */
         /* xf86InitValuatorDefaults(dev, 1); */

        
        if (InitProximityClassDeviceStruct (dev) == FALSE)
        {
                ErrorF("Unable to allocate Fuji touchscreen ProximityClassDeviceStruct\n");
                return !Success;
        }

        if (InitPtrFeedbackClassDeviceStruct(dev, ControlProc) == FALSE) 
        {
                ErrorF("Unable to init ptr feedback for Fuji\n");
                return !Success;
        }

        /* 
         * Allocate the motion events buffer.
         */
        xf86MotionHistoryAllocate (local);

        return (Success);
}




static Bool
DeviceControl(DeviceIntPtr dev,
              int mode)
{
        Bool RetValue;

        switch (mode)
        {
        case DEVICE_INIT:
                RetValue = DeviceInit(dev);
                break;
        case DEVICE_ON:
                RetValue = DeviceOn(dev);
                break;
        case DEVICE_OFF:
        case DEVICE_CLOSE:
                RetValue = DeviceOff(dev);
                break;
        default:
                RetValue = BadValue;
        }

        return(RetValue);
}




static int
FujiGetPacketSizeType(FujiPrivatePtr priv, unsigned char packet_type) 
{
        priv->packet_type = PACK_UNKNOWN;
        if (priv->cold_reset) {
                priv->cold_reset = 0;
                return 33; /* 32 bytes more to ignore */
        }

        DBG(8, ErrorF("%s packet_type = %02x\n", __FUNCTION__, packet_type));
        if ( (packet_type & 0xB0) == 0x80 ) {
                priv->packet_type = PACK_COORDINATE;

                switch (packet_type & 0x83) {
                case 0x80:
                        DBG(4, ErrorF("%s touch\n", __FUNCTION__));
                        priv->packet_type |= PACK_TOUCH;
                        break;
                case 0x82:
                        DBG(4, ErrorF("%s untouch\n", __FUNCTION__));
                        priv->packet_type |= PACK_UNTOUCH;
                        break;
                }
                return 5;
        } else {               
                switch(packet_type) {

                
                case 0x90:
                case 0xD0:
                        return 2;
                case 0x91:
                        return 7;
                case 0x92:
                case 0xD2:
                        return 1;
                case 0x9F:
                        return 115;
                default:
                        return -1;
                }
        }
}




static Bool
FujiGetPacket(FujiPrivatePtr priv)
{
        static int count;
        int packet_size = 0;
        int c;
        CARD32 now;

        count = 0;
        memset(&priv->packet, 0, FUJI_MAX_PACKET_SIZE);
        DBGOUT(2, "%s\n", __FUNCTION__);
        while (1) {
                if (debug_level >= 9)
                        XisbTrace(priv->buffer, 1);
                c = XisbRead(priv->buffer);
                priv->packet[count] = c;
                if (count == 0) { 
                        now = GetTimeInMillis();
                        libtouchSetTime(priv->libtouch, now);
                        packet_size = FujiGetPacketSizeType(priv, c);
                        DBGOUT( 2, "%s: packet_size = %d\n", __FUNCTION__,
                                packet_size);
                        if (packet_size < 0)
                                break;
                }

                if (count == packet_size - 1) {
                        DBGOUT( 2, "%s: success \n", __FUNCTION__);
                        return Success;
                }
                count++;
        }
        DBGOUT( 2, "%s\n", __FUNCTION__);
        return (!Success);
}




static void FujiHandleCoordinate(LocalDevicePtr local)
{
        FujiPrivatePtr priv = (FujiPrivatePtr)local->private;
	int v0 = 0;
	int v1 = 0;

        DBGOUT( 2, "%s\n", __FUNCTION__);

        priv->old_x = priv->cur_x;
        priv->old_y = priv->cur_y;

        v0 = ((unsigned short)priv->packet[1]) | 
                ( ((unsigned short)(priv->packet[2])) << 7 );
        v1 = ((unsigned short)priv->packet[3]) | 
                ( ((unsigned short)(priv->packet[4])) << 7 );

	ConvertProc(local, 0, 2, v0, v1,
		    0, 0, 0, 0,
                    &priv->cur_x, &priv->cur_y);

        libtouchSetPos(priv->libtouch, priv->cur_x, priv->cur_y);

        DBGOUT( 2, "setting (x/y)=(%d/%d)\n", 
                priv->cur_x, priv->cur_y);

        xf86XInputSetScreen(local, priv->screen_num, 
                            priv->cur_x, 
                            priv->cur_y);
                
        xf86PostProximityEvent(local->dev, 1, 0, 2, 
                               priv->cur_x, 
                               priv->cur_y);
                        
        /* 
         * Send events.
         *
         * We *must* generate a motion before a button change if pointer
         * location has changed as DIX assumes this. This is why we always
         * emit a motion, regardless of the kind of packet processed.
         */
        xf86PostMotionEvent (local->dev, TRUE, 0, 2, 
                             priv->cur_x, 
                             priv->cur_y);
}




static void ReadInput (LocalDevicePtr local)
{
        FujiPrivatePtr priv = (FujiPrivatePtr) (local->private);
        CARD32 now;

        /* 
         * set blocking to -1 on the first call because we know there is data to
         * read. Xisb automatically clears it after one successful read so that
         * succeeding reads are preceeded buy a select with a 0 timeout to prevent
         * read from blocking infinately.
         */
        XisbBlockDuration (priv->buffer, -1);
        FujiGetPacket(priv);

        now = GetTimeInMillis();
        if (priv->packet_type & PACK_COORDINATE) {
                FujiHandleCoordinate(local);
                if (priv->packet_type & PACK_UNTOUCH)
                        libtouchTriggerSM(priv->libtouch, PEN_UNTOUCHED);
                else 
                        libtouchTriggerSM(priv->libtouch, PEN_TOUCHED);
        }
}




void
ControlProc(DeviceIntPtr device, PtrCtrl *ctrl)
{
        DBGOUT(2, "ControlProc\n");
}




void
CloseProc (LocalDevicePtr local)
{
        xf86ErrorFVerb(2, "CLOSEPROC\n" );
}




int
SwitchMode (ClientPtr client, DeviceIntPtr dev, int mode)
{
        return (!Success);
}




Bool
ConvertProc(LocalDevicePtr local,
            int first,
            int num,
            int v0,
            int v1,
            int v2,
            int v3,
            int v4,
            int v5,
            int *x,
            int *y)
{
        /*
          correction factors depending on current position of pointer
        */
        float cx[3];
        float cy[3];
        float dx = 0, dy = 0;

        int max_x, max_y;
        int xc, yc;
        int screen_width  = 0;
        int screen_height = 0;
#ifdef FUJIDBG
        int i = 0;
#endif

        FujiPrivatePtr priv = (FujiPrivatePtr) (local->private);  
	ScrnInfoPtr pScrn = xf86Screens[priv->screen_num];
	Rotation rotation = RRGetRotation(pScrn->pScreen);

        DBGOUT(2, "FIRST: v0=%d   v1=%d\n", v0, v1);

        /*correction of raw coordinates*/
        if ( (priv->fifo>0) && (priv->calibrate) ){
                DBGOUT(8, "writing to FIFO\n");
                SYSCALL(write (priv->fifo, &v0, sizeof(v0)));
                SYSCALL(write (priv->fifo, &v1, sizeof(v1)));
        }

        if (!priv->calibrate) {
                xc = v0 - priv->min_x;
                yc = v1 - priv->min_y;
        
                max_x = priv->max_x - priv->min_x;
                max_y = priv->max_y - priv->min_y;

                if (priv->rotate == FUJI_ROTATE_NONE) {
                        screen_width  = priv->phys_width;
                        screen_height = priv->phys_height;
                } else {
                        screen_width  = priv->phys_height;
                        screen_height = priv->phys_width;
                }


                if (xc < (max_x / 2)) {
                        /*
                          left
                        */
                        if (yc>(max_y / 2)) {
                                /*
                                  upper
                                */
                                cx[1] = ((float) xc / (max_x / 2) );
                                cx[0] = (float) 1 - cx[1];
                                cy[0] = ((float) (yc-(max_y/2)) / (max_y/2) );
                                cy[1] = (float) 1 - cy[0];

                                dx = ((float) (cx[1] * cy[0] * priv->diff[1][0]) +
                                      (float)(cx[0] * cy[0] * priv->diff[0][0]) +
                                      (float)(cx[1] * cy[1] * priv->diff[4][0]) +
                                      (float)(cx[0] * cy[1] * priv->diff[3][0]));
                                
                                dy = ((float) (cx[1] * cy[0] * priv->diff[1][1]) +
                                      (float)(cx[0] * cy[0] * priv->diff[0][1]) +
                                      (float)(cx[1] * cy[1] * priv->diff[4][1]) +
                                      (float)(cx[0] * cy[1] * priv->diff[3][1]));
                        }
                        else {
                                /*
                                  lower
                                */
                                cx[1] = ((float) xc / (max_x/2) );
                                cx[0] = (float) 1 - cx[1];
                                cy[0] = ((float) yc / (max_y/2) );
                                cy[1] = (float) 1 - cy[0];

                                dx = ((float) (cx[1] * cy[0] * priv->diff[4][0]) +
                                      (float)(cx[0] * cy[0] * priv->diff[3][0]) +
                                      (float)(cx[1] * cy[1] * priv->diff[7][0]) +
                                      (float)(cx[0] * cy[1] * priv->diff[6][0]));
                                
                                dy = ((float) (cx[1] * cy[0] * priv->diff[4][1]) +
                                      (float)(cx[0] * cy[0] * priv->diff[3][1]) +
                                      (float)(cx[1] * cy[1] * priv->diff[7][1]) +
                                      (float)(cx[0] * cy[1] * priv->diff[6][1]));

                        }
                } else {
                        /*
                          right
                        */
                        if (yc>(max_y/2)) {
                                /*
                                  upper
                                */
                                cx[1] = ((float) (xc-(max_x/2)) / (max_x/2) );
                                cx[0] = (float)1 - cx[1];
                                cy[0] = ((float) (yc-(max_y/2)) / (max_y/2) );
                                cy[1] = (float)1 - cy[0];

                                dx = ((float) (cx[1] * cy[0] * priv->diff[2][0]) +
				      (float)(cx[0] * cy[0] * priv->diff[1][0]) +
				      (float)(cx[1] * cy[1] * priv->diff[5][0]) +
				      (float)(cx[0] * cy[1] * priv->diff[4][0]));
                        
                                dy = ((float) (cx[1] * cy[0] * priv->diff[2][1]) +
				      (float)(cx[0] * cy[0] * priv->diff[1][1]) +
                                      (float)(cx[1] * cy[1] * priv->diff[5][1]) +
                                      (float)(cx[0] * cy[1] * priv->diff[4][1]));

                                DBGOUT(8, "TEST: dx=%f   dy=%f\n", dx, dy);
                        } else {
                                /*
                                  lower
                                */
                                cx[1] = ((float) (xc-(max_x/2)) / (max_x/2) );
                                cx[0] = (float) 1 - cx[1];
                                cy[0] = ((float) yc / (max_y/2) );
                                cy[1] = (float) 1 - cy[0];

                                dx = ((float) (cx[1] * cy[0] * priv->diff[5][0]) +
                                      (float)(cx[0] * cy[0] * priv->diff[4][0]) +
                                      (float)(cx[1] * cy[1] * priv->diff[8][0]) +
                                      (float)(cx[0] * cy[1] * priv->diff[7][0]));
                                
                                dy = ((float) (cx[1] * cy[0] * priv->diff[5][1]) +
                                      (float)(cx[0] * cy[0] * priv->diff[4][1]) +
                                      (float)(cx[1] * cy[1] * priv->diff[8][1]) +
                                      (float)(cx[0] * cy[1] * priv->diff[7][1]));
                        }
                }


#ifdef FUJIDBG
                for (i = 0; i < 3; i++) 
                        DBGOUT(8, "cx[%d]=%f   cy[%d]=%f\n", i, cx[i]
                               ,i, cy[i]);
                
                DBGOUT(8, "ViewPort_X0=%d   ViewPort_Y0=%d\n", 
                    *(priv->pViewPort_X0), 
                    *(priv->pViewPort_Y0));
                DBGOUT(8, "dx=%f   dy=%f\n", dx, dy);
#endif


                xc = ( ((float)xc / max_x) * screen_width ) + dx;
                yc = ( ((float)yc / max_y) * screen_height) + dy;

                if (priv->swap_y == TRUE)
                        yc = screen_height - yc;

                /* ususally we DON'T swap x -- but if swap_x is 1 
                   => go on and swap */
                if (priv->swap_x == TRUE)
                        xc = screen_width - xc;

                int tmp = 0;
                /* rotation mixes x and y up a bit */
                if (priv->rotate == FUJI_ROTATE_CW) {
                        tmp = xc;
                        xc = yc;
                        yc = screen_width - tmp;
                } else if (priv->rotate == FUJI_ROTATE_CCW) {
                        tmp = xc;
                        xc = screen_height - yc;
                        yc = tmp;
                }

		switch (rotation) {
			case RR_Rotate_0:
				v0 = xc;
				v1 = yc;
				break;
			case RR_Rotate_180:
				v0 = screen_width - xc;
				v1 = screen_height - yc;
				break;
			case RR_Rotate_90:
                                tmp = xc;
				v0  = screen_height - yc;
                                v1  = tmp;
				break;
			case RR_Rotate_270:
                                tmp = xc;
				v0 = yc;
				v1 = screen_width - tmp;
				break;
			default:
				break;
		}
        }

        DBGOUT(2, "FINAL: v0=%d   v1=%d\n", v0, v1);

        *x = v0;
        *y = v1;

        return (TRUE);
}




Bool
QueryHardware (LocalDevicePtr local)
{
        xf86ErrorFVerb(2, "QUERY HARDWARE\n" );

        return Success;
}




InputInfoPtr
FujiPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
        /* LocalDevicePtr local; */
        InputInfoPtr local;
        FujiPrivatePtr priv;

        int i = 0;
        char *s;
        char tmp_str[8];
        int timeo = 0;
        priv = xcalloc (1, sizeof(FujiPrivateRec));
        if (!priv)
                return NULL;

        local = xf86AllocateInput(drv, 0);
        if (!local) {
                xfree(priv);
                return NULL;
        }

        local->name = dev->identifier;
        local->type_name = XI_TOUCHSCREEN;
        local->device_control = DeviceControl;
        local->read_input = ReadInput;
        local->control_proc = NULL;
        local->close_proc = CloseProc;
        local->switch_mode = SwitchMode;
        local->conversion_proc = ConvertProc;
        local->reverse_conversion_proc = NULL;
        local->fd = -1;
        local->dev = NULL;
        local->private = priv;
        priv->local = local;
        local->private_flags = 0;
        local->flags = XI86_POINTER_CAPABLE | XI86_SEND_DRAG_EVENTS;
        local->conf_idev = dev;

        xf86CollectInputOptions(local, default_options, NULL);

        xf86OptionListReport(local->options);

        local->fd = xf86OpenSerial (local->options);
        if (local->fd == -1)
        {
                ErrorF ("Fuji driver unable to open device\n");
                goto SetupProc_fail;
        }
        xf86ErrorFVerb( 3, "Device opened successfully\n" );

        priv->libtouch = xcalloc(1, sizeof(LibTouchRec));
        libtouchInit(priv->libtouch, local);
        priv->calibrate = xf86SetIntOption(local->options, "Calibrate", 0);
        priv->min_x = xf86SetIntOption(local->options, "MinX", 0 );
        priv->max_x = xf86SetIntOption(local->options, "MaxX", FUJI_SCREEN_WIDTH );
        priv->min_y = xf86SetIntOption(local->options, "MinY", 0 );
        priv->max_y = xf86SetIntOption(local->options, "MaxY", FUJI_SCREEN_HEIGHT );
        priv->screen_num    = xf86SetIntOption(local->options, "ScreenNumber", 0 );
        priv->button_number = xf86SetIntOption(local->options, "ButtonNumber", 2 );

        debug_level = xf86SetIntOption(local->options, "DebugLevel", 0);
        libtouchSetDebugLevel(debug_level);

        timeo = xf86SetIntOption(local->options, "TapTimer", 90);
        libtouchSetTapTimeo(priv->libtouch, timeo);

        timeo = xf86SetIntOption(local->options, "LongtouchTimer", 160);
        libtouchSetLongtouchTimeo(priv->libtouch, timeo);

        libtouchSetMoveLimit(priv->libtouch, 
                             xf86SetIntOption( local->options, 
                                               "MoveLimit", 180 ));

        priv->rotate     = FUJI_ROTATE_NONE;
        s = xf86FindOptionValue(local->options, "Rotate");
        if (s) {
                if (xf86NameCmp(s, "CW") == 0) {
                        priv->rotate = FUJI_ROTATE_CW;                           
                } else if (xf86NameCmp(s, "CCW") == 0 ) {
                        priv->rotate = FUJI_ROTATE_CCW;
                } 
        }

        if (priv->rotate == FUJI_ROTATE_NONE) {
                priv->max_rel_x = priv->max_x;
                priv->max_rel_y = priv->max_y;
                priv->min_rel_x = priv->min_x;
                priv->min_rel_y = priv->min_y;             
        } else {
                priv->max_rel_x = priv->max_y;
                priv->max_rel_y = priv->max_x;
                priv->min_rel_x = priv->min_y;
                priv->min_rel_y = priv->min_x;
        }

        priv->swap_y = xf86SetBoolOption(local->options, "SwapY", FALSE);
        priv->swap_x = xf86SetBoolOption(local->options, "SwapX", FALSE);

        /* 
           get calibration parameters from XF86Config 
        */
        for (i = 0; i < 9; i++){
                sprintf(tmp_str, "x%d", i);
                priv->diff[i][0] = xf86SetIntOption( local->options,
                                                     tmp_str, 0 );
                sprintf(tmp_str, "y%d", i);
                priv->diff[i][1] = xf86SetIntOption( local->options,
                                                     tmp_str, 0 );
                DBGOUT(2, "(diff[%d][0]/diff[%d][1])=(%d/%d)\n", i, i, 
                    priv->diff[i][0], priv->diff[i][1]);
        }
        

        /* Initial position of pointer on screen: Centered */
        priv->cur_x = (priv->max_x - priv->min_x) / 2;
        priv->cur_y = (priv->max_y - priv->min_y) / 2;
        libtouchSetPos(priv->libtouch, priv->cur_x, priv->cur_y);

        priv->buffer = XisbNew (local->fd, 200);
        priv->packet_type = PACK_UNKNOWN;

        DBG (9, XisbTrace (priv->buffer, 1));

        if (QueryHardware(local) != Success)
        {
                ErrorF ("Unable to query/initialize Fuji hardware.\n");
                goto SetupProc_fail;
        }

        local->history_size = xf86SetIntOption( local->options, "HistorySize", 0 );

        /* prepare to process touch packets */
        memset(&priv->packet, 0, FUJI_MAX_PACKET_SIZE);

        /* 
           if started in calibration-mode:
           - open named pipe 
        */
        if (priv->calibrate) {
                SYSCALL(priv->fifo=open("/tmp/ev_calibrate", O_RDWR, 0));
                if (priv->fifo < 0)
                        xf86ErrorFVerb(2, "open FIFO FAILED\n");
        }

        /* this results in an xstrdup that must be freed later */
        local->name = xf86SetStrOption( local->options, "DeviceName", "Fujitsu Serial TouchScreen" );
        xf86ProcessCommonOptions(local, local->options);
        local->flags |= XI86_CONFIGURED;

        if (local->fd != -1)
        { 
                if (priv->buffer)
                {
                        XisbFree(priv->buffer);
                        priv->buffer = NULL;
                }
                xf86CloseSerial(local->fd);
        }
        local->fd = -1;

        return (local);

 SetupProc_fail:
        if ((local) && (local->fd))
                xf86CloseSerial (local->fd);
        if ((local) && (local->name))
                xfree (local->name);

        if ((priv) && (priv->buffer))
                XisbFree (priv->buffer);
        if (priv)
                xfree (priv);
        return (local);
}



Bool
FujiInitHW(LocalDevicePtr local)
{
        FujiPrivatePtr priv = (FujiPrivatePtr) local->private;
        unsigned char cmd;
        int res = 0;

        priv->cold_reset = 1;

        cmd = DUMMY_DATA;
        res = XisbWrite(priv->buffer, &cmd, sizeof(cmd));
        if (res != sizeof(cmd)) {
                xf86ErrorFVerb(3, "System error while sending DUMMY_DATA to device\n");
                return !Success;
        }
        sleep(1);
//        SYSCALL(usleep(50 * 1000));

        cmd = CMD_COLD_RST;
        XisbWrite(priv->buffer, &cmd, sizeof(cmd));
        if (res != sizeof(cmd)) {
                xf86ErrorFVerb(3, "System error while sending reset to device\n");
                return !Success;
        }

        return (Success);
}
