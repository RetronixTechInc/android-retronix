/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/* Copyright (C) 2010-2011 Freescale Semiconductor,Inc. */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/route.h>

#include <android/log.h>
#include <cutils/properties.h>
#include <cutils/sockets.h>

#define SERVICE_PPPD_GPRS "pppd_gprs"

#define MAX_PPPD_MSG_LEN (32)
#define PPPD_MSG_PPP_DOWN "ppp-down"

#define SOCKET_NAME_RIL_PPP "rild-ppp"

void sendNotification()
{
    struct sockaddr_in local;
    struct sockaddr_in dest;
    char snd_buf[MAX_PPPD_MSG_LEN];
    int pppdSocket = socket(PF_UNIX, SOCK_STREAM, 0);
    if (pppdSocket < 0) {
        __android_log_print(ANDROID_LOG_INFO, "ip-down-ppp0", "cannot open socket \n");
	return;
    }

    if(socket_local_client_connect(pppdSocket,
                                SOCKET_NAME_RIL_PPP,
                                ANDROID_SOCKET_NAMESPACE_RESERVED,
                                SOCK_STREAM) < 0)
    {
        __android_log_print(ANDROID_LOG_INFO, "ip-down-ppp0", "cannot connect rild-ppp socket \n");
        close(pppdSocket);
    }

    memset(snd_buf,0,MAX_PPPD_MSG_LEN);  
    strcpy(snd_buf,PPPD_MSG_PPP_DOWN);  

    //send info to rild  
    __android_log_print(ANDROID_LOG_INFO, "ip-down-ppp0", "send  ppp-down to rild-ppp\n");
    write(pppdSocket,snd_buf,sizeof(snd_buf));  
    __android_log_print(ANDROID_LOG_INFO, "ip-down-ppp0", "send  ppp-down to rild-ppp end\n");

    close(pppdSocket);
}

int main(int argc, char **argv)
{
/* args is like this:
    argv[1] = ifname;
    argv[2] = devnam;
    argv[3] = strspeed;
    argv[4] = strlocal;
    argv[5] = strremote;
    argv[6] = ipparam;
*/
    if (argc < 6)
	return EINVAL;

    __android_log_print(ANDROID_LOG_INFO, "ip-down-ppp0", "ip-down-ppp0 script is launched with \n");
    __android_log_print(ANDROID_LOG_INFO, "ip-down-ppp0", "\tInterface\t:\t%s", 	argv[1]);
    __android_log_print(ANDROID_LOG_INFO, "ip-down-ppp0", "\tTTY device\t:\t%s",	argv[2]);
    __android_log_print(ANDROID_LOG_INFO, "ip-down-ppp0", "\tTTY speed\t:\t%s",	argv[3]);
    __android_log_print(ANDROID_LOG_INFO, "ip-down-ppp0", "\tLocal IP\t:\t%s",	argv[4]);
    __android_log_print(ANDROID_LOG_INFO, "ip-down-ppp0", "\tRemote IP\t:\t%s",	argv[5]);

    /* We don't need delete "default" entry in route table since it will be deleted 
       automatically by kernel when ppp0 interface is down
    */

    /* Reset all those properties like net.ppp0.xxx which are set by ip-up-ppp0 */
    {
	property_set("net.ppp0.local-ip", "");
	property_set("net.ppp0.remote-ip", "");
	property_set("net.ppp0.dns1", "");
	property_set("net.ppp0.dns2", "");

    }
    //Send a notification to rild
    sendNotification();
    return 0;
}

