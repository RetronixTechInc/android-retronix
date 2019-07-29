/* //device/system/reference-ril/reference-ril.c
**
** Copyright (C) 2011-2014 Freescale Semiconductor, Inc. 
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*
 * Partly base on the work contributed by
 * Author: Christian Bejram <christian.bejram@stericsson.com>
 * Author: Kim Tommy Humborstad <kim.tommy.humborstad@stericsson.com>
*/
/* Copyright (C) ST-Ericsson AB 2008-2010 */

#include <telephony/ril_cdma_sms.h>
#include <telephony/librilutils.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <alloca.h>
#include "atchannel.h"
#include "at_tok.h"
#include "misc.h"
#include <getopt.h>
#include <sys/socket.h>
#include <cutils/sockets.h>
#include <cutils/properties.h>
#include <termios.h>
#include <sys/system_properties.h>

#include "ril.h"
#include "hardware/qemu_pipe.h"
#include <linux/if.h>

#undef LOG_TAG
#define LOG_TAG "RIL"
#include <utils/Log.h>

static void *noopRemoveWarning( void *a ) { return a; }
#define RIL_UNUSED_PARM(a) noopRemoveWarning((void *)&(a));
#include "runtime/runtime.h"
#include "fcp_parser.h"

#define MAX_AT_RESPONSE 0x1000

/* property to control the delay (in seconds) to initialize RIL */
#define RIL_DELAY_TIME "gsm.ril.delay"

/* pathname returned from RIL_REQUEST_SETUP_DATA_CALL / RIL_REQUEST_SETUP_DEFAULT_PDP */
#define PPP_TTY_PATH "ppp0"
#define PPP_OPERSTATE_PATH "/sys/class/net/ppp0/operstate"
#define SERVICE_PPPD_GPRS "pppd_gprs"
#define PROPERTY_PPPD_EXIT_CODE "net.gprs.ppp-exit"
// Max wait time to 2*10 secondes for ppp enable
#define POLL_PPP_SYSFS_SECONDS 2
#define POLL_PPP_SYSFS_RETRY   10

// Default MTU value
#define DEFAULT_MTU 1500

#ifdef USE_TI_COMMANDS

// Enable a workaround
// 1) Make incoming call, do not answer
// 2) Hangup remote end
// Expected: call should disappear from CLCC line
// Actual: Call shows as "ACTIVE" before disappearing
#define WORKAROUND_ERRONEOUS_ANSWER 1

// Some varients of the TI stack do not support the +CGEV unsolicited
// response. However, they seem to send an unsolicited +CME ERROR: 150
#define WORKAROUND_FAKE_CGEV 1
#endif

/* Modem Technology bits */
#define MDM_GSM         0x01
#define MDM_WCDMA       0x02
#define MDM_CDMA        0x04
#define MDM_EVDO        0x08
#define MDM_LTE         0x10

typedef struct {
    int supportedTechs; // Bitmask of supported Modem Technology bits
    int currentTech;    // Technology the modem is currently using (in the format used by modem)
    int isMultimode;

    // Preferred mode bitmask. This is actually 4 byte-sized bitmasks with different priority values,
    // in which the byte number from LSB to MSB give the priority.
    //
    //          |MSB|   |   |LSB
    // value:   |00 |00 |00 |00
    // byte #:  |3  |2  |1  |0
    //
    // Higher byte order give higher priority. Thus, a value of 0x0000000f represents
    // a preferred mode of GSM, WCDMA, CDMA, and EvDo in which all are equally preferrable, whereas
    // 0x00000201 represents a mode with GSM and WCDMA, in which WCDMA is preferred over GSM
    int32_t preferredNetworkMode;
    int subscription_source;

} ModemInfo;

static ModemInfo *sMdmInfo;
// TECH returns the current technology in the format used by the modem.
// It can be used as an l-value
#define TECH(mdminfo)                 ((mdminfo)->currentTech)
// TECH_BIT returns the bitmask equivalent of the current tech
#define TECH_BIT(mdminfo)            (1 << ((mdminfo)->currentTech))
#define IS_MULTIMODE(mdminfo)         ((mdminfo)->isMultimode)
#define TECH_SUPPORTED(mdminfo, tech) ((mdminfo)->supportedTechs & (tech))
#define PREFERRED_NETWORK(mdminfo)    ((mdminfo)->preferredNetworkMode)
// CDMA Subscription Source
#define SSOURCE(mdminfo)              ((mdminfo)->subscription_source)

static int net2modem[] = {
    MDM_GSM | MDM_WCDMA,                                 // 0  - GSM / WCDMA Pref
    MDM_GSM,                                             // 1  - GSM only
    MDM_WCDMA,                                           // 2  - WCDMA only
    MDM_GSM | MDM_WCDMA,                                 // 3  - GSM / WCDMA Auto
    MDM_CDMA | MDM_EVDO,                                 // 4  - CDMA / EvDo Auto
    MDM_CDMA,                                            // 5  - CDMA only
    MDM_EVDO,                                            // 6  - EvDo only
    MDM_GSM | MDM_WCDMA | MDM_CDMA | MDM_EVDO,           // 7  - GSM/WCDMA, CDMA, EvDo
    MDM_LTE | MDM_CDMA | MDM_EVDO,                       // 8  - LTE, CDMA and EvDo
    MDM_LTE | MDM_GSM | MDM_WCDMA,                       // 9  - LTE, GSM/WCDMA
    MDM_LTE | MDM_CDMA | MDM_EVDO | MDM_GSM | MDM_WCDMA, // 10 - LTE, CDMA, EvDo, GSM/WCDMA
    MDM_LTE,                                             // 11 - LTE only
};

static int32_t net2pmask[] = {
    MDM_GSM | (MDM_WCDMA << 8),                          // 0  - GSM / WCDMA Pref
    MDM_GSM,                                             // 1  - GSM only
    MDM_WCDMA,                                           // 2  - WCDMA only
    MDM_GSM | MDM_WCDMA,                                 // 3  - GSM / WCDMA Auto
    MDM_CDMA | MDM_EVDO,                                 // 4  - CDMA / EvDo Auto
    MDM_CDMA,                                            // 5  - CDMA only
    MDM_EVDO,                                            // 6  - EvDo only
    MDM_GSM | MDM_WCDMA | MDM_CDMA | MDM_EVDO,           // 7  - GSM/WCDMA, CDMA, EvDo
    MDM_LTE | MDM_CDMA | MDM_EVDO,                       // 8  - LTE, CDMA and EvDo
    MDM_LTE | MDM_GSM | MDM_WCDMA,                       // 9  - LTE, GSM/WCDMA
    MDM_LTE | MDM_CDMA | MDM_EVDO | MDM_GSM | MDM_WCDMA, // 10 - LTE, CDMA, EvDo, GSM/WCDMA
    MDM_LTE,                                             // 11 - LTE only
};

static int is3gpp2(int radioTech) {
    switch (radioTech) {
        case RADIO_TECH_IS95A:
        case RADIO_TECH_IS95B:
        case RADIO_TECH_1xRTT:
        case RADIO_TECH_EVDO_0:
        case RADIO_TECH_EVDO_A:
        case RADIO_TECH_EVDO_B:
        case RADIO_TECH_EHRPD:
            return 1;
        default:
            return 0;
    }
}

typedef enum {
    SIM_ABSENT = 0,
    SIM_NOT_READY = 1,
    SIM_READY = 2, /* SIM_READY means the radio state is RADIO_STATE_SIM_READY */
    SIM_PIN = 3,
    SIM_PUK = 4,
    SIM_NETWORK_PERSONALIZATION = 5,
    RUIM_ABSENT = 6,
    RUIM_NOT_READY = 7,
    RUIM_READY = 8,
    RUIM_PIN = 9,
    RUIM_PUK = 10,
    RUIM_NETWORK_PERSONALIZATION = 11,
    SIM_PIN2 = 12, /* SIM PIN2 lock */
    SIM_PUK2 = 13, /* SIM PUK2 lock */
    SIM_NETWORK_SUBSET_PERSO = 14, /* Network Subset Personalization */
    SIM_SERVICE_PROVIDER_PERSO = 15, /* Service Provider Personalization */
    SIM_CORPORATE_PERSO = 16, /* Corporate Personalization */
    SIM_SIM_PERSO = 17, /* SIM/USIM Personalization */
    SIM_STERICSSON_LOCK = 18, /* ST-Ericsson Extended SIM */
    SIM_BLOCKED = 19, /* SIM card is blocked */
    SIM_PERM_BLOCKED = 20, /* SIM card is permanently blocked */
    SIM_NETWORK_PERSO_PUK = 21, /* Network Personalization PUK */
    SIM_NETWORK_SUBSET_PERSO_PUK = 22, /* Network Subset Perso. PUK */
    SIM_SERVICE_PROVIDER_PERSO_PUK = 23,/* Service Provider Perso. PUK */
    SIM_CORPORATE_PERSO_PUK = 24, /* Corporate Personalization PUK */
    SIM_SIM_PERSO_PUK = 25, /* SIM Personalization PUK (unused) */
    SIM_PUK2_PERM_BLOCKED = 26 /* PUK2 is permanently blocked */
} SIM_Status;

typedef enum {
    UICC_TYPE_UNKNOWN,
    UICC_TYPE_SIM,
    UICC_TYPE_USIM,
} UICC_Type;

/* Huawei E770W subsys_mode spec. */
typedef enum{
	SUB_SYSMODE_NO_SERVICE = 0,
	SUB_SYSMODE_GSM = 1,
	SUB_SYSMODE_GPRS = 2,
	SUB_SYSMODE_EDGE = 3,
	SUB_SYSMODE_WCDMA = 4,
	SUB_SYSMODE_HSDPA = 5,
	SUB_SYSMODE_HSUPA = 6,
	SUB_SYSMODE_HSUPA_HSDPA = 7,
	SUB_SYSMODE_INVALID = 8,
}SUB_SYSMODE;

static void onRequest (int request, void *data, size_t datalen, RIL_Token t);
static RIL_RadioState currentState();
static int onSupports (int requestCode);
static void onCancel (RIL_Token t);
static const char *getVersion();
static int isRadioOn();
static SIM_Status getSIMStatus();
static int getCardStatus(RIL_CardStatus_v6 **pp_card_status);
static void freeCardStatus(RIL_CardStatus_v6 *p_card_status);
static void onDataCallListChanged(void *param);
static void onNewSmsArrived(void *param);
static void onNewSmsReportArrived(void *param);

extern const char * requestToString(int request);

/*** Static Variables ***/
static const RIL_RadioFunctions s_callbacks = {
    RIL_VERSION,
    onRequest,
    currentState,
    onSupports,
    onCancel,
    getVersion
};

#ifdef RIL_SHLIB
static const struct RIL_Env *s_rilenv;

#define RIL_onRequestComplete(t, e, response, responselen) s_rilenv->OnRequestComplete(t,e, response, responselen)
#define RIL_onUnsolicitedResponse(a,b,c) s_rilenv->OnUnsolicitedResponse(a,b,c)
#define RIL_requestTimedCallback(a,b,c) s_rilenv->RequestTimedCallback(a,b,c)
#endif

static RIL_RadioState sState = RADIO_STATE_UNAVAILABLE;

static pthread_mutex_t s_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_state_cond = PTHREAD_COND_INITIALIZER;

static int s_port = -1;
static const char * s_device_path = NULL;
#ifdef HAVE_DATA_DEVICE
static const char * s_data_device_path = NULL;
#endif
static int          s_device_socket = 0;

// flag for pppd status. 1: started; 0: stoped 
static int pppd = 0;

/* trigger change to this with s_state_cond */
static int s_closed = 0;

static int sFD;     /* file desc of AT channel */
static char sATBuffer[MAX_AT_RESPONSE+1];
static char *sATBufferCur = NULL;
static int smsStorageIndex = 0;
//"SM", "ME", "SR"
static char smsReadStorage[3];

static const struct timeval TIMEVAL_SIMPOLL = {1,0};
static const struct timeval TIMEVAL_CALLSTATEPOLL = {0,500000};
static const struct timeval TIMEVAL_0 = {0,0};
static struct timeval TIMEVAL_DELAYINIT = {0,0}; // will be set according to property value

static int s_ims_registered  = 0;        // 0==unregistered
static int s_ims_services    = 1;        // & 0x1 == sms over ims supported
static int s_ims_format    = 1;          // FORMAT_3GPP(1) vs FORMAT_3GPP2(2);
static int s_ims_cause_retry = 0;        // 1==causes sms over ims to temp fail
static int s_ims_cause_perm_failure = 0; // 1==causes sms over ims to permanent fail
static int s_ims_gsm_retry   = 0;        // 1==causes sms over gsm to temp fail
static int s_ims_gsm_fail    = 0;        // 1==causes sms over gsm to permanent fail

#ifdef WORKAROUND_ERRONEOUS_ANSWER
// Max number of times we'll try to repoll when we think
// we have a AT+CLCC race condition
#define REPOLL_CALLS_COUNT_MAX 4

// Line index that was incoming or waiting at last poll, or -1 for none
static int s_incomingOrWaitingLine = -1;
// Number of times we've asked for a repoll of AT+CLCC
static int s_repollCallsCount = 0;
// Should we expect a call to be answered in the next CLCC?
static int s_expectAnswer = 0;
#endif /* WORKAROUND_ERRONEOUS_ANSWER */

static int s_cell_info_rate_ms = INT_MAX;
static int s_mcc = 0;
static int s_mnc = 0;
static int s_lac = 0;
static int s_cid = 0;
static int sUnsolictedCREG_failed = 0;
static int sUnsolictedCGREG_failed = 0;
/*
 * Some modem, like Huawei E770W, doesn't contain network type field in CGREG response.
 *  We need to get network type through other way.
 */
static int sNetworkType = 0; //Unknown
static int sLastNetworkType = -1; //Unknown

static void pollSIMState (void *param);
static void setRadioState(RIL_RadioState newState);
static void setRadioTechnology(ModemInfo *mdm, int newtech);
static int query_ctec(ModemInfo *mdm, int *current, int32_t *preferred);
static int parse_technology_response(const char *response, int *current, int32_t *preferred);
static int techFromModemType(int mdmtype);
static int convertRILRadioTechnology(int subsys_mode, int modem_type);

static int clccStateToRILState(int state, RIL_CallState *p_state)

{
    switch(state) {
        case 0: *p_state = RIL_CALL_ACTIVE;   return 0;
        case 1: *p_state = RIL_CALL_HOLDING;  return 0;
        case 2: *p_state = RIL_CALL_DIALING;  return 0;
        case 3: *p_state = RIL_CALL_ALERTING; return 0;
        case 4: *p_state = RIL_CALL_INCOMING; return 0;
        case 5: *p_state = RIL_CALL_WAITING;  return 0;
        default: return -1;
    }
}

/**
 * Note: directly modified line and has *p_call point directly into
 * modified line
 */
static int callFromCLCCLine(char *line, RIL_Call *p_call)
{
        //+CLCC: 1,0,2,0,0,\"+18005551212\",145
        //     index,isMT,state,mode,isMpty(,number,TOA)?

    int err;
    int state;
    int mode;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(p_call->index));
    if (err < 0) goto error;

    err = at_tok_nextbool(&line, &(p_call->isMT));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &state);
    if (err < 0) goto error;

    err = clccStateToRILState(state, &(p_call->state));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &mode);
    if (err < 0) goto error;

    p_call->isVoice = (mode == 0);

    err = at_tok_nextbool(&line, &(p_call->isMpty));
    if (err < 0) goto error;

    if (at_tok_hasmore(&line)) {
        err = at_tok_nextstr(&line, &(p_call->number));

        /* tolerate null here */
        if (err < 0) return 0;

        // Some lame implementations return strings
        // like "NOT AVAILABLE" in the CLCC line
        if (p_call->number != NULL
            && 0 == strspn(p_call->number, "+0123456789")
        ) {
            p_call->number = NULL;
        }

        err = at_tok_nextint(&line, &p_call->toa);
        if (err < 0) goto error;
    }

    p_call->uusInfo = NULL;

    return 0;

error:
    RLOGE("invalid CLCC line\n");
    return -1;
}


/** do post-AT+CFUN=1 initialization */
static void onRadioPowerOn()
{
#ifdef USE_TI_COMMANDS
    /*  Must be after CFUN=1 */
    /*  TI specific -- notifications for CPHS things such */
    /*  as CPHS message waiting indicator */

    at_send_command("AT%CPHS=1", NULL);

    /*  TI specific -- enable NITZ unsol notifs */
    at_send_command("AT%CTZV=1", NULL);
#endif

    pollSIMState(NULL);
}

/** do post- SIM ready initialization */
static void onSIMReady()
{
    at_send_command_singleline("AT+CSMS=1", "+CSMS:", NULL);
    /*
     * Always send SMS messages directly to the TE
     *
     * mode = 1 // discard when link is reserved (link should never be
     *             reserved)
     * mt = 2   // most messages routed to TE
     * bm = 2   // new cell BM's routed to TE
     * ds = 1   // Status reports routed to TE
     * bfr = 1  // flush buffer
     */
    if (current_modem_type != HUAWEI_MODEM)
        at_send_command("AT+CNMI=1,2,2,1,1", NULL);
    else
        at_send_command("AT+CNMI=2,1,2,2,0", NULL);

    at_send_command("AT+CPMS=\"ME\",\"ME\",\"ME\"",NULL);
    at_send_command("AT+CMGD=1,4",NULL);
}

static void requestRadioPower(void *data, size_t datalen, RIL_Token t)
{
    int onOff;

    int err;
    ATResponse *p_response = NULL;

    assert (datalen >= sizeof(int *));
    onOff = ((int *)data)[0];

    if (onOff == 0 && sState != RADIO_STATE_OFF) {
	    if (current_modem_type == AMAZON_MODEM)
		    err = at_send_command("AT+CFUN=4", &p_response);
	    else
		    err = at_send_command("AT+CFUN=0", &p_response);
	    if (err < 0 || p_response->success == 0)
		    goto error;
	    setRadioState(RADIO_STATE_OFF);
    } else if (onOff > 0 && sState == RADIO_STATE_OFF) {
        err = at_send_command("AT+CFUN=1", &p_response);
        if (err < 0|| p_response->success == 0) {
            // Some stacks return an error when there is no SIM,
            // but they really turn the RF portion on
            // So, if we get an error, let's check to see if it
            // turned on anyway

            if (isRadioOn() != 1) {
                goto error;
            }
        }
        setRadioState(RADIO_STATE_ON);
    }

    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;
error:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestShutdown(RIL_Token t)
{
    int onOff;

    int err;
    ATResponse *p_response = NULL;

    if (sState != RADIO_STATE_OFF) {
        err = at_send_command("AT+CFUN=0", &p_response);
        setRadioState(RADIO_STATE_UNAVAILABLE);
    }

    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;
}

static void requestOrSendDataCallList(RIL_Token *t);

static void onDataCallListChanged(void *param __unused)
{
    requestOrSendDataCallList(NULL);
}

static void readNewSms(void *param, int response_index)
{
    ATResponse *p_response;
    ATLine *p_cur;
    int err;
    int index = *(int *)param;
    char *cmd;
    char *line;
    char oldReadStorage[3];
    char *storage;
    int setStorage = 0;
    //Fetch current read storage
    //Set the prefer read storage
    err = at_send_command_singleline("AT+CPMS?", "+CPMS:", &p_response);
    if (err != 0 || p_response->success == 0) {
        LOGE("Cannot read prefer SMS storage");
        return;
    }
    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0)
        goto error;
    err = at_tok_nextstr(&line, &storage);
    if (err < 0)
        goto error;
    memset(oldReadStorage, 0, 3);
    memcpy(oldReadStorage, storage, 2);
    LOGI("oldReadStorage %s, current storage %s",oldReadStorage,smsReadStorage);
    
    //Set the prefer read storage
    asprintf(&cmd, "AT+CPMS=\"%s\"", smsReadStorage);
    at_send_command(cmd,NULL);
    setStorage = 1;

    asprintf(&cmd, "AT+CMGR=%d", index);
    err = at_send_command_multiline(cmd, "+CMGR:", &p_response);
    if (err != 0 || p_response->success == 0) {
        LOGE("No sms store in ME");
        return;
    }
    
    //Read the sms pdu
    //<CR><LF>+CMGR:<stat>,[<reserved>],<length><CR><LF><pdu><CR><LF><CR><LF>OK<CR><LF>
    p_cur = p_response->p_intermediates;
    if(p_cur != NULL) {
        int state = 0;
        char *reserved = NULL;
        int lenght = 0;
        line = p_cur->line;

        err = at_tok_start(&line);
        if (err < 0)
            goto error;
        err = at_tok_nextint(&line, &state);
        if (err < 0)
            goto error;

        p_cur = p_cur->p_next;
        if(p_cur != NULL) {
            char *sms_pdu = NULL;
            sms_pdu = p_cur->line;
            if(sms_pdu != NULL) {
                RIL_onUnsolicitedResponse (
                    response_index,
                    sms_pdu, strlen(sms_pdu));
            }
        }
    }

    //Delete the sms in storage
    asprintf(&cmd, "AT+CMGD=%d", index);
    at_send_command(cmd,NULL);

error:
    //Need recover after read 
    if(setStorage){
        asprintf(&cmd, "AT+CPMS=\"%s\"", oldReadStorage);
        at_send_command(cmd,NULL);
    }

    at_response_free(p_response);
}

static void onNewSmsReportArrived(void *param)
{
    readNewSms(param,RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT);
}

static void onNewSmsArrived(void *param)
{
    readNewSms(param,RIL_UNSOL_RESPONSE_NEW_SMS);
}

static void requestDataCallList(void *data __unused, size_t datalen __unused, RIL_Token t)
{
    requestOrSendDataCallList(&t);
}

void onDeactiveDataCallList()
{
    RIL_Data_Call_Response_v4 *responses =
        alloca(sizeof(RIL_Data_Call_Response_v4));
    LOGI("onDeactiveDataCallList");
    //For n pdp context, or just one pdp context?
    //Always cid =1 ?
    //CDMADataConnectionTracker::onDataStateChanged()
    //or GSMDataConnectionTracker::onPdpStateChanged(), will handle this result

    responses->cid = 1;
    //LINK_INACTIVE = 0
    //LINK_DOWN = 1
    //LINK_UP = 2;
    responses->active = 0;
    responses->type = "";
    responses->apn = "";
    responses->address = "";

    RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED,
                                  responses,
                                  sizeof(RIL_Data_Call_Response_v4));
}

static void requestOrSendDataCallList(RIL_Token *t)
{
    ATResponse *p_response;
    ATLine *p_cur;
    int err;
    int n = 0;
    char *out;

    err = at_send_command_multiline ("AT+CGACT?", "+CGACT:", &p_response);
    if (err != 0 || p_response->success == 0) {
        if (t != NULL)
            RIL_onRequestComplete(*t, RIL_E_GENERIC_FAILURE, NULL, 0);
        else
            RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED,
                                      NULL, 0);
        return;
    }

    for (p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next)
        n++;

    RIL_Data_Call_Response_v11 *responses =
        alloca(n * sizeof(RIL_Data_Call_Response_v11));

    int i;
    for (i = 0; i < n; i++) {
        responses[i].status = -1;
        responses[i].suggestedRetryTime = -1;
        responses[i].cid = -1;
        responses[i].active = -1;
        responses[i].type = "";
        responses[i].ifname = "";
        responses[i].addresses = "";
        responses[i].dnses = "";
        responses[i].gateways = "";
        responses[i].pcscf = "";
        responses[i].mtu = 0;
    }

    RIL_Data_Call_Response_v11 *response = responses;
    for (p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
        char *line = p_cur->line;

        err = at_tok_start(&line);
        if (err < 0)
            goto error;

        err = at_tok_nextint(&line, &response->cid);
        if (err < 0)
            goto error;

        err = at_tok_nextint(&line, &response->active);
        if (err < 0)
            goto error;

        response++;
    }

    at_response_free(p_response);

    err = at_send_command_multiline ("AT+CGDCONT?", "+CGDCONT:", &p_response);
    if (err != 0 || p_response->success == 0) {
        if (t != NULL)
            RIL_onRequestComplete(*t, RIL_E_GENERIC_FAILURE, NULL, 0);
        else
            RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED,
                                      NULL, 0);
        return;
    }

    for (p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
        char *line = p_cur->line;
        int cid;

        err = at_tok_start(&line);
        if (err < 0)
            goto error;

        err = at_tok_nextint(&line, &cid);
        if (err < 0)
            goto error;

        for (i = 0; i < n; i++) {
            if (responses[i].cid == cid)
                break;
        }

        if (i >= n) {
            /* details for a context we didn't hear about in the last request */
            continue;
        }

        // Assume no error
        responses[i].status = 0;

        // type
        err = at_tok_nextstr(&line, &out);
        if (err < 0)
            goto error;
        responses[i].type = alloca(strlen(out) + 1);
        strcpy(responses[i].type, out);

        // APN ignored for v5
        err = at_tok_nextstr(&line, &out);
        if (err < 0)
            goto error;

        responses[i].ifname = alloca(strlen(PPP_TTY_PATH) + 1);
        strcpy(responses[i].ifname, PPP_TTY_PATH);

        err = at_tok_nextstr(&line, &out);
        if (err < 0)
            goto error;

        responses[i].addresses = alloca(strlen(out) + 1);
        strcpy(responses[i].addresses, out);

        {
            char  propValue[PROP_VALUE_MAX];

            if (__system_property_get("ro.kernel.qemu", propValue) != 0) {
                /* We are in the emulator - the dns servers are listed
                 * by the following system properties, setup in
                 * /system/etc/init.goldfish.sh:
                 *  - net.eth0.dns1
                 *  - net.eth0.dns2
                 *  - net.eth0.dns3
                 *  - net.eth0.dns4
                 */
                const int   dnslist_sz = 128;
                char*       dnslist = alloca(dnslist_sz);
                const char* separator = "";
                int         nn;

                dnslist[0] = 0;
                for (nn = 1; nn <= 4; nn++) {
                    /* Probe net.eth0.dns<n> */
                    char  propName[PROP_NAME_MAX];
                    snprintf(propName, sizeof propName, "net.eth0.dns%d", nn);

                    /* Ignore if undefined */
                    if (__system_property_get(propName, propValue) == 0) {
                        continue;
                    }

                    /* Append the DNS IP address */
                    strlcat(dnslist, separator, dnslist_sz);
                    strlcat(dnslist, propValue, dnslist_sz);
                    separator = " ";
                }
                responses[i].dnses = dnslist;

                /* There is only on gateway in the emulator */
                responses[i].gateways = "10.0.2.2";
                responses[i].mtu = DEFAULT_MTU;
            }
            else {
                /* I don't know where we are, so use the public Google DNS
                 * servers by default and no gateway.
                 */
                //responses[i].dnses = "8.8.8.8 8.8.4.4";
                //responses[i].gateways = "";
                const char* separator = " ";
                const int   dnslist_sz = 128;
                char*       dnslist = alloca(dnslist_sz);
                char  propName[PROP_VALUE_MAX];
                memset(dnslist, 0, 128);
                property_get("net.ppp0.dns1", propName, "8.8.8.8");
                strlcat(dnslist, propName, dnslist_sz);
                strlcat(dnslist, separator, dnslist_sz);
                property_get("net.ppp0.dns2", propName, "8.8.4.4");
                strlcat(dnslist, propName, dnslist_sz);
                responses[i].dnses = dnslist;
                property_get("net.ppp0.remote-ip", propName, "");
                responses[i].gateways = propName;
                property_get("net.ppp0.local-ip", propName, "");
                responses[i].addresses = propName;
            }
        }
    }

    at_response_free(p_response);

    if (t != NULL)
        RIL_onRequestComplete(*t, RIL_E_SUCCESS, responses,
                              n * sizeof(RIL_Data_Call_Response_v11));
    else
        RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED,
                                  responses,
                                  n * sizeof(RIL_Data_Call_Response_v11));

    return;

error:
    if (t != NULL)
        RIL_onRequestComplete(*t, RIL_E_GENERIC_FAILURE, NULL, 0);
    else
        RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED,
                                  NULL, 0);

    at_response_free(p_response);
}

static void requestQueryNetworkSelectionMode(
                void *data __unused, size_t datalen __unused, RIL_Token t)
{
    int err;
    ATResponse *p_response = NULL;
    int response = 0;
    char *line;
	
    err = at_send_command_singleline("AT+COPS?", "+COPS:", &p_response);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);

    if (err < 0) {
        goto error;
    }

    err = at_tok_nextint(&line, &response);

    if (err < 0) {
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(int));
    at_response_free(p_response);
    return;
error:
    at_response_free(p_response);
    RLOGE("requestQueryNetworkSelectionMode must never return error when radio is on");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void sendCallStateChanged(void *param __unused)
{
    RIL_onUnsolicitedResponse (
        RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED,
        NULL, 0);
}

static void requestGetCurrentCalls(void *data __unused, size_t datalen __unused, RIL_Token t)
{
    int err;
    ATResponse *p_response;
    ATLine *p_cur;
    int countCalls;
    int countValidCalls;
    RIL_Call *p_calls;
    RIL_Call **pp_calls;
    int i;
    int needRepoll = 0;

#ifdef WORKAROUND_ERRONEOUS_ANSWER
    int prevIncomingOrWaitingLine;

    prevIncomingOrWaitingLine = s_incomingOrWaitingLine;
    s_incomingOrWaitingLine = -1;
#endif /*WORKAROUND_ERRONEOUS_ANSWER*/

    err = at_send_command_multiline ("AT+CLCC", "+CLCC:", &p_response);

    if (err != 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    }

    /* count the calls */
    for (countCalls = 0, p_cur = p_response->p_intermediates
            ; p_cur != NULL
            ; p_cur = p_cur->p_next
    ) {
        countCalls++;
    }

    /* yes, there's an array of pointers and then an array of structures */

    pp_calls = (RIL_Call **)alloca(countCalls * sizeof(RIL_Call *));
    p_calls = (RIL_Call *)alloca(countCalls * sizeof(RIL_Call));
    memset (p_calls, 0, countCalls * sizeof(RIL_Call));

    /* init the pointer array */
    for(i = 0; i < countCalls ; i++) {
        pp_calls[i] = &(p_calls[i]);
    }

    for (countValidCalls = 0, p_cur = p_response->p_intermediates
            ; p_cur != NULL
            ; p_cur = p_cur->p_next
    ) {
        err = callFromCLCCLine(p_cur->line, p_calls + countValidCalls);

        if (err != 0) {
            continue;
        }

#ifdef WORKAROUND_ERRONEOUS_ANSWER
        if (p_calls[countValidCalls].state == RIL_CALL_INCOMING
            || p_calls[countValidCalls].state == RIL_CALL_WAITING
        ) {
            s_incomingOrWaitingLine = p_calls[countValidCalls].index;
        }
#endif /*WORKAROUND_ERRONEOUS_ANSWER*/

        if (p_calls[countValidCalls].state != RIL_CALL_ACTIVE
            && p_calls[countValidCalls].state != RIL_CALL_HOLDING
        ) {
            needRepoll = 1;
        }

        countValidCalls++;
    }

#ifdef WORKAROUND_ERRONEOUS_ANSWER
    // Basically:
    // A call was incoming or waiting
    // Now it's marked as active
    // But we never answered it
    //
    // This is probably a bug, and the call will probably
    // disappear from the call list in the next poll
    if (prevIncomingOrWaitingLine >= 0
            && s_incomingOrWaitingLine < 0
            && s_expectAnswer == 0
    ) {
        for (i = 0; i < countValidCalls ; i++) {

            if (p_calls[i].index == prevIncomingOrWaitingLine
                    && p_calls[i].state == RIL_CALL_ACTIVE
                    && s_repollCallsCount < REPOLL_CALLS_COUNT_MAX
            ) {
                RLOGI(
                    "Hit WORKAROUND_ERRONOUS_ANSWER case."
                    " Repoll count: %d\n", s_repollCallsCount);
                s_repollCallsCount++;
                goto error;
            }
        }
    }

    s_expectAnswer = 0;
    s_repollCallsCount = 0;
#endif /*WORKAROUND_ERRONEOUS_ANSWER*/

    RIL_onRequestComplete(t, RIL_E_SUCCESS, pp_calls,
            countValidCalls * sizeof (RIL_Call *));

    at_response_free(p_response);

#ifdef POLL_CALL_STATE
    if (countValidCalls) {  // We don't seem to get a "NO CARRIER" message from
                            // smd, so we're forced to poll until the call ends.
#else
    if (needRepoll) {
#endif
        RIL_requestTimedCallback (sendCallStateChanged, NULL, &TIMEVAL_CALLSTATEPOLL);
    }

    return;
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestDial(void *data, size_t datalen __unused, RIL_Token t)
{
    RIL_Dial *p_dial;
    char *cmd;
    const char *clir;
    int ret;

    p_dial = (RIL_Dial *)data;

    switch (p_dial->clir) {
        case 1: clir = "I"; break;  /*invocation*/
        case 2: clir = "i"; break;  /*suppression*/
        default:
        case 0: clir = ""; break;   /*subscription default*/
    }

    asprintf(&cmd, "ATD%s%s;", p_dial->address, clir);

    ret = at_send_command(cmd, NULL);

    free(cmd);

    /* success or failure is ignored by the upper layer here.
       it will call GET_CURRENT_CALLS and determine success that way */
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

static void requestWriteSmsToSim(void *data, size_t datalen __unused, RIL_Token t)
{
    RIL_SMS_WriteArgs *p_args;
    char *cmd;
    int length;
    int err;
    ATResponse *p_response = NULL;

    p_args = (RIL_SMS_WriteArgs *)data;

    length = strlen(p_args->pdu)/2;
    asprintf(&cmd, "AT+CMGW=%d,%d", length, p_args->status);

    err = at_send_command_sms(cmd, p_args->pdu, "+CMGW:", &p_response);

    if (err != 0 || p_response->success == 0) goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);

    return;
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestHangup(void *data, size_t datalen __unused, RIL_Token t)
{
    int *p_line;

    int ret;
    char *cmd;

    p_line = (int *)data;

    // 3GPP 22.030 6.5.5
    // "Releases a specific active call X"
    asprintf(&cmd, "AT+CHLD=1%d", p_line[0]);

    ret = at_send_command(cmd, NULL);

    free(cmd);

    /* success or failure is ignored by the upper layer here.
       it will call GET_CURRENT_CALLS and determine success that way */
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

static void requestSignalStrength(void *data __unused, size_t datalen __unused, RIL_Token t)
{
    ATResponse *p_response = NULL;
    int err;
    char *line;
    int count =0;
	int numofElements;
	int *response;
	int modem_type;
	modem_type = runtime_3g_port_type();

	if ((HUAWEI_MODEM == modem_type) ||
		(AMAZON_MODEM == modem_type)){
		/* Huawei EM770W response is in RIL_GW_SignalStrength form */
		numofElements=sizeof(RIL_GW_SignalStrength)/sizeof(int);
	}else{
		numofElements=sizeof(RIL_SignalStrength_v6)/sizeof(int);
	}
	response = (int *)calloc(numofElements, sizeof(int));
    if (!response) goto error;
    //int response[numofElements];

    if(sUnsolictedCREG_failed) {
        LOGW("Retry the AT+CREG event report setting");
        /*  Network registration events */
        err = at_send_command("AT+CREG=2", &p_response);
    
        /* some handsets -- in tethered mode -- don't support CREG=2 */
        if (err < 0 || p_response->success == 0) {
            at_response_free(p_response);
            err = at_send_command("AT+CREG=1", &p_response);
        }
    
        if (err < 0 || p_response->success == 0) {
            LOGE("Warning!No network registration events reported");
            sUnsolictedCREG_failed = 1;
        }
        else {
            sUnsolictedCREG_failed = 0;
        }
        at_response_free(p_response);
    }

    if(sUnsolictedCGREG_failed) {
        LOGW("Retry the AT+CGREG event report setting");
        /*  GPRS registration events */
        err = at_send_command("AT+CGREG=1", &p_response);
        if (err < 0 || p_response->success == 0) {
          LOGE("Warning!No GPRS registration events reported");
          sUnsolictedCGREG_failed = 1;
        }
        else {
          sUnsolictedCGREG_failed = 0;
       }

      at_response_free(p_response);
    }


    err = at_send_command_singleline("AT+CSQ", "+CSQ:", &p_response);

    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    for (count =0; count < numofElements; count ++) {
        err = at_tok_nextint(&line, &(response[count]));
        if (err < 0) goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

    at_response_free(p_response);
	free(response);
    return;

error:
    RLOGE("requestSignalStrength must never return an error when radio is on");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
	free(response);

}

/**
 * networkModePossible. Decides whether the network mode is appropriate for the
 * specified modem
 */
static int networkModePossible(ModemInfo *mdm, int nm)
{
    if ((net2modem[nm] & mdm->supportedTechs) == net2modem[nm]) {
       return 1;
    }
    return 0;
}
static void requestSetPreferredNetworkType( int request __unused, void *data,
                                            size_t datalen __unused, RIL_Token t )
{
    ATResponse *p_response = NULL;
    char *cmd = NULL;
    int value = *(int *)data;
    int current, old;
    int err;
    int32_t preferred = net2pmask[value];

    RLOGD("requestSetPreferredNetworkType: current: %x. New: %x", PREFERRED_NETWORK(sMdmInfo), preferred);
    if (!networkModePossible(sMdmInfo, value)) {
        RIL_onRequestComplete(t, RIL_E_MODE_NOT_SUPPORTED, NULL, 0);
        return;
    }
    if (query_ctec(sMdmInfo, &current, NULL) < 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    }
    old = PREFERRED_NETWORK(sMdmInfo);
    RLOGD("old != preferred: %d", old != preferred);
    if (old != preferred) {
        asprintf(&cmd, "AT+CTEC=%d,\"%x\"", current, preferred);
        RLOGD("Sending command: <%s>", cmd);
        err = at_send_command_singleline(cmd, "+CTEC:", &p_response);
        free(cmd);
        if (err || !p_response->success) {
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            return;
        }
        PREFERRED_NETWORK(sMdmInfo) = value;
        if (!strstr( p_response->p_intermediates->line, "DONE") ) {
            int current;
            int res = parse_technology_response(p_response->p_intermediates->line, &current, NULL);
            switch (res) {
                case -1: // Error or unable to parse
                    break;
                case 1: // Only able to parse current
                case 0: // Both current and preferred were parsed
                    setRadioTechnology(sMdmInfo, current);
                    break;
            }
        }
    }
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

static void requestGetPreferredNetworkType(int request __unused, void *data __unused,
                                   size_t datalen __unused, RIL_Token t)
{
    int preferred;
    unsigned i;

    switch ( query_ctec(sMdmInfo, NULL, &preferred) ) {
        case -1: // Error or unable to parse
        case 1: // Only able to parse current
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            break;
        case 0: // Both current and preferred were parsed
            for ( i = 0 ; i < sizeof(net2pmask) / sizeof(int32_t) ; i++ ) {
                if (preferred == net2pmask[i]) {
                    RIL_onRequestComplete(t, RIL_E_SUCCESS, &i, sizeof(int));
                    return;
                }
            }
            RLOGE("Unknown preferred mode received from modem: %d", preferred);
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            break;
    }

}

static void requestCdmaPrlVersion(int request __unused, void *data __unused,
                                   size_t datalen __unused, RIL_Token t)
{
    int err;
    char * responseStr;
    ATResponse *p_response = NULL;
    const char *cmd;
    char *line;

    err = at_send_command_singleline("AT+WPRL?", "+WPRL:", &p_response);
    if (err < 0 || !p_response->success) goto error;
    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;
    err = at_tok_nextstr(&line, &responseStr);
    if (err < 0 || !responseStr) goto error;
    RIL_onRequestComplete(t, RIL_E_SUCCESS, responseStr, strlen(responseStr));
    at_response_free(p_response);
    return;
error:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestCdmaBaseBandVersion(int request __unused, void *data __unused,
                                   size_t datalen __unused, RIL_Token t)
{
    int err;
    char * responseStr;
    ATResponse *p_response = NULL;
    const char *cmd;
    const char *prefix;
    char *line, *p;
    int commas;
    int skip;
    int count = 4;

    // Fixed values. TODO: query modem
    responseStr = strdup("1.0.0.0");
    RIL_onRequestComplete(t, RIL_E_SUCCESS, responseStr, sizeof(responseStr));
    free(responseStr);
}

static void requestCdmaDeviceIdentity(int request __unused, void *data __unused,
                                        size_t datalen __unused, RIL_Token t)
{
    int err;
    int response[4];
    char * responseStr[4];
    ATResponse *p_response = NULL;
    const char *cmd;
    const char *prefix;
    char *line, *p;
    int commas;
    int skip;
    int count = 4;

    // Fixed values. TODO: Query modem
    responseStr[0] = "----";
    responseStr[1] = "----";
    responseStr[2] = "77777777";

    err = at_send_command_numeric("AT+CGSN", &p_response);
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    } else {
        responseStr[3] = p_response->p_intermediates->line;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, responseStr, count*sizeof(char*));
    at_response_free(p_response);

    return;
error:
    RLOGE("requestCdmaDeviceIdentity must never return an error when radio is on");
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestCdmaGetSubscriptionSource(int request __unused, void *data,
                                        size_t datalen __unused, RIL_Token t)
{
    int err;
    int *ss = (int *)data;
    ATResponse *p_response = NULL;
    char *cmd = NULL;
    char *line = NULL;
    int response;

    asprintf(&cmd, "AT+CCSS?");
    if (!cmd) goto error;

    err = at_send_command_singleline(cmd, "+CCSS:", &p_response);
    if (err < 0 || !p_response->success)
        goto error;

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &response);
    free(cmd);
    cmd = NULL;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(response));

    return;
error:
    free(cmd);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestCdmaSetSubscriptionSource(int request __unused, void *data,
                                        size_t datalen, RIL_Token t)
{
    int err;
    int *ss = (int *)data;
    ATResponse *p_response = NULL;
    char *cmd = NULL;

    if (!ss || !datalen) {
        RLOGE("RIL_REQUEST_CDMA_SET_SUBSCRIPTION without data!");
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    }
    asprintf(&cmd, "AT+CCSS=%d", ss[0]);
    if (!cmd) goto error;

    err = at_send_command(cmd, &p_response);
    if (err < 0 || !p_response->success)
        goto error;
    free(cmd);
    cmd = NULL;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);

    RIL_onUnsolicitedResponse(RIL_UNSOL_CDMA_SUBSCRIPTION_SOURCE_CHANGED, ss, sizeof(ss[0]));

    return;
error:
    free(cmd);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestCdmaSubscription(int request __unused, void *data __unused,
                                        size_t datalen __unused, RIL_Token t)
{
    int err;
    int response[5];
    char * responseStr[5];
    ATResponse *p_response = NULL;
    const char *cmd;
    const char *prefix;
    char *line, *p;
    int commas;
    int skip;
    int count = 5;

    // Fixed values. TODO: Query modem
    responseStr[0] = "8587777777"; // MDN
    responseStr[1] = "1"; // SID
    responseStr[2] = "1"; // NID
    responseStr[3] = "8587777777"; // MIN
    responseStr[4] = "1"; // PRL Version
    RIL_onRequestComplete(t, RIL_E_SUCCESS, responseStr, count*sizeof(char*));

    return;
error:
    RLOGE("requestRegistrationState must never return an error when radio is on");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestCdmaGetRoamingPreference(int request __unused, void *data __unused,
                                                 size_t datalen __unused, RIL_Token t)
{
    int roaming_pref = -1;
    ATResponse *p_response = NULL;
    char *line;
    int res;

    res = at_send_command_singleline("AT+WRMP?", "+WRMP:", &p_response);
    if (res < 0 || !p_response->success) {
        goto error;
    }
    line = p_response->p_intermediates->line;

    res = at_tok_start(&line);
    if (res < 0) goto error;

    res = at_tok_nextint(&line, &roaming_pref);
    if (res < 0) goto error;

     RIL_onRequestComplete(t, RIL_E_SUCCESS, &roaming_pref, sizeof(roaming_pref));
    return;
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestCdmaSetRoamingPreference(int request __unused, void *data,
                                                 size_t datalen __unused, RIL_Token t)
{
    int *pref = (int *)data;
    ATResponse *p_response = NULL;
    char *line;
    int res;
    char *cmd = NULL;

    asprintf(&cmd, "AT+WRMP=%d", *pref);
    if (cmd == NULL) goto error;

    res = at_send_command(cmd, &p_response);
    if (res < 0 || !p_response->success)
        goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    free(cmd);
    return;
error:
    free(cmd);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static int parseRegistrationState(char *str, int *type, int *items, int **response)
{
    int err;
    char *line = str, *p;
    int *resp = NULL;
    int skip;
    int count = 3;
    int commas;

    RLOGD("parseRegistrationState. Parsing: %s",str);
    err = at_tok_start(&line);
    if (err < 0) goto error;

    /* Ok you have to be careful here
     * The solicited version of the CREG response is
     * +CREG: n, stat, [lac, cid]
     * and the unsolicited version is
     * +CREG: stat, [lac, cid]
     * The <n> parameter is basically "is unsolicited creg on?"
     * which it should always be
     *
     * Now we should normally get the solicited version here,
     * but the unsolicited version could have snuck in
     * so we have to handle both
     *
     * Also since the LAC and CID are only reported when registered,
     * we can have 1, 2, 3, or 4 arguments here
     *
     * finally, a +CGREG: answer may have a fifth value that corresponds
     * to the network type, as in;
     *
     *   +CGREG: n, stat [,lac, cid [,networkType]]
     */

    /* count number of commas */
    commas = 0;
    for (p = line ; *p != '\0' ;p++) {
        if (*p == ',') commas++;
    }

    //resp = (int *)calloc(commas + 1, sizeof(int));
    /* Huawei EM770W has the response of +CREG: <n>, <stat>
        * it gets networkType in some other way, so assume it
        * has long response to accommodate networkType value.
        */
    #define COMMAS_MAX 4
    resp = (int *)calloc(COMMAS_MAX + 1, sizeof(int));
    if (!resp) goto error;
    switch (commas) {
        case 0: /* +CREG: <stat> */
            err = at_tok_nextint(&line, &resp[0]);
            if (err < 0) goto error;
            resp[1] = -1;
            resp[2] = -1;
        break;

        case 1: /* +CREG: <n>, <stat> */
            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &resp[0]);
            if (err < 0) goto error;
            resp[1] = -1;
            resp[2] = -1;
            if (err < 0) goto error;
        break;

        case 2: /* +CREG: <stat>, <lac>, <cid> */
            err = at_tok_nextint(&line, &resp[0]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &resp[1]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &resp[2]);
            if (err < 0) goto error;
        break;
        case 3: /* +CREG: <n>, <stat>, <lac>, <cid> */
            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &resp[0]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &resp[1]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &resp[2]);
            if (err < 0) goto error;
        break;
        /* special case for CGREG, there is a fourth parameter
         * that is the network type (unknown/gprs/edge/umts)
         */
        case 4: /* +CGREG: <n>, <stat>, <lac>, <cid>, <networkType> */
            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &resp[0]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &resp[1]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &resp[2]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &resp[3]);
            if (err < 0) goto error;
            count = 4;
        break;
        default:
            goto error;
    }
    s_lac = resp[1];
    s_cid = resp[2];
    if (response)
        *response = resp;
    if (items)
        *items = commas + 1;
    if (type)
        *type = techFromModemType(TECH(sMdmInfo));
    return 0;
error:
    free(resp);
    return -1;
}

#define REG_STATE_LEN 15
#define REG_DATA_STATE_LEN 6
static void requestRegistrationState(int request, void *data __unused,
                                        size_t datalen __unused, RIL_Token t)
{
    int err;
    int *registration;
    char **responseStr = NULL;
    ATResponse *p_response = NULL;
    const char *cmd;
    const char *prefix;
    char *line;
    int i = 0, j, numElements = 0;
    int count = 3;
    int type, startfrom;

    RLOGD("requestRegistrationState");
    if (request == RIL_REQUEST_VOICE_REGISTRATION_STATE) {
        cmd = "AT+CREG?";
        prefix = "+CREG:";
        numElements = REG_STATE_LEN;
    } else if (request == RIL_REQUEST_DATA_REGISTRATION_STATE) {
        cmd = "AT+CGREG?";
        prefix = "+CGREG:";
        numElements = REG_DATA_STATE_LEN;
    } else {
        assert(0);
        goto error;
    }

    err = at_send_command_singleline(cmd, prefix, &p_response);

    if (err != 0 || p_response->success == 0) goto error;

    line = p_response->p_intermediates->line;

    if (parseRegistrationState(line, &type, &count, &registration)) goto error;

    responseStr = malloc(numElements * sizeof(char *));
    if (!responseStr) goto error;
    memset(responseStr, 0, numElements * sizeof(char *));
    /**
     * The first '4' bytes for both registration states remain the same.
     * But if the request is 'DATA_REGISTRATION_STATE',
     * the 5th and 6th byte(s) are optional.
     */
    if (is3gpp2(type) == 1) {
        RLOGD("registration state type: 3GPP2");
        // TODO: Query modem
        startfrom = 3;
        if(request == RIL_REQUEST_VOICE_REGISTRATION_STATE) {
            asprintf(&responseStr[3], "8");     // EvDo revA
            asprintf(&responseStr[4], "1");     // BSID
            asprintf(&responseStr[5], "123");   // Latitude
            asprintf(&responseStr[6], "222");   // Longitude
            asprintf(&responseStr[7], "0");     // CSS Indicator
            asprintf(&responseStr[8], "4");     // SID
            asprintf(&responseStr[9], "65535"); // NID
            asprintf(&responseStr[10], "0");    // Roaming indicator
            asprintf(&responseStr[11], "1");    // System is in PRL
            asprintf(&responseStr[12], "0");    // Default Roaming indicator
            asprintf(&responseStr[13], "0");    // Reason for denial
            asprintf(&responseStr[14], "0");    // Primary Scrambling Code of Current cell
      } else if (request == RIL_REQUEST_DATA_REGISTRATION_STATE) {
            asprintf(&responseStr[3], "8");   // Available data radio technology
      }
    } else { // type == RADIO_TECH_3GPP
        RLOGD("registration state type: 3GPP");
        RLOGD("%s: registration[0]..[3]: %d, %d, %d, %d", __func__, registration[0],registration[1],registration[2],registration[3]);
        startfrom = 0;
        asprintf(&responseStr[1], "%x", registration[1]);
        asprintf(&responseStr[2], "%x", registration[2]);
        if (count > 3){
	    registration[3] = convertRILRadioTechnology(registration[3], runtime_3g_port_type());
            asprintf(&responseStr[3], "%d", registration[3]);
	}
	else{
	    /*
	     * In case +CGREG doesn't respond with <networkType>, networkType
	     * may be obtained in some other way.
	     */
	    registration[3] = sNetworkType;
	    asprintf(&responseStr[3], "%d", registration[3]);
        }
    }
    asprintf(&responseStr[0], "%d", registration[0]);

    /**
     * Optional bytes for DATA_REGISTRATION_STATE request
     * 4th byte : Registration denial code
     * 5th byte : The max. number of simultaneous Data Calls
     */
    if(request == RIL_REQUEST_DATA_REGISTRATION_STATE) {
        // asprintf(&responseStr[4], "3");
        // asprintf(&responseStr[5], "1");
    }

    for (j = startfrom; j < numElements; j++) {
        if (!responseStr[i]) goto error;
    }
    free(registration);
    registration = NULL;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, responseStr, numElements*sizeof(responseStr));
    for (j = 0; j < numElements; j++ ) {
        free(responseStr[j]);
        responseStr[j] = NULL;
    }
    free(responseStr);
    responseStr = NULL;
    at_response_free(p_response);

    return;
error:
    if (responseStr) {
        for (j = 0; j < numElements; j++) {
            free(responseStr[j]);
            responseStr[j] = NULL;
        }
        free(responseStr);
        responseStr = NULL;
    }
    RLOGE("requestRegistrationState must never return an error when radio is on");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestOperator(void *data __unused, size_t datalen __unused, RIL_Token t)
{
    int err;
    int i;
    int skip;
    ATLine *p_cur;
    char *response[3];

    memset(response, 0, sizeof(response));

    ATResponse *p_response = NULL;

    err = at_send_command_multiline(
        "AT+COPS=3,0;+COPS?;+COPS=3,1;+COPS?;+COPS=3,2;+COPS?",
        "+COPS:", &p_response);

    /* we expect 3 lines here:
     * +COPS: 0,0,"T - Mobile"
     * +COPS: 0,1,"TMO"
     * +COPS: 0,2,"310170"
     */

    if (err != 0) goto error;

    for (i = 0, p_cur = p_response->p_intermediates
            ; p_cur != NULL
            ; p_cur = p_cur->p_next, i++
    ) {
        char *line = p_cur->line;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &skip);
        if (err < 0) goto error;

        // If we're unregistered, we may just get
        // a "+COPS: 0" response
        if (!at_tok_hasmore(&line)) {
            response[i] = NULL;
            continue;
        }

        err = at_tok_nextint(&line, &skip);
        if (err < 0) goto error;

        // a "+COPS: 0, n" response is also possible
        if (!at_tok_hasmore(&line)) {
            response[i] = NULL;
            continue;
        }

        err = at_tok_nextstr(&line, &(response[i]));
        if (err < 0) goto error;
        // Simple assumption that mcc and mnc are 3 digits each
        if (strlen(response[i]) == 6) {
            if (sscanf(response[i], "%3d%3d", &s_mcc, &s_mnc) != 2) {
                RLOGE("requestOperator expected mccmnc to be 6 decimal digits");
            }
        }
    }

    if (i != 3) {
        /* expect 3 lines exactly */
        goto error;
    }

    //Workaround for 770w with below response
    //+COPS: 0,0,"FFFFFFFFFFFFFFFF",2
    //+COPS: 0,1,"FFFFFFFFFFFFFFFF",2
    //+COPS: 0,2,"46001",2
    //"China Unicom","UNICOM","46001"
    //"China Mobile Com","CMCC","46000"
    //"China Mobile Com","CMCC","46002"
    //"China Mobile Com","CMCC","46007"
    char *invalid_operator = "FFFFFFFFFFFFFFFF";
    if (response[0] && response[2] &&
        !strncmp(response[0], invalid_operator, strlen(invalid_operator))) {
        //Set the long operator name based on mcc/mnc
        if (!strncmp(response[2], "46001", 5))
            strcpy(response[0], "China Unicom");
        if (!strncmp(response[2], "46000", 5)||
           !strncmp(response[2], "46002", 5)||
           !strncmp(response[2], "46007", 5))
            strcpy(response[0], "China Mobile Com");
    }
    if (response[1] && response[2] &&
        !strncmp(response[1], invalid_operator, strlen(invalid_operator))) {
        //Set the long operator name based on mcc/mnc
        if (!strncmp(response[2], "46001", 5))
            strcpy(response[1], "UNICOM");
        if (!strncmp(response[2], "46000", 5)||
           !strncmp(response[2], "46002", 5)||
           !strncmp(response[2], "46007", 5))
            strcpy(response[1], "CMCC");
    }
    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
    at_response_free(p_response);

    return;
error:
    RLOGE("requestOperator must not return error when radio is on");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestCdmaSendSMS(void *data, size_t datalen, RIL_Token t)
{
    int err = 1; // Set to go to error:
    RIL_SMS_Response response;
    RIL_CDMA_SMS_Message* rcsm;

    RLOGD("requestCdmaSendSMS datalen=%zu, sizeof(RIL_CDMA_SMS_Message)=%zu",
            datalen, sizeof(RIL_CDMA_SMS_Message));

    // verify data content to test marshalling/unmarshalling:
    rcsm = (RIL_CDMA_SMS_Message*)data;
    RLOGD("TeleserviceID=%d, bIsServicePresent=%d, \
            uServicecategory=%d, sAddress.digit_mode=%d, \
            sAddress.Number_mode=%d, sAddress.number_type=%d, ",
            rcsm->uTeleserviceID,  rcsm->bIsServicePresent,
            rcsm->uServicecategory,rcsm->sAddress.digit_mode,
            rcsm->sAddress.number_mode,rcsm->sAddress.number_type);

    if (err != 0) goto error;

    // Cdma Send SMS implementation will go here:
    // But it is not implemented yet.

    memset(&response, 0, sizeof(response));
    response.messageRef = 1;
    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(response));
    return;

error:
    // Cdma Send SMS will always cause send retry error.
    response.messageRef = -1;
    RIL_onRequestComplete(t, RIL_E_SMS_SEND_FAIL_RETRY, &response, sizeof(response));
}

static void requestSendSMS(void *data, size_t datalen, RIL_Token t)
{
    int err;
    const char *smsc;
    const char *pdu;
    int tpLayerLength;
    char *cmd1, *cmd2;
    RIL_SMS_Response response;
    ATResponse *p_response = NULL;

    memset(&response, 0, sizeof(response));
    RLOGD("requestSendSMS datalen =%zu", datalen);

    if (s_ims_gsm_fail != 0) goto error;
    if (s_ims_gsm_retry != 0) goto error2;

    smsc = ((const char **)data)[0];
    pdu = ((const char **)data)[1];

    tpLayerLength = strlen(pdu)/2;

    // "NULL for default SMSC"
    if (smsc == NULL) {
        smsc= "00";
    }

    asprintf(&cmd1, "AT+CMGS=%d", tpLayerLength);
    asprintf(&cmd2, "%s%s", smsc, pdu);

    err = at_send_command_sms(cmd1, cmd2, "+CMGS:", &p_response);

    if (err != 0 || p_response->success == 0) goto error;

    /* FIXME fill in messageRef and ackPDU */
    response.messageRef = 1;
    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(response));
    at_response_free(p_response);

    return;
error:
    response.messageRef = -2;
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, &response, sizeof(response));
    at_response_free(p_response);
    return;
error2:
    // send retry error.
    response.messageRef = -1;
    RIL_onRequestComplete(t, RIL_E_SMS_SEND_FAIL_RETRY, &response, sizeof(response));
    at_response_free(p_response);
    return;
    }

static void requestImsSendSMS(void *data, size_t datalen, RIL_Token t)
{
    RIL_IMS_SMS_Message *p_args;
    RIL_SMS_Response response;

    memset(&response, 0, sizeof(response));

    RLOGD("requestImsSendSMS: datalen=%zu, "
        "registered=%d, service=%d, format=%d, ims_perm_fail=%d, "
        "ims_retry=%d, gsm_fail=%d, gsm_retry=%d",
        datalen, s_ims_registered, s_ims_services, s_ims_format,
        s_ims_cause_perm_failure, s_ims_cause_retry, s_ims_gsm_fail,
        s_ims_gsm_retry);

    // figure out if this is gsm/cdma format
    // then route it to requestSendSMS vs requestCdmaSendSMS respectively
    p_args = (RIL_IMS_SMS_Message *)data;

    if (0 != s_ims_cause_perm_failure ) goto error;

    // want to fail over ims and this is first request over ims
    if (0 != s_ims_cause_retry && 0 == p_args->retry) goto error2;

    if (RADIO_TECH_3GPP == p_args->tech) {
        return requestSendSMS(p_args->message.gsmMessage,
                datalen - sizeof(RIL_RadioTechnologyFamily),
                t);
    } else if (RADIO_TECH_3GPP2 == p_args->tech) {
        return requestCdmaSendSMS(p_args->message.cdmaMessage,
                datalen - sizeof(RIL_RadioTechnologyFamily),
                t);
    } else {
        RLOGE("requestImsSendSMS invalid format value =%d", p_args->tech);
    }

error:
    response.messageRef = -2;
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, &response, sizeof(response));
    return;

error2:
    response.messageRef = -1;
    RIL_onRequestComplete(t, RIL_E_SMS_SEND_FAIL_RETRY, &response, sizeof(response));
}

static void requestDeactivateDataCall(void *data, size_t datalen, RIL_Token t)
{
    const char *cid;
    char *cmd;
    int err;

    cid = ((const char **)data)[0];
    LOGD("requesting deactivating data connection with CID '%s'", cid);

    /* Stop PPPd firstly */
    LOGD("Stopping PPPD");
    property_set("ctl.stop", SERVICE_PPPD_GPRS);

    /* Dactivate PDP Context */
    asprintf(&cmd, "AT+CGACT=0,%s", cid);
    err = at_send_command(cmd, NULL);

    if (err < 0 ) {
	LOGW("Dactivate PDP failure");
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    free(cmd);

    return;
}

static void requestSetupDataCall(void *data, size_t datalen, RIL_Token t)
{
    const char *apn;
    char *cmd;
    int err;
    ATResponse *p_response = NULL;
    char *response[3] = { "1", PPP_TTY_PATH, ""};

    apn = ((const char **)data)[2];

#ifdef USE_TI_COMMANDS
    // Config for multislot class 10 (probably default anyway eh?)
    err = at_send_command("AT%CPRIM=\"GMM\",\"CONFIG MULTISLOT_CLASS=<10>\"",
                        NULL);

    err = at_send_command("AT%DATA=2,\"UART\",1,,\"SER\",\"UART\",0", NULL);
#endif /* USE_TI_COMMANDS */

    if (current_modem_type == UNKNOWN_MODEM) {
    int fd, qmistatus;
    size_t cur = 0;
    size_t len;
    ssize_t written, rlen;
    char status[32] = {0};
    int retry = 10;
    const char *pdp_type;

    RLOGD("requesting data connection to APN '%s'", apn);

    fd = open ("/dev/qmi", O_RDWR);
    if (fd >= 0) { /* the device doesn't exist on the emulator */

        RLOGD("opened the qmi device\n");
        asprintf(&cmd, "up:%s", apn);
        len = strlen(cmd);

        while (cur < len) {
            do {
                written = write (fd, cmd + cur, len - cur);
            } while (written < 0 && errno == EINTR);

            if (written < 0) {
                RLOGE("### ERROR writing to /dev/qmi");
                close(fd);
                goto error;
            }

            cur += written;
        }

        // wait for interface to come online

        do {
            sleep(1);
            do {
                rlen = read(fd, status, 31);
            } while (rlen < 0 && errno == EINTR);

            if (rlen < 0) {
                RLOGE("### ERROR reading from /dev/qmi");
                close(fd);
                goto error;
            } else {
                status[rlen] = '\0';
                RLOGD("### status: %s", status);
            }
        } while (strncmp(status, "STATE=up", 8) && strcmp(status, "online") && --retry);

        close(fd);

        if (retry == 0) {
            RLOGE("### Failed to get data connection up\n");
            goto error;
        }

        qmistatus = system("netcfg rmnet0 dhcp");

        RLOGD("netcfg rmnet0 dhcp: status %d\n", qmistatus);

        if (qmistatus < 0) goto error;

    } else {

        if (datalen > 6 * sizeof(char *)) {
            pdp_type = ((const char **)data)[6];
        } else {
            pdp_type = "IP";
        }

        asprintf(&cmd, "AT+CGDCONT=1,\"%s\",\"%s\",,0,0", pdp_type, apn);
        //FIXME check for error here
        err = at_send_command(cmd, NULL);
        free(cmd);

        // Set required QoS params to default
        err = at_send_command("AT+CGQREQ=1", NULL);

        // Set minimum QoS params to default
        err = at_send_command("AT+CGQMIN=1", NULL);

        // packet-domain event reporting
        err = at_send_command("AT+CGEREP=1,0", NULL);

        // Hangup anything that's happening there now
        err = at_send_command("AT+CGACT=1,0", NULL);

        // Start data on PDP context 1
        err = at_send_command("ATD*99***1#", &p_response);

        if (err < 0 || p_response->success == 0) {
            goto error;
        }
    }
    } else {/* if current_modem_type == UNKNOWN_MODEM */

    int fd;
    char buffer[20];
    char exit_code[PROPERTY_VALUE_MAX];
    static char local_ip[PROPERTY_VALUE_MAX];
    int retry = POLL_PPP_SYSFS_RETRY;

    if(pppd) {
	// clean up any existing PPP connection before activating new PDP context
	LOGW("Stop existing PPPd before activating PDP");
	property_set("ctl.stop", SERVICE_PPPD_GPRS);
	pppd = 0;
    }

    LOGD("requesting data connection to APN '%s'", apn);

    asprintf(&cmd, "AT+CGDCONT=1,\"IP\",\"%s\",,0,0", apn);
    err = at_send_command(cmd, NULL);
    free(cmd);

    // Set required QoS params to default
    err = at_send_command("AT+CGQREQ=1", NULL);

    // Set minimum QoS params to default
    err = at_send_command("AT+CGQMIN=1", NULL);

    // packet-domain event reporting
    err = at_send_command("AT+CGEREP=1,0", NULL);

    if (current_modem_type != AMAZON_MODEM) {
	    /* Hangup anything that's happening there now */
	    err = at_send_command("AT+CGACT=0,1", NULL);

	    /* Start data on PDP context 1 */
	    err = at_send_command("ATD*99***1#", &p_response);

	    if (err < 0 || p_response->success == 0)
		    goto error;
    } else {
	    if (err < 0)
		    goto error;
    }
    // Setup PPP connection after PDP Context activated successfully
    // The ppp service name is specified in /init.rc
    property_set(PROPERTY_PPPD_EXIT_CODE, "");
    property_set("net.ppp0.local-ip", "");
    err = property_set("ctl.start", SERVICE_PPPD_GPRS);
    pppd = 1;
    if (err < 0) {
	LOGW("Can not start PPPd");
	goto ppp_error;
    }
    LOGD("PPPd started");

    // Wait for PPP interface ready
    if (current_modem_type == AMAZON_MODEM)
	    sleep(2);
    else
	    sleep(1);
    do {
	// Check whether PPPD exit abnormally
	property_get(PROPERTY_PPPD_EXIT_CODE, exit_code, "");
	if(strcmp(exit_code, "") != 0) {
	    LOGW("PPPd exit with code %s", exit_code);
	    retry = 0;
	    break;
        }

        fd  = open(PPP_OPERSTATE_PATH, O_RDONLY);
        if (fd >= 0)  {
	    buffer[0] = 0;
            read(fd, buffer, sizeof(buffer));
	    close(fd);
	    if(!strncmp(buffer, "up", strlen("up")) || !strncmp(buffer, "unknown", strlen("unknown"))) {
		// Should already get local IP address from PPP link after IPCP negotation 
		// system property net.ppp0.local-ip is created by PPPD in "ip-up" script 
        local_ip[0] = 0;
		property_get("net.ppp0.local-ip", local_ip, "");
		if(!strcmp(local_ip, "")) {
		    LOGW("PPP link is up but no local IP is assigned. Will retry %d times after %d seconds", \
			  retry, POLL_PPP_SYSFS_SECONDS);
		} else {
		    LOGD("PPP link is up with local IP address %s", local_ip);
		    // other info like dns will be passed to framework via property (set in ip-up script)
            response[1] = "ppp0";
		    response[2] = local_ip;  
		    // now we think PPPd is ready
		    break;	
		}
	    } else {
		LOGW("PPP link status in %s is %s. Will retry %d times after %d seconds", \
		    PPP_OPERSTATE_PATH, buffer, retry, POLL_PPP_SYSFS_SECONDS);
	    }
	} else {
    	    LOGW("Can not detect PPP state in %s. Will retry %d times after %d seconds", \
		  PPP_OPERSTATE_PATH, retry-1, POLL_PPP_SYSFS_SECONDS);
	}
        sleep(POLL_PPP_SYSFS_SECONDS);
    } while (--retry);

    if(retry == 0)
	goto ppp_error;
    }
    requestOrSendDataCallList(&t);
    at_response_free(p_response);

    return;

ppp_error:
    /* PDP activated successfully, but PPP connection can't be setup. So deactivate PDP context */
    at_send_command("AT+CGACT=0,1", NULL);
    /* If PPPd is already launched, stop it */
    if(pppd) {
	property_set("ctl.stop", SERVICE_PPPD_GPRS);
	pppd = 0;
    }
    // fall through
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);

}

static void requestSMSAcknowledge(void *data, size_t datalen __unused, RIL_Token t)
{
    int ackSuccess;
    int err;

    ackSuccess = ((int *)data)[0];

    if (ackSuccess == 1) {
        err = at_send_command("AT+CNMA=1", NULL);
    } else if (ackSuccess == 0)  {
        err = at_send_command("AT+CNMA=2", NULL);
    } else {
        RLOGE("unsupported arg to RIL_REQUEST_SMS_ACKNOWLEDGE\n");
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);

}

static int convertSimIoFcp(RIL_SIM_IO_Response *sr, char **cvt)
{
    int err = 0;
    size_t fcplen;
    struct ts_51011_921_resp resp;
    void *cvt_buf = NULL;

    if (!sr->simResponse || !cvt) {
        err = -1;
        goto error;
    }

    fcplen = strlen(sr->simResponse);
    if ((fcplen == 0) || (fcplen & 1)) {
        err = -1;
        goto error;
    }

    err = fcp_to_ts_51011(sr->simResponse, fcplen, &resp);
    if (err < 0){
        goto error;
    }

    cvt_buf = malloc(sizeof(resp) * 2 + 1);
    if (!cvt_buf) {
        err = -1;
        goto error;
    }

    err = binaryToString((unsigned char*)(&resp),
                   sizeof(resp), cvt_buf);
    if (err < 0){
        goto error;
    }

    /* cvt_buf ownership is moved to the caller */
    *cvt = cvt_buf;
    cvt_buf = NULL;

finally:
    return err;

error:
    free(cvt_buf);
    goto finally;
}

/**
* Fetch information about UICC card type (SIM/USIM)
*
* \return UICC_Type: type of UICC card.
*/
static UICC_Type getUICCType()
{
    ATResponse *atresponse = NULL;
    static UICC_Type UiccType = UICC_TYPE_UNKNOWN; /* FIXME: Static variable */
    int err;
    int swx;
    char *line = NULL;
    char *dir = NULL;

    if (currentState() == RADIO_STATE_OFF ||
        currentState() == RADIO_STATE_UNAVAILABLE) {
        UiccType = UICC_TYPE_UNKNOWN;
        goto exit;
    }

    /* No need to get type again, it is stored */
    if (UiccType != UICC_TYPE_UNKNOWN)
        goto exit;

    /*
     * For USIM, 'AT+CRSM=192,28480,0,0,15' will respond with TLV tag "62" which indicates
     * FCP template (refer to ETSI TS 101 220). Currently, USIM detection succeeds on
     * Innocomm Amazon1901 and Huawei EM770W.
     */
    err = at_send_command_singleline("AT+CRSM=192,28480,0,0,15", "+CRSM:", &atresponse);

    if (err != 0 || !atresponse->success)
        goto error;

    if (atresponse->p_intermediates != NULL) {
        line = atresponse->p_intermediates->line;

        err = at_tok_start(&line);
        if (err < 0)
            goto error;

        err = at_tok_nextint(&line, &swx);
        if (err < 0)
            goto error;
        err = at_tok_nextint(&line, &swx);
        if (err < 0)
            goto error;

	if (at_tok_hasmore(&line)) {
	 	err = at_tok_nextstr(&line, &dir);
		if (err < 0) goto error;
	}

        if (strstr(dir, "62") == dir) {
            UiccType = UICC_TYPE_USIM;
            LOGI("Detected card type USIM - stored");
            goto finally;
        }
    }

    UiccType = UICC_TYPE_SIM;
    LOGI("Detected card type SIM - stored");
    goto finally;

error:
    UiccType = UICC_TYPE_UNKNOWN;
    LOGW("%s(): Failed to detect card type - Retry at next request", __func__);

finally:
    at_response_free(atresponse);

exit:
    return UiccType;
}



static void  requestSIM_IO(void *data, size_t datalen __unused, RIL_Token t)
{
    ATResponse *p_response = NULL;
    RIL_SIM_IO_Response sr;
    int err;
    char *cmd = NULL;
    RIL_SIM_IO_v6 *p_args;
    char *line;
    int cvt_done = 0;

    memset(&sr, 0, sizeof(sr));

    p_args = (RIL_SIM_IO_v6 *)data;

    /* FIXME handle pin2 */

    if (p_args->data == NULL) {
        asprintf(&cmd, "AT+CRSM=%d,%d,%d,%d,%d",
                    p_args->command, p_args->fileid,
                    p_args->p1, p_args->p2, p_args->p3);
    } else {
        asprintf(&cmd, "AT+CRSM=%d,%d,%d,%d,%d,%s",
                    p_args->command, p_args->fileid,
                    p_args->p1, p_args->p2, p_args->p3, p_args->data);
    }

    err = at_send_command_singleline(cmd, "+CRSM:", &p_response);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(sr.sw1));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(sr.sw2));
    if (err < 0) goto error;

    if (at_tok_hasmore(&line)) {
        err = at_tok_nextstr(&line, &(sr.simResponse));
        if (err < 0) goto error;
    }

/*
* In case the command is GET_RESPONSE and cardtype is 3G SIM
* conversion to 2G FCP is required
*/
    if (p_args->command == 0xC0 && getUICCType() == UICC_TYPE_USIM) {
        if (convertSimIoFcp(&sr, &sr.simResponse) < 0) {
            //rilErrorCode = RIL_E_GENERIC_FAILURE;
            goto error;
        }
        cvt_done = 1; /* sr.simResponse needs to be freed */
    }
    RIL_onRequestComplete(t, RIL_E_SUCCESS, &sr, sizeof(sr));
    at_response_free(p_response);

    if (cvt_done)
    free(sr.simResponse);

    free(cmd);

    return;
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    free(cmd);

}

/**
* Get the number of retries left for pin functions
* Huawei EM770W is supposed to use %CPIN? to query pin_times, puk_times,
* pin2_times and puk2_times, but it seems not support well. Here we bypass
* this feature.
*/
static int getNumRetries(int request)
{
	return 1;
}

/**
* Enter SIM PIN, might be PIN, PIN2, PUK, PUK2, etc.
*
* Data can hold pointers to one or two strings, depending on what we
* want to enter. (PUK requires new PIN, etc.).
*
*/
static void initializeCallback(void *param);
void requestEnterSimPin(void *data, size_t datalen, RIL_Token t, int request)
{
    ATResponse *atresponse = NULL;
    int err;
    char *cmd = NULL;
    const char **strings = (const char **) data;
    int num_retries = -1;
    AT_CME_Error cme_error_code;

	/*
	 * Datalen becomes 1 * sizeof(char *) larger than usual.
	 * Need to keep align with before.
	 */
	if (datalen == 2 * sizeof(char *) && strings[1] == NULL)
		datalen -= sizeof(char *);
	if (datalen == 3 * sizeof(char *) && strings[2] == NULL)
		datalen -= sizeof(char *);
	
    if (datalen == sizeof(char *)) {
	/*
	* Entering PIN(2) is not possible using AT+CPIN unless SIM state is
	* PIN(2) required. The workaround is to change PIN(2) to the same value
	* using AT+CPWD.
	*/
        if (request == RIL_REQUEST_ENTER_SIM_PIN && getSIMStatus() != SIM_PIN)
            asprintf(&cmd, "AT+CPWD=\"SC\",\"%s\",\"%s\"", strings[0],
                     strings[0]);
        else if (request == RIL_REQUEST_ENTER_SIM_PIN2 &&
                 getSIMStatus() != SIM_PIN2)
            asprintf(&cmd, "AT+CPWD=\"P2\",\"%s\",\"%s\"", strings[0],
                     strings[0]);
        else
            asprintf(&cmd, "AT+CPIN=\"%s\"", strings[0]);
    } else if (datalen == 2 * sizeof(char *)) {
	/*
	* Unblocking PIN(2) is not possible using AT+CPIN unless SIM state is
	* PUK(2) required. We need to support this due to 3GPP TS 31.121
	* section 6.1.3. Using ATD for unblocking PIN only works when ME is
	* camping on network.
	*/
        if (request == RIL_REQUEST_ENTER_SIM_PUK && getSIMStatus() != SIM_PUK)
            asprintf(&cmd, "ATD**05*%s*%s*%s#;", strings[0], strings[1],
                     strings[1]);
        else if (request == RIL_REQUEST_ENTER_SIM_PUK2 &&
                 getSIMStatus() != SIM_PUK2)
            asprintf(&cmd, "ATD**052*%s*%s*%s#;", strings[0], strings[1],
                     strings[1]);
        else
	        asprintf(&cmd, "AT+CPIN=\"%s\",\"%s\"", strings[0], strings[1]);
    }else
        goto error;

    err = at_send_command(cmd, &atresponse);
    free(cmd);

    if (err < 0) {
        goto error;
    }
    if (atresponse->success == 0) {
        if (cme_error_code = at_get_cme_error(atresponse)) {
            switch (cme_error_code) {
            case CME_SIM_PIN_REQUIRED:
            case CME_SIM_PUK_REQUIRED:
            case CME_INCORRECT_PASSWORD:
            case CME_SIM_PIN2_REQUIRED:
            case CME_SIM_PUK2_REQUIRED:
            case CME_SIM_FAILURE:
                num_retries = getNumRetries(request);
                RIL_onRequestComplete(t, RIL_E_PASSWORD_INCORRECT, &num_retries, sizeof(int *));
                goto finally;

            default:
                break;
            }
        }
        goto error;
    }

    /* Got OK, return success */
    num_retries = getNumRetries(request);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, &num_retries, sizeof(int *));

finally:
	if (!err){
		/*
		* Currently, we didn't know how to trigger poll of SIM state, so we call it here.
		* Maybe they can be put where reporting SIM state change. initializeCallback()
		* is called to get ready to obtain IMSI.
		*/
		initializeCallback(NULL);
		pollSIMState(NULL);
		 err = at_send_command_multiline(
		 "AT+COPS=3,0;+COPS?;+COPS=3,1;+COPS?;+COPS=3,2;+COPS?",
		 "+COPS:", &atresponse);
		RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL,0);
	}

    at_response_free(atresponse);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    goto finally;
}

static void  requestSendUSSD(void *data, size_t datalen __unused, RIL_Token t)
{
    const char *ussdRequest;

    ussdRequest = (char *)(data);

    RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);

// @@@ TODO

}

static void requestExitEmergencyMode(void *data __unused, size_t datalen __unused, RIL_Token t)
{
    int err;
    ATResponse *p_response = NULL;

    err = at_send_command("AT+WSOS=0", &p_response);

    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

// TODO: Use all radio types
static int techFromModemType(int mdmtype)
{
    int ret = -1;
    switch (1 << mdmtype) {
        case MDM_CDMA:
            ret = RADIO_TECH_1xRTT;
            break;
        case MDM_EVDO:
            ret = RADIO_TECH_EVDO_A;
            break;
        case MDM_GSM:
            ret = RADIO_TECH_GPRS;
            break;
        case MDM_WCDMA:
            ret = RADIO_TECH_HSPA;
            break;
        case MDM_LTE:
            ret = RADIO_TECH_LTE;
            break;
    }
    return ret;
}

static void requestGetCellInfoList(void *data __unused, size_t datalen __unused, RIL_Token t)
{
    uint64_t curTime = ril_nano_time();
    RIL_CellInfo ci[1] =
    {
        { // ci[0]
            1, // cellInfoType
            1, // registered
            curTime - 1000, // Fake some time in the past
            { // union CellInfo
                {  // RIL_CellInfoGsm gsm
                    {  // gsm.cellIdneityGsm
                        s_mcc, // mcc
                        s_mnc, // mnc
                        s_lac, // lac
                        s_cid, // cid
                        0  // psc
                    },
                    {  // gsm.signalStrengthGsm
                        10, // signalStrength
                        0  // bitErrorRate
                    }
                }
            }
        }
    };

    RIL_onRequestComplete(t, RIL_E_SUCCESS, ci, sizeof(ci));
}


static void requestSetCellInfoListRate(void *data, size_t datalen, RIL_Token t)
{
    // For now we'll save the rate but no RIL_UNSOL_CELL_INFO_LIST messages
    // will be sent.
    assert (datalen == sizeof(int));
    s_cell_info_rate_ms = ((int *)data)[0];

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

static void requestGetHardwareConfig(void *data, size_t datalen, RIL_Token t)
{
   // TODO - hook this up with real query/info from radio.

   RIL_HardwareConfig hwCfg;

   RIL_UNUSED_PARM(data);
   RIL_UNUSED_PARM(datalen);

   hwCfg.type = -1;

   RIL_onRequestComplete(t, RIL_E_SUCCESS, &hwCfg, sizeof(hwCfg));
}


static void  requestBasebandVersion(void *data, size_t datalen, RIL_Token t)
{
    ATResponse   *p_response = NULL;
    int           err;
    char *response = { "UNKNOWN"};
    char *line;

    err = at_send_command_numeric("AT+CGMR", &p_response);

    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        line = p_response->p_intermediates->line;
        if(line != NULL) {
            RIL_onRequestComplete(t, RIL_E_SUCCESS, line, sizeof(line));
        }
        else{
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        }
    }
    at_response_free(p_response);
}


static void requestChangePassword(char *facility, void *data, size_t datalen,
                           RIL_Token t, int request)
{
    int err = 0;
    char *oldPassword = NULL;
    char *newPassword = NULL;
    char *cmd = NULL;
    ATResponse *atresponse = NULL;
    int num_retries = -1;
    RIL_Errno errorril = RIL_E_GENERIC_FAILURE;
    AT_CME_Error cme_error_code;

	/*
	 * datalen becomes 1 * sizeof(char *) larger than usual.
	 * Need to keep align with before.
	 */
	if (datalen == 3 * sizeof(char **) && ((char **) data)[2] == NULL)
		datalen -= sizeof(char **);
    if (datalen != 2 * sizeof(char **) || strlen(facility) != 2) {
        goto error;
    }

    oldPassword = ((char **) data)[0];
    newPassword = ((char **) data)[1];

    asprintf(&cmd, "AT+CPWD=\"%s\",\"%s\",\"%s\"", facility, oldPassword,
             newPassword);

    err = at_send_command(cmd, &atresponse);
    free(cmd);

    num_retries = getNumRetries(request);

    if (err < 0) {
        goto error;
    }
    if (atresponse->success == 0) {
        if (cme_error_code = at_get_cme_error(atresponse)) {
            switch (cme_error_code) {
            case CME_INCORRECT_PASSWORD: /* CME ERROR 16: "Incorrect password" */
                LOGI("%s(): Incorrect password", __func__);
                errorril = RIL_E_PASSWORD_INCORRECT;
                break;
            case CME_SIM_PUK2_REQUIRED: /* CME ERROR 18: "SIM PUK2 required" happens when wrong
PIN2 is used 3 times in a row */
                LOGI("%s(): PIN2 locked, change PIN2 with PUK2", __func__);
                num_retries = 0;/* PUK2 required */
                errorril = RIL_E_SIM_PUK2;
                break;
            default: /* some other error */
                break;
            }
        }
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &num_retries, sizeof(int *));
finally:
    at_response_free(atresponse);
    return;

error:
    RIL_onRequestComplete(t, errorril, &num_retries, sizeof(int *));
    goto finally;
}

/**
* RIL_REQUEST_CHANGE_SIM_PIN
*
* Change PIN 1.
*/
void requestChangeSimPin(void *data, size_t datalen, RIL_Token t, int request)
{
    requestChangePassword("SC", data, datalen, t, request);
}

/**
* RIL_REQUEST_CHANGE_SIM_PIN2
*
* Change PIN 2.
*/
void requestChangeSimPin2(void *data, size_t datalen, RIL_Token t, int request)
{
    requestChangePassword("P2", data, datalen, t, request);
}

/**
* RIL_REQUEST_SET_FACILITY_LOCK
*
* Enable/disable one facility lock.
*/
void requestSetFacilityLock(void *data, size_t datalen, RIL_Token t)
{
    int err;
    ATResponse *atresponse = NULL;
    char *cmd = NULL;
    char *facility_string = NULL;
    int facility_mode = -1;
    char *facility_mode_str = NULL;
    char *facility_password = NULL;
    char *facility_class = NULL;
    int num_retries = -1;
    int classx;
    size_t i;
    RIL_Errno errorril = RIL_E_GENERIC_FAILURE;
    AT_CME_Error cme_error_code;
    char *barr_facilities[] = {"AO", "OI", "AI", "IR", "OX", "AB", "AG",
        "AC", NULL};

    assert(datalen >= (4 * sizeof(char **)));

    facility_string = ((char **) data)[0];
    facility_mode_str = ((char **) data)[1];
    facility_password = ((char **) data)[2];
    facility_class = ((char **) data)[3];
    classx = atoi(facility_class);

    assert(*facility_mode_str == '0' || *facility_mode_str == '1');
    facility_mode = atoi(facility_mode_str);

	/*
	* Android send class 0 for USSD strings that didn't contain a class.
	* Class 0 is not considered a valid value and according to 3GPP 24.080 a
	* missing BasicService (BS) parameter in the Supplementary Service string
	* indicates all BS'es.
	*
	* Therefore we convert a class of 0 into 255 (all classes) before sending
	* the AT command for the following barrings:
	*
	* "AO": barr All Outgoing calls
	* "OI": barr Outgoing International calls
	* "AI": barr All Incomming calls
	* "IR": barr Incoming calls when Roaming outside the home country
	* "OX": barr Outgoing international calls eXcept to home country
	* "AB": all barring services (only unlock mode=0)
	* "AG": all outgoing barring services (only unlock mode=0)
	* "AC": all incoming barring services (only unlock mode=0)
	*/
    for (i = 0; barr_facilities[i] != NULL; i++) {
        if (!strncmp(facility_string, barr_facilities[i], 2)) {
            if (classx == 0)
                classx = 255;
            break;
        }
    }

	/*
	* Skip adding facility_password to AT command parameters if it is NULL,
	* printing NULL with %s will give string "(null)".
	*/
    asprintf(&cmd, "AT+CLCK=\"%s\",%d,\"%s\",%d", facility_string,
        facility_mode, (facility_password ? facility_password : "" ), classx);

    err = at_send_command(cmd, &atresponse);
    free(cmd);
    if (err < 0) {
        goto exit;
    }
    if (atresponse->success == 0) {
        if (cme_error_code = at_get_cme_error(atresponse)) {
            switch (cme_error_code) {
            /* CME ERROR 11: "SIM PIN required" happens when PIN is wrong */
            case CME_SIM_PIN_REQUIRED:
                LOGI("requestSetFacilityLock(): wrong PIN");
                num_retries = getNumRetries(RIL_REQUEST_ENTER_SIM_PIN);
                errorril = RIL_E_PASSWORD_INCORRECT;
                break;
			/*
			* CME ERROR 12: "SIM PUK required" happens when wrong PIN is used
			* 3 times in a row
			*/
            case CME_SIM_PUK_REQUIRED:
                LOGI("requestSetFacilityLock() PIN locked,"
                " change PIN with PUK");
                num_retries = 0;/* PUK required */
                errorril = RIL_E_PASSWORD_INCORRECT;
                break;
            /* CME ERROR 16: "Incorrect password" happens when PIN is wrong */
            case CME_INCORRECT_PASSWORD:
                LOGI("%s(): Incorrect password, Facility: %s", __func__,
                     facility_string);
                errorril = RIL_E_PASSWORD_INCORRECT;
                break;
            /* CME ERROR 17: "SIM PIN2 required" happens when PIN2 is wrong */
            case CME_SIM_PIN2_REQUIRED:
                LOGI("requestSetFacilityLock() wrong PIN2");
                num_retries = getNumRetries(RIL_REQUEST_ENTER_SIM_PIN2);
                errorril = RIL_E_PASSWORD_INCORRECT;
                break;
			/*
			* CME ERROR 18: "SIM PUK2 required" happens when wrong PIN2 is used
			* 3 times in a row
			*/
            case CME_SIM_PUK2_REQUIRED:
                LOGI("requestSetFacilityLock() PIN2 locked, change PIN2"
                "with PUK2");
                num_retries = 0;/* PUK2 required */
                errorril = RIL_E_SIM_PUK2;
                break;
            default: /* some other error */
                num_retries = -1;
                break;
            }
        }
        goto finally;
    }

    errorril = RIL_E_SUCCESS;

finally:
    if (num_retries == -1 && strncmp(facility_string, "SC", 2) == 0)
        num_retries = getNumRetries(RIL_REQUEST_ENTER_SIM_PIN);
exit:
    at_response_free(atresponse);
    RIL_onRequestComplete(t, errorril, &num_retries, sizeof(int *));
}

/**
* RIL_REQUEST_QUERY_FACILITY_LOCK
*
* Query the status of a facility lock state.
*/
void requestQueryFacilityLock(void *data, size_t datalen, RIL_Token t)
{
    int err, response = 0;
    ATResponse *atresponse = NULL;
    char *cmd = NULL;
    char *line = NULL;
    char *facility_string = NULL;
    char *facility_class = NULL;
    int classx;
    ATLine *cursor;
    size_t i;
    bool barring_service = false;

	/*
	* The following barring services may return multiple lines of intermediate
	* result codes and will return two parameters in the +CLCK response.
	*
	* "AO": barr All Outgoing calls
	* "OI": barr Outgoing International calls
	* "AI": barr All Incomming calls
	* "IR": barr Incoming calls when Roaming outside the home country
	* "OX": barr Outgoing international calls eXcept to home country
	*/
    char *barr_facilities[] = {"AO", "OI", "AI", "IR", "OX", NULL};

    assert(datalen >= (3 * sizeof(char **)));

    facility_string = ((char **) data)[0];
    facility_class = ((char **) data)[2];
    classx = atoi(facility_class);

	/*
	* Android send class 0 for USSD strings that didn't contain a class.
	* Class 0 is not considered a valid value and according to 3GPP 24.080 a
	* missing BasicService (BS) parameter in the Supplementary Service string
	* indicates all BS'es.
	*
	* Therefore we convert a class of 0 into 255 (all classes) before sending
	* the AT command.
	*/
    if (classx == 0)
        classx = 255;

    for (i = 0; barr_facilities[i] != NULL; i++) {
        if (!strncmp(facility_string, barr_facilities[i], 2)) {
            barring_service = true;
        }
    }

    /* password is not needed for query of facility lock. */
    asprintf(&cmd, "AT+CLCK=\"%s\",2,,%d", facility_string, classx);

    err = at_send_command_multiline(cmd, "+CLCK:", &atresponse);
    free(cmd);
    if (err < 0 || atresponse->success == 0)
        goto error;

    for (cursor = atresponse->p_intermediates; cursor != NULL;
         cursor = cursor->p_next) {
        int status = 0;

        line = cursor->line;

        err = at_tok_start(&line);

        if (err < 0)
            goto error;

        err = at_tok_nextint(&line, &status);

        if (err < 0)
            goto error;

        if (barring_service) {
            err = at_tok_nextint(&line, &classx);

            if (err < 0)
                goto error;

            if (status == 1)
                response += classx;
        } else {
            switch (status) {
            case 1:
                /* Default value including voice, data and fax services */
                response = 7;
                break;
            default:
                response = 0;
            }
			/*
			* There will be only 1 line of intermediate result codes when <fac>
			* is not a barring service.
			*/
            break;
        }
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(int *));

finally:
    at_response_free(atresponse);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    goto finally;
}

/**
 * RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC
 *
 * Specify that the network should be selected automatically.
 */
void requestSetNetworkSelectionAutomatic(void *data, size_t datalen,
                                         RIL_Token t)
{
    int err = 0;

    err = at_send_command("AT+COPS=0", NULL);
    if (err < 0)
        goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    return;
}

/**
 * RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL
 *
 * Manually select a specified network.
 *
 * The radio baseband/RIL implementation will try to camp on the manually
 * selected network regardless of coverage, i.e. there is no fallback to
 * automatic network selection.
 */
void requestSetNetworkSelectionManual(void *data, size_t datalen,
                                      RIL_Token t)
{
    /*
     * AT+COPS=[<mode>[,<format>[,<oper>[,<AcT>]]]]
     *    <mode>   = 1 = Manual (<oper> field shall be present and AcT
     *                   optionally)
     *    <format> = 2 = Numeric <oper>, the number has structure:
     *                   (country code digit 3)(country code digit 2)
     *                   (country code digit 1)(network code digit 2)
     *                   (network code digit 1)
     */

    int err = 0;
    char *cmd = NULL;
    ATResponse *atresponse = NULL;
    const char *mccMnc = (const char *) data;

    /* Check inparameter. */
    if (mccMnc == NULL)
        goto error;
    /* Build and send command. */
    asprintf(&cmd, "AT+COPS=1,2,\"%s\"", mccMnc);
    err = at_send_command(cmd, &atresponse);
    if (err < 0 || atresponse->success == 0)
        goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
finally:

    at_response_free(atresponse);

    if (cmd != NULL)
        free(cmd);

    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    goto finally;
}

/*
 * The parameters of the response to RIL_REQUEST_QUERY_AVAILABLE_NETWORKS are
 * defined in ril.h
 */
#define QUERY_NW_NUM_PARAMS 4

/**
 * RIL_REQUEST_QUERY_AVAILABLE_NETWORKS
 *
 * Scans for available networks.
 */
void requestQueryAvailableNetworks(void *data, size_t datalen, RIL_Token t)
{
    /*
     * AT+COPS=?
     *   +COPS: [list of supported (<stat>,long alphanumeric <oper>
     *           ,short alphanumeric <oper>,numeric <oper>[,<AcT>])s]
     *          [,,(list of supported <mode>s),(list of supported <format>s)]
     *
     *   <stat>
     *     0 = unknown
     *     1 = available
     *     2 = current
     *     3 = forbidden
     */

    int err = 0;
    ATResponse *atresponse = NULL;
    const char *statusTable[] =
    { "unknown", "available", "current", "forbidden" };
    char **responseArray = NULL;
    char *p;
    int n = 0, i = 0, j = 0, numStoredNetworks = 0;
    char *s = NULL;

    err = at_send_command_multiline("AT+COPS=?", "+COPS:",
                                                 &atresponse);
    if (err < 0 ||
        atresponse->success == 0 || atresponse->p_intermediates == NULL)
        goto error;

    p = atresponse->p_intermediates->line;

    /* count number of '('. */
    err = at_tok_charcounter(p, '(', &n);
    if (err < 0) goto error;

    /* Allocate array of strings, blocks of 4 strings. */
    n = n / 3;
    responseArray = alloca(n * QUERY_NW_NUM_PARAMS * sizeof(char *));

    /* Loop and collect response information into the response array. */
    for (i = 0; i < n; i++) {
        int status = 0;
        char *line = NULL;
        char *longAlphaNumeric = NULL;
        char *shortAlphaNumeric = NULL;
        char *numeric = NULL;
        char *remaining = NULL;
        int continueOuterLoop = 0;

        s = line = getFirstElementValue(p, "(", ")", &remaining);
        p = remaining;

        if (line == NULL) {
            LOGE("Null pointer while parsing COPS response. This should not "
                 "happen.");
            goto error;
        }
        /* <stat> */
        err = at_tok_nextint(&line, &status);
        if (err < 0)
            goto error;

        /* long alphanumeric <oper> */
        err = at_tok_nextstr(&line, &longAlphaNumeric);
        if (err < 0)
            goto error;

        /* short alphanumeric <oper> */
        err = at_tok_nextstr(&line, &shortAlphaNumeric);
        if (err < 0)
            goto error;

        /* numeric <oper> */
        err = at_tok_nextstr(&line, &numeric);
        if (err < 0)
            goto error;

        /*
         * The response of AT+COPS=? returns GSM networks and WCDMA networks as
         * separate network search hits. The RIL API does not support network
         * type parameter and the RIL must prevent duplicates.
         */
        for (j = numStoredNetworks - 1; j >= 0; j--)
            if (strcmp(responseArray[j * QUERY_NW_NUM_PARAMS + 2],
                       numeric) == 0) {
                LOGI("%s(): Skipped storing duplicate operator: %s.",
                     __func__, longAlphaNumeric);
                continueOuterLoop = 1;
                break;
            }

        if (continueOuterLoop) {
            free(s);
            s = NULL;
            continue; /* Skip storing this duplicate operator */
        }

        responseArray[numStoredNetworks * QUERY_NW_NUM_PARAMS + 0] =
            alloca(strlen(longAlphaNumeric) + 1);
        strcpy(responseArray[numStoredNetworks * QUERY_NW_NUM_PARAMS + 0],
                             longAlphaNumeric);

        responseArray[numStoredNetworks * QUERY_NW_NUM_PARAMS + 1] =
            alloca(strlen(shortAlphaNumeric) + 1);
        strcpy(responseArray[numStoredNetworks * QUERY_NW_NUM_PARAMS + 1],
                             shortAlphaNumeric);

        responseArray[numStoredNetworks * QUERY_NW_NUM_PARAMS + 2] =
            alloca(strlen(numeric) + 1);
        strcpy(responseArray[numStoredNetworks * QUERY_NW_NUM_PARAMS + 2],
               numeric);

        /* Add status */
        responseArray[numStoredNetworks * QUERY_NW_NUM_PARAMS + 3] =
            alloca(strlen(statusTable[status]) + 1);
        sprintf(responseArray[numStoredNetworks * QUERY_NW_NUM_PARAMS + 3],
                "%s", statusTable[status]);

        numStoredNetworks++;
        free(s);
        s = NULL;

        /* bypass the mode and format list   */
        s = line = getFirstElementValue(p, "(", ")", &remaining);
        if (line == NULL) {
            LOGE("Null pointer while parsing COPS response. This should not "
                 "happen.");
            break;
        }
        free(s);
        s = NULL;

        s = line = getFirstElementValue(p, "(", ")", &remaining);
        if (line == NULL) {
            LOGE("Null pointer while parsing COPS response. This should not "
                 "happen.");
            break;
        }
        free(s);
        s = NULL;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, responseArray, numStoredNetworks *
                          QUERY_NW_NUM_PARAMS * sizeof(char *));

finally:
    at_response_free(atresponse);
    return;

error:
    free(s);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    goto finally;
}


/*** Callback methods from the RIL library to us ***/

/**
 * Call from RIL to us to make a RIL_REQUEST
 *
 * Must be completed with a call to RIL_onRequestComplete()
 *
 * RIL_onRequestComplete() may be called from any thread, before or after
 * this function returns.
 *
 * Will always be called from the same thread, so returning here implies
 * that the radio is ready to process another command (whether or not
 * the previous command has completed).
 */
static void
onRequest (int request, void *data, size_t datalen, RIL_Token t)
{
    ATResponse *p_response;
    int err;

    RLOGD("onRequest: %s", requestToString(request));

    /* Ignore all requests except RIL_REQUEST_GET_SIM_STATUS
     * when RADIO_STATE_UNAVAILABLE.
     */
    if (sState == RADIO_STATE_UNAVAILABLE
        && request != RIL_REQUEST_GET_SIM_STATUS
    ) {
        RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
        return;
    }

    /* Ignore all non-power requests when RADIO_STATE_OFF
     * (except RIL_REQUEST_GET_SIM_STATUS)
     */
    if (sState == RADIO_STATE_OFF
        && !(request == RIL_REQUEST_RADIO_POWER
            || request == RIL_REQUEST_GET_SIM_STATUS)
    ) {
        RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
        return;
    }

    switch (request) {
        case RIL_REQUEST_GET_SIM_STATUS: {
            RIL_CardStatus_v6 *p_card_status;
            char *p_buffer;
            int buffer_size;

            int result = getCardStatus(&p_card_status);
            if (result == RIL_E_SUCCESS) {
                p_buffer = (char *)p_card_status;
                buffer_size = sizeof(*p_card_status);
            } else {
                p_buffer = NULL;
                buffer_size = 0;
            }
            RIL_onRequestComplete(t, result, p_buffer, buffer_size);
            freeCardStatus(p_card_status);
            break;
        }
        case RIL_REQUEST_GET_CURRENT_CALLS:
            requestGetCurrentCalls(data, datalen, t);
            break;
        case RIL_REQUEST_DIAL:
            requestDial(data, datalen, t);
            break;
        case RIL_REQUEST_HANGUP:
            requestHangup(data, datalen, t);
            break;
        case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND:
            // 3GPP 22.030 6.5.5
            // "Releases all held calls or sets User Determined User Busy
            //  (UDUB) for a waiting call."
            at_send_command("AT+CHLD=0", NULL);

            /* success or failure is ignored by the upper layer here.
               it will call GET_CURRENT_CALLS and determine success that way */
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND:
            // 3GPP 22.030 6.5.5
            // "Releases all active calls (if any exist) and accepts
            //  the other (held or waiting) call."
            at_send_command("AT+CHLD=1", NULL);

            /* success or failure is ignored by the upper layer here.
               it will call GET_CURRENT_CALLS and determine success that way */
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        case RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE:
            // 3GPP 22.030 6.5.5
            // "Places all active calls (if any exist) on hold and accepts
            //  the other (held or waiting) call."
            at_send_command("AT+CHLD=2", NULL);

#ifdef WORKAROUND_ERRONEOUS_ANSWER
            s_expectAnswer = 1;
#endif /* WORKAROUND_ERRONEOUS_ANSWER */

            /* success or failure is ignored by the upper layer here.
               it will call GET_CURRENT_CALLS and determine success that way */
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        case RIL_REQUEST_ANSWER:
            at_send_command("ATA", NULL);

#ifdef WORKAROUND_ERRONEOUS_ANSWER
            s_expectAnswer = 1;
#endif /* WORKAROUND_ERRONEOUS_ANSWER */

            /* success or failure is ignored by the upper layer here.
               it will call GET_CURRENT_CALLS and determine success that way */
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        case RIL_REQUEST_CONFERENCE:
            // 3GPP 22.030 6.5.5
            // "Adds a held call to the conversation"
            at_send_command("AT+CHLD=3", NULL);

            /* success or failure is ignored by the upper layer here.
               it will call GET_CURRENT_CALLS and determine success that way */
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        case RIL_REQUEST_UDUB:
            /* user determined user busy */
            /* sometimes used: ATH */
            at_send_command("ATH", NULL);

            /* success or failure is ignored by the upper layer here.
               it will call GET_CURRENT_CALLS and determine success that way */
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;

        case RIL_REQUEST_SEPARATE_CONNECTION:
            {
                char  cmd[12];
                int   party = ((int*)data)[0];

                // Make sure that party is in a valid range.
                // (Note: The Telephony middle layer imposes a range of 1 to 7.
                // It's sufficient for us to just make sure it's single digit.)
                if (party > 0 && party < 10) {
                    sprintf(cmd, "AT+CHLD=2%d", party);
                    at_send_command(cmd, NULL);
                    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
                } else {
                    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                }
            }
            break;

        case RIL_REQUEST_SIGNAL_STRENGTH:
            requestSignalStrength(data, datalen, t);
            break;
        case RIL_REQUEST_VOICE_REGISTRATION_STATE:
        case RIL_REQUEST_DATA_REGISTRATION_STATE:
            requestRegistrationState(request, data, datalen, t);
            break;
        case RIL_REQUEST_OPERATOR:
            requestOperator(data, datalen, t);
            break;
        case RIL_REQUEST_RADIO_POWER:
            requestRadioPower(data, datalen, t);
            break;
        case RIL_REQUEST_DTMF: {
            char c = ((char *)data)[0];
            char *cmd;
            asprintf(&cmd, "AT+VTS=%c", (int)c);
            at_send_command(cmd, NULL);
            free(cmd);
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        }
        case RIL_REQUEST_SEND_SMS:
        case RIL_REQUEST_SEND_SMS_EXPECT_MORE:
            requestSendSMS(data, datalen, t);
            break;
        case RIL_REQUEST_CDMA_SEND_SMS:
            requestCdmaSendSMS(data, datalen, t);
            break;
        case RIL_REQUEST_IMS_SEND_SMS:
            requestImsSendSMS(data, datalen, t);
            break;
        case RIL_REQUEST_SETUP_DATA_CALL:
            requestSetupDataCall(data, datalen, t);
            break;
        case RIL_REQUEST_DEACTIVATE_DATA_CALL:
            requestDeactivateDataCall(data, datalen, t);
            break;
        case RIL_REQUEST_SMS_ACKNOWLEDGE:
            requestSMSAcknowledge(data, datalen, t);
            break;

        case RIL_REQUEST_GET_IMSI:
            p_response = NULL;
            err = at_send_command_numeric("AT+CIMI", &p_response);

            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS,
                    p_response->p_intermediates->line, sizeof(char *));
            }
            at_response_free(p_response);
            break;

        case RIL_REQUEST_GET_IMEI:
            p_response = NULL;
            err = at_send_command_numeric("AT+CGSN", &p_response);

            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS,
                    p_response->p_intermediates->line, sizeof(char *));
            }
            at_response_free(p_response);
            break;

        case RIL_REQUEST_SIM_IO:
            requestSIM_IO(data,datalen,t);
            break;

        case RIL_REQUEST_SEND_USSD:
            requestSendUSSD(data, datalen, t);
            break;

        case RIL_REQUEST_CANCEL_USSD:
            p_response = NULL;
            err = at_send_command_numeric("AT+CUSD=2", &p_response);

            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS,
                    p_response->p_intermediates->line, sizeof(char *));
            }
            at_response_free(p_response);
            break;

        case RIL_REQUEST_DATA_CALL_LIST:
            requestDataCallList(data, datalen, t);
            break;

        case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE:
            requestQueryNetworkSelectionMode(data, datalen, t);
            break;

        case RIL_REQUEST_OEM_HOOK_RAW:
            // echo back data
            RIL_onRequestComplete(t, RIL_E_SUCCESS, data, datalen);
            break;


        case RIL_REQUEST_OEM_HOOK_STRINGS: {
            int i;
            const char ** cur;

            RLOGD("got OEM_HOOK_STRINGS: 0x%8p %lu", data, (long)datalen);


            for (i = (datalen / sizeof (char *)), cur = (const char **)data ;
                    i > 0 ; cur++, i --) {
                RLOGD("> '%s'", *cur);
            }

            // echo back strings
            RIL_onRequestComplete(t, RIL_E_SUCCESS, data, datalen);
            break;
        }

        case RIL_REQUEST_WRITE_SMS_TO_SIM:
            requestWriteSmsToSim(data, datalen, t);
            break;

        case RIL_REQUEST_DELETE_SMS_ON_SIM: {
            char * cmd;
            p_response = NULL;
            asprintf(&cmd, "AT+CMGD=%d", ((int *)data)[0]);
            err = at_send_command(cmd, &p_response);
            free(cmd);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }

        case RIL_REQUEST_ENTER_SIM_PIN:
        case RIL_REQUEST_ENTER_SIM_PUK:
        case RIL_REQUEST_ENTER_SIM_PIN2:
        case RIL_REQUEST_ENTER_SIM_PUK2:
            requestEnterSimPin(data, datalen, t, request);
            break;

        case RIL_REQUEST_IMS_REGISTRATION_STATE: {
            int reply[2];
            //0==unregistered, 1==registered
            reply[0] = s_ims_registered;

            //to be used when changed to include service supporated info
            //reply[1] = s_ims_services;

            // FORMAT_3GPP(1) vs FORMAT_3GPP2(2);
            reply[1] = s_ims_format;

            RLOGD("IMS_REGISTRATION=%d, format=%d ",
                    reply[0], reply[1]);
            if (reply[1] != -1) {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, reply, sizeof(reply));
            } else {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            }
            break;
        }

        case RIL_REQUEST_VOICE_RADIO_TECH:
            {
                int tech = techFromModemType(TECH(sMdmInfo));
                if (tech < 0 )
                    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                else
                    RIL_onRequestComplete(t, RIL_E_SUCCESS, &tech, sizeof(tech));
            }
            break;

        case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE:
            requestSetPreferredNetworkType(request, data, datalen, t);
            break;

        case RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE:
            requestGetPreferredNetworkType(request, data, datalen, t);
            break;

        case RIL_REQUEST_GET_CELL_INFO_LIST:
            requestGetCellInfoList(data, datalen, t);
            break;

        case RIL_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE:
            requestSetCellInfoListRate(data, datalen, t);
            break;

        case RIL_REQUEST_GET_HARDWARE_CONFIG:
            requestGetHardwareConfig(data, datalen, t);
            break;

        case RIL_REQUEST_SHUTDOWN:
            requestShutdown(t);
            break;

        /* CDMA Specific Requests */
        case RIL_REQUEST_BASEBAND_VERSION:
            if (TECH_BIT(sMdmInfo) == MDM_CDMA) {
                requestCdmaBaseBandVersion(request, data, datalen, t);
                break;
            } // Fall-through if tech is not cdma
            else if (current_modem_type == HUAWEI_MODEM){
                requestBasebandVersion(data, datalen, t);
                break;
            }
            else {
                RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;
            }

        case RIL_REQUEST_DEVICE_IDENTITY:
            if (TECH_BIT(sMdmInfo) == MDM_CDMA) {
                requestCdmaDeviceIdentity(request, data, datalen, t);
                break;
            } // Fall-through if tech is not cdma

        case RIL_REQUEST_CDMA_SUBSCRIPTION:
            if (TECH_BIT(sMdmInfo) == MDM_CDMA) {
                requestCdmaSubscription(request, data, datalen, t);
            } else{
                /* Should it's current tech is not MDM_CDMA, complete it with FAILURE.
                 * Otherwise RILJ will crash in readRilMessage.
                */
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            }
            break;

        case RIL_REQUEST_CDMA_SET_SUBSCRIPTION_SOURCE:
            if (TECH_BIT(sMdmInfo) == MDM_CDMA) {
                requestCdmaSetSubscriptionSource(request, data, datalen, t);
                break;
            } // Fall-through if tech is not cdma

        case RIL_REQUEST_CDMA_GET_SUBSCRIPTION_SOURCE:
            if (TECH_BIT(sMdmInfo) == MDM_CDMA) {
                requestCdmaGetSubscriptionSource(request, data, datalen, t);
                break;
            } // Fall-through if tech is not cdma

        case RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE:
            if (TECH_BIT(sMdmInfo) == MDM_CDMA) {
                requestCdmaGetRoamingPreference(request, data, datalen, t);
                break;
            } // Fall-through if tech is not cdma

        case RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE:
            if (TECH_BIT(sMdmInfo) == MDM_CDMA) {
                requestCdmaSetRoamingPreference(request, data, datalen, t);
                break;
            } // Fall-through if tech is not cdma

        case RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE:
            if (TECH_BIT(sMdmInfo) == MDM_CDMA) {
                requestExitEmergencyMode(data, datalen, t);
                break;
            } // Fall-through if tech is not cdma
        case RIL_REQUEST_CHANGE_SIM_PIN:
            requestChangeSimPin(data, datalen, t, request);
            break;
        	
        case RIL_REQUEST_CHANGE_SIM_PIN2:
            requestChangeSimPin2(data, datalen, t, request);
            break;
        	
        case RIL_REQUEST_SET_FACILITY_LOCK:
            requestSetFacilityLock(data, datalen, t);
            break;
        	
        case RIL_REQUEST_QUERY_FACILITY_LOCK:
            requestQueryFacilityLock(data, datalen, t);
            break;

        case RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL:
            requestSetNetworkSelectionManual(data, datalen, t);
            break;

        case RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC:
            requestSetNetworkSelectionAutomatic(data, datalen, t);
            break;

        case RIL_REQUEST_QUERY_AVAILABLE_NETWORKS:
            requestQueryAvailableNetworks(data, datalen, t);
            break;

        default:
            RLOGD("Request not supported. Tech: %d",TECH(sMdmInfo));
            RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
            break;
    }
}

/**
 * Synchronous call from the RIL to us to return current radio state.
 * RADIO_STATE_UNAVAILABLE should be the initial state.
 */
static RIL_RadioState
currentState()
{
    return sState;
}
/**
 * Call from RIL to us to find out whether a specific request code
 * is supported by this implementation.
 *
 * Return 1 for "supported" and 0 for "unsupported"
 */

static int
onSupports (int requestCode __unused)
{
    //@@@ todo

    return 1;
}

static void onCancel (RIL_Token t __unused)
{
    //@@@todo

}

#define REFERENCE_RIL_DEF_VERSION "android reference-ril 1.0"
#define REFERENCE_RIL_HUAWEI_VERSION "HUAWEI reference-ril 1.0"
#define REFERENCE_RIL_AMAZON_VERSION "AMAZON reference-ril 1.0"
static const char * getVersion(void)
{
	int modem_type;
	modem_type = runtime_3g_port_type();

	if (AMAZON_MODEM == modem_type)
		return REFERENCE_RIL_AMAZON_VERSION;
	else if (HUAWEI_MODEM == modem_type)
		return REFERENCE_RIL_HUAWEI_VERSION;
	else
		return REFERENCE_RIL_DEF_VERSION;
}

static void
setRadioTechnology(ModemInfo *mdm, int newtech)
{
    RLOGD("setRadioTechnology(%d)", newtech);

    int oldtech = TECH(mdm);

    if (newtech != oldtech) {
        RLOGD("Tech change (%d => %d)", oldtech, newtech);
        TECH(mdm) = newtech;
        if (techFromModemType(newtech) != techFromModemType(oldtech)) {
            int tech = techFromModemType(TECH(sMdmInfo));
            if (tech > 0 ) {
                RIL_onUnsolicitedResponse(RIL_UNSOL_VOICE_RADIO_TECH_CHANGED,
                                          &tech, sizeof(tech));
            }
        }
    }
}

static void
setRadioState(RIL_RadioState newState)
{
    RLOGD("setRadioState(%d)", newState);
    RIL_RadioState oldState;

    pthread_mutex_lock(&s_state_mutex);

    oldState = sState;

    if (s_closed > 0) {
        // If we're closed, the only reasonable state is
        // RADIO_STATE_UNAVAILABLE
        // This is here because things on the main thread
        // may attempt to change the radio state after the closed
        // event happened in another thread
        newState = RADIO_STATE_UNAVAILABLE;
    }

    if (sState != newState || s_closed > 0) {
        sState = newState;
        pthread_cond_broadcast (&s_state_cond);
    }

    pthread_mutex_unlock(&s_state_mutex);


    /* do these outside of the mutex */
    if (sState != oldState) {
        RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED,
                                    NULL, 0);
        // Sim state can change as result of radio state change
        RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                                    NULL, 0);

        /* FIXME onSimReady() and onRadioPowerOn() cannot be called
         * from the AT reader thread
         * Currently, this doesn't happen, but if that changes then these
         * will need to be dispatched on the request thread
         */
        if (sState == RADIO_STATE_ON) {
            onRadioPowerOn();
        }
    }
}

/** Returns RUIM_NOT_READY on error */
static SIM_Status
getRUIMStatus()
{
    ATResponse *p_response = NULL;
    int err;
    int ret;
    char *cpinLine;
    char *cpinResult;

    if (sState == RADIO_STATE_OFF || sState == RADIO_STATE_UNAVAILABLE) {
        ret = SIM_NOT_READY;
        goto done;
    }

    err = at_send_command_singleline("AT+CPIN?", "+CPIN:", &p_response);

    if (err != 0) {
        ret = SIM_NOT_READY;
        goto done;
    }

    switch (at_get_cme_error(p_response)) {
        case CME_SUCCESS:
            break;

        case CME_SIM_NOT_INSERTED:
            ret = SIM_ABSENT;
            goto done;

        default:
            ret = SIM_NOT_READY;
            goto done;
    }

    /* CPIN? has succeeded, now look at the result */

    cpinLine = p_response->p_intermediates->line;
    err = at_tok_start (&cpinLine);

    if (err < 0) {
        ret = SIM_NOT_READY;
        goto done;
    }

    err = at_tok_nextstr(&cpinLine, &cpinResult);

    if (err < 0) {
        ret = SIM_NOT_READY;
        goto done;
    }

    if (0 == strcmp (cpinResult, "SIM PIN")) {
        ret = SIM_PIN;
        goto done;
    } else if (0 == strcmp (cpinResult, "SIM PUK")) {
        ret = SIM_PUK;
        goto done;
    } else if (0 == strcmp (cpinResult, "PH-NET PIN")) {
        return SIM_NETWORK_PERSONALIZATION;
    } else if (0 != strcmp (cpinResult, "READY"))  {
        /* we're treating unsupported lock types as "sim absent" */
        ret = SIM_ABSENT;
        goto done;
    }

    at_response_free(p_response);
    p_response = NULL;
    cpinResult = NULL;

    ret = SIM_READY;

done:
    at_response_free(p_response);
    return ret;
}

/** Returns SIM_NOT_READY on error */
static SIM_Status
getSIMStatus()
{
    ATResponse *p_response = NULL;
    int err;
    int ret;
    char *cpinLine;
    char *cpinResult;

    RLOGD("getSIMStatus(). sState: %d",sState);
    if (sState == RADIO_STATE_OFF || sState == RADIO_STATE_UNAVAILABLE) {
        ret = SIM_NOT_READY;
        goto done;
    }

    err = at_send_command_singleline("AT+CPIN?", "+CPIN:", &p_response);

    if (err != 0) {
        ret = SIM_NOT_READY;
        goto done;
    }

    switch (at_get_cme_error(p_response)) {
        case CME_SUCCESS:
            break;

        case CME_SIM_NOT_INSERTED:
            ret = SIM_ABSENT;
            goto done;

        default:
            ret = SIM_NOT_READY;
            goto done;
    }

    /* CPIN? has succeeded, now look at the result */

    cpinLine = p_response->p_intermediates->line;
    err = at_tok_start (&cpinLine);

    if (err < 0) {
        ret = SIM_NOT_READY;
        goto done;
    }

    err = at_tok_nextstr(&cpinLine, &cpinResult);

    if (err < 0) {
        ret = SIM_NOT_READY;
        goto done;
    }

    if (0 == strcmp (cpinResult, "SIM PIN")) {
        ret = SIM_PIN;
        goto done;
    } else if (0 == strcmp (cpinResult, "SIM PUK")) {
        ret = SIM_PUK;
        goto done;
    } else if (0 == strcmp (cpinResult, "PH-NET PIN")) {
        return SIM_NETWORK_PERSONALIZATION;
    } else if (0 != strcmp (cpinResult, "READY"))  {
        /* we're treating unsupported lock types as "sim absent" */
        ret = SIM_ABSENT;
        goto done;
    }

    at_response_free(p_response);
    p_response = NULL;
    cpinResult = NULL;

    ret = SIM_READY;

done:
    at_response_free(p_response);
    return ret;
}


/**
 * Get the current card status.
 *
 * This must be freed using freeCardStatus.
 * @return: On success returns RIL_E_SUCCESS
 */
static int getCardStatus(RIL_CardStatus_v6 **pp_card_status) {
    static RIL_AppStatus app_status_array[] = {
        // SIM_ABSENT = 0
        { RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_UNKNOWN, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
        // SIM_NOT_READY = 1
        { RIL_APPTYPE_SIM, RIL_APPSTATE_DETECTED, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
        // SIM_READY = 2
        { RIL_APPTYPE_SIM, RIL_APPSTATE_READY, RIL_PERSOSUBSTATE_READY,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
        // SIM_PIN = 3
        { RIL_APPTYPE_SIM, RIL_APPSTATE_PIN, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
        // SIM_PUK = 4
        { RIL_APPTYPE_SIM, RIL_APPSTATE_PUK, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_BLOCKED, RIL_PINSTATE_UNKNOWN },
        // SIM_NETWORK_PERSONALIZATION = 5
        { RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
        // RUIM_ABSENT = 6
        { RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_UNKNOWN, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
        // RUIM_NOT_READY = 7
        { RIL_APPTYPE_RUIM, RIL_APPSTATE_DETECTED, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
        // RUIM_READY = 8
        { RIL_APPTYPE_RUIM, RIL_APPSTATE_READY, RIL_PERSOSUBSTATE_READY,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
        // RUIM_PIN = 9
        { RIL_APPTYPE_RUIM, RIL_APPSTATE_PIN, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
        // RUIM_PUK = 10
        { RIL_APPTYPE_RUIM, RIL_APPSTATE_PUK, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_BLOCKED, RIL_PINSTATE_UNKNOWN },
        // RUIM_NETWORK_PERSONALIZATION = 11
        { RIL_APPTYPE_RUIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK,
           NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN }
    };
    RIL_CardState card_state;
    int num_apps;

    int sim_status = getSIMStatus();
    if (sim_status == SIM_ABSENT) {
        card_state = RIL_CARDSTATE_ABSENT;
        num_apps = 0;
    } else {
        card_state = RIL_CARDSTATE_PRESENT;
        num_apps = 2;
    }

    // Allocate and initialize base card status.
    RIL_CardStatus_v6 *p_card_status = malloc(sizeof(RIL_CardStatus_v6));
    p_card_status->card_state = card_state;
    p_card_status->universal_pin_state = RIL_PINSTATE_UNKNOWN;
    p_card_status->gsm_umts_subscription_app_index = RIL_CARD_MAX_APPS;
    p_card_status->cdma_subscription_app_index = RIL_CARD_MAX_APPS;
    p_card_status->ims_subscription_app_index = RIL_CARD_MAX_APPS;
    p_card_status->num_applications = num_apps;

    // Initialize application status
    int i;
    for (i = 0; i < RIL_CARD_MAX_APPS; i++) {
        p_card_status->applications[i] = app_status_array[SIM_ABSENT];
    }

    // Pickup the appropriate application status
    // that reflects sim_status for gsm.
    if (num_apps != 0) {
        // Only support one app, gsm
        p_card_status->num_applications = 2;
        p_card_status->gsm_umts_subscription_app_index = 0;
        p_card_status->cdma_subscription_app_index = 1;

        // Get the correct app status
        p_card_status->applications[0] = app_status_array[sim_status];
        p_card_status->applications[1] = app_status_array[sim_status + RUIM_ABSENT];
    }

    *pp_card_status = p_card_status;
    return RIL_E_SUCCESS;
}

/**
 * Free the card status returned by getCardStatus
 */
static void freeCardStatus(RIL_CardStatus_v6 *p_card_status) {
    free(p_card_status);
}

/**
 * SIM ready means any commands that access the SIM will work, including:
 *  AT+CPIN, AT+CSMS, AT+CNMI, AT+CRSM
 *  (all SMS-related commands)
 */

static void pollSIMState (void *param __unused)
{
    ATResponse *p_response;
    int ret;

    if (sState != RADIO_STATE_ON &&
	sState != RADIO_STATE_SIM_LOCKED_OR_ABSENT) {
    // no longer valid to poll
        return;
    }
    switch(getSIMStatus()) {
        case SIM_ABSENT:
        case SIM_PIN:
        case SIM_PUK:
        case SIM_NETWORK_PERSONALIZATION:
        default:
            RLOGI("SIM ABSENT or LOCKED");
            RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL, 0);
        return;

        case SIM_NOT_READY:
            RIL_requestTimedCallback (pollSIMState, NULL, &TIMEVAL_SIMPOLL);
        return;

        case SIM_READY:
            RLOGI("SIM_READY");
            onSIMReady();
            RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL, 0);
        return;
    }
}

/** returns 1 if on, 0 if off, and -1 on error */
static int isRadioOn()
{
    ATResponse *p_response = NULL;
    int err;
    char *line;
    char ret;

    err = at_send_command_singleline("AT+CFUN?", "+CFUN:", &p_response);

    if (err < 0 || p_response->success == 0) {
        // assume radio is off
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextbool(&line, &ret);
    if (err < 0) goto error;

    at_response_free(p_response);

    return (int)ret;

error:

    at_response_free(p_response);
    return -1;
}

/**
 * Parse the response generated by a +CTEC AT command
 * The values read from the response are stored in current and preferred.
 * Both current and preferred may be null. The corresponding value is ignored in that case.
 *
 * @return: -1 if some error occurs (or if the modem doesn't understand the +CTEC command)
 *          1 if the response includes the current technology only
 *          0 if the response includes both current technology and preferred mode
 */
int parse_technology_response( const char *response, int *current, int32_t *preferred )
{
    int err;
    char *line, *p;
    int ct;
    int32_t pt = 0;
    char *str_pt;

    line = p = strdup(response);
    RLOGD("Response: %s", line);
    err = at_tok_start(&p);
    if (err || !at_tok_hasmore(&p)) {
        RLOGD("err: %d. p: %s", err, p);
        free(line);
        return -1;
    }

    err = at_tok_nextint(&p, &ct);
    if (err) {
        free(line);
        return -1;
    }
    if (current) *current = ct;

    RLOGD("line remaining after int: %s", p);

    err = at_tok_nexthexint(&p, &pt);
    if (err) {
        free(line);
        return 1;
    }
    if (preferred) {
        *preferred = pt;
    }
    free(line);

    return 0;
}

int query_supported_techs( ModemInfo *mdm __unused, int *supported )
{
    ATResponse *p_response;
    int err, val, techs = 0;
    char *tok;
    char *line;

    RLOGD("query_supported_techs");
    err = at_send_command_singleline("AT+CTEC=?", "+CTEC:", &p_response);
    if (err || !p_response->success)
        goto error;
    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err || !at_tok_hasmore(&line))
        goto error;
    while (!at_tok_nextint(&line, &val)) {
        techs |= ( 1 << val );
    }
    if (supported) *supported = techs;
    return 0;
error:
    at_response_free(p_response);
    return -1;
}

/**
 * query_ctec. Send the +CTEC AT command to the modem to query the current
 * and preferred modes. It leaves values in the addresses pointed to by
 * current and preferred. If any of those pointers are NULL, the corresponding value
 * is ignored, but the return value will still reflect if retreiving and parsing of the
 * values suceeded.
 *
 * @mdm Currently unused
 * @current A pointer to store the current mode returned by the modem. May be null.
 * @preferred A pointer to store the preferred mode returned by the modem. May be null.
 * @return -1 on error (or failure to parse)
 *         1 if only the current mode was returned by modem (or failed to parse preferred)
 *         0 if both current and preferred were returned correctly
 */
int query_ctec(ModemInfo *mdm __unused, int *current, int32_t *preferred)
{
    ATResponse *response = NULL;
    int err;
    int res;
	int modem_type;
	modem_type = runtime_3g_port_type();

    if ((HUAWEI_MODEM == modem_type) ||
            (AMAZON_MODEM == modem_type)){
        return -1;
    }else{
        RLOGD("query_ctec. current: %d, preferred: %d", (int)current, (int) preferred);
        err = at_send_command_singleline("AT+CTEC?", "+CTEC:", &response);
        if (!err && response->success) {
            res = parse_technology_response(response->p_intermediates->line, current, preferred);
            at_response_free(response);
            return res;
        }
        RLOGE("Error executing command: %d. response: %x. status: %d", err, (int)response, response? response->success : -1);
        at_response_free(response);
        return -1;
    }
}

int is_multimode_modem(ModemInfo *mdm)
{
    ATResponse *response;
    int err;
    char *line;
    int tech;
    int32_t preferred;

    if (query_ctec(mdm, &tech, &preferred) == 0) {
        mdm->currentTech = tech;
        mdm->preferredNetworkMode = preferred;
        if (query_supported_techs(mdm, &mdm->supportedTechs)) {
            return 0;
        }
        return 1;
    }
    return 0;
}

/**
 * Find out if our modem is GSM, CDMA or both (Multimode)
 */
static void probeForModemMode(ModemInfo *info)
{
    ATResponse *response;
    int err;
    int modem_type;
    modem_type = runtime_3g_port_type();

    if ((HUAWEI_MODEM == modem_type) ||
            (AMAZON_MODEM == modem_type)){
        /* AT+CTEC is not supported by EM770W. Even worse, AT process would be blocked
         * when it encounters unknown command. So here just hard code ModemInfo.
         */
        info->preferredNetworkMode = MDM_GSM | MDM_WCDMA; //Has equal preference
    }else{

        assert (info);
        // Currently, our only known multimode modem is qemu's android modem,
        // which implements the AT+CTEC command to query and set mode.
        // Try that first

        if (is_multimode_modem(info)) {
            RLOGI("Found Multimode Modem. Supported techs mask: %8.8x. Current tech: %d",
                    info->supportedTechs, info->currentTech);
            return;
        }

        /* Being here means that our modem is not multimode */
        info->isMultimode = 0;

        /* CDMA Modems implement the AT+WNAM command */
        err = at_send_command_singleline("AT+WNAM","+WNAM:", &response);
        if (!err && response->success) {
            at_response_free(response);
            // TODO: find out if we really support EvDo
            info->supportedTechs = MDM_CDMA | MDM_EVDO;
            info->currentTech = MDM_CDMA;
            RLOGI("Found CDMA Modem");
            return;
        }
        if (!err) at_response_free(response);
    }
    // TODO: find out if modem really supports WCDMA/LTE
    info->supportedTechs = MDM_GSM | MDM_WCDMA | MDM_LTE;
    info->currentTech = MDM_GSM;
    RLOGI("Found GSM Modem");
}

/**
 * Initialize everything that can be configured while we're still in
 * AT+CFUN=0
 */
static void initializeCallback(void *param __unused)
{
    ATResponse *p_response = NULL;
    int err;

    setRadioState (RADIO_STATE_OFF);

    at_handshake();

    probeForModemMode(sMdmInfo);
    /* note: we don't check errors here. Everything important will
       be handled in onATTimeout and onATReaderClosed */

    /*  atchannel is tolerant of echo but it must */
    /*  have verbose result codes */
    at_send_command("ATE0Q0V1", NULL);

    /*  No auto-answer */
    at_send_command("ATS0=0", NULL);

    /*  Extended errors */
    at_send_command("AT+CMEE=1", NULL);

    /*  Network registration events */
    err = at_send_command("AT+CREG=2", &p_response);

    /* some handsets -- in tethered mode -- don't support CREG=2 */
    if (err < 0 || p_response->success == 0) {
        at_response_free(p_response);
        err = at_send_command("AT+CREG=1", &p_response);
    }

    if (err < 0 || p_response->success == 0) {
        LOGE("Warning!No network registration events reported");
        sUnsolictedCREG_failed = 1;
    }
    else {
        sUnsolictedCREG_failed = 0;
    }
    at_response_free(p_response);

    /*  GPRS registration events */
    err = at_send_command("AT+CGREG=1", &p_response);
    if (err < 0 || p_response->success == 0) {
        LOGE("Warning!No GPRS registration events reported");
        sUnsolictedCGREG_failed = 1;
    }
    else {
        sUnsolictedCGREG_failed = 0;
    }

    at_response_free(p_response);

    /*  Call Waiting notifications */
    at_send_command("AT+CCWA=1", NULL);

    /*  Alternating voice/data off */
    at_send_command("AT+CMOD=0", NULL);

    /*  Not muted */
    at_send_command("AT+CMUT=0", NULL);

    /*  +CSSU unsolicited supp service notifications */
    at_send_command("AT+CSSN=0,1", NULL);

    /*  no connected line identification */
    at_send_command("AT+COLP=0", NULL);

    /*  HEX character set */
    at_send_command("AT+CSCS=\"HEX\"", NULL);

    /*  USSD unsolicited */
    at_send_command("AT+CUSD=1", NULL);

    /*  Enable +CGEV GPRS event notifications, but don't buffer */
    at_send_command("AT+CGEREP=1,0", NULL);

    /*  SMS PDU mode */
    at_send_command("AT+CMGF=0", NULL);

#ifdef USE_TI_COMMANDS

    at_send_command("AT%CPI=3", NULL);

    /*  TI specific -- notifications when SMS is ready (currently ignored) */
    at_send_command("AT%CSTAT=1", NULL);

#endif /* USE_TI_COMMANDS */


    /* assume radio is off on error */
    if (isRadioOn() > 0) {
        setRadioState (RADIO_STATE_ON);
    }
}

static void waitForClose()
{
    pthread_mutex_lock(&s_state_mutex);

    while (s_closed == 0) {
        pthread_cond_wait(&s_state_cond, &s_state_mutex);
    }

    pthread_mutex_unlock(&s_state_mutex);
}

static void sendUnsolImsNetworkStateChanged()
{
#if 0 // to be used when unsol is changed to return data.
    int reply[2];
    reply[0] = s_ims_registered;
    reply[1] = s_ims_services;
    reply[1] = s_ims_format;
#endif
    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_IMS_NETWORK_STATE_CHANGED,
            NULL, 0);
}

/**
* Convert radio technology between Android telephony and Huawei EM770W spec.
*/
static int convertRILRadioTechnology(int subsys_mode, int modem_type)
{
	int ret;
	if (HUAWEI_MODEM == modem_type){
		switch (subsys_mode){
			case SUB_SYSMODE_NO_SERVICE:
				ret = RADIO_TECH_UNKNOWN;
			break;
			case SUB_SYSMODE_GSM:
				ret = RADIO_TECH_GSM;
			break;
			case SUB_SYSMODE_GPRS:
				ret = RADIO_TECH_GPRS;
			break;
			case SUB_SYSMODE_EDGE:
				ret = RADIO_TECH_EDGE;
			break;
			case SUB_SYSMODE_WCDMA:
				ret = RADIO_TECH_UMTS;
			break;
			case SUB_SYSMODE_HSDPA:
				ret = RADIO_TECH_HSDPA;
			break;
			case SUB_SYSMODE_HSUPA:
				ret = RADIO_TECH_HSUPA;
			break;
			case SUB_SYSMODE_HSUPA_HSDPA:
				ret = RADIO_TECH_HSPA;
			break;
			default:
				ret = RADIO_TECH_UNKNOWN;
		}
	}else if (AMAZON_MODEM == modem_type){
		/* ToDo: convert radio technology for Amazon1*/
		ret = RADIO_TECH_UNKNOWN;
	}else{
		ret = subsys_mode;
	}
	return ret;
}
/**
 * Called by atchannel when an unsolicited line appears
 * This is called on atchannel's reader thread. AT commands may
 * not be issued here
 */
static void onUnsolicited (const char *s, const char *sms_pdu)
{
    char *line = NULL, *p;
    int err;

    /* Ignore unsolicited responses until we're initialized.
     * This is OK because the RIL library will poll for initial state
     */
    if (sState == RADIO_STATE_UNAVAILABLE) {
        return;
    }

    if (strStartsWith(s, "%CTZV:")) {
        /* TI specific -- NITZ time */
        char *response;

        line = p = strdup(s);
        at_tok_start(&p);

        err = at_tok_nextstr(&p, &response);

        free(line);
        if (err != 0) {
            RLOGE("invalid NITZ line %s\n", s);
        } else {
            RIL_onUnsolicitedResponse (
                RIL_UNSOL_NITZ_TIME_RECEIVED,
                response, strlen(response));
        }
    } else if (strStartsWith(s,"+CRING:")
                || strStartsWith(s,"RING")
                || strStartsWith(s,"NO CARRIER")
                || strStartsWith(s,"+CCWA")
    ) {
        RIL_onUnsolicitedResponse (
            RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED,
            NULL, 0);
#ifdef WORKAROUND_FAKE_CGEV
        RIL_requestTimedCallback (onDataCallListChanged, NULL, NULL); //TODO use new function
#endif /* WORKAROUND_FAKE_CGEV */
    } else if (strStartsWith(s,"+CREG:")
                || strStartsWith(s,"+CGREG:")
    ) {
        RIL_onUnsolicitedResponse (
            RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
            NULL, 0);
#ifdef WORKAROUND_FAKE_CGEV
        RIL_requestTimedCallback (onDataCallListChanged, NULL, NULL);
#endif /* WORKAROUND_FAKE_CGEV */
    } else if (strStartsWith(s, "+CMT:")) {
        RIL_onUnsolicitedResponse (
            RIL_UNSOL_RESPONSE_NEW_SMS,
            sms_pdu, strlen(sms_pdu));
    } else if (strStartsWith(s, "+CMTI:")) {
        char *storage = NULL;
        //read the sms in ME
        line = strdup(s);
        at_tok_start(&line);

        //+CMTI: "ME",1
        err = at_tok_nextstr(&line, &storage);
        if (err < 0) return;

        err = at_tok_nextint(&line, &smsStorageIndex);
        if (err < 0) return;

        memset(smsReadStorage, 0, 3);
        //"SM","ME","SR"
        strncpy(smsReadStorage, storage, 2);

        RIL_requestTimedCallback (onNewSmsArrived, &smsStorageIndex, NULL);
        free(line);
    } else if (strStartsWith(s, "+CDS:")) {
        RIL_onUnsolicitedResponse (
            RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT,
            sms_pdu, strlen(sms_pdu));
    } else if (strStartsWith(s, "+CDSI:")) {
        char *storage = NULL;
        //read the sms in ME
        line = strdup(s);
        at_tok_start(&line);
        //+CDSI: "SM",8
        err = at_tok_nextstr(&line, &storage);
        if (err < 0) return;
        
        err = at_tok_nextint(&line, &smsStorageIndex);
        if (err < 0) return;

        memset(smsReadStorage, 0, 3);
        //"SM","ME","SR"
        strncpy(smsReadStorage, storage, 2);
        RIL_requestTimedCallback (onNewSmsReportArrived, &smsStorageIndex, NULL);
        free(line);
    } else if (strStartsWith(s, "+CGEV:")) {
        /* Really, we can ignore NW CLASS and ME CLASS events here,
         * but right now we don't since extranous
         * RIL_UNSOL_DATA_CALL_LIST_CHANGED calls are tolerated
         */
        /* can't issue AT commands here -- call on main thread */
        RIL_requestTimedCallback (onDataCallListChanged, NULL, NULL);
#ifdef WORKAROUND_FAKE_CGEV
    } else if (strStartsWith(s, "+CME ERROR: 150")) {
        RIL_requestTimedCallback (onDataCallListChanged, NULL, NULL);
#endif /* WORKAROUND_FAKE_CGEV */
    } else if (strStartsWith(s, "+CTEC: ")) {
        int tech, mask;
        switch (parse_technology_response(s, &tech, NULL))
        {
            case -1: // no argument could be parsed.
                RLOGE("invalid CTEC line %s\n", s);
                break;
            case 1: // current mode correctly parsed
            case 0: // preferred mode correctly parsed
                mask = 1 << tech;
                if (mask != MDM_GSM && mask != MDM_CDMA &&
                     mask != MDM_WCDMA && mask != MDM_LTE) {
                    RLOGE("Unknown technology %d\n", tech);
                } else {
                    setRadioTechnology(sMdmInfo, tech);
                }
                break;
        }
    } else if (strStartsWith(s, "+CCSS: ")) {
        int source = 0;
        line = p = strdup(s);
        if (!line) {
            RLOGE("+CCSS: Unable to allocate memory");
            return;
        }
        if (at_tok_start(&p) < 0) {
            free(line);
            return;
        }
        if (at_tok_nextint(&p, &source) < 0) {
            RLOGE("invalid +CCSS response: %s", line);
            free(line);
            return;
        }
        SSOURCE(sMdmInfo) = source;
        RIL_onUnsolicitedResponse(RIL_UNSOL_CDMA_SUBSCRIPTION_SOURCE_CHANGED,
                                  &source, sizeof(source));
    } else if (strStartsWith(s, "+WSOS: ")) {
        char state = 0;
        int unsol;
        line = p = strdup(s);
        if (!line) {
            RLOGE("+WSOS: Unable to allocate memory");
            return;
        }
        if (at_tok_start(&p) < 0) {
            free(line);
            return;
        }
        if (at_tok_nextbool(&p, &state) < 0) {
            RLOGE("invalid +WSOS response: %s", line);
            free(line);
            return;
        }
        free(line);

        unsol = state ?
                RIL_UNSOL_ENTER_EMERGENCY_CALLBACK_MODE : RIL_UNSOL_EXIT_EMERGENCY_CALLBACK_MODE;

        RIL_onUnsolicitedResponse(unsol, NULL, 0);

    } else if (strStartsWith(s, "+WPRL: ")) {
        int version = -1;
        line = p = strdup(s);
        if (!line) {
            RLOGE("+WPRL: Unable to allocate memory");
            return;
        }
        if (at_tok_start(&p) < 0) {
            RLOGE("invalid +WPRL response: %s", s);
            free(line);
            return;
        }
        if (at_tok_nextint(&p, &version) < 0) {
            RLOGE("invalid +WPRL response: %s", s);
            free(line);
            return;
        }
        free(line);
        RIL_onUnsolicitedResponse(RIL_UNSOL_CDMA_PRL_CHANGED, &version, sizeof(version));
    } else if (strStartsWith(s, "+CFUN: 0")) {
        setRadioState(RADIO_STATE_OFF);
    } else if (strStartsWith(s, "^MODE")){
		/*
		 * Huawei E770W reports it's subsys_mode as networkType.
		 */
		char *response;

		line = strdup(s);
		at_tok_start(&line);

		err = at_tok_nextstr(&line, &response);

		if (err != 0) {
			LOGE("invalid MODE line %s\n", s);
		} else {
			response[0] = atoi(line);
			sNetworkType = convertRILRadioTechnology(response[0], runtime_3g_port_type());
			if (sLastNetworkType != sNetworkType){
				sLastNetworkType = sNetworkType;
				RIL_onUnsolicitedResponse (
					RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
					NULL, 0);
			}
		}
	}
}


/* Called on command or reader thread */
static void onATReaderClosed()
{
    RLOGI("AT channel closed\n");
    at_close();
    s_closed = 1;

    setRadioState (RADIO_STATE_UNAVAILABLE);
}

/* Called on command thread */
static void onATTimeout()
{
    RLOGI("AT channel timeout; closing\n");
    at_close();

    s_closed = 1;

    /* FIXME cause a radio reset here */

    setRadioState (RADIO_STATE_UNAVAILABLE);
}

/* Called to pass hardware configuration information to telephony
 * framework.
 */
static void setHardwareConfiguration(int num, RIL_HardwareConfig *cfg)
{
   RIL_onUnsolicitedResponse(RIL_UNSOL_HARDWARE_CONFIG_CHANGED, cfg, num*sizeof(*cfg));
}

static void usage(char *s)
{
#ifdef RIL_SHLIB
#ifdef HAVE_DATA_DEVICE
    fprintf(stderr, "reference-ril requires: -p <tcp port> or -d /dev/tty_device -u /dev/data_device\n");
#else
    fprintf(stderr, "reference-ril requires: -p <tcp port> or -d /dev/tty_device\n");
#endif
#else
    fprintf(stderr, "usage: %s [-p <tcp port>] [-d /dev/tty_device]\n", s);
    exit(-1);
#endif
}

static void *
mainLoop(void *param __unused)
{
    int fd;
    int ret;
#ifdef HAVE_DATA_DEVICE
    struct stat dummy;
#endif
    struct termios new_termios, old_termios;
    char delay_init[PROPERTY_VALUE_MAX];
    int delay;

    AT_DUMP("== ", "entering mainLoop()", -1 );
    at_set_on_reader_closed(onATReaderClosed);
    at_set_on_timeout(onATTimeout);

    for (;;) {
        fd = -1;
        while  (fd < 0) {
#ifdef HAVE_DATA_DEVICE
	    /* If the modem have two channel, e.g. UART for AT command and USB for data,
	       we check the data channel firstly (e.g. /dev/ttyUSB0) so that after AT handshake
	       we can make sure the data channel is ready.
	    */
	    if(stat(s_data_device_path, &dummy)) {
                LOGE ("opening data channel device - %s. retrying...", s_data_device_path);
                sleep(10);
		continue;
	    }
#endif
            if (s_port > 0) {
                fd = socket_loopback_client(s_port, SOCK_STREAM);
            } else if (s_device_socket) {
                if (!strcmp(s_device_path, "/dev/socket/qemud")) {
                    /* Before trying to connect to /dev/socket/qemud (which is
                     * now another "legacy" way of communicating with the
                     * emulator), we will try to connecto to gsm service via
                     * qemu pipe. */
                    fd = qemu_pipe_open("qemud:gsm");
                    if (fd < 0) {
                        /* Qemu-specific control socket */
                        fd = socket_local_client( "qemud",
                                                  ANDROID_SOCKET_NAMESPACE_RESERVED,
                                                  SOCK_STREAM );
                        if (fd >= 0 ) {
                            char  answer[2];

                            if ( write(fd, "gsm", 3) != 3 ||
                                 read(fd, answer, 2) != 2 ||
                                 memcmp(answer, "OK", 2) != 0)
                            {
                                close(fd);
                                fd = -1;
                            }
                       }
                    }
                }
                else
                    fd = socket_local_client( s_device_path,
                                            ANDROID_SOCKET_NAMESPACE_FILESYSTEM,
                                            SOCK_STREAM );
            } else if (s_device_path != NULL) {
                fd = open (s_device_path, O_RDWR);
		if (fd < 0)
			LOGE("Error On open:%s:%s", s_device_path, strerror(errno));
                if ( fd >= 0 && !memcmp( s_device_path, "/dev/ttyS", 9 ) ) {
                    /* disable echo on serial ports */
                    struct termios  ios;
                    tcgetattr( fd, &ios );
                    ios.c_lflag = 0;  /* disable ECHO, ICANON, etc... */
                    tcsetattr( fd, TCSANOW, &ios );
                }
            }

            if (fd < 0) {
                LOGE ("opening AT interface. retrying...");
                sleep(10);
                /* never returns */
            }
        }
	if (current_modem_type != UNKNOWN_MODEM) {
		/* Set UART parameters (e.g. Buad rate) for connecting with HUAWEI modem */
		tcgetattr(fd, &old_termios);
		new_termios = old_termios;
		new_termios.c_lflag &= ~(ICANON | ECHO | ISIG);
		new_termios.c_cflag |= (CREAD | CLOCAL);
		new_termios.c_cflag &= ~(CSTOPB | PARENB | CRTSCTS);
		new_termios.c_cflag &= ~(CBAUD | CSIZE) ;
		new_termios.c_cflag |= (B115200 | CS8);
		ret = tcsetattr(fd, TCSANOW, &new_termios);
		if (ret < 0) {
			LOGE ("Fail to set UART parameters. tcsetattr return %d \n", ret);
			return 0;
		}
	}
        s_closed = 0;
        ret = at_open(fd, onUnsolicited);

        if (ret < 0) {
            RLOGE ("AT error %d on at_open\n", ret);
            return 0;
        }

        //RIL framework can handle the modem not response on AT+COPS in time
        //So remove the delay here
        TIMEVAL_DELAYINIT.tv_sec = 0;

        RIL_requestTimedCallback(initializeCallback, NULL, &TIMEVAL_DELAYINIT);

        // Give initializeCallback a chance to dispatched, since
        // we don't presently have a cancellation mechanism
        sleep(1);

        waitForClose();
        RLOGI("Re-opening after close");
    }
}

#ifdef RIL_SHLIB

pthread_t s_tid_mainloop;

const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int argc, char **argv)
{
    int ret;
    int fd = -1;
    int opt;
    pthread_attr_t attr;

    s_rilenv = env;
#ifdef HAVE_DATA_DEVICE
    while ( -1 != (opt = getopt(argc, argv, "p:d:u:s:c:"))) {
#else
    while ( -1 != (opt = getopt(argc, argv, "p:d:s:c:"))) {
#endif
        switch (opt) {
            case 'p':
                s_port = atoi(optarg);
                if (s_port == 0) {
                    usage(argv[0]);
                    return NULL;
                }
                RLOGI("Opening loopback port %d\n", s_port);
            break;

            case 'd':
                s_device_path = optarg;
                RLOGI("Opening tty device %s\n", s_device_path);
            break;

#ifdef HAVE_DATA_DEVICE
	   /* e.g. /dev/ttyUSB0 for seperate data channel */
	    case 'u':
		s_data_device_path = optarg;
	    break;
#endif

            case 's':
                s_device_path   = optarg;
                s_device_socket = 1;
                RLOGI("Opening socket %s\n", s_device_path);
            break;

            case 'c':
                RLOGI("Client id received %s\n", optarg);
            break;

            default:
                usage(argv[0]);
                return NULL;
        }
    }

    if (s_device_path == NULL)
	    s_device_path = runtime_3g_port_device();
    if (s_port < 0 && s_device_path == NULL) {
        usage(argv[0]);
        return NULL;
    }

    sMdmInfo = calloc(1, sizeof(ModemInfo));
    if (!sMdmInfo) {
        RLOGE("Unable to alloc memory for ModemInfo");
        return NULL;
    }

#ifdef HAVE_DATA_DEVICE
    if (s_data_device_path == NULL)
	    s_data_device_path = runtime_3g_port_data();
    if (s_data_device_path == NULL) {
        usage(argv[0]);
        return NULL;
    }
#endif

    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&s_tid_mainloop, &attr, mainLoop, NULL);

    return &s_callbacks;
}
#else /* RIL_SHLIB */
int main (int argc, char **argv)
{
    int ret;
    int fd = -1;
    int opt;

    while ( -1 != (opt = getopt(argc, argv, "p:d:"))) {
        switch (opt) {
            case 'p':
                s_port = atoi(optarg);
                if (s_port == 0) {
                    usage(argv[0]);
                }
                RLOGI("Opening loopback port %d\n", s_port);
            break;

            case 'd':
                s_device_path = optarg;
                RLOGI("Opening tty device %s\n", s_device_path);
            break;

            case 's':
                s_device_path   = optarg;
                s_device_socket = 1;
                RLOGI("Opening socket %s\n", s_device_path);
            break;

            default:
                usage(argv[0]);
        }
    }

    if (s_port < 0 && s_device_path == NULL) {
        usage(argv[0]);
    }

    RIL_register(&s_callbacks);

    mainLoop(NULL);

    return 0;
}

#endif /* RIL_SHLIB */
