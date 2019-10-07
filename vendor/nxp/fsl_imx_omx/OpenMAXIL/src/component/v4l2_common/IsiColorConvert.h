#ifndef ISICOLORCONVERT_H
#define ISICOLORCONVERT_H
#include "V4l2Device.h"
#include "V4l2Object.h"
#include "VThread.h"
#include "Process3.h"
class IsiColorConvert;

typedef struct {
    OMX_U32 width;
    OMX_U32 height;
    OMX_U32 format;
    OMX_U32 stride;
    OMX_U32 bufferSize;
}ISI_FORMAT;


typedef OMX_ERRORTYPE ISI_CALLBACKTYPE(IsiColorConvert *pProcess,OMX_PTR pAppData);

class IsiColorConvert{
public:
    IsiColorConvert();
    ~IsiColorConvert();
    OMX_ERRORTYPE Create();
    //OMX_ERRORTYPE Start();
    OMX_ERRORTYPE Flush(); 
    //OMX_ERRORTYPE Destroy();

    OMX_ERRORTYPE AddInputFrame(OMX_BUFFERHEADERTYPE *bufHdlr);
    OMX_ERRORTYPE GetInputBuffer(OMX_BUFFERHEADERTYPE **bufHdlr);
    OMX_ERRORTYPE GetInputReturnBuffer(OMX_BUFFERHEADERTYPE **bufHdlr);

    OMX_ERRORTYPE AddOutputFrame(DmaBufferHdr *bufHdlr);
    OMX_ERRORTYPE GetOutputBuffer(DmaBufferHdr **bufHdlr);
    OMX_ERRORTYPE GetOutputReturnBuffer(DmaBufferHdr **bufHdlr);

    friend void *colorConvertThreadHandler(void *arg);
 
    OMX_ERRORTYPE SetCallbackFunc(ISI_CALLBACKTYPE *callback, OMX_PTR appData);

    OMX_ERRORTYPE ConfigInput(ISI_FORMAT* fmt);
    OMX_ERRORTYPE ConfigOutput(ISI_FORMAT* fmt);
    
    OMX_ERRORTYPE SetBufferCount(OMX_U32 in_cnt, OMX_U32 out_cnt);
    OMX_BOOL GetEos();

private:
    OMX_U8 devName[32];
    ISI_FORMAT sInFormat;
    ISI_FORMAT sOutFormat;

    OMX_S32 nFd;

    V4l2Dev* pColorDev;

    V4l2Object *inObj;
    V4l2Object *outObj;

    VThread *pThread;
    //fsl_osal_mutex sMutex;
    ISI_CALLBACKTYPE* pCallback;
    OMX_PTR pAppData;

    OMX_BOOL bCreated;
    OMX_BOOL bPrepared;
    OMX_BOOL bEos;
    OMX_U32 nInputBufferCnt;
    OMX_U32 nOutputBufferCnt;
    OMX_U32 nCnt;
    OMX_U64 nInputCnt;
    OMX_U64 nOutputCnt;


    OMX_ERRORTYPE setParam();

    OMX_ERRORTYPE prepare();
    OMX_U32 getV4l2Format(OMX_U32 color_format);
};
#endif
