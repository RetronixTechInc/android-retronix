/* //device/system/reference-ril/atchannel.h
**
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

/* Copyright (C) 2013 Freescale Semiconductor, Inc. */

#ifndef ATCHANNEL_H
#define ATCHANNEL_H 1

#ifdef __cplusplus
extern "C" {
#endif

/* define AT_DEBUG to send AT traffic to /tmp/radio-at.log" */
#define AT_DEBUG  0

#if AT_DEBUG
extern void  AT_DUMP(const char* prefix, const char*  buff, int  len);
#else
#define  AT_DUMP(prefix,buff,len)  do{}while(0)
#endif

#define AT_ERROR_GENERIC -1
#define AT_ERROR_COMMAND_PENDING -2
#define AT_ERROR_CHANNEL_CLOSED -3
#define AT_ERROR_TIMEOUT -4
#define AT_ERROR_INVALID_THREAD -5 /* AT commands may not be issued from
                                       reader thread (or unsolicited response
                                       callback */
#define AT_ERROR_INVALID_RESPONSE -6 /* eg an at_send_command_singleline that
                                        did not get back an intermediate
                                        response */


typedef enum {
    NO_RESULT,   /* no intermediate response expected */
    NUMERIC,     /* a single intermediate response starting with a 0-9 */
    SINGLELINE,  /* a single intermediate response starting with a prefix */
    MULTILINE    /* multiple line intermediate response
                    starting with a prefix */
} ATCommandType;

/** a singly-lined list of intermediate responses */
typedef struct ATLine  {
    struct ATLine *p_next;
    char *line;
} ATLine;

/** Free this with at_response_free() */
typedef struct {
    int success;              /* true if final response indicates
                                    success (eg "OK") */
    char *finalResponse;      /* eg OK, ERROR */
    ATLine  *p_intermediates; /* any intermediate responses */
} ATResponse;

/**
 * a user-provided unsolicited response handler function
 * this will be called from the reader thread, so do not block
 * "s" is the line, and "sms_pdu" is either NULL or the PDU response
 * for multi-line TS 27.005 SMS PDU responses (eg +CMT:)
 */
typedef void (*ATUnsolHandler)(const char *s, const char *sms_pdu);

int at_open(int fd, ATUnsolHandler h);
void at_close();

/* This callback is invoked on the command thread.
   You should reset or handshake here to avoid getting out of sync */
void at_set_on_timeout(void (*onTimeout)(void));
/* This callback is invoked on the reader thread (like ATUnsolHandler)
   when the input stream closes before you call at_close
   (not when you call at_close())
   You should still call at_close()
   It may also be invoked immediately from the current thread if the read
   channel is already closed */
void at_set_on_reader_closed(void (*onClose)(void));

int at_send_command_singleline (const char *command,
                                const char *responsePrefix,
                                 ATResponse **pp_outResponse);

int at_send_command_numeric (const char *command,
                                 ATResponse **pp_outResponse);

int at_send_command_multiline (const char *command,
                                const char *responsePrefix,
                                 ATResponse **pp_outResponse);


int at_handshake();

int at_send_command (const char *command, ATResponse **pp_outResponse);

int at_send_command_sms (const char *command, const char *pdu,
                            const char *responsePrefix,
                            ATResponse **pp_outResponse);

void at_response_free(ATResponse *p_response);

typedef enum {
    CME_ERROR_NON_CME = -1,
    CME_SUCCESS = 0,
	CME_NO_CONNECTION_TO_PHONE = 1,
	CME_PHONE_ADAPTOR_LINK_RESERVED = 2,
	CME_OPERATION_NOT_ALLOWED = 3,
	CME_OPERATION_NOT_SUPPORTED = 4,
	CME_PH_SIM_PIN_REQUIRED = 5,
	CME_PH_FSIM_PIN_REQUIRED = 6,
	CME_PH_FSIM_PUK_REQUIRED = 7,
	CME_SIM_NOT_INSERTED = 10,
	CME_SIM_PIN_REQUIRED = 11,
	CME_SIM_PUK_REQUIRED = 12,
	CME_SIM_FAILURE = 13,
	CME_SIM_BUSY = 14,
	CME_SIM_WRONG = 15,
	CME_INCORRECT_PASSWORD = 16,
	CME_SIM_PIN2_REQUIRED = 17,
	CME_SIM_PUK2_REQUIRED = 18,
	CME_MEMORY_FULL = 20,
	CME_INVALID_INDEX = 21,
	CME_NOT_FOUND = 22,
	CME_MEMORY_FAILURE = 23,
	CME_TEXT_STRING_TOO_LONG = 24,
	CME_INVALID_CHARACTERS_IN_TEXT_STRING = 25,
	CME_DIAL_STRING_TOO_LONG = 26,
	CME_INVALID_CHARACTERS_IN_DIAL_STRING = 27,
	CME_NO_NETWORK_SERVICE = 30,
	CME_NETWORK_TIMEOUT = 31,
	CME_NETWORK_NOT_ALLOWED_EMERGENCY_CALLS_ONLY = 32,
	CME_NETWORK_PERSONALIZATION_PIN_REQUIRED = 40,
	CME_NETWORK_PERSONALIZATION_PUK_REQUIRED = 41,
	CME_NETWORK_SUBSET_PERSONALIZATION_PIN_REQUIRED = 42,
	CME_NETWORK_SUBSET_PERSONALIZATION_PUK_REQUIRED = 43,
	CME_SERVICE_PROVIDER_PERSONALIZATION_PIN_REQUIRED = 44,
	CME_SERVICE_PROVIDER_PERSONALIZATION_PUK_REQUIRED = 45,
	CME_CORPORATE_PERSONALIZATION_PIN_REQUIRED = 46,
	CME_CORPORATE_PERSONALIZATION_PUK_REQUIRED = 47,
	CME_HIDDEN_KEY_REQUIRED = 48,
	CME_EAP_METHOD_NOT_SUPPORTED = 49,
	CME_INCORRECT_PARAMETERS = 50,
	CME_UNKNOWN = 100,
	CME_ILLEGAL_MS = 103,
	CME_ILLEGAL_ME = 106,
	CME_GPRS_SERVICES_NOT_ALLOWED = 107,
	CME_PLMN_NOT_ALLOWED = 111,
	CME_LOCATION_AREA_NOT_ALLOWED = 112,
	CME_ROAMING_NOT_ALLOWED_IN_THIS_LOCATION_AREA = 113,
	CME_SERVICE_OPTION_NOT_SUPPORTED = 132,
	CME_REQUESTED_SERVICE_OPTION_NOT_SUBSCRIBED = 133,
	CME_SERVICE_OPTION_TEMPORARILY_OUT_OF_ORDER = 134,
	CME_UNSPECIFIED_GPRS_ERROR = 148,
	CME_PDP_AUTHENTICATION_FAILURE = 149,
	CME_INVALID_MOBILE_CLASS = 150,
	CME_PH_SIMLOCK_PIN_REQUIRED = 200,
	CME_PRE_DIAL_CHECK_ERROR = 350,
} AT_CME_Error;

AT_CME_Error at_get_cme_error(const ATResponse *p_response);

#ifdef __cplusplus
}
#endif

#endif /*ATCHANNEL_H*/
