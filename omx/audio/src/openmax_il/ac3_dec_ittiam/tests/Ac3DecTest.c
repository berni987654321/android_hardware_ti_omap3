
/*****************************************************************************/
/*                                                                           */
/*                             AC3 Decoder                                   */
/*                                                                           */
/*                   ITTIAM SYSTEMS PVT LTD, BANGALORE                       */
/*                          COPYRIGHT(C) 2009                                */
/*                                                                           */
/*  This program is proprietary to Ittiam Systems Pvt. Ltd. and is protected */
/*  under Indian Copyright Act as an unpublished work.Its use and disclosure */
/*  is  limited by  the terms and conditions of a license  agreement. It may */
/*  be copied or  otherwise reproduced or  disclosed  to persons outside the */
/*  licensee 's  organization  except  in  accordance  with  the  terms  and */
/*  conditions of  such an agreement. All  copies and reproductions shall be */
/*  the  property  of Ittiam Systems Pvt.  Ltd. and  must  bear  this notice */
/*  in its entirety.                                                         */
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/
/*                                                                           */
/*  File Name        : Ac3DecTest.c                                          */
/*                                                                           */
/*  Description      :                                                       */
/*                                                                           */
/*  List of Functions: None                                                  */
/*                                                                           */
/*  Issues / Problems: None                                                  */
/*                                                                           */
/*  Revision History :                                                       */
/*                                                                           */
/*****************************************************************************/

#ifdef UNDER_CE
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <sys/select.h>
#include <pthread.h>
#include <linux/vt.h>
#include <signal.h>
#include <sys/stat.h>
#include <linux/soundcard.h>
#endif

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include <OMX_Index.h>
#include <OMX_Types.h>
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Audio.h>
#include <TIDspOmx.h>
#include <stdio.h>
#ifdef DSP_RENDERING_ON
#include <AudioManagerAPI.h>
#endif
#ifdef OMX_GETTIME
#include <OMX_Common_Utils.h>
#include <OMX_GetTime.h>
#endif
/*
 *     M A C R O S
 */


#include <OMX_IttiamAc3Dec_Utils.h>


#undef APP_DEBUG
#define APP_INFO
#define APP_ERROR
#define DASF
#define USE_BUFFER
#undef AC3DEC_DEBUGMEM\


/*#define GT_PERFM  *//*Defines the Performance and measurements mode*/
/*#undef GT_PERFM Defines the Performance and measurements mode*/

#ifdef  APP_INFO
        #define APP_IPRINT(...)    fprintf(stderr,__VA_ARGS__)			/* Information prints */
#else
        #define APP_IPRINT(...)
#endif


#ifdef  APP_ERROR
        #define APP_EPRINT(...)    fprintf(stderr,__VA_ARGS__)			/* errors & warnings prints */
#else
        #define APP_EPRINT(...)
#endif


#ifdef  APP_DEBUG
        #define APP_DPRINT(...)    fprintf(stderr,__VA_ARGS__)			/* Debug prints */
#else
        #define APP_DPRINT(...)
#endif

#ifdef OMX_GETTIME
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  int GT_FlagE = 0;  /* Fill Buffer 1 = First Buffer,  0 = Not First Buffer  */
  int GT_FlagF = 0;  /*Empty Buffer  1 = First Buffer,  0 = Not First Buffer  */
  static OMX_NODE* pListHead = NULL;
#endif


#ifdef APP_MEMCHECK
    #define APP_MEMPRINT(...)    fprintf(stderr,__VA_ARGS__)
#else
    #define APP_MEMPRINT(...)
#endif

#define MONO 			 1
#define STEREO 			 2
#define SLEEP_TIME  	 10
#define INPUT_PORT  	 0
#define OUTPUT_PORT 	 1
#define MAX_NUM_OF_BUFS  5
#define INPUT_AC3DEC_BUFFER_SIZE 3840*10
#define FIFO1 "/dev/fifo.1"
#define FIFO2 "/dev/fifo.2"

#define Min_32Kbps 		 32000
#define Max_640Kbps 	 640000
#define Min_volume 		 0										/* Minimum volume 					*/
#define Act_volume 		 50										/* Current volume 					*/
#define Max_volume 		 100

#define Max_48Kbps		 48000
#define Max_44Kbps		 44100
#define Max_32Kbps		 32000

//#define ObjectTypeLC	 2
//#define ObjectTypeHE	 5
//#define ObjectTypeHE2	 29

#define BITS16			 16										/* unmasking  command line parameter */
#define BITS24			 24										/* unmasking  command line parameter */
#define Upto48kHz		 48000

#define newmalloc(x) mymalloc((x),(&ListHeader))				/* new prototype of malloc function 	*/
#define newfree(z) myfree((z),(&ListHeader))					/* new prototype of free function 		*/


#undef  WAITFORRESOURCES

#ifdef AC3DEC_DEBUGMEM
void *arr[500];
int lines[500];
int bytes[500];
char file[500][50];
int r=0;
#endif

/*
 *    TYPE DEFINITIONS
 */

/* Structure for Linked List */
typedef struct DataList ListMember;
struct DataList
{
	int ListCounter;											/* Instance Counter 					*/
	void* Struct_Ptr;											/* Pointer to new alocate data 		*/
	ListMember* NextListMember;									/* Pointer to next instance	 		*/
};

typedef enum COMPONENTS
{
    COMP_1,
    COMP_2
}COMPONENTS;

/* Ouput file format */
typedef enum FILE_FORMAT
{
    RAW = 0,
    ADIF,
	ADTS
}FILE_FORMAT;

/*Structure for Wait for state sync */
typedef struct
{
	OMX_BOOL WaitForStateFlag;									/* flag  				 */
	pthread_cond_t  cond; 										/* conditional mutex 	*/
	pthread_mutex_t Mymutex;									/* Mutex 			*/
}Mutex;


/*
 *    FUNTIONS DECLARATION
 */

OMX_ERRORTYPE AddMemberToList(void* ptr, ListMember** ListHeader);

OMX_ERRORTYPE FreeListMember(void* ptr, ListMember** ListHeader);

OMX_ERRORTYPE CleanList(ListMember** ListHeader);

void * mymalloc(int size, ListMember** ListHeader );

int myfree(void *dp, ListMember** ListHeader);

static OMX_ERRORTYPE FreeAllResources( OMX_HANDLETYPE pHandle,
			                OMX_BUFFERHEADERTYPE* pBufferIn,
			                OMX_BUFFERHEADERTYPE* pBufferOut,
			                int NIB, int NOB,
			                FILE* fIn, FILE* fOut,
			                ListMember* ListHeader);

#ifdef USE_BUFFER
static OMX_ERRORTYPE  freeAllUseResources(OMX_HANDLETYPE pHandle,
						  OMX_U8* UseInpBuf[],
						  OMX_U8* UseOutBuf[],
						  int NIB, int NOB,
						  FILE* fIn, FILE* fOut,
						  ListMember* ListHeader );

#endif



/*
 *   GLOBAL VARIBLES
 */

int IpBuf_Pipe[2];
int OpBuf_Pipe[2];
int Event_Pipe[2];

int channel = 0;
int ObjectType=0;

int preempted = 0;
int firstbuffer = 1;

#ifdef DSP_RENDERING_ON
AM_COMMANDDATATYPE cmd_data;
#endif
OMX_STRING strAc3Decoder = "OMX.ITTIAM.AC3.decode";


/* flag for Invalid State condition*/
static OMX_BOOL bInvalidState;

/* New instance of Mutex Structure */
Mutex WaitForStateMutex={OMX_TRUE,PTHREAD_COND_INITIALIZER,PTHREAD_MUTEX_INITIALIZER};

Mutex WaitForCommandMutex={OMX_TRUE,PTHREAD_COND_INITIALIZER,PTHREAD_MUTEX_INITIALIZER};

/* Flags for mutex control */
OMX_BOOL bWaiting = OMX_FALSE;
OMX_BOOL bWaiting_cmd = OMX_FALSE;

OMX_STATETYPE waiting_state=OMX_StateInvalid;
OMX_U32 command_flag = 0;

OMX_COMMANDTYPE waiting_command=0;

/* Flag to stop component */
OMX_BOOL bPlayCompleted = OMX_FALSE;
OMX_BOOL outputportreconfig = OMX_FALSE;

FILE* fOut= NULL;										/* ouput File pointer 							 */
int nFrameCount = 0;


/*
 *   FUNCTIONS DEFINITION
 */


/*-------------------------------------------------------------------*/
/**
  * maxint()  Returns the biggest from two number
  *
  *  @param a					First Integer number
  *  @param  b					Second Integer number
  *
  * @retval 						The biggest number
  *
  **/
/*-------------------------------------------------------------------*/
int maxint(int a, int b)
{
   return (a>b) ? a : b;
}


/*-------------------------------------------------------------------*/
/**
  * WaitForState()  Waits for signal state transition if  the transition has not ocurred
  *
  *  @param pHandle					Component pointer
  *		      DesiredState				State to wait
  *
  * @retval OMX_ErrorNone   			Success on Transition
  *              OMX_StateInvalid		 	Wrong transition
  **/
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE WaitForState(OMX_HANDLETYPE pHandle,
                                OMX_STATETYPE DesiredState)
{
     OMX_STATETYPE CurState = OMX_StateInvalid;
     OMX_ERRORTYPE eError   = OMX_ErrorNone;

	 APP_DPRINT("%d: APP: waiting for %d \n",__LINE__,DesiredState);
     eError = OMX_GetState(pHandle, &CurState);
	 if(eError !=OMX_ErrorNone)
	 {
		APP_EPRINT("App: Error in GetState from WaitForState() \n" );
		goto EXIT;
	 }

	 APP_DPRINT("%d: APP: CurState : %d, DesiredState : %d \n",__LINE__,CurState, DesiredState);
	 if( waiting_state == DesiredState )
	 {
		 CurState = DesiredState ; //just in case we missed the event notification
		 waiting_state = -1;
	 }
   	 if (CurState != DesiredState)
   	 {
   		 bWaiting	   = OMX_TRUE;													/*	flag is enable since now we have to wait to the event */
   		 waiting_state = DesiredState;
		 APP_DPRINT("Now is waiting.... bWaiting:%d\n",bWaiting);

   		 pthread_mutex_lock(&WaitForStateMutex.Mymutex);
   		 pthread_cond_wait(&WaitForStateMutex.cond,&WaitForStateMutex.Mymutex);		/*  Block on a Condition Variable"  */
   		 pthread_mutex_unlock( &WaitForStateMutex.Mymutex);
   	 }
   	 else if(CurState == DesiredState)
   	 {
		APP_DPRINT("...No need to wait \n");
   		 eError = OMX_ErrorNone;
   	 }


EXIT:
	return eError;
}


/*-------------------------------------------------------------------*/
/**
  * WaitForState()  Waits for signal state transition if  the transition has not ocurred
  *
  *  @param pHandle					Component pointer
  *		      DesiredState				State to wait
  *
  * @retval OMX_ErrorNone   			Success on Transition
  *              OMX_StateInvalid		 	Wrong transition
  **/
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE WaitForCommand(OMX_HANDLETYPE pHandle,
                                OMX_COMMANDTYPE command)
{

     OMX_ERRORTYPE eError   = OMX_ErrorNone;

	 APP_DPRINT("%d: APP: waiting for %d \n",__LINE__,command);

	 if(waiting_command == command)
	     command_flag = 1;
	 waiting_command = command;

   	 if (!command_flag)
   	 {
   		 bWaiting_cmd	   = OMX_TRUE;													/*	flag is enable since now we have to wait to the event */
   		 waiting_command = command;
		 APP_DPRINT("Now is waiting.... \n");

   		 pthread_mutex_lock(&WaitForCommandMutex.Mymutex);
   		 pthread_cond_wait(&WaitForCommandMutex.cond,&WaitForCommandMutex.Mymutex);		/*  Block on a Condition Variable"  */
   		 pthread_mutex_unlock( &WaitForCommandMutex.Mymutex);
   	 }
   	 else
   	 {
		APP_DPRINT("...No need to wait \n");
   		 eError = OMX_ErrorNone;
   		 //Reset the command_flag
   		 command_flag = 0;
   	 }


EXIT:
	return eError;
}

/*-------------------------------------------------------------------*/
/**
  * EventHandler()  Event Callback from Component. Method to notify when an event of
  *				   interest occurs within the component.
  *
  *  @param hComponent				The handle of the component that is calling this function.
  *			pAppData				additional event-specific data.
  *			eEvent					The event that the component is communicating
  *			nData1					The first integer event-specific parameter.
  *			nData2					The second integer event-specific parameter.
  *
  * @retval OMX_ErrorNone   				Success, Event Gotten
  *              OMX_ErrorBadParameter		Error on parameters
  **/
/*-------------------------------------------------------------------*/

OMX_ERRORTYPE EventHandler(OMX_HANDLETYPE hComponent,
				  OMX_PTR pAppData,
                  OMX_EVENTTYPE eEvent,
                  OMX_U32 nData1,
                  OMX_U32 nData2,
                  OMX_PTR pEventData)
{

   APP_DPRINT( "%d :: App: Entering EventHandler \n", __LINE__);
   OMX_STATETYPE state;
   OMX_ERRORTYPE eError = OMX_ErrorNone;

   OMX_U8 writeValue;

#ifdef  APP_DEBUG
   int iComp = *((int *)(pAppData));
#endif

   eError = OMX_GetState (hComponent, &state);
   if(eError != OMX_ErrorNone)
   {
       APP_DPRINT("%d :: App: Error returned from GetState\n",__LINE__);
	   goto EXIT;
   }

   APP_DPRINT( "%d :: App: Component eEvent = %d\n", __LINE__,eEvent);
   switch (eEvent) {
       case OMX_EventCmdComplete:
		APP_DPRINT ("%d :: App: Event Command Complete bWaiting:%d \n", __LINE__,bWaiting);

	   if( (waiting_state== nData2) && (nData1 == OMX_CommandStateSet) && (bWaiting) )		/* ensure that came from transition */
		   {
					APP_DPRINT ("%d :: App: Inside If %d\n", __LINE__);
					bWaiting = OMX_FALSE;
					pthread_mutex_lock(&WaitForStateMutex.Mymutex);
					pthread_cond_signal(&WaitForStateMutex.cond);								/* Unblock a Specific Thread" */
					APP_DPRINT("App: Mutex signal sent \n");
					pthread_mutex_unlock( &WaitForStateMutex.Mymutex);							/*  unlock.  */
					APP_DPRINT ("%d :: App: Mutex unlocked %d\n", __LINE__);
					APP_DPRINT ("%d :: App: Component State Changed To %d\n", __LINE__,state);
		   }

		if( bWaiting_cmd)
		{
		  if(nData1 == OMX_CommandFlush)
			 APP_DPRINT ("%d :: App: Event for OMX_CommandFlush %d\n", __LINE__,nData1);
		 else if(nData1 == OMX_CommandPortDisable)
			 APP_DPRINT ("%d :: App: Event for OMX_CommandPortDisable %d\n", __LINE__,nData1);
		 else if(nData1 == OMX_CommandPortEnable)
			APP_DPRINT ("%d :: App: Event for OMX_CommandPortEnable %d\n", __LINE__,nData1);

		APP_DPRINT ("%d :: App: bWaiting_cmd  %d\n", __LINE__,bWaiting_cmd);


	  if( (waiting_command== nData1) && (bWaiting_cmd) )		/* ensure that came from transition */
		   {
					bWaiting_cmd = OMX_FALSE;
					command_flag = 1;
					pthread_mutex_lock(&WaitForCommandMutex.Mymutex);
					pthread_cond_signal(&WaitForCommandMutex.cond);								/* Unblock a Specific Thread" */
					APP_DPRINT("App: Mutex signal sent \n");
					pthread_mutex_unlock( &WaitForCommandMutex.Mymutex);							/*  unlock.  */

					APP_DPRINT ("%d :: App: Component Command completed To %d\n", __LINE__,state);
		   }
	   }

		   waiting_command = nData1;
		   waiting_state = nData2;
		   APP_DPRINT(	"%d :: App: OMX_EventCmdComplete %d\n", __LINE__,eEvent);
           break;
       case OMX_EventError:
           if (nData1 != OMX_ErrorNone)
		   {
               APP_DPRINT ("%d:: App: ErrorNotofication Came: \
                           Component Name : %d : Error Num %x \n",
                           __LINE__,iComp, (unsigned int)nData1);
           }
		   if (nData1 == OMX_ErrorInvalidState) {
		   		bInvalidState =OMX_TRUE;
		   }
		   else if(nData1 == OMX_ErrorResourcesPreempted) {
            preempted=1;
            writeValue = 0;
            write(Event_Pipe[1], &writeValue, sizeof(OMX_U8));
	       }

	       else if (nData1 == OMX_ErrorResourcesLost) {
	            bWaiting = 0;
	            pthread_mutex_lock(&WaitForStateMutex.Mymutex);
	            pthread_cond_signal(&WaitForStateMutex.cond);/*Sending Waking Up Signal*/
	            pthread_mutex_unlock(&WaitForStateMutex.Mymutex);
	        }

           break;
       case OMX_EventMax:
		   APP_DPRINT( "%d :: App: Component OMX_EventMax = %d\n", __LINE__,eEvent);
           break;
       case OMX_EventMark:
		   APP_DPRINT( "%d :: App: Component OMX_EventMark = %d\n", __LINE__,eEvent);
           break;
       case OMX_EventPortSettingsChanged:
		   APP_DPRINT( "%d :: App: Component OMX_EventPortSettingsChanged = %d\n", __LINE__,eEvent);

			if(nData1 == OUTPUT_PORT)
			{
			APP_DPRINT( "%d :: App: Setting outputportreconfig to = %d\n", __LINE__,outputportreconfig);
			outputportreconfig = 1;
		    }


       	   break;
       case OMX_EventBufferFlag:
		   APP_DPRINT( "%d :: App: Component OMX_EventBufferFlag = %d\n", __LINE__,eEvent);
		   APP_DPRINT( "%d :: App: OMX_EventBufferFlag on port = %d\n", __LINE__, (int)nData1);
		   /* event for the input port to stop component  , since there is one for output port */
		   if(nData1 == OUTPUT_PORT && nData2 == OMX_BUFFERFLAG_EOS)
		   {
		   		bPlayCompleted = OMX_TRUE;
			    APP_DPRINT( "%d :: App: Setting flag for playcompleted \n", __LINE__);
		   }

		   #ifdef WAITFORRESOURCES
		   writeValue = 2;
	       write(Event_Pipe[1], &writeValue, sizeof(OMX_U8));
		   #endif

       	   break;
       case OMX_EventResourcesAcquired:
		   APP_DPRINT( "%d :: App: Component OMX_EventResourcesAcquired = %d\n", __LINE__,eEvent);
           writeValue = 1;
           write(Event_Pipe[1], &writeValue, sizeof(OMX_U8));
           preempted=0;

       	   break;
	   default:
		   break;
   }
EXIT:
	APP_DPRINT( "%d :: App: Exiting EventHandler \n", __LINE__);
	return eError;

}


/*-------------------------------------------------------------------*/
/**
  * FillBufferDone()  Callback from component which returns an ouput buffer
  *
  *  @param hComponent				The handle of the component that is calling this function.
  *			pBuffer					Returned output buffer
  *			ptr						A pointer to IL client-defined data
  *
  *
  * @retval  None
  *
  **/
/*-------------------------------------------------------------------*/

void FillBufferDone (OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer)
{
	APP_DPRINT("%d:: APP: Entering FillBufferDone:%d \n",__LINE__);

	if(outputportreconfig == 0)
    write(OpBuf_Pipe[1], &pBuffer, sizeof(pBuffer));

    if(outputportreconfig == 1)
    {
		APP_DPRINT("%d:: APP: Dumping op to file in case of port reconfig :bytes:%d \n",__LINE__, pBuffer->nFilledLen);

    	//Write output to the file
    	if(pBuffer->nFilledLen > 0 )
    	{
    	fwrite (pBuffer->pBuffer, 1, (pBuffer->nFilledLen), fOut);
    	nFrameCount++;
    	APP_DPRINT("%d:: APP: Dumped op to file  :bytes:%d \n",__LINE__, pBuffer->nFilledLen);
	}


	}


   #ifdef OMX_GETTIME
      if (GT_FlagF == 1 ) /* First Buffer Reply*/  /* 1 = First Buffer,  0 = Not First Buffer  */
      {
        GT_END("Call to FillBufferDone  <First: FillBufferDone>");
        GT_FlagF = 0 ;   /* 1 = First Buffer,  0 = Not First Buffer  */
      }
    #endif
}


/*-------------------------------------------------------------------*/
/**
  * EmptyBufferDone()  Callback from component which returns an input buffer
  *
  *  @param hComponent				The handle of the component that is calling this function.
  *			pBuffer					Returned input buffer
  *			ptr						A pointer to IL client-defined data
  *
  *
  * @retval  None
  *
  **/
/*-------------------------------------------------------------------*/

void EmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer)
{
   APP_DPRINT("%d:: APP: Inside empty buffer done \n",__LINE__);

	if (!preempted)
	   write(IpBuf_Pipe[1], &pBuffer, sizeof(pBuffer));

   	#ifdef OMX_GETTIME
      if (GT_FlagE == 1 ) /* First Buffer Reply*/  /* 1 = First Buffer,  0 = Not First Buffer  */
      {
        GT_END("Call to EmptyBufferDone <First: EmptyBufferDone>");
       GT_FlagE = 0;   /* 1 = First Buffer,  0 = Not First Buffer  */
      }
   #endif
}



/*-------------------------------------------------------------------*/
/**
  * main()  Test App main function. Function called from the runtime startup routine after
  		   the runtime environment has been initialized
  *
  *  @param argv				Argument vector. Pointer to an array of string pointers passed to  main function
  *		      argc				Holds the number of arguments that are passed.
  *
  *
  *
  * @retval   error 			Return Value to OS
  *
  **/
/*-------------------------------------------------------------------*/



int main(int argc, char* argv[])
{


    struct timeval tv;
    int retval,i =0;
    int frmCount = 0;
	int testcnt  = 1;
	int testcnt1 = 1;
    char fname[20] = "output";
	int nRead = 0;
	int done  = 0;
	int nIpBuffs = 0;
	int nOpBuffs = 0;
	int numofinbuff  = 0;
	int numofoutbuff = 0;
	int frmCnt = 1,chanrout;;
    int status,fdmax,jj,kk,k;

	/* Used for Seek testing */
	int seek_frm_cnt[10] = {10, 30, 45, 59, 98, 156, 202, 356, 784, 999};
	int seek_ptr_cnt[10] = {234646, 376543, 981614, 564332, 686331, 638513, 373198, 865232, 34533, 200101, 673111};
	int seek_counter = 0;

    int framemode = 0;

    OMX_CALLBACKTYPE Ac3CaBa = {(void *)EventHandler,(void*) EmptyBufferDone,
                                                     (void*)FillBufferDone};
    OMX_HANDLETYPE *pHandle = NULL;
    OMX_ERRORTYPE  error = OMX_ErrorNone;
    OMX_U32 AppData = 100;
    OMX_PARAM_PORTDEFINITIONTYPE* pCompPrivateStruct =NULL;
    OMX_AUDIO_PARAM_AC3TYPE *pAc3Param =NULL;
	OMX_AUDIO_PARAM_PCMMODETYPE *iPcmParam =NULL;
    OMX_COMPONENTTYPE *pComponent=NULL;
    OMX_STATETYPE state;
	TI_OMX_DSP_DEFINITION audioinfo;
	OMX_AUDIO_CONFIG_VOLUMETYPE* pCompPrivateStructGain = NULL;
    OMX_BUFFERHEADERTYPE* pInputBufferHeader[MAX_NUM_OF_BUFS];
    OMX_BUFFERHEADERTYPE* pOutputBufferHeader[MAX_NUM_OF_BUFS];
	OMX_INDEXTYPE index;
	//TI_OMX_STREAM_INFO *streaminfo =NULL;
	OMX_U32 OutputBufferSize =0;							/* Calculated size per Ouput buffer                      */
	bInvalidState=OMX_FALSE;								/* flag for invalid state transition                          */
    //TI_OMX_DATAPATH dataPath;
	ListMember* ListHeader = NULL;							/* Linked list Header							 */
	FILE* fIn = NULL;										/* input File pointer 							 */

#ifdef DSP_RENDERING_ON
int Ac3dec_fdwrite;
int Ac3dec_fdread;
#endif

	/*----------------------------------------------
 	First allocated structure
 	----------------------------------------------*/

	/*streaminfo = newmalloc(sizeof(TI_OMX_STREAM_INFO));
	if(NULL == streaminfo)
	{
        APP_EPRINT("%d :: App: Malloc Failed\n",__LINE__);
        goto EXIT;
	}*/

#ifdef USE_BUFFER
    OMX_U8* pInputBuffer[5];
    OMX_U8* pOutputBuffer[5];
#endif

    fd_set rfds;

	APP_IPRINT("------------------------------------------------------------\n");
    APP_IPRINT("This is Main Thread In Dolby Ac3 DECODER Test Application:\n");
    APP_IPRINT("Test Core 1.5 - " __DATE__ ":" __TIME__ "\n");
    APP_IPRINT("------------------------------------------------------------\n");

#ifdef OMX_GETTIME
    APP_IPRINT("Line %d\n",__LINE__);
      GTeError = OMX_ListCreate(&pListHead);
        APP_IPRINT("Line %d\n",__LINE__);
      APP_IPRINT("eError = %d\n",GTeError);
      GT_START();
  APP_IPRINT("Line %d\n",__LINE__);
#endif

#ifdef USE_BUFFER
	APP_IPRINT("                     Use Buffer Enabled\n");
	APP_IPRINT("------------------------------------------------------------\n");
#endif

	/* check to see the number of parameters from the command line */
    if(  (argc < 8) || (argc > 25)  )
	{

        APP_EPRINT("Usage:   test: [INFILE] [OUTFILE] [FRAME_MODE:0/1] [TESTCASE] [PCM WIDTH:16/24] [IP_BUF] [OUT_BUF]\n");
        APP_EPRINT("[KARAOKE_MODE] [DYNRANGE_MODE] [LFE CHANNEL] [OUT_CHAN_CONFIG] [NUM_CHAN] [PCM_SCALEFACTOR] [STEREO_OUT_MODE] [DUAL_MONOMODE]\n");
        APP_EPRINT("[DYNRANGE_CUT_FACTOR] [DYNRANGE_BOOST_FACTOR] [CHANNEL ROUTING INFORMATION][ROBUSTNESS]\n");
	  	APP_EPRINT("KARAOKE_MODE = Karaoke mode (default 3)\n");
        APP_EPRINT("                  0 = no vocal\n");
        APP_EPRINT("                  1 = left vocal\n");
        APP_EPRINT("                  2 = right vocal\n");
        APP_EPRINT("                  3 = both vocals\n");
	  	APP_EPRINT("DYNRANGE_MODE   = Dynamic range compression mode (default 2)\n");
        APP_EPRINT("                  0 = custom mode, analog dialnorm\n");
        APP_EPRINT("                  1 = custom mode, digital dialnorm\n");
        APP_EPRINT("                  2 = line out mode\n");
        APP_EPRINT("                  3 = RF remod mode\n");
	  	APP_EPRINT("LFE CHANNEL  = Output lfe channel present (default 1)\n");
	  	APP_EPRINT("OUT_CHAN_CONFIG = Output channel configuration (default 7)\n");
        APP_EPRINT("                  0 = reserved\n");
        APP_EPRINT("                  1 = 1/0 (C)\n");
        APP_EPRINT("                  2 = 2/0 (L, R)\n");
        APP_EPRINT("                  3 = 3/0 (L, C, R)");
	  	APP_EPRINT("                  4 = 2/1 (L, R, l)\n");
        APP_EPRINT("                  5 = 3/1 (L, C, R, l)\n");
        APP_EPRINT("                  6 = 2/2 (L, R, l, r)\n");
        APP_EPRINT("                  7 = 3/2 (L, C, R, l, r)\n");
	  	APP_EPRINT("NUM_CHAN = Number of output channels (default 6)\n");
	  	APP_EPRINT("PCM_SCALEFACTOR = PCM scale factor (default 1.0)\n");
	  	APP_EPRINT("STEREO_OUT_MODE = Stereo output mode (default 0)\n");
        APP_EPRINT("                Only effective when OUT_CHAN_CONFIG = 2\n");
        APP_EPRINT("                  0 = auto detect\n");
        APP_EPRINT("                  1 = Dolby Surround compatible (Lt/Rt)\n");
        APP_EPRINT("                  2 = Stereo (Lo/Ro)\n");
	  	APP_EPRINT("DUAL_MONOMODE = Dual mono reproduction mode (default 0)\n");
	  	APP_EPRINT("                  0 = Stereo\n");
        APP_EPRINT("                  1 = Left mono\n");
        APP_EPRINT("                  2 = Right mono\n");
        APP_EPRINT("                  3 = Mixed mono\n");
	  	APP_EPRINT("DYNRANGE_CUT_FACTOR = Dynamic range compression cut scale factor (default 1.0)\n");
	  	APP_EPRINT("DYNRANGE_BOOST_FACTOR = Dynamic range compression boost scale factor (default 1.0)\n");
	  	APP_EPRINT("CHANNEL ROUTING INFORMATION = 0..5  Channel routing information\n");
        APP_EPRINT("                  Route arbitrary input channels (0 = L, 1 = C, 2 = R, 3 = l, 4 = r, 5 = s) to\n");
        APP_EPRINT("                  arbitrary interleaved output channel (0..5).\n");
        APP_EPRINT("                  Example: 0 1 routes left bitstream channel to first\n");
        APP_EPRINT("                  interleaved output channel, and routes right bitstream\n");
        APP_EPRINT("                  channel to second interleaved output channel.\n");
        APP_EPRINT("                  Note: use l to designate mono surround in 2/1 or 3/1 modes,\n");
        APP_EPRINT("                  use L and R to designate independent channels in 1+1 mode.\n");
        APP_EPRINT("                  Default: 0 1 2 3 4 5\n");
        APP_EPRINT("Example: AC3Decoder_Test ip.ac3 op.pcm 1 5 16 1 1 0 3 2 6 32768 0 0 32768 32768 0 1 2 3 4 5\n");
        goto EXIT;
    }

    /* check to see that the input file exists */
    struct stat sb = {0};
    status = stat(argv[1], &sb);
    if( status != 0 )
	{
        APP_EPRINT("%d :: App: Cannot find file %s. (%u)\n",__LINE__, argv[1], errno);
        goto EXIT;
    }

	/* check to see the test case number */
    switch (atoi(argv[4]))
	{
		case 1:
			APP_IPRINT ("---------------------------------------------\n");
			APP_IPRINT ("Testing Simple Playout till Predefined frames \n");
			APP_IPRINT ("---------------------------------------------\n");
			break;
		case 2:
			APP_IPRINT ("---------------------------------------------\n");
			APP_IPRINT ("Testing Stop After Playout \n");
			APP_IPRINT ("---------------------------------------------\n");
			break;
		case 3:
			APP_IPRINT ("---------------------------------------------\n");
			APP_IPRINT ("Testing PAUSE & RESUME Command\n");
			APP_IPRINT ("---------------------------------------------\n");
			break;
		case 4:
			APP_IPRINT ("-------------------------------------------------\n");
			APP_IPRINT ("Testing Repeated PLAY without Deleting Component\n");
			APP_IPRINT ("-------------------------------------------------\n");
			strcat (fname,"_tc5.pcm");
			if( (argc == 25) )
			{
				if(!strcmp("ROBUST",argv[24]))
				{
					APP_IPRINT("%d :: APP: AC3: 100 Iterations - ROBUSTNESS Test mode\n",__LINE__);
					testcnt = 100;
				}
			}
			else
				testcnt = 20;
			break;
		case 5:
			APP_IPRINT ("------------------------------------------------\n");
			APP_IPRINT ("Testing Repeated PLAY with Deleting Component\n");
			APP_IPRINT ("------------------------------------------------\n");
			strcat (fname,"_tc6.pcm");
			if( (argc == 25))
			{
				if(!strcmp("ROBUST",argv[24]))
				{
					APP_IPRINT("%d :: APP: AC3: 100 Iterations - ROBUSTNESS Test mode\n",__LINE__);
					testcnt1 = 100;
				}
			}
			else
				testcnt1 = 20;
			break;
		case 6:
			APP_IPRINT ("-------------------------------------\n");
			APP_IPRINT ("Testing Stop and Play \n");
			APP_IPRINT ("-------------------------------------\n");
			strcat(fname,"_tc7.pcm");
			testcnt = 2;
			break;
		case 7:
			APP_IPRINT ("-------------------------------------\n");
			APP_IPRINT ("VOLUME \n");
			APP_IPRINT ("-------------------------------------\n");
			break;
		case 8:
			APP_IPRINT ("-------------------------------------\n");
			APP_IPRINT ("SEEK TEST \n");
			APP_IPRINT ("-------------------------------------\n");
			break;
    }

	/*Opening INPUT FILE */
	fIn = fopen(argv[1], "r");
    if(fIn == NULL)
	{
        APP_EPRINT("APP: Error:  failed to open the file %s for readonly access\n", argv[1]);
        goto EXIT;
    }

	/*Opening  OUTPUT FILE */
    fOut = fopen(argv[2], "w");
    if(fOut == NULL)
	{
        APP_EPRINT("APP: Error:  failed to create the output file %s\n", argv[2]);
        goto EXIT;
    }

	APP_DPRINT("%d :: APP: AC3 Decoding Test --- first create file handle\n",__LINE__);
	APP_DPRINT("%d :: APP: AC3 Decoding Test --- fIn = [%p]\n", __LINE__, fIn);
	APP_DPRINT("%d :: APP: AC3 Decoding Test --- fOut = [%p]\n",__LINE__, fOut);

	/* check to see the STEREO/MONO mode */
	if(argc>=13)
		channel = atoi(argv[12]);
	else
		channel = 6;

/*----------------------------------------------
 Main Loop for Deleting component test
 ----------------------------------------------*/
    jj=0;
    APP_IPRINT("%d :: APP: AC3 DEC Test --- will call [%d] time for decoder\n",__LINE__, jj+1);
    for(jj=0; jj<testcnt1; jj++)
	{

		if ( atoi(argv[4])== 5)
		{
			APP_IPRINT ("***************************************\n");
			APP_IPRINT ("%d :: TC-5 counter = %d\n",__LINE__,jj);
			APP_IPRINT ("***************************************\n");
		}

#ifdef DSP_RENDERING_ON
		if((Ac3dec_fdwrite=open(FIFO1,O_WRONLY))<0) {
	        APP_EPRINT("%d :: APP: - failure to open WRITE pipe\n",__LINE__);
	    }
	    else {
	        APP_DPRINT("%d :: APP: - opened WRITE pipe\n",__LINE__);
	    }

	    if((Ac3dec_fdread=open(FIFO2,O_RDONLY))<0) {
	        APP_EPRINT("%d :: APP: - failure to open READ pipe\n",__LINE__);
	        goto EXIT;
	    }
	    else {
	        APP_DPRINT("%d :: APP: - opened READ pipe\n",__LINE__);
	    }
#endif
	    /* Create a pipe used to queue data from the callback. */
        retval = pipe(IpBuf_Pipe);
        if( retval != 0)
		{
            APP_EPRINT("App: Error: Fill Data Pipe failed to open\n");
            goto EXIT;
		}

        retval = pipe(OpBuf_Pipe);
        if( retval != 0)
		{
            APP_EPRINT( "App: Error: Empty Data Pipe failed to open\n");
            goto EXIT;
		}
		APP_DPRINT("%d :: APP: - Created OpBuf_Pipe, fd[0] fd[1]:%d %d\n",__LINE__,OpBuf_Pipe[0],OpBuf_Pipe[1]);

		retval = pipe(Event_Pipe);
    	if( retval != 0) {
		    APP_DPRINT( "Error:Empty Data Pipe failed to open\n");
		    goto EXIT;
	    }


		/* save off the "max" of the handles for the selct statement */
        fdmax = maxint(IpBuf_Pipe[0], OpBuf_Pipe[0]);
		fdmax = maxint(fdmax,Event_Pipe[0]);

        error = TIOMX_Init();
        if(error != OMX_ErrorNone)
		{
            APP_EPRINT("%d :: APP: Error returned by OMX_Init()\n",__LINE__);
            goto EXIT;
		}

		if(fIn == NULL)
		{
			fIn = fopen(argv[1], "r");
			if( fIn == NULL )
			{
				APP_EPRINT("App: Error:  failed to open the file %s for readonly access\n", argv[1]);
				goto EXIT;
			}
		}
		if(fOut == NULL)
		{
			fOut = fopen(fname, "w");
			if( fOut == NULL )
			{
				APP_EPRINT("App: Error:  failed to create the output file %s\n", argv[2]);
				goto EXIT;
			}
		}

		/*Component handler */
		pHandle = newmalloc(sizeof(OMX_HANDLETYPE));
        if(NULL == pHandle)
		{
            APP_EPRINT("%d :: App: Malloc Failed\n",__LINE__);
            goto EXIT;
		}
        APP_IPRINT("%d :: App: pHandle = %p\n",__LINE__,pHandle);
#ifdef OMX_GETTIME
	    GT_START();
	    error = OMX_GetHandle(pHandle,strAc3Decoder,&AppData, &Ac3CaBa);
	    GT_END("Call to GetHandle");
#else
	    error = TIOMX_GetHandle(pHandle,strAc3Decoder,&AppData, &Ac3CaBa);
#endif
		if( (error != OMX_ErrorNone) || (*pHandle == NULL) )
		{
            APP_EPRINT("%d :: App: Error in Get Handle function %d \n",__LINE__,error);
			goto EXIT;
		}
		APP_DPRINT("%d :: APP: GetHandle Done..........\n",__LINE__);


		/* Check to see the F2F or DASF mode */
	    //audioinfo.dasfMode = atoi(argv[5]);
	    audioinfo.dasfMode = 0;
		{
		    APP_IPRINT("%d :: APP: AC3 Decoding in FILE MODE\n",__LINE__);
		}

		/* Setting No. of Input and Output Buffers for the Component */
	    numofinbuff = atoi(argv[6]);
		numofoutbuff = atoi(argv[7]);

		numofoutbuff = 4;

		/* Ensuring the propper value of input buffers for DASF mode : Should be 0 */
		{

			if( (numofinbuff < 0) || (numofinbuff > 4) ){
				APP_EPRINT ("%d :: App: ERROR: Input buffers value incorrect (0-4) \n",__LINE__);
				goto EXIT;
			}

			if( (numofoutbuff< 0) || (numofoutbuff > 6) ){
				APP_EPRINT ("%d :: App: ERROR: Output buffers value incorrect (0-4) \n",__LINE__);
				goto EXIT;
			}

			/* Ensuring the propper value of buffers for STEREO mode */
			/*if((channel == 6) && (numofoutbuff != 6))
			{
				APP_EPRINT ("%d :: App: WARNING: 5.1 should use 6 output buffers \n",__LINE__);
				APP_EPRINT ("%d :: App: WARNING: Changing the number of output buffers to 6 \n",__LINE__);
				numofoutbuff = 7;
			}*/
	    }
		APP_DPRINT("\n%d :: App: numofinbuff = %ld \n",__LINE__, (long int)numofinbuff);
	    APP_DPRINT("\n%d :: App: numofoutbuff = %ld \n",__LINE__, (long int)numofoutbuff);

		pCompPrivateStruct = newmalloc (sizeof (OMX_PARAM_PORTDEFINITIONTYPE));
        if(NULL == pCompPrivateStruct)
		{
           APP_DPRINT("%d :: APP: Malloc Failed\n",__LINE__);
           error = OMX_ErrorInsufficientResources;
           goto EXIT;
		}

	    /*Calculating an optimun size of Ouput  buffer according to number of frames*/
		OutputBufferSize = 18432*10;   /*Sample frec,  Bit rate , frames*/

		framemode = argv[3];
		/* getting index for  framemode */
		error = OMX_GetExtensionIndex(*pHandle, "OMX.ITTIAM.index.config.ac3dec.datapath",&index);
	    if (error != OMX_ErrorNone)
		{
		    APP_DPRINT("%d :: APP: Error getting extension index\n",__LINE__);
		    goto EXIT;
		}
		/* Setting the Frame mode to component */
		error = OMX_SetConfig (*pHandle, index, &framemode);
        if(error != OMX_ErrorNone)
		{
            error = OMX_ErrorBadParameter;
            APP_DPRINT("%d :: APP: Error from OMX_SetConfig() function\n",__LINE__);
            goto EXIT;
		}


		/* Setting INPUT port */
		pCompPrivateStruct->nPortIndex                        = INPUT_PORT;
		error = OMX_GetParameter (*pHandle,OMX_IndexParamPortDefinition, pCompPrivateStruct);

		APP_DPRINT("%d :: APP: Setting input port config\n",__LINE__);
		pCompPrivateStruct->nSize                             = sizeof (OMX_PARAM_PORTDEFINITIONTYPE);
		pCompPrivateStruct->nVersion.s.nVersionMajor          = 0xF1;
		pCompPrivateStruct->nVersion.s.nVersionMinor          = 0xF2;
		pCompPrivateStruct->nPortIndex                        = INPUT_PORT;
		pCompPrivateStruct->eDir                              = OMX_DirInput;
		pCompPrivateStruct->nBufferCountActual                = numofinbuff;
		pCompPrivateStruct->nBufferCountMin                   = numofinbuff;
		pCompPrivateStruct->nBufferSize                       = INPUT_AC3DEC_BUFFER_SIZE;
		pCompPrivateStruct->bEnabled                          = OMX_TRUE;
        pCompPrivateStruct->bPopulated                        = OMX_FALSE;
		pCompPrivateStruct->format.audio.eEncoding            = OMX_AUDIO_CodingAC3;
#ifdef OMX_GETTIME
	GT_START();
		error = OMX_SetParameter (*pHandle,OMX_IndexParamPortDefinition, pCompPrivateStruct);
	GT_END("Set Parameter Test-SetParameter");
#else
		error = OMX_SetParameter (*pHandle,OMX_IndexParamPortDefinition, pCompPrivateStruct);
#endif
		if(error != OMX_ErrorNone)
		{
			error = OMX_ErrorBadParameter;
			APP_DPRINT("%d :: APP: OMX_ErrorBadParameter\n",__LINE__);
			goto EXIT;
		}

		/* Setting OUPUT port */
		APP_DPRINT("%d :: APP: Setting output port config\n",__LINE__);
		pCompPrivateStruct->nSize                             = sizeof (OMX_PARAM_PORTDEFINITIONTYPE);
		pCompPrivateStruct->nVersion.s.nVersionMajor          = 0xF1;
		pCompPrivateStruct->nVersion.s.nVersionMinor          = 0xF2;
		pCompPrivateStruct->nPortIndex                        = OUTPUT_PORT;
		pCompPrivateStruct->eDir                              = OMX_DirOutput;
		pCompPrivateStruct->nBufferCountActual                = numofoutbuff;
		pCompPrivateStruct->nBufferCountMin                   = numofoutbuff;
		pCompPrivateStruct->nBufferSize                       = OutputBufferSize;
		pCompPrivateStruct->bEnabled                          = OMX_TRUE;
        pCompPrivateStruct->bPopulated                        = OMX_FALSE;
		pCompPrivateStruct->format.audio.eEncoding            = OMX_AUDIO_CodingPCM;
#ifdef OMX_GETTIME
	GT_START();
	    error = OMX_SetParameter (*pHandle,OMX_IndexParamPortDefinition, pCompPrivateStruct);
	GT_END("Set Parameter Test-SetParameter");
#else
	    error = OMX_SetParameter (*pHandle,OMX_IndexParamPortDefinition, pCompPrivateStruct);
#endif
		if(error != OMX_ErrorNone)
		{
			error = OMX_ErrorBadParameter;
			APP_DPRINT("%d :: APP: OMX_ErrorBadParameter\n",__LINE__);
			goto EXIT;
		}

		iPcmParam = newmalloc (sizeof (OMX_AUDIO_PARAM_PCMMODETYPE));
		if(NULL == iPcmParam)
		{
           APP_DPRINT("%d :: APP: Malloc Failed\n",__LINE__);
           error = OMX_ErrorInsufficientResources;
           goto EXIT;
		}

		/* Setting PCM params */
		iPcmParam->nSize 					= sizeof (OMX_AUDIO_PARAM_PCMMODETYPE);
		iPcmParam->nVersion.s.nVersionMajor = 0xF1;
		iPcmParam->nVersion.s.nVersionMinor = 0xF2;
		iPcmParam->nPortIndex 				= OUTPUT_PORT;
		iPcmParam->nChannels     			= 6; //channel
        iPcmParam->nSamplingRate 			= 32000;

		if (24==argv[5])
		{
			iPcmParam->nBitPerSample 			= BITS24;   /* BitsPerSample; */
		}
		else
		{
			iPcmParam->nBitPerSample 			= BITS16;   /* BitsPerSample; */
		}
#ifdef OMX_GETTIME
	GT_START();
		error = OMX_SetParameter (*pHandle, OMX_IndexParamAudioPcm, iPcmParam);
	GT_END("Set Parameter Test-SetParameter");
#else
		error = OMX_SetParameter (*pHandle, OMX_IndexParamAudioPcm, iPcmParam);
#endif
		if(error != OMX_ErrorNone)
		{
			error = OMX_ErrorBadParameter;
			APP_DPRINT("%d :: APP: OMX_ErrorBadParameter\n",__LINE__);
			goto EXIT;
		}

		pAc3Param = newmalloc (sizeof (OMX_AUDIO_PARAM_AC3TYPE));
		if(NULL == pAc3Param)
		{
           APP_EPRINT("%d :: APP: Malloc Failed\n",__LINE__);
           error = OMX_ErrorInsufficientResources;
           goto EXIT;
		}


		/* Setting AC3 params */
	    	pAc3Param->nSize 					= sizeof (OMX_AUDIO_PARAM_AC3TYPE);
		pAc3Param->nVersion.s.nVersionMajor = 0xF1;
		pAc3Param->nVersion.s.nVersionMinor = 0xF2;
		pAc3Param->nPortIndex 				= INPUT_PORT;


		pAc3Param->nSampleRate 				= 32000;
		pAc3Param->i_output_pcm_fmt			= atoi(argv[5]);

		if(argc>=13)
		{
			pAc3Param->nChannels 				= atoi(argv[12]);

		}
		else
			pAc3Param->nChannels 				= 6;

		if(argc>=9)
		{
			pAc3Param->i_k_capable_mode			= atoi(argv[8]);

		}
		else
			pAc3Param->i_k_capable_mode			= 3;

		if(argc>=12)
		{
			pAc3Param->eChannelMode 			= atoi(argv[11]);

		}
		else
			pAc3Param->eChannelMode 			= 7;

		if(argc>=10)
		{
			pAc3Param->i_comp_mode				= atoi(argv[9]);

		}
		else
			pAc3Param->i_comp_mode				= 2;

		if(argc>=14)
		{
			pAc3Param->i_pcm_scale_fac			= atoi(argv[13]);

		}
		else
			pAc3Param->i_pcm_scale_fac			= 1<<15;

		if(argc>=15)
		{
			pAc3Param->i_stereo_mode 			= atoi(argv[14]);

		}
		else
			pAc3Param->i_stereo_mode 			= 0;

		if(argc>=16)
		{
			pAc3Param->i_dualmono_mode 			= atoi(argv[15]);

		}
		else
			pAc3Param->i_dualmono_mode 			= 0;

		if(argc>=17)
		{
			pAc3Param->i_dyn_rng_scale_hi 		= atoi(argv[16]);

		}
		else
			pAc3Param->i_dyn_rng_scale_hi 		= 1<<15;

		if(argc>=18)
		{
			pAc3Param->i_dyn_rng_scale_low 		= atoi(argv[17]);

		}
		else
			pAc3Param->i_dyn_rng_scale_low 		= 1<<15;

		if(argc>=19)
		{
			chanrout = atoi(argv[18]);

	        if(0 == chanrout )
				pAc3Param->chan_ptr[0] = 'L';
			else if(1 == chanrout )
				pAc3Param->chan_ptr[0] = 'C';
			else if(2 == chanrout )
				pAc3Param->chan_ptr[0] = 'R';
			else if(3 == chanrout )
				pAc3Param->chan_ptr[0] = 'l';
			else if(4 == chanrout )
				pAc3Param->chan_ptr[0] = 'r';
			else if(5 == chanrout )
				pAc3Param->chan_ptr[0] = 's';
		}
		else
			pAc3Param->chan_ptr[0] = 'L';

		if(argc>=20)
		{
			chanrout = atoi(argv[19]);

			if(0 == chanrout )
				pAc3Param->chan_ptr[1] = 'L';
			else if(1 == chanrout )
				pAc3Param->chan_ptr[1] = 'C';
			else if(2 == chanrout )
				pAc3Param->chan_ptr[1] = 'R';
			else if(3 == chanrout )
				pAc3Param->chan_ptr[1] = 'l';
			else if(4 == chanrout )
				pAc3Param->chan_ptr[1] = 'r';
			else if(5 == chanrout )
				pAc3Param->chan_ptr[1] = 's';
		}
		else
			pAc3Param->chan_ptr[1] = 'C';

		if(argc>=21)
	   {
			chanrout = atoi(argv[20]);

			if(0 == chanrout )
				pAc3Param->chan_ptr[2] = 'L';
			else if(1 == chanrout )
				pAc3Param->chan_ptr[2] = 'C';
			else if(2 == chanrout )
				pAc3Param->chan_ptr[2] = 'R';
			else if(3 == chanrout )
				pAc3Param->chan_ptr[2] = 'l';
			else if(4 == chanrout )
				pAc3Param->chan_ptr[2] = 'r';
			else if(5 == chanrout )
				pAc3Param->chan_ptr[2] = 's';
		}
		else
			pAc3Param->chan_ptr[2] = 'R';

		if(argc>=22)
		{
		 	chanrout = atoi(argv[21]);

			if(0 == chanrout )
				pAc3Param->chan_ptr[3] = 'L';
			else if(1 == chanrout )
				pAc3Param->chan_ptr[3] = 'C';
			else if(2 == chanrout )
				pAc3Param->chan_ptr[3] = 'R';
			else if(3 == chanrout )
				pAc3Param->chan_ptr[3] = 'l';
			else if(4 == chanrout )
				pAc3Param->chan_ptr[3] = 'r';
			else if(5 == chanrout )
				pAc3Param->chan_ptr[3] = 's';
		}
		else
			pAc3Param->chan_ptr[3] = 'l';

		if(argc>=23)
		{
			chanrout = atoi(argv[22]);

			if(0 == chanrout )
				pAc3Param->chan_ptr[4] = 'L';
			else if(1 == chanrout )
				pAc3Param->chan_ptr[4] = 'C';
			else if(2 == chanrout )
				pAc3Param->chan_ptr[4] = 'R';
			else if(3 == chanrout )
				pAc3Param->chan_ptr[4] = 'l';
			else if(4 == chanrout )
				pAc3Param->chan_ptr[4] = 'r';
			else if(5 == chanrout )
				pAc3Param->chan_ptr[4] = 's';
		}
		else
			pAc3Param->chan_ptr[4] = 'r';

		if(argc>=24)
		{
			chanrout = atoi(argv[23]);

			if(0 == chanrout )
				pAc3Param->chan_ptr[5] = 'L';
			else if(1 == chanrout )
				pAc3Param->chan_ptr[5] = 'C';
			else if(2 == chanrout )
				pAc3Param->chan_ptr[5] = 'R';
			else if(3 == chanrout )
				pAc3Param->chan_ptr[5] = 'l';
			else if(4 == chanrout )
				pAc3Param->chan_ptr[5] = 'r';
			else if(5 == chanrout )
				pAc3Param->chan_ptr[5] = 's';
		}
		else
			pAc3Param->chan_ptr[5] = 's';


		if(argc>=11)
		{
			pAc3Param->lfeflag = atoi(argv[10]);

		}
		else
			pAc3Param->lfeflag = 1;



		/*printf("i_output_pcm_fmt  = %d\n",pAc3Param->i_output_pcm_fmt );
		printf("nChannels  = %d\n",pAc3Param->nChannels );
		printf("i_k_capable_mode  = %d\n",pAc3Param->i_k_capable_mode );
		printf("eChannelMode  = %d\n",pAc3Param->eChannelMode );
		printf("i_comp_mode  = %d\n",pAc3Param->i_comp_mode );
		printf("i_pcm_scale_fac  = %d\n",pAc3Param->i_pcm_scale_fac );
		printf("i_stereo_mode  = %d\n",pAc3Param->i_stereo_mode );
		printf("i_dualmono_mode  = %d\n",pAc3Param->i_dualmono_mode );
		printf("i_dyn_rng_scale_hi  = %d\n",pAc3Param->i_dyn_rng_scale_hi );
		printf("i_dyn_rng_scale_low  = %d\n",pAc3Param->i_dyn_rng_scale_low );
		printf("chanrou  = %d\n",pAc3Param->chan_ptr[0]);
		printf("chanrou  = %d\n",pAc3Param->chan_ptr[1]);
		printf("chanrou  = %d\n",pAc3Param->chan_ptr[2]);
		printf("chanrou  = %d\n",pAc3Param->chan_ptr[3]);
		printf("chanrou  = %d\n",pAc3Param->chan_ptr[4]);
		printf("chanrout  = %d\n",pAc3Param->chan_ptr[5]);
		printf("lfeflag  = %d\n",pAc3Param->lfeflag );*/

	//	pAc3Param->eFormat 					= OMX_AUDIO_AC3StreamFormatMP1Layer3;

#ifdef OMX_GETTIME
	GT_START();
		error = OMX_SetParameter (*pHandle, OMX_IndexParamAudioAc3, pAc3Param);
	GT_END("Set Parameter Test-SetParameter");
#else
		error = OMX_SetParameter (*pHandle, OMX_IndexParamAudioAc3, pAc3Param);
#endif
		if(error != OMX_ErrorNone)
		{
			error = OMX_ErrorBadParameter;
			APP_DPRINT("%d :: APP: OMX_ErrorBadParameter\n",__LINE__);
			goto EXIT;
		}


#ifndef USE_BUFFER
	    APP_DPRINT("%d :: APP: About to call OMX_AllocateBuffer\n",__LINE__);
	    for(i = 0; i < numofinbuff; i++)
		{
		   /* allocate input buffer */
		   APP_DPRINT("%d :: APP: About to call OMX_AllocateBuffer for pInputBufferHeader[%d]\n",__LINE__, i);
		   error = OMX_AllocateBuffer(*pHandle, &pInputBufferHeader[i], 0, NULL, INPUT_AC3DEC_BUFFER_SIZE);
		   if(error != OMX_ErrorNone)
		   {
			  APP_EPRINT("%d :: APP: Error returned by OMX_AllocateBuffer for pInputBufferHeader[%d]\n",__LINE__, i);
			  goto EXIT;
		   }
		}
        APP_DPRINT("\n%d :: APP: pCompPrivateStruct->nBufferSize --> %ld \n",__LINE__,
    												pCompPrivateStruct->nBufferSize);
	    for(i = 0; i < numofoutbuff; i++)
		{
		   /* allocate output buffer */
		   APP_DPRINT("%d :: APP: About to call OMX_AllocateBuffer for pOutputBufferHeader[%d]\n",__LINE__, i);
		   error = OMX_AllocateBuffer(*pHandle, &pOutputBufferHeader[i], 1, NULL, OutputBufferSize);
		   if(error != OMX_ErrorNone)
		   {
			  APP_EPRINT("%d :: APP: Error returned by OMX_AllocateBuffer for pOutputBufferHeader[%d]\n",__LINE__, i);
			  goto EXIT;
		   }
		}
#else
	    for(i = 0; i < numofinbuff; i++)
		{
		   pInputBuffer[i] = (OMX_U8*)newmalloc(INPUT_AC3DEC_BUFFER_SIZE + 256);
		   APP_DPRINT("%d :: APP: pInputBuffer[%d] = %p\n",__LINE__,i,pInputBuffer[i]);
		   if(NULL == pInputBuffer[i])
		   {
			  APP_EPRINT("%d :: APP: Malloc Failed\n",__LINE__);
			  error = OMX_ErrorInsufficientResources;
			  goto EXIT;
		   }
		   pInputBuffer[i] = pInputBuffer[i] + 128;
		   /* pass input buffer */
		   APP_DPRINT("%d :: APP: About to call OMX_UseBuffer\n",__LINE__);
		   APP_DPRINT("%d :: APP: pInputBufferHeader[%d] = %p\n",__LINE__,i,pInputBufferHeader[i]);
		   error = OMX_UseBuffer(*pHandle, &pInputBufferHeader[i], 0, NULL, INPUT_AC3DEC_BUFFER_SIZE, pInputBuffer[i]);
		   if(error != OMX_ErrorNone)
		   {
			  APP_EPRINT("%d :: APP: Error returned by OMX_UseBuffer()\n",__LINE__);
			  goto EXIT;
		   }
		}

	    for(i = 0; i < numofoutbuff; i++)
		{
		   pOutputBuffer[i] = (OMX_U8*) newmalloc (OutputBufferSize + 256);
		   APP_DPRINT("%d :: APP: pOutputBuffer[%d] = %p\n",__LINE__,i,pOutputBuffer[i]);
		   if(NULL == pOutputBuffer[i])
		   {
			  APP_EPRINT("%d :: APP: Malloc Failed\n",__LINE__);
			  error = OMX_ErrorInsufficientResources;
			  goto EXIT;
		   }
		   pOutputBuffer[i] = pOutputBuffer[i] + 128;

		   /* allocate output buffer */
		   APP_DPRINT("%d :: APP: About to call OMX_UseBuffer\n",__LINE__);
		   APP_DPRINT("%d :: APP: pOutputBufferHeader[%d] = %p\n",__LINE__,i,pOutputBufferHeader[i]);
		   error = OMX_UseBuffer(*pHandle, &pOutputBufferHeader[i], 1, NULL, OutputBufferSize, pOutputBuffer[i]);
		   if(error != OMX_ErrorNone)
		   {
			  APP_EPRINT("%d :: APP: Error returned by OMX_UseBuffer()\n",__LINE__);
			  goto EXIT;
		   }
		}
#endif

		/* --------Change to Idle  ---------*/
		APP_DPRINT ("%d:: APP: Sending OMX_StateIdle Command\n",__LINE__);
	#ifdef OMX_GETTIME
		GT_START();
	#endif
		error = OMX_SendCommand(*pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
		if(error != OMX_ErrorNone)
		{
			APP_EPRINT("APP: Error from SendCommand-Idle(Init) State function\n");
			goto EXIT;
		}
		/* Wait for startup to complete */
	error = WaitForState(*pHandle, OMX_StateIdle);
#ifdef OMX_GETTIME
	GT_END("Call to SendCommand <OMX_StateIdle>");
#endif
		if(error != OMX_ErrorNone)
		{
			APP_EPRINT("APP: Error: WaitForState reports an error %X\n", error);
			goto EXIT;
		}



/*----------------------------------------------
 Main Loop for Non Deleting component test
 ----------------------------------------------*/
		kk = 0;
		for(kk=0; kk<testcnt; kk++)
		{
			APP_DPRINT ("%d :: APP: Test counter = %d \n",__LINE__,kk);
			if(kk > 0)
			{
	            APP_IPRINT ("Decoding the file one more Time\n");

	            close(IpBuf_Pipe[0]);
	            close(IpBuf_Pipe[1]);
	            close(OpBuf_Pipe[0]);
	            close(OpBuf_Pipe[1]);
	            APP_DPRINT("%d :: APP: - Closing OpBuf_Pipe - !!!!! \n",__LINE__);

	            /* Create a pipe used to queue data from the callback. */
	            retval = pipe(IpBuf_Pipe);
	            if( retval != 0)
				{
	                APP_EPRINT( "%d :: APP: Error: Fill Data Pipe failed to open\n",__LINE__);
	                goto EXIT;
	            }

	            retval = pipe(OpBuf_Pipe);
	            APP_DPRINT("%d :: APP: - Created again OpBuf_Pipe, fd[0] fd[1]\n",__LINE__,OpBuf_Pipe[0],OpBuf_Pipe[1]);
	            if( retval != 0)
				{
	                APP_EPRINT( "%d :: APP: Error: Empty Data Pipe failed to open\n",__LINE__);
	                goto EXIT;
	            }
				{
		            fIn = fopen(argv[1], "r");
		            if(fIn == NULL)
					{
		                APP_EPRINT("Error:  failed to open the file %s for readonly access\n", argv[1]);
		                goto EXIT;
		            }
				}
	            fOut = fopen(fname, "w");
	            if(fOut == NULL)
				{
	               APP_EPRINT("Error:  failed to create the output file \n");
	                goto EXIT;
	            }
        	}
			nFrameCount = 0;
			APP_IPRINT("------------------------------------------------------------\n");
			APP_IPRINT ("%d :: APP: Decoding the file [%d] Time\n",__LINE__, kk+1);
			APP_IPRINT("------------------------------------------------------------\n");
			if ( atoi(argv[4])== 4)
			{
				APP_IPRINT("------------------------------------------------------------\n");
				APP_IPRINT ("Testing Repeated RECORD without Deleting Component\n");
				APP_IPRINT("------------------------------------------------------------\n");
			}
			if(fIn == NULL)
			{
				fIn = fopen(argv[1], "r");
				if(fIn == NULL)
				{
					APP_EPRINT("APP: Error:  failed to open the file %s for readonly access\n", argv[1]);
					goto EXIT;
				}
			}

			if(fOut == NULL)
			{
				fOut = fopen(fname, "w");
				if(fOut == NULL)
				{
					APP_EPRINT("APP: Error:  failed to create the output file %s\n", argv[2]);
					goto EXIT;
				}
			}

			/* -------- Change to Executing ------------ */
			done = 0;
			APP_DPRINT ("%d :: APP: Sending OMX_StateExecuting Command\n",__LINE__);
		#ifdef OMX_GETTIME
			GT_START();
		#endif
			error = OMX_SendCommand(*pHandle,OMX_CommandStateSet, OMX_StateExecuting, NULL);
			if(error != OMX_ErrorNone)
			{
				APP_EPRINT ("APP: Error from SendCommand-Executing State function \n");
				goto EXIT;
			}
			error = WaitForState(*pHandle, OMX_StateExecuting);
		#ifdef OMX_GETTIME
			GT_END("Call to SendCommand <OMX_StateExecuting>");
		#endif
			if(error != OMX_ErrorNone)
			{
				APP_EPRINT ( "APP: WaitForState reports an error \n");
				goto EXIT;
			}

			pComponent = (OMX_COMPONENTTYPE *)*pHandle;
		    error = OMX_GetState(*pHandle, &state);
	        if(error != OMX_ErrorNone)
			{
		        APP_EPRINT ("%d :: APP: OMX_GetState has returned status %X\n",__LINE__, error);
		        goto EXIT;
			}

			{
				for(i = 0; i < numofinbuff; i++)
				{
		            nRead = fread(pInputBufferHeader[i]->pBuffer, 1, pInputBufferHeader[i]->nAllocLen, fIn);
		            APP_DPRINT("%d :: APP: Reading InputBuffer = %d from the input file nRead = %d\n",__LINE__, nIpBuffs, nRead);
				    if((nRead <= 0/*pInputBufferHeader[i]->nAllocLen*/) && (done == 0))
					{

			            APP_DPRINT("%d :: APP: Sending Last Input Buffer from TestApp(which can be zero or less than Buffer length) ---------- \n",__LINE__);
					    pInputBufferHeader[i]->nFlags = OMX_BUFFERFLAG_EOS;
					}
				    APP_DPRINT("%d :: APP :: Input Buffer: Calling EmptyThisBuffer: %p\n",__LINE__,pInputBufferHeader[i]);
				    pInputBufferHeader[i]->nFilledLen = nRead;
					pInputBufferHeader[i]->nTimeStamp= rand() % 100;
                    pInputBufferHeader[i]->nTickCount = rand() % 100;
				#ifdef OMX_GETTIME
					if (k==0)
					{
						GT_FlagE=1;  /* 1 = First Buffer,  0 = Not First Buffer  */
						GT_START(); /* Empty Bufffer */
					}
				#endif

				if (!preempted)
				    OMX_EmptyThisBuffer(*pHandle, pInputBufferHeader[i]);


					APP_DPRINT("APP: pInputBufferHeader[%d]->nTimeStamp = %lli\n",i,pInputBufferHeader[i]->nTimeStamp);
				    nIpBuffs++;
				}
			}

	        for (k=0; k < numofoutbuff; k++)
			{
		        APP_DPRINT("%d :: APP: Before Fill this buffer is called = %x\n",__LINE__, (unsigned int)pOutputBufferHeader[k]);
			#ifdef OMX_GETTIME
				if (k==0)
					{
						GT_FlagF=1;  /* 1 = First Buffer,  0 = Not First Buffer  */
						GT_START(); /* Fill Buffer */
					}
			#endif
                OMX_FillThisBuffer(*pHandle,  pOutputBufferHeader[k]);
			}

/*----------------------------------------------
 Main while for the buffers process
 ----------------------------------------------*/

 			 /* Component is stopping now by procesing the playcomplete event  (bPlayCompleted Flag) */

#ifndef WAITFORRESOURCES
			while(( (error == OMX_ErrorNone) && (state != OMX_StateIdle)) && (state != OMX_StateInvalid) && (!bPlayCompleted))
			{
				APP_DPRINT("%d :: APP: Entering while loop !! outputportreconfig = %d\n",__LINE__, outputportreconfig);
				if(outputportreconfig)
				{


					APP_DPRINT("%d :: APP: About to send OMX_CommandFlush \n",__LINE__);
					error = OMX_SendCommand (*pHandle,OMX_CommandFlush,OUTPUT_PORT,NULL);
					if(error != OMX_ErrorNone)
					{
						APP_EPRINT ("APP: Error from SendCommand-Executing State function \n");
						goto EXIT;
					}

					APP_DPRINT("%d :: APP: Waiting for OMX_CommandFlush to complete \n",__LINE__);
					WaitForCommand(*pHandle,OMX_CommandFlush);

					APP_DPRINT("%d :: APP: About to send OMX_CommandPortDisable \n",__LINE__);
					OMX_SendCommand(*pHandle, OMX_CommandPortDisable,OUTPUT_PORT,NULL);

					APP_DPRINT("%d :: APP: About to call EmptyOutputBufferPipe \n",__LINE__);
					EmptyOutputBufferPipe();

					for(i=0; i < numofoutbuff; i++)
					{
					   APP_DPRINT("%d :: APP: About to free pOutputBufferHeader[%d]\n",__LINE__, i);
					   error = OMX_FreeBuffer(*pHandle, OUTPUT_PORT, pOutputBufferHeader[i]);
					   if((error != OMX_ErrorNone))
					   {
						  APP_DPRINT("%d :: APP: Error in Free Buffer function\n",__LINE__);
						  goto EXIT;
						}
					}

					for(i=0; i < numofoutbuff; i++)
					{
						if (pOutputBuffer[i] != NULL)
						{
							pOutputBuffer[i] = pOutputBuffer[i] - 128;
							APP_DPRINT("%d :: [TESTAPPFREE] pOutputBuffer[%d] = %p\n",__LINE__,i, pOutputBuffer[i]);
							if(pOutputBuffer[i] != NULL)
							{
								newfree(pOutputBuffer[i]);
								pOutputBuffer[i] = NULL;
						   }
						}
					}

					APP_DPRINT("%d :: APP: Waiting for OMX_CommandPortDisable to complete \n",__LINE__);
					WaitForCommand(*pHandle,OMX_CommandPortDisable);

					APP_DPRINT("%d :: APP: OMX_GetParameter called \n",__LINE__);
					error = OMX_GetParameter (*pHandle,OMX_IndexParamAudioPcm, iPcmParam);

					if(error != OMX_ErrorNone)
					{
						error = OMX_ErrorBadParameter;
						APP_DPRINT("%d :: APP: OMX_ErrorBadParameter\n",__LINE__);
						goto EXIT;
					}

					APP_DPRINT("%d :: APP: About to send OMX_CommandPortEnable \n",__LINE__);
					OMX_SendCommand(*pHandle, OMX_CommandPortEnable,OUTPUT_PORT,NULL);

					for(i = 0; i < numofoutbuff; i++)
					{
					   pOutputBuffer[i] = (OMX_U8*) newmalloc (OutputBufferSize + 256);
					   APP_DPRINT("%d :: APP: pOutputBuffer[%d] = %p\n",__LINE__,i,pOutputBuffer[i]);
					   if(NULL == pOutputBuffer[i])
					   {
						  APP_EPRINT("%d :: APP: Malloc Failed\n",__LINE__);
						  error = OMX_ErrorInsufficientResources;
						  goto EXIT;
					   }


					   pOutputBuffer[i] = pOutputBuffer[i] + 128;

					   /* allocate output buffer */
					   APP_DPRINT("%d :: APP: About to call OMX_UseBuffer\n",__LINE__);
					   APP_DPRINT("%d :: APP: pOutputBufferHeader[%d] = %p\n",__LINE__,i,pOutputBufferHeader[i]);
					   error = OMX_UseBuffer(*pHandle, &pOutputBufferHeader[i], 1, NULL, OutputBufferSize, pOutputBuffer[i]);
					   if(error != OMX_ErrorNone)
					   {
						  APP_EPRINT("%d :: APP: Error returned by OMX_UseBuffer()\n",__LINE__);
						  goto EXIT;
					   }
					}

					outputportreconfig = 0;

					//send the new buffers to the component
					 for (k=0; k < numofoutbuff; k++)
					{
						APP_DPRINT("%d :: APP: Before Fill this buffer is called = %x\n",__LINE__, (unsigned int)pOutputBufferHeader[k]);
					#ifdef OMX_GETTIME
						if (k==0)
							{
								GT_FlagF=1;  /* 1 = First Buffer,  0 = Not First Buffer  */
								GT_START(); /* Fill Buffer */
							}
					#endif
						OMX_FillThisBuffer(*pHandle,  pOutputBufferHeader[k]);
					}

					/*APP_DPRINT("%d :: APP: Waiting for OMX_CommandPortEnable to complete \n",__LINE__);
					WaitForCommand(*pHandle,OMX_CommandPortEnable);*/


				}

			if(1){
#else
    		while(1)
    		{
       		if((error == OMX_ErrorNone) && (state != OMX_StateIdle) && (state != OMX_StateInvalid) && (!bPlayCompleted)){
#endif

				FD_ZERO(&rfds);
				FD_SET(IpBuf_Pipe[0], &rfds);
				FD_SET(OpBuf_Pipe[0], &rfds);
				FD_SET(Event_Pipe[0], &rfds);

				tv.tv_sec = 1;
				tv.tv_usec = 0;

				retval = select(fdmax+1, &rfds, NULL, NULL, &tv);
				if(retval == -1)
				{
					perror("select()");
					fprintf (stderr, " : Error \n");
					break;
				}

				if(retval == 0)
				{
					APP_DPRINT("%d :: APP: The current state of the component = %d \n",__LINE__,state);
					APP_DPRINT("\n\n\n%d ::!!!!!!!     App Timeout !!!!!!!!!!! \n",__LINE__);
					APP_DPRINT("%d :: ---------------------------------------\n\n\n",__LINE__);
				}
				/*Check if any input buffer is available. If yes, fill the buffer and send to the component*/
				switch (atoi(argv[4]))
				{
					case 1:
					case 4:
					case 5:
						{
						    APP_DPRINT("%d :: APP: AC3 DECODER RUNNING UNDER FILE 2 FILE MODE \n",__LINE__);
		    		        if(FD_ISSET(IpBuf_Pipe[0], &rfds))
							{
            		            OMX_BUFFERHEADERTYPE* pBuffer;
            		            read(IpBuf_Pipe[0], &pBuffer, sizeof(pBuffer));
            		            if(done == 0)
								{
						            nRead = fread(pBuffer->pBuffer, 1, pBuffer->nAllocLen, fIn);
						            APP_DPRINT("%d :: APP: Reading InputBuffer = %d from the input file nRead = %d\n",__LINE__, nIpBuffs, nRead);
						            if((nRead <= 0/*pBuffer->nAllocLen*/) && (done == 0))
									{
							            APP_IPRINT("%d :: APP: Sending Last Input Buffer from TestApp \n",__LINE__);
							            done 			= 1;
							            pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
									}
						            APP_DPRINT("%d :: APP :: Input Buffer: Calling EmptyThisBuffer: %p\n",__LINE__,pBuffer);
						            pBuffer->nFilledLen = nRead;
                                    pBuffer->nTimeStamp= rand() % 100;
                                    pBuffer->nTickCount = rand() % 100;
						            OMX_EmptyThisBuffer(*pHandle, pBuffer);
						            nIpBuffs++;
								}
						   }
						}
						break;

					case 2:
						{
					  	    APP_DPRINT("%d :: APP: AC3 DECODER RUNNING UNDER FILE 2 FILE MODE \n",__LINE__);
					        if( FD_ISSET(IpBuf_Pipe[0], &rfds) )
							{
					            OMX_BUFFERHEADERTYPE* pBuffer;
					            read(IpBuf_Pipe[0], &pBuffer, sizeof(pBuffer));
					            if(done == 0)
								{
						            APP_DPRINT("%d :: APP: Reading InputBuffer = %d from the input file nRead = %d\n",__LINE__, nIpBuffs, nRead);
						            nRead = fread(pBuffer->pBuffer, 1, pBuffer->nAllocLen, fIn);
						            if(frmCnt == 20)
									{
						                APP_DPRINT("%d :: APP: Sending Stop.........From APP \n",__LINE__);
						                APP_DPRINT("%d :: APP: Shutting down ---------- \n",__LINE__);
									#ifdef OMX_GETTIME
										GT_START();
									#endif
										error = OMX_SendCommand(*pHandle,OMX_CommandStateSet, OMX_StateIdle, NULL);
						                if(error != OMX_ErrorNone)
										{
							               APP_EPRINT("APP: Error from SendCommand-Idle(Stop) State function\n");
							                goto EXIT;
										}
										error = WaitForState(*pHandle, OMX_StateIdle);
									#ifdef OMX_GETTIME
										GT_END("Call to SendCommand <OMX_StateIdle>");
									#endif
										if(error != OMX_ErrorNone)
										{
											APP_EPRINT("APP: WaitForState reports an error \n");
											goto EXIT;
										}
						                done 				 = 1;
						                pBuffer->nFlags 	 = OMX_BUFFERFLAG_EOS;
						                pBuffer->nFilledLen = 0;
									}
									else if((nRead <= 0/*pBuffer->nAllocLen-*/) && (done == 0))
									{
							             APP_DPRINT("%d :: APP: Sending Last Input Buffer from TestApp(which can be zero or less than Buffer length)\n",__LINE__);
							             done = 1;
							             pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
									}
							        APP_DPRINT("%d :: APP :: Input buffer: Calling EmptyThisBuffer: %p\n",__LINE__,pBuffer);
                					pBuffer->nTimeStamp= rand() % 100;				/* random value for time stamp */
                                    pBuffer->nTickCount = rand() % 100;
						            pBuffer->nFilledLen = nRead;
							        OMX_EmptyThisBuffer(*pHandle, pBuffer);
									nIpBuffs++;
							        frmCnt++;
								}
							}
						}
						break;

					case 3:
						{
							APP_DPRINT("%d :: APP: AC3 DECODER RUNNING UNDER FILE 2 FILE MODE \n",__LINE__);
		    		        if(FD_ISSET(IpBuf_Pipe[0], &rfds))
							{
            		            OMX_BUFFERHEADERTYPE* pBuffer;
            		            read(IpBuf_Pipe[0], &pBuffer, sizeof(pBuffer));
            		            if(done == 0)
								{
						            nRead = fread(pBuffer->pBuffer, 1, pBuffer->nAllocLen, fIn);
						            APP_DPRINT("%d :: APP: Reading InputBuffer = %d from the input file nRead = %d\n",__LINE__, nIpBuffs, nRead);
						            if((nRead <= 0/*pBuffer->nAllocLen*/) && (done == 0))
									{
							            APP_DPRINT("%d :: APP: Sending Last Input Buffer from TestApp(which can be zero or less than Buffer length) ---------- \n",__LINE__);
							            done 			= 1;
							            pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
									}
						            APP_DPRINT("%d :: APP : Input Buffer- Calling EmptyThisBuffer: %p\n",__LINE__,pBuffer);
						            pBuffer->nFilledLen = nRead;
                					pBuffer->nTimeStamp= rand() % 100;				/* random value for time stamp */
                                    pBuffer->nTickCount = rand() % 100;
						            OMX_EmptyThisBuffer(*pHandle, pBuffer);
						            nIpBuffs++;
								}
							}
							if(nIpBuffs == 9)
							{
							#ifdef OMX_GETTIME
								GT_START();
							#endif
								error = OMX_SendCommand(*pHandle,OMX_CommandStateSet, OMX_StatePause, NULL);
								if(error != OMX_ErrorNone)
								{
									APP_EPRINT("APP: Error from SendCommand-Idle(Stop) State function\n");
									goto EXIT;
								}
								APP_DPRINT("%d :: APP: Pause: OpBuffs received = %d\n",__LINE__,nOpBuffs);
								error = WaitForState(*pHandle, OMX_StatePause);
							#ifdef OMX_GETTIME
								GT_END("Call to SendCommand <OMX_StatePause>");
							#endif
								if(error != OMX_ErrorNone)
								{
									APP_EPRINT("APP: Error: WaitForState reports an error %X\n", error);
									goto EXIT;
								}
								APP_DPRINT("%d :: APP: Pause: State paused = %d\n",__LINE__,nOpBuffs);
								APP_IPRINT("%d :: APP: Pausing component...\n",__LINE__);
								APP_DPRINT("%d :: APP: Is Sleeping here for %d seconds\n",__LINE__, SLEEP_TIME);
								sleep(SLEEP_TIME);
							#ifdef OMX_GETTIME
								GT_START();
							#endif
								error = OMX_SendCommand(*pHandle,OMX_CommandStateSet, OMX_StateExecuting, NULL);
								if(error != OMX_ErrorNone)
								{
									APP_EPRINT("APP: Error from SendCommand-Executing State function\n");
									goto EXIT;
								}
								APP_DPRINT("%d :: APP: Resumed: OpBuffs received = %d\n",__LINE__,nOpBuffs);
								error = WaitForState(*pHandle, OMX_StateExecuting);
							#ifdef OMX_GETTIME
								GT_END("Call to SendCommand <OMX_StateIdle>");
							#endif
								if(error != OMX_ErrorNone)
								{
									APP_DPRINT ( "APP: WaitForState reports an error \n");
									goto EXIT;
								}
							}
						}
						break;
					case 6:
						{
						    APP_DPRINT("%d :: APP: AC3 DECODER RUNNING UNDER FILE 2 FILE MODE \n",__LINE__);
							if(nIpBuffs == 20)
							{
									APP_DPRINT("APP: Sending Stop Command after sending 20 frames \n");
								#ifdef OMX_GETTIME
									GT_START();
								#endif
									error = OMX_SendCommand(*pHandle,OMX_CommandStateSet, OMX_StateIdle, NULL);
									if(error != OMX_ErrorNone)
									{
										APP_EPRINT("APP: Error from SendCommand-Idle(Stop) State function\n");
										goto EXIT;
									}
									error = WaitForState(*pHandle, OMX_StateIdle);
								#ifdef OMX_GETTIME
									GT_END("Call to SendCommand <OMX_StateIdle>");
								#endif
									if(error != OMX_ErrorNone)
									{
										APP_EPRINT("APP: Error: WaitForState reports an error %X\n", error);
										goto EXIT;
									}
									APP_DPRINT("%d :: APP: About to call GetState() \n",__LINE__);
					                error = OMX_GetState(*pHandle, &state);
					                if(error != OMX_ErrorNone)
									{
						                APP_EPRINT("APP: Warning:  hAc3Decoder->GetState has returned status %X\n", error);
						                goto EXIT;
									}
							}
						    else if(FD_ISSET(IpBuf_Pipe[0], &rfds))
							{
            		            OMX_BUFFERHEADERTYPE* pBuffer;
            		            read(IpBuf_Pipe[0], &pBuffer, sizeof(pBuffer));
            		            if(done == 0)
								{
						            nRead = fread(pBuffer->pBuffer, 1, pBuffer->nAllocLen, fIn);
						            APP_DPRINT("%d :: APP: Reading InputBuffer = %d from the input file nRead = %d\n",__LINE__, nIpBuffs, nRead);
						            if((nRead <= 0/*pBuffer->nAllocLen*/) && (done == 0))
									{
							            APP_DPRINT("%d :: APP: Sending Last Input Buffer from TestApp(which can be zero or less than Buffer length) \n",__LINE__);
							            done 			= 1;
							            pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
									}
									APP_DPRINT("%d :: APP: Input Buffer: Calling EmptyThisBuffer: %p\n",__LINE__,pBuffer);
						            pBuffer->nFilledLen = nRead;
                					pBuffer->nTimeStamp= rand() % 100;				/* random value for time stamp */
                                    pBuffer->nTickCount = rand() % 100;
						            OMX_EmptyThisBuffer(*pHandle, pBuffer);
						            nIpBuffs++;
								}
							}
						}
						break;

					case 7:
						{
						    APP_IPRINT("%d :: APP: This test is not applied to file mode\n",__LINE__);
						    goto EXIT;
						}
					break;

					case 8:
						{

					  	    APP_DPRINT("%d :: APP: AC3 DECODER RUNNING UNDER FILE 2 FILE MODE \n",__LINE__);
					        if( FD_ISSET(IpBuf_Pipe[0], &rfds) )
							{
					            OMX_BUFFERHEADERTYPE* pBuffer;
					            read(IpBuf_Pipe[0], &pBuffer, sizeof(pBuffer));
					            if(done == 0)
								{
						            APP_DPRINT("%d :: APP: Reading InputBuffer = %d from the input file nRead = %d\n",__LINE__, nIpBuffs, nRead);
						            nRead = fread(pBuffer->pBuffer, 1, pBuffer->nAllocLen, fIn);
						            if(frmCnt == seek_frm_cnt[seek_counter])
									{
						                APP_DPRINT("%d :: APP: Seeking to new location \n",__LINE__);

										fseek (fIn, seek_ptr_cnt[seek_counter], SEEK_CUR);

						                outputportreconfig = 1;

						                seek_counter++;
						                if(seek_counter >=10)
						                	seek_counter = 0;
									}
									else if((nRead <= 0/*pBuffer->nAllocLen-*/) && (done == 0))
									{
							             APP_DPRINT("%d :: APP: Sending Last Input Buffer from TestApp(which can be zero or less than Buffer length)\n",__LINE__);
							             done = 1;
							             pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
									}
							        APP_DPRINT("%d :: APP :: Input buffer: Calling EmptyThisBuffer: %p\n",__LINE__,pBuffer);
                					pBuffer->nTimeStamp= rand() % 100;				/* random value for time stamp */
                                    pBuffer->nTickCount = rand() % 100;
						            pBuffer->nFilledLen = nRead;
							        OMX_EmptyThisBuffer(*pHandle, pBuffer);
									nIpBuffs++;
							        frmCnt++;
								}
							}
						}
						break;

					default:
						APP_DPRINT("%d :: APP: ### Running Simple DEFAULT Case Here ###\n",__LINE__);
				} /* end of switch loop */

				/*Check if any output buffer is available. If yes, send the buffer to the component*/
				if(FD_ISSET(OpBuf_Pipe[0], &rfds))
				{

					OMX_BUFFERHEADERTYPE* pBuf;

					APP_DPRINT("%d :: APP: ### OutputBuffer is available in the pipe ###\n",__LINE__);
					APP_DPRINT("%d :: APP: - OpBuf_Pipe, fd[0] fd[1]:%d %d\n",__LINE__,OpBuf_Pipe[0],OpBuf_Pipe[1]);

					read(OpBuf_Pipe[0], &pBuf, sizeof(pBuf));

					APP_DPRINT("%d :: App: Came out of read:pBuf: %p \n",__LINE__,pBuf);

					if (firstbuffer){   /* Discard first buffer - Config audio (PV) */
					    APP_DPRINT("%d :: App: Came inside firstbuffer :pBuf: %p, pBuf->nAllocLen : %d \n",__LINE__,pBuf,pBuf->nAllocLen);
						memset(pBuf->pBuffer, 0x0, pBuf->nAllocLen);
						APP_DPRINT("%d :: App: Came after memset :pBuf: %p \n",__LINE__,pBuf);
	                    pBuf->nFilledLen=0;

						firstbuffer = 0;
					}
					APP_DPRINT("%d :: App: Buffer to write to the file: %p \n",__LINE__,pBuf);
					APP_DPRINT("%d :: ------------- App File Write --------------\n",__LINE__);
					APP_DPRINT("%d :: App: %ld bytes are being written\n",__LINE__,(pBuf->nFilledLen));
					APP_DPRINT("%d :: ------------- App File Write --------------\n\n",__LINE__);
					nOpBuffs++;
					fwrite (pBuf->pBuffer, 1, (pBuf->nFilledLen), fOut);
					OMX_FillThisBuffer(*pHandle, pBuf);
					APP_DPRINT("%d :: APP: Sent %p Emptied Output Buffer = %d to Comp\n",__LINE__,pBuf,nFrameCount+1);
					nFrameCount++;
				}

				if( FD_ISSET(Event_Pipe[0], &rfds) )
				{

					OMX_U8 pipeContents;
					read(Event_Pipe[0], &pipeContents, sizeof(OMX_U8));

					if (pipeContents == 0)
					{
						APP_IPRINT("Test app received OMX_ErrorResourcesPreempted\n");
						WaitForState(*pHandle,OMX_StateIdle);

						error = OMX_FreeBuffer(pHandle,OMX_DirInput,pInputBufferHeader[i]);
						if( (error != OMX_ErrorNone)) {
							APP_DPRINT ("%d :: Error in Free Handle function\n",__LINE__);
						}

						error = OMX_FreeBuffer(pHandle,OMX_DirOutput,pOutputBufferHeader[i]);
						if( (error != OMX_ErrorNone)) {
							APP_DPRINT ("%d:: Error in Free Handle function\n",__LINE__);
						}
#ifdef USE_BUFFER

						for(i=0; i < numofinbuff; i++)
						{
							if (pInputBuffer[i] != NULL)
							{
							   pInputBuffer[i] = pInputBuffer[i] - 128;
							   APP_DPRINT("%d :: [TESTAPPFREE] pInputBuffer[%d] = %p\n",__LINE__,i,pInputBuffer[i]);
							   if(pInputBuffer[i] != NULL)
							   {
									newfree(pInputBuffer[i]);
									pInputBuffer[i] = NULL;
							   }
							}
						}

						for(i=0; i < numofoutbuff; i++)
						{
							if (pOutputBuffer[i] != NULL)
							{
								pOutputBuffer[i] = pOutputBuffer[i] - 128;
								APP_DPRINT("%d :: [TESTAPPFREE] pOutputBuffer[%d] = %p\n",__LINE__,i, pOutputBuffer[i]);
								if(pOutputBuffer[i] != NULL)
								{
									newfree(pOutputBuffer[i]);
									pOutputBuffer[i] = NULL;
							   }
							}
						}
#endif

						OMX_SendCommand(*pHandle,OMX_CommandStateSet, OMX_StateLoaded, NULL);
						WaitForState(*pHandle, OMX_StateLoaded);

						OMX_SendCommand(*pHandle,OMX_CommandStateSet,OMX_StateWaitForResources,NULL);
						WaitForState(*pHandle,OMX_StateWaitForResources);

					}
					else if (pipeContents == 1)
					{

						APP_IPRINT("Test app received OMX_ErrorResourcesAcquired\n");

						OMX_SendCommand(*pHandle,OMX_CommandStateSet,OMX_StateIdle,NULL);
						error = OMX_AllocateBuffer(pHandle,
												&pOutputBufferHeader[i],
												1,
												NULL,
												OutputBufferSize);

						APP_DPRINT("%d :: called OMX_AllocateBuffer\n",__LINE__);
						if(error != OMX_ErrorNone) {
							APP_DPRINT("%d :: Error returned by OMX_AllocateBuffer()\n",__LINE__);
							goto EXIT;
						}

						WaitForState(*pHandle,OMX_StateIdle);

						OMX_SendCommand(*pHandle,OMX_CommandStateSet,OMX_StateExecuting,NULL);
						WaitForState(*pHandle,OMX_StateExecuting);

						rewind(fIn);

						if (!preempted)
							OMX_EmptyThisBuffer(*pHandle, pInputBufferHeader[i]);

					   /* send_input_buffer (pHandle, pOutputBufferHeader, fIn); */
					}

					if (pipeContents == 2)
					{

#ifdef OMX_GETTIME
						GT_START();
#endif
						OMX_SendCommand(*pHandle,OMX_CommandStateSet,OMX_StateIdle,NULL);
						WaitForState(*pHandle,OMX_StateIdle);
#ifdef OMX_GETTIME
						GT_END("Call to SendCommand <OMX_StateIdle>");
#endif

#ifdef WAITFORRESOURCES

						for(i=0; i<numofinbuff; i++)
						{

							error = OMX_FreeBuffer(pHandle,OMX_DirInput,pInputBufferHeader[i]);
							if( (error != OMX_ErrorNone)) {
								APP_DPRINT ("%d :: Error in Free Handle function\n",__LINE__);
							}
						}
						for(i=0; i<numofoutbuff; i++)
						{

							error = OMX_FreeBuffer(pHandle,OMX_DirOutput,pOutputBufferHeader[i]);
							if( (error != OMX_ErrorNone)) {
								APP_DPRINT ("%d:: Error in Free Handle function\n",__LINE__);
							}

						}

						OMX_SendCommand(*pHandle,OMX_CommandStateSet, OMX_StateLoaded, NULL);
						WaitForState(*pHandle, OMX_StateLoaded);

						goto SHUTDOWN;

#endif

					}
				 }

				if(done == 1)
				{
                    APP_DPRINT("%d :: APP: About to call GetState() \n",__LINE__);
					error = OMX_GetState(*pHandle, &state);
					if(error != OMX_ErrorNone)
					{
						APP_EPRINT("APP: Warning:  hAc3Decoder->GetState has returned status %X\n", error);
						goto EXIT;
					}
				}

				}
	            else if (preempted) {
	                sched_yield();
	            }
	            else {
	                goto SHUTDOWN;
	            }
			} /* end of while loop */

			if (bPlayCompleted == OMX_TRUE )	/* Stop componet - just for F2F  mode*/
			{
				DumptoOutputFileFromOutputBufferPipe(fOut);
			#ifdef OMX_GETTIME
				GT_START();
			#endif
				APP_DPRINT("%d :: APP:  OMX_SendCommand : OMX_StateIdle \n",__LINE__);
				error = OMX_SendCommand(*pHandle,OMX_CommandStateSet, OMX_StateIdle, NULL);
				if(error != OMX_ErrorNone)
				{
					APP_EPRINT("APP: Error from SendCommand-Idle(Stop) State function\n");
					goto EXIT;
				}
				error = WaitForState(*pHandle, OMX_StateIdle);

			#ifdef OMX_GETTIME
				GT_END("Call to SendCommand <OMX_StateIdle>");
			#endif

				if(error != OMX_ErrorNone)
				{
					APP_EPRINT("APP: Error: WaitForState reports an error %X\n", error);
					goto EXIT;
				}
				bPlayCompleted = OMX_FALSE;
			}


/*----------------------------------------------
	 Final stage : cleaning and closing
 ----------------------------------------------*/
 			//APP_IPRINT("%d :: APP:  Final stage : cleaning and closing  \n",__LINE__);
			APP_DPRINT("%d :: APP: The current state of the component = %d \n",__LINE__,state);
			fclose(fOut);
			fclose(fIn);
			fOut=NULL;
			fIn=NULL;
			frmCount = 0;
		} /* End of internal loop*/

	    error = OMX_SendCommand(*pHandle, OMX_CommandPortDisable, -1, NULL);
        if(error != OMX_ErrorNone)
		{
           APP_DPRINT("%d:: APP: Error from SendCommand OMX_CommandPortDisable\n",__LINE__);
        	goto EXIT;
		}

	    /* free the Allocate Buffers */
	    for(i=0; i < numofinbuff; i++)
		{
		   APP_DPRINT("%d :: APP: About to free pInputBufferHeader[%d]\n",__LINE__, i);
		   error = OMX_FreeBuffer(*pHandle, INPUT_PORT, pInputBufferHeader[i]);
		   if((error != OMX_ErrorNone))
		   {
			  APP_DPRINT("%d:: APP: Error in FreeBuffer function\n",__LINE__);
			  goto EXIT;
		   }
		}
	    for(i=0; i < numofoutbuff; i++)
		{
		   APP_DPRINT("%d :: APP: About to free pOutputBufferHeader[%d]\n",__LINE__, i);
		   error = OMX_FreeBuffer(*pHandle, OUTPUT_PORT, pOutputBufferHeader[i]);
		   if((error != OMX_ErrorNone))
		   {
			  APP_DPRINT("%d :: APP: Error in Free Buffer function\n",__LINE__);
			  goto EXIT;
		   }
		}
#ifdef USE_BUFFER
	    /* free the UseBuffers */
	    for(i=0; i < numofinbuff; i++)
		{
			if (pInputBuffer[i] != NULL)
			{
			   pInputBuffer[i] = pInputBuffer[i] - 128;
			   APP_DPRINT("%d :: [TESTAPPFREE] pInputBuffer[%d] = %p\n",__LINE__,i,pInputBuffer[i]);
			   if(pInputBuffer[i] != NULL)
			   {
				  newfree(pInputBuffer[i]);
				  pInputBuffer[i] = NULL;
			   }
			}
		}

	    for(i=0; i < numofoutbuff; i++)
		{
			if (pOutputBuffer[i] != NULL)
			{
			   pOutputBuffer[i] = pOutputBuffer[i] - 128;
			   APP_DPRINT("%d :: [TESTAPPFREE] pOutputBuffer[%d] = %p\n",__LINE__,i, pOutputBuffer[i]);
			   if(pOutputBuffer[i] != NULL)
			   {
				  newfree(pOutputBuffer[i]);
				  pOutputBuffer[i] = NULL;
			   }
			}
		}
#endif

		/* --------Change to Loaded  ---------*/
		APP_DPRINT("%d :: APP: Sending the StateLoaded Command\n",__LINE__);
	#ifdef OMX_GETTIME
		GT_START();
	#endif
		error = OMX_SendCommand(*pHandle,OMX_CommandStateSet, OMX_StateLoaded, NULL);
		if(error != OMX_ErrorNone)
		{
			APP_EPRINT("APP: Error from SendCommand-Idle State function\n");
			goto EXIT;
		}
		/* Wait for new state */
		error = WaitForState(*pHandle, OMX_StateLoaded);



	#ifdef OMX_GETTIME
		GT_END("Call to SendCommand <OMX_StateLoaded>");
	#endif
		if(error != OMX_ErrorNone)
		{
			APP_EPRINT("APP: Error:  hAc3Decoder->WaitForState reports an error %X\n", error);
			goto EXIT;

		}
		APP_DPRINT("%d :: APP: State Of Component Is Loaded Now\n",__LINE__);


#ifdef WAITFORRESOURCES
		error = OMX_SendCommand(pHandle,OMX_CommandStateSet, OMX_StateWaitForResources, NULL);
		if(error != OMX_ErrorNone) {
			APP_DPRINT ("%d Error from SendCommand-Idle State function\n",__LINE__);
			goto EXIT;
		}
		error = WaitForState(pHandle, OMX_StateWaitForResources);

		/* temporarily put this here until I figure out what should really happen here */
		sleep(10);
		/* temporarily put this here until I figure out what should really happen here */
#endif


SHUTDOWN:

		APP_MEMPRINT("%d :: [TESTAPPFREE] %p\n",__LINE__,pAc3Param);
        if(pAc3Param != NULL)
		{
	        newfree(pAc3Param);
	        pAc3Param = NULL;
		}
		APP_MEMPRINT("%d :: [TESTAPPFREE] %p\n",__LINE__,iPcmParam);
        if(iPcmParam != NULL)
		{
			APP_DPRINT("iPcmParam %p \n",iPcmParam);
	        newfree(iPcmParam);
	        iPcmParam = NULL;
			APP_DPRINT("iPcmParam %p \n",iPcmParam);
		}
        APP_MEMPRINT("%d :: [TESTAPPFREE] %p\n",__LINE__,pCompPrivateStruct);
	    if(pCompPrivateStruct != NULL)
		{
		    APP_DPRINT("ipCompPrivateStruct %p \n",pCompPrivateStruct);
		    newfree(pCompPrivateStruct);
		    pCompPrivateStruct = NULL;
		}
		APP_MEMPRINT("%d :: [TESTAPPFREE] %p\n",__LINE__,pCompPrivateStructGain);
	    if(pCompPrivateStructGain != NULL)
		{
		   newfree(pCompPrivateStructGain);
		    pCompPrivateStructGain = NULL;
		}
		APP_MEMPRINT("%d :: [TESTAPPFREE] %p\n",__LINE__,audioinfo.ac3decHeaderInfo);
#if 0
APP_MEMPRINT("%d :: [TESTAPPFREE] %p\n",__LINE__,audioinfo.ac3decHeaderInfo);

	    if(audioinfo.ac3decHeaderInfo != NULL)
		{
		    newfree(audioinfo.ac3decHeaderInfo);
		    audioinfo.ac3decHeaderInfo = NULL;
		}
#endif
	    error = close (IpBuf_Pipe[0]);
	    if (0 != error && OMX_ErrorNone == error)
		{
		    error = OMX_ErrorHardware;
		    APP_DPRINT("%d :: APP: Error while closing IpBuf_Pipe[0]\n",__LINE__);
		    goto EXIT;
		}
	    error = close (IpBuf_Pipe[1]);
	    if (0 != error && OMX_ErrorNone == error)
		{
		    error = OMX_ErrorHardware;
		    APP_DPRINT("%d :: APP: Error while closing IpBuf_Pipe[1]\n",__LINE__);
		    goto EXIT;
		}
	    error = close (OpBuf_Pipe[0]);
	    if (0 != error && OMX_ErrorNone == error)
		{
		    error = OMX_ErrorHardware;
		    APP_DPRINT("%d :: APP: Error while closing OpBuf_Pipe[0]\n",__LINE__);
		    goto EXIT;
		}
	    error = close (OpBuf_Pipe[1]);
	    if (0 != error && OMX_ErrorNone == error)
		{
		    error = OMX_ErrorHardware;
		    APP_DPRINT("%d :: APP: Error while closing OpBuf_Pipe[1]\n",__LINE__);
		    goto EXIT;
		}
		APP_DPRINT("%d :: APP: - Closed OpBuf_Pipe !!!!!\n",__LINE__);
		error = close(Event_Pipe[0]);
		if (0 != error && OMX_ErrorNone == error) {
			error = OMX_ErrorHardware;
			APP_DPRINT("%d :: Error while closing Event_Pipe[0]\n",__LINE__);
			goto EXIT;
		}

		error = close(Event_Pipe[1]);
		if (0 != error && OMX_ErrorNone == error) {
			error = OMX_ErrorHardware;
			APP_DPRINT("%d :: Error while closing Event_Pipe[1]\n",__LINE__);
			goto EXIT;
		}

#ifdef DSP_RENDERING_ON
	    cmd_data.hComponent = *pHandle;
	    cmd_data.AM_Cmd = AM_Exit;

	    if((write(Ac3dec_fdwrite, &cmd_data, sizeof(cmd_data)))<0)
	        APP_EPRINT("%d :: APP: ---send command to audio manager\n",__LINE__);

	    close(Ac3dec_fdwrite);
	    close(Ac3dec_fdread);
#endif

	    /* Free the Ac3Decoder Component */
		error = TIOMX_FreeHandle(*pHandle);
		if( (error != OMX_ErrorNone)) {
			APP_EPRINT("APP: Error in Free Handle function\n");
			goto EXIT;
		}
		APP_DPRINT("%d :: App: pHandle = %p\n",__LINE__,pHandle);
		APP_IPRINT("%d :: APP: Free Handle returned Successfully \n",__LINE__);

		error = TIOMX_Deinit();
		if( (error != OMX_ErrorNone)) {
			APP_EPRINT("APP: Error in Deinit Core function\n");
			goto EXIT;
		}
		APP_IPRINT("%d :: APP: TIOMX_Deinit returned Successfully \n",__LINE__);
		error= newfree(pHandle);
		if( (error != OMX_ErrorNone)) {
			APP_EPRINT("APP: Error in free PHandle\n");
			goto EXIT;
		}
		APP_IPRINT("%d :: APP: newfree returned Successfully \n",__LINE__);


    } /*--------- end of for loop--------- */

//	APP_IPRINT("%d :: APP: Came out of while loop Successfully \n",__LINE__);
	if (ListHeader != NULL)
	{
		error= CleanList(&ListHeader);			/* it frees streaminfo */
		if( (error != OMX_ErrorNone))
		{
			APP_DPRINT("APP: Error in CleanList function\n");
			goto EXIT;
		}
		APP_IPRINT("%d :: APP: CleanList returned Successfully \n",__LINE__);
	}

	pthread_cond_destroy(&WaitForStateMutex.cond);
	pthread_mutex_destroy(&WaitForStateMutex.Mymutex);

	APP_IPRINT("%d :: APP: Mutex Destroy Successfully \n",__LINE__);

#ifdef AC3DEC_DEBUGMEM
	APP_IPRINT("\n-Printing memory not delete-\n");
    for(r=0;r<500;r++)
	{
        if (lines[r]!=0){
             APP_IPRINT(" --->%d Bytes allocated on %p File:%s Line: %d\n",bytes[r],arr[r],file[r],lines[r]);
        }

    }
#endif


EXIT:
	if(bInvalidState==OMX_TRUE)
	{
#ifndef USE_BUFFER

		error = FreeAllResources(*pHandle,
								pInputBufferHeader[0],
								pOutputBufferHeader[0],
								numofinbuff,
								numofoutbuff,
								fIn,fOut,ListHeader);
#else
		error = freeAllUseResources(*pHandle,
									pInputBuffer,
									pOutputBuffer,
									numofinbuff,
									numofoutbuff,
									fIn,fOut,ListHeader);
#endif
	}
#ifdef OMX_GETTIME
  GT_END("AC3_DEC test <End>");
  OMX_ListDestroy(pListHead);
#endif

	APP_IPRINT("%d :: APP: freeAllUseResources returned Successfully \n",__LINE__);
    return error;
}


/*-------------------------------------------------------------------*/
/**
  *  mymalloc() function to perform dynamic memory allocation.
  *
  * @param size         			Size of memory requested
  * @param ListHeader		Top pointer of the linked List
  *
  * @retval p   				Pointer to the allocated memory
  *
  **/
/*-------------------------------------------------------------------*/

void * mymalloc(int size,ListMember** ListHeader)
{
   int error=0;
   void *p;
   p = malloc(size);

   if(p==NULL)
   	{
       APP_EPRINT("APP: Memory not available\n");
       exit(1);
    }
   else
   	{
		error = AddMemberToList(p,ListHeader);
		if(error)
			exit(1);
	    return p;
    }
}

 /*-------------------------------------------------------------------*/
 /**
   *  myfree() function to free dynamic memory allocated.
   *
   * @param dp				 Dinamic memory pointer to be freed
   * @param ListHeader		 Top pointer of the linked List
   *
   * @retval OMX_ErrorNone	 Success on freeing memory
   *
   **/
 /*-------------------------------------------------------------------*/

int myfree(void *dp, ListMember** ListHeader)
{
	  int error=0;
	  error = FreeListMember(dp, ListHeader);
     /* free(dp);  */
	  if (error)
	  	APP_EPRINT("APP: Error freeing \n");

	  return error;
}


/*-------------------------------------------------------------------*/
/**
  *  FreeAllResources() function that release all allocated resources when an important error is produced.
 * 					 Buffers were allocated by component
  *
  * @parameters  pointers  from most of allocated resources
  *
  *
  * @retval OMX_ErrorNone		Success on freeing resources
  *
  **/
/*-------------------------------------------------------------------*/

OMX_ERRORTYPE FreeAllResources( OMX_HANDLETYPE pHandle,
			                OMX_BUFFERHEADERTYPE* pBufferIn,
			                OMX_BUFFERHEADERTYPE* pBufferOut,
			                int NIB, int NOB,
			                FILE* fIn, FILE* fOut,
			                ListMember* ListHeader)
{
	APP_DPRINT("%d:: APP: Freeing all resources by state invalid \n",__LINE__);
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_U16 i;
	for(i=0; i < NIB; i++)
	{
		   APP_DPRINT("%d :: APP: About to free pInputBufferHeader[%d]\n",__LINE__, i);
		   eError = OMX_FreeBuffer(pHandle, INPUT_PORT, pBufferIn+i);
		 	if(eError != OMX_ErrorNone)
			{
				APP_DPRINT("APP: Error:  Freebuffer by MMU_Fault %X\n", eError);
				goto EXIT_ERROR;
			}

	}
	for(i=0; i < NOB; i++)
	{
		   APP_DPRINT("%d :: APP: About to free pOutputBufferHeader[%d]\n",__LINE__, i);
		   eError = OMX_FreeBuffer(pHandle, OUTPUT_PORT, pBufferOut+i);
		   if(eError != OMX_ErrorNone)
		   {
			   APP_DPRINT("APP: Error:	Freebuffer by MMU_Fault %X\n", eError);
			   goto EXIT_ERROR;
		   }
	}

	/* Freeing Linked list */
	if(ListHeader != NULL)
	eError= CleanList(&ListHeader);
	if( (eError != OMX_ErrorNone))
	{
		APP_DPRINT("APP: Error in CleanList function\n");
		goto EXIT_ERROR;
	}

	pthread_cond_destroy(&WaitForStateMutex.cond);
	pthread_mutex_destroy(&WaitForStateMutex.Mymutex);

    eError = close (IpBuf_Pipe[0]);
    eError = close (IpBuf_Pipe[1]);
    eError = close (OpBuf_Pipe[0]);
    eError = close (OpBuf_Pipe[1]);
    APP_DPRINT("APP: Closing OpBuf_Pipe \n");

	if(fOut != NULL)
	{
		fclose(fOut);
		fOut=NULL;
	}

	if(fIn != NULL)
	{	fclose(fIn);
		fIn=NULL;
	}

	eError = TIOMX_FreeHandle(pHandle);
	if( (eError != OMX_ErrorNone))
	{
		APP_EPRINT("APP: Error in Free Handle function\n");
		goto EXIT_ERROR;
	}
	eError = TIOMX_Deinit();

EXIT_ERROR:

	return eError;
}


/*-------------------------------------------------------------------*/
/**
  *  freeAllUseResources() function that release all allocated resources from APP
  * 						when an important error is produced. Buffers were allocated by App.
  *
  * @parameters  pointers  from most of allocated resources
  *
  *
  * @retval OMX_ErrorNone		Success on freeing resources
  *
  **/
/*-------------------------------------------------------------------*/

#ifdef USE_BUFFER

OMX_ERRORTYPE freeAllUseResources(OMX_HANDLETYPE pHandle,
							OMX_U8* UseInpBuf[],
							OMX_U8* UseOutBuf[],
							int NIB,int NOB,
							FILE* fIn, FILE* fOut,
							ListMember* ListHeader)
{

		OMX_ERRORTYPE eError = OMX_ErrorNone;
		OMX_U16 i;
		APP_DPRINT("%d ::APP: Freeing all resources by state invalid \n",__LINE__);
    	/* free the UseBuffers */
	    for(i=0; i < NIB; i++)
		{

			if( UseInpBuf[i] != NULL )
			{
			   UseInpBuf[i] = UseInpBuf[i] - 128;
			   APP_DPRINT("%d :: [TESTAPPFREE] pInputBuffer[%d] = %p\n",__LINE__,i,(UseInpBuf[i]));
			   if(UseInpBuf[i] != NULL)
			   {
				  newfree(UseInpBuf[i]);
				  UseInpBuf[i] = NULL;
			   }
			}
		}

	    for(i=0; i < NOB; i++)
		{
			if (UseOutBuf[i] != NULL)
			{
			   UseOutBuf[i] = UseOutBuf[i] - 128;
			   APP_DPRINT("%d :: [TESTAPPFREE] pOutputBuffer[%d] = %p\n",__LINE__,i, UseOutBuf[i]);
			   if(UseOutBuf[i] != NULL)
			   {
				  newfree(UseOutBuf[i]);
				  UseOutBuf[i] = NULL;
			   }
			}
		}

		/* Freeing Linked list */
	if(ListHeader != NULL)
		eError= CleanList(&ListHeader);
		if( (eError != OMX_ErrorNone))
		{
			APP_DPRINT("APP: Error in CleanList function\n");
			goto EXIT_ERROR;
		}

		pthread_cond_destroy(&WaitForStateMutex.cond);
		pthread_mutex_destroy(&WaitForStateMutex.Mymutex);

		eError = close (IpBuf_Pipe[0]);
		eError = close (IpBuf_Pipe[1]);
		eError = close (OpBuf_Pipe[0]);
		eError = close (OpBuf_Pipe[1]);
		APP_DPRINT("APP: Closing OpBuf_Pipe \n");

		if (fOut != NULL)	/* Could have been closed  previously */
		{
			fclose(fOut);
			fOut=NULL;
		}

		if (fIn != NULL)
		{	fclose(fIn);
			fIn=NULL;
		}

		eError = TIOMX_FreeHandle(pHandle);
		if( (eError != OMX_ErrorNone))
		{
			APP_EPRINT("APP: Error in Free Handle function\n");
			goto EXIT_ERROR;
		}
		eError = TIOMX_Deinit();

EXIT_ERROR:

		return eError;

}

#endif





/*-------------------------------------------------------------------*/
/**
  *  AddMemberToList() Adds a memeber to the list for allocated memory pointers
  *
  * @param ptr         			memory pointer to add to the member list
  * @param ListHeader		Top pointer of the List
  *   *
  * @retval OMX_ErrorNone   					 Success, member added
 *               OMX_ErrorInsufficientResources		 Memory  failure
  **/
/*-------------------------------------------------------------------*/

OMX_ERRORTYPE AddMemberToList(void* ptr, ListMember** ListHeader)
{
	int Error = OMX_ErrorNone;										/* No Error  */
	static int InstanceCounter = 0;
	ListMember* temp;
	if(*ListHeader == NULL)
	{
		InstanceCounter =0;											/* reset counter */
	}

	temp = (ListMember*)malloc(sizeof(ListMember));					/* New Member */
	if(NULL == temp)
	{
		APP_EPRINT("%d :: App: Malloc Failed\n",__LINE__);
		Error = OMX_ErrorInsufficientResources;						/* propper Error */
		goto EXIT_ERROR;
	}
	APP_DPRINT("\nNew Instance created pointer : %p \n",temp);

	APP_DPRINT("Header parameter pointer : %p \n",*ListHeader);
	temp->NextListMember 		= *ListHeader;						/* Adding the new member */
	APP_DPRINT("Next linked pointer  : %p \n",temp->NextListMember);
	temp->ListCounter			= ++InstanceCounter;				/* Pre-increment */
	APP_DPRINT("Instance counter %d \n",temp->ListCounter);
	temp->Struct_Ptr 			= ptr;								/* Saving passed pointer (allocated memory)*/
	APP_DPRINT("Parameter pointer to save : %p \n",ptr);
	*ListHeader				 	= temp;								/* saving the Header */
	APP_DPRINT("New Header pointer : %p \n",*ListHeader);


EXIT_ERROR:
	return Error;
}


/*-------------------------------------------------------------------*/
/**
  * CleanList() Frees the complete Linked list from memory
  *
  *  @param ListHeader				Root List  pointer
  *
  * @retval OMX_ErrorNone   			 Success, memory freed
 *               OMX_ErrorUndefined		 Memory  failure
  **/
/*-------------------------------------------------------------------*/

OMX_ERRORTYPE CleanList(ListMember** ListHeader)
{
	int Error = OMX_ErrorNone;										/* No Error  */
	int ListCounter=0;
	ListMember* Temp;												/* Temporal pointer */
	ListCounter = (*ListHeader)->ListCounter;

	while (*ListHeader != NULL)
	{
		APP_DPRINT("\nNum Instance to free %d \n",(*ListHeader)->ListCounter);
		Temp=(*ListHeader)->NextListMember;
		if( (*ListHeader)->Struct_Ptr != NULL)						/* Ensure there is something to free */
		{
			APP_DPRINT(" Struct to free %p \n",(*ListHeader)->Struct_Ptr);
			free((*ListHeader)->Struct_Ptr);						/* Free memory */
			(*ListHeader)->Struct_Ptr = NULL;						/* Point to NULL */
		}
		else
		{
			APP_DPRINT("APP:  Warning: Content has alreadey been freed \n");
		}

		APP_DPRINT("freeing List Member %p \n",*ListHeader);
		free(*ListHeader);											/* Free member (first) */
		*ListHeader=Temp;											/* Asign Next member to header  */
	}
	if(*ListHeader== NULL)
	{
		APP_DPRINT("%d :: App: Freed the complete list: Header = %p \n",__LINE__,*ListHeader);
	}
	else
	{
		APP_EPRINT("%d :: App: Error Freeing List \n",__LINE__);
		Error =OMX_ErrorUndefined;
	}
	return Error;
}


/*-------------------------------------------------------------------*/
/**
  * FreeListMember() Frees  a member from the Linked list with its member allocated memory
  *
  *  @param ListHeader				 Root List  pointer
  *
  * @retval OMX_ErrorNone   			 Success, member freed
 *              OMX_ErrorUndefined		 Memory  failure
  **/
/*-------------------------------------------------------------------*/

OMX_ERRORTYPE FreeListMember(void* ptr, ListMember** ListHeader)
{

	int Error = OMX_ErrorNone;													/* No Error  */
	int ListCounter=0;
	ListMember* Temp= NULL;															/* Temporal pointer */
	ListMember*	Backtemp=NULL;														/* follower of temporal pointer */
	ListCounter = (*ListHeader)->ListCounter;
	int count=0;

	Temp =(*ListHeader);
	if (Temp != NULL)
	{
		while(Temp != NULL && Temp->Struct_Ptr != ptr)
		{	Backtemp = Temp;													/* One member back  */
			Temp = Temp->NextListMember;										/* next member */
			count++;
		}
		APP_DPRINT("Search ends: %p \n",Temp);
		if (Temp != NULL)														/* found it */
		{
			if (count > 0)														/* inside the List */
			{
				Backtemp->NextListMember = Temp->NextListMember;
				APP_DPRINT("About to free Content of List member: %p \n",Temp->Struct_Ptr);
				free(Temp->Struct_Ptr); 										/* free content */
				APP_DPRINT("About to free List member: %p Number: %d \n",Temp,Temp->ListCounter);
				free(Temp); 													/* free element */
			}
			else if (count == 0)												/* it was the first */
			{
					APP_DPRINT("About to free FIRST List member: %p \n",*ListHeader);
					Temp = Temp->NextListMember;								/* next member */
					free((*ListHeader)->Struct_Ptr);							/* free content */
					free(*ListHeader);											/* Free member (first) */
					*ListHeader=Temp;											/* Asign Next member to header  */
			}
		}
		else{																	/* Not found it */
			APP_DPRINT("Nothing to free \n");
		}

	}
	else{
		APP_DPRINT("List is empty \n");
	}

	return Error;

}


void EmptyOutputBufferPipe()
{
        OMX_S32 ret = 0;
        OMX_ERRORTYPE eError = OMX_ErrorNone;
        OMX_BUFFERHEADERTYPE *pOutputBufHeader;
        int status;
        struct timespec tv;
        fd_set rfds;
        int fdmax;
        fdmax = OpBuf_Pipe[0];


        while(1)
                {
                 FD_ZERO (&rfds);
                 FD_SET(OpBuf_Pipe[0], &rfds);
                 tv.tv_sec = 1;
                 tv.tv_nsec = 0;

                 status = pselect (fdmax+1, &rfds, NULL, NULL, &tv);
                 if(status == 0)
                 {
                     APP_EPRINT( "EmptyOutputBufferPipe:  Time Out !!!!! \n");
                     break;
                 }

                 APP_IPRINT("EmptyOutputBufferPipe: Inside whil loop \n");
                 if (FD_ISSET (OpBuf_Pipe[0], &rfds))
                 {

                                APP_IPRINT(" EmptyOutputBufferPipe: calling read on Output buffer pipe \n");
                                ret = read(OpBuf_Pipe[0], &pOutputBufHeader, sizeof(pOutputBufHeader));
                                if (ret == -1)
                                {
                                        APP_EPRINT( " Error while reading from the Output Buffer pipe\n");
                                        eError = OMX_ErrorHardware;
                                        exit(1);
                                }

                 }
                 else
                 {
                                break;
	 	 }
		}
}


void DumptoOutputFileFromOutputBufferPipe(FILE *fOut)
{
        OMX_S32 ret = 0;
        OMX_ERRORTYPE eError = OMX_ErrorNone;
        OMX_BUFFERHEADERTYPE *pOutputBufHeader;
        int status;
        struct timespec tv;
        fd_set rfds;
        int fdmax;
        fdmax = OpBuf_Pipe[0];


        while(1)
                {
                 FD_ZERO (&rfds);
                 FD_SET(OpBuf_Pipe[0], &rfds);
                 tv.tv_sec = 1;
                 tv.tv_nsec = 0;

                 status = pselect (fdmax+1, &rfds, NULL, NULL, &tv);
                 if(status == 0)
                 {
                     APP_EPRINT( "DumptoOutputFileFromOutputBufferPipe :  Time Out !!!!! \n");
                     break;
                 }

                 APP_IPRINT("DumptoOutputFileFromOutputBufferPipe : Inside whil loop \n");
                 if (FD_ISSET (OpBuf_Pipe[0], &rfds))
                 {

                      APP_IPRINT(" DumptoOutputFileFromOutputBufferPipe : calling read on Output buffer pipe \n");
                      ret = read(OpBuf_Pipe[0], &pOutputBufHeader, sizeof(pOutputBufHeader));
                      if (ret == -1)
                      {
                        APP_EPRINT( " Error while reading from the Output Buffer pipe\n");
                        exit(1);
                      }
    	    			fwrite (pOutputBufHeader->pBuffer, 1, (pOutputBufHeader->nFilledLen), fOut);

                 }
                 else
                 {
                                break;
	 	 }
		}
                      APP_IPRINT(" DumptoOutputFileFromOutputBufferPipe : Exiting \n");
}



