/*
 * Message queues definitions
 *
 * Copyright 1993 Alexandre Julliard
 */

#ifndef __WINE_QUEUE_H
#define __WINE_QUEUE_H

#include "wintypes.h"
#include "wine/winuser16.h"
#include "thread.h"


  /* Message as stored in the queue (contains the extraInfo field) */
typedef struct tagQMSG
{
    MSG32   msg;
    DWORD   extraInfo;  /* Only in 3.1 */
    
    struct tagQMSG *nextMsg;
    struct tagQMSG *prevMsg;
} QMSG;

typedef struct
{
  LRESULT   lResult;
  BOOL16    bPending;
} QSMCTRL;

/* Per-queue data for the message queue
 * Note that we currently only store the current values for
 * Active, Capture and Focus windows currently.
 * It might be necessary to store a pointer to the system message queue
 * as well since windows 9x maintains per thread system message queues
 */
typedef struct tagPERQUEUEDATA      
{
  HWND32    hWndFocus;              /* Focus window */
  HWND32    hWndActive;             /* Active window */
  HWND32    hWndCapture;            /* Capture window */
  INT16     nCaptureHT;             /* Capture info (hit-test) */
  ULONG     ulRefCount;             /* Reference count */
  CRITICAL_SECTION cSection;        /* Critical section for thread safe access */
} PERQUEUEDATA;

/* Message queue */
typedef struct tagMESSAGEQUEUE
{
  HQUEUE16  next;                   /* Next queue */
  HQUEUE16  self;                   /* Handle to self (was: reserved) */
  THDB*     thdb;                   /* Thread owning queue */
  HANDLE32  hEvent;                 /* Event handle */
  CRITICAL_SECTION cSection;        /* Queue access critical section */

  DWORD     magic;                  /* magic number should be QUEUE_MAGIC */
  DWORD     lockCount;              /* reference counter */
  
  WORD      flags;                  /* Queue flags */
  WORD      wWinVersion;            /* Expected Windows version */
  
  WORD      msgCount;               /* Number of waiting messages */
  QMSG*     firstMsg;               /* First message in linked list */
  QMSG*     lastMsg;                /* Last message in linked list */
  
  WORD      wPostQMsg;              /* PostQuitMessage flag */
  WORD      wExitCode;              /* PostQuitMessage exit code */
  WORD      wPaintCount;            /* Number of WM_PAINT needed */
  WORD      wTimerCount;            /* Number of timers for this task */

  WORD      changeBits;             /* Changed wake-up bits */
  WORD      wakeBits;               /* Queue wake-up bits */
  WORD      wakeMask;               /* Queue wake-up mask */

  DWORD     GetMessageTimeVal;      /* Value for GetMessageTime */
  DWORD     GetMessagePosVal;       /* Value for GetMessagePos */
  DWORD     GetMessageExtraInfoVal; /* Value for GetMessageExtraInfo */
  
  HQUEUE16  InSendMessageHandle;    /* Queue of task that sent a message */
  HTASK16   hSendingTask;           /* Handle of task that sent a message */
  HTASK16   hPrevSendingTask;       /* Handle of previous sender */
  
  HWND32    hWnd32;                 /* Send message arguments */
  UINT32    msg32;                  
  WPARAM32  wParam32;               
  LPARAM    lParam;                 
  DWORD     SendMessageReturn;      /* Return value for SendMessage */

  QSMCTRL*  smResultInit;           /* SendMesage result pointers */
  QSMCTRL*  smResultCurrent;        
  QSMCTRL*  smResult;               
  
  HANDLE16  hCurHook;               /* Current hook */
  HANDLE16  hooks[WH_NB_HOOKS];     /* Task hooks list */

  PERQUEUEDATA *pQData;             /* pointer to (shared) PERQUEUEDATA structure */
  
} MESSAGEQUEUE;


/* Extra (undocumented) queue wake bits - see "Undoc. Windows" */
#define QS_SMRESULT      0x8000  /* Queue has a SendMessage() result */
#define QS_SMPARAMSFREE  0x4000  /* SendMessage() parameters are available */

/* Queue flags */
#define QUEUE_SM_WIN32     0x0002  /* Currently sent message is Win32 */
#define QUEUE_SM_UNICODE   0x0004  /* Currently sent message is Unicode */

#define QUEUE_MAGIC        0xD46E80AF

/* Per queue data management methods */
PERQUEUEDATA* PERQDATA_CreateInstance( );
ULONG PERQDATA_Addref( PERQUEUEDATA* pQData );
ULONG PERQDATA_Release( PERQUEUEDATA* pQData );
HWND32 PERQDATA_GetFocusWnd( PERQUEUEDATA *pQData );
HWND32 PERQDATA_SetFocusWnd( PERQUEUEDATA *pQData, HWND32 hWndFocus );
HWND32 PERQDATA_GetActiveWnd( PERQUEUEDATA *pQData );
HWND32 PERQDATA_SetActiveWnd( PERQUEUEDATA *pQData, HWND32 hWndActive );
HWND32 PERQDATA_GetCaptureWnd( PERQUEUEDATA *pQData );
HWND32 PERQDATA_SetCaptureWnd( PERQUEUEDATA *pQData, HWND32 hWndCapture );
INT16  PERQDATA_GetCaptureInfo( PERQUEUEDATA *pQData );
INT16 PERQDATA_SetCaptureInfo( PERQUEUEDATA *pQData, INT16 nCaptureHT );
    
/* Message queue management methods */
extern MESSAGEQUEUE *QUEUE_Lock( HQUEUE16 hQueue );
extern void QUEUE_Unlock( MESSAGEQUEUE *queue );
extern void QUEUE_DumpQueue( HQUEUE16 hQueue );
extern void QUEUE_WalkQueues(void);
extern BOOL32 QUEUE_IsExitingQueue( HQUEUE16 hQueue );
extern void QUEUE_SetExitingQueue( HQUEUE16 hQueue );
extern MESSAGEQUEUE *QUEUE_GetSysQueue(void);
extern void QUEUE_SetWakeBit( MESSAGEQUEUE *queue, WORD bit );
extern void QUEUE_ClearWakeBit( MESSAGEQUEUE *queue, WORD bit );
extern void QUEUE_ReceiveMessage( MESSAGEQUEUE *queue );
extern void QUEUE_WaitBits( WORD bits );
extern void QUEUE_IncPaintCount( HQUEUE16 hQueue );
extern void QUEUE_DecPaintCount( HQUEUE16 hQueue );
extern void QUEUE_IncTimerCount( HQUEUE16 hQueue );
extern void QUEUE_DecTimerCount( HQUEUE16 hQueue );
extern BOOL32 QUEUE_CreateSysMsgQueue( int size );
extern BOOL32 QUEUE_DeleteMsgQueue( HQUEUE16 hQueue );
extern HTASK16 QUEUE_GetQueueTask( HQUEUE16 hQueue );
extern BOOL32 QUEUE_AddMsg( HQUEUE16 hQueue, MSG32 * msg, DWORD extraInfo );
extern QMSG* QUEUE_FindMsg( MESSAGEQUEUE * msgQueue, HWND32 hwnd,
                          int first, int last );
extern void QUEUE_RemoveMsg( MESSAGEQUEUE * msgQueue, QMSG *qmsg );
extern void QUEUE_FlushMessages(HQUEUE16);
extern void hardware_event( WORD message, WORD wParam, LONG lParam,
			    int xPos, int yPos, DWORD time, DWORD extraInfo );

extern HQUEUE16 WINAPI InitThreadInput( WORD unknown, WORD flags );

#endif  /* __WINE_QUEUE_H */
