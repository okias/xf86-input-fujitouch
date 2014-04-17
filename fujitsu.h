#ifndef _fujitsu_H_
#define _fujitsu_H_


#define FUJI_MAX_PACKET_SIZE     116


/*
  COMMANDS
*/
#define CMD_RST      0x80
#define CMD_COLD_RST 0x81
#define CMD_STOP     0x82
#define CMD_START    0x83
#define CMD_CAL_IN   0x84
#define CMD_CAL_OUT  0x85
#define CMD_CAL_SET  0x86
#define CMD_CANCEL   0x87
#define CMD_REPORT   0x88
#define CMD_DIAG     0x89
#define CMD_SET_TIME 0x8A
#define CMD_SET_RATE 0x8B
#define CMD_WRITE    0x8c
#define CMD_READ     0x8d
#define CMD_CALTRN   0xe0
#define CMD_CALRCV   0xe1
#define DUMMY_DATA   0xff


/******************************************************************************
 *  Definitions
 *  structs, typedefs, #defines, enums
 *****************************************************************************/
#define FUJI_ROTATE_NONE 0
#define FUJI_ROTATE_CW   1
#define FUJI_ROTATE_CCW  2

/* Physical Screen Dimensions. (for default values)
   For the Lifebook P Series currently 1024x600 pixels */
#define FUJI_SCREEN_WIDTH       1024
#define FUJI_SCREEN_HEIGHT      600
#define FUJI_AXIS_MIN_RES       0
#define FUJI_AXIS_MAX_RES       10000
#define FUJI_PAN_BORDER         12
        
#define FUJI_TIMEOUT            500

#define PACK_UNKNOWN    0
#define PACK_COORDINATE 1
#define PACK_TOUCH      2
#define PACK_UNTOUCH    4

#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))

typedef struct _FujiPrivateRec
{
        int diff[9][2];
        int min_x;  /* Minimum x reported by calibration        */
        int max_x;  /* Maximum x                    */
        int min_y;  /* Minimum y reported by calibration        */
        int max_y;  /* Maximum y                    */

        int cur_x;
        int cur_y;
        int old_x;
        int old_y;

        int min_rel_x;  /* Minimum x reported by calibration        */
        int max_rel_x;  /* Maximum x                    */
        int min_rel_y;  /* Minimum y reported by calibration        */
        int max_rel_y;  /* Maximum y                    */

        int debug_level;

        int calibrate;
        int fifo;  /*fd of the fifo used for communication with the calibration programm*/
        /* pointers to the current viewport coordinates */
        int *pViewPort_X0;    /* Min X */
        int *pViewPort_X1;    /* Max X */
        int *pViewPort_Y0;    /* Min Y */
        int *pViewPort_Y1;    /* Max Y */
        int virtual;          /* virtual=1 indicates that there is a virtual screen */
        int rotate;           /* 90 deg CW, -90 deg CCW, default is 0 */
        Bool swap_y;           /* swap the y axis */
        Bool swap_x;           /* swap the x axis */
        int x;                /* x in screen coords */
        int y;                /* y in screen coords */
        int phys_width;       /* Physical X-Resolution */
        int phys_height;      /* Physical Y-Resolution */

        Bool pan_viewport;

        int button_number;   /* which button to report */
        int reporting_mode;   /* TS_Raw or TS_Scaled */

        int screen_num;    /* Screen associated with the device */
        int screen_width;   /* Width of the associated X screen  */
        int screen_height;   /* Height of the screen              */
        unsigned char cold_reset;
        
        unsigned int packet_type;
        XISBuffer *buffer;
        unsigned char packet[FUJI_MAX_PACKET_SIZE]; /* packet being/just read */

        LibTouchRecPtr libtouch;
        LocalDevicePtr local;
} FujiPrivateRec, *FujiPrivatePtr;

/* 
 *    DO NOT PUT ANYTHING AFTER THIS ENDIF
 */
#endif
