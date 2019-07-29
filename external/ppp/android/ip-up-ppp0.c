/*
 * Copyright (C) 2009 The Android Open Source Project
 * Copyright (C) 2013 Freescale Semiconductor, Inc.
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

static char propstr[65]; 	// hold "net.$LINKNAME"
static char propstr1[128];	// hold "net.$LINKNAME.xxx"

static inline struct in_addr *in_addr(struct sockaddr *sa)
{
    return &((struct sockaddr_in *)sa)->sin_addr;
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

    __android_log_print(ANDROID_LOG_INFO, "ip-up-ppp0", "ip-up-ppp0 script is launched with \n");
    __android_log_print(ANDROID_LOG_INFO, "ip-up-ppp0", "\tInterface\t:\t%s", 	argv[1]);
    __android_log_print(ANDROID_LOG_INFO, "ip-up-ppp0", "\tTTY device\t:\t%s",	argv[2]);
    __android_log_print(ANDROID_LOG_INFO, "ip-up-ppp0", "\tTTY speed\t:\t%s",	argv[3]);
    __android_log_print(ANDROID_LOG_INFO, "ip-up-ppp0", "\tLocal IP\t:\t%s",	argv[4]);
    __android_log_print(ANDROID_LOG_INFO, "ip-up-ppp0", "\tRemote IP\t:\t%s",	argv[5]);

    {
	// we need setup lots of property like net.$LINKNAME.[local-ip|remote-ip|dns1|dns2]
	strcpy(propstr, "net."); 
	strncat(propstr, getenv("LINKNAME"), 60);  // we never allow $LINKNAME exceed 60 chars
	strncat(propstr, ".", 1);
        __android_log_print(ANDROID_LOG_INFO, "ip-up-ppp0", "LINKNAME is %s", propstr);

	// set net.$LINKNAME.local-ip prop to $IPLOCAL
	strcpy(propstr1, propstr);
	strcat(propstr1, "local-ip");
	char *iplocal = getenv("IPLOCAL");
	property_set(propstr1, iplocal ? iplocal : "");
        __android_log_print(ANDROID_LOG_INFO, "ip-up-ppp0", "Setprop %s to %s", propstr1, iplocal ? iplocal : "");

	// set net.$LINKNAME.remote-ip prop to $IPREMOTE
	strcpy(propstr1, propstr);
	strcat(propstr1, "remote-ip");
	char *ipremote = getenv("IPREMOTE");
	property_set(propstr1, ipremote ? ipremote : "");
        __android_log_print(ANDROID_LOG_INFO, "ip-up-ppp0", "Setprop %s to %s", propstr1, ipremote ? ipremote : "");

	// set net.$LINKNAME.dns1 prop to $DNS1
	strcpy(propstr1, propstr);
	strcat(propstr1, "dns1");
	char *dns1 = getenv("DNS1");
	property_set(propstr1, dns1 ? dns1 : "");
    __android_log_print(ANDROID_LOG_INFO, "ip-up-ppp0", "Setprop %s to %s", propstr1, dns1 ? dns1 : "");

	// set net.$LINKNAME.dns2 prop to $DNS2
	strcpy(propstr1, propstr);
	strcat(propstr1, "dns2");
	char *dns2 = getenv("DNS2");
	property_set(propstr1, dns2 ? dns2 : "");
     __android_log_print(ANDROID_LOG_INFO, "ip-up-ppp0", "Setprop %s to %s", propstr1, dns2 ? dns2 : "");
    }
    __android_log_print(ANDROID_LOG_INFO, "ip-up-ppp0",
                        "All traffic is now redirected to %s", argv[1]);

    return 0;
}
