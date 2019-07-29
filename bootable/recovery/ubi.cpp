/*
 * Copyright (C) 2014-2015 Freescale Semiconductor, Inc.
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

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cutils/log.h>
#include<unistd.h>
#include <errno.h>


static int exec_cmd(const char* path, char* const argv[]) {
    int status;
    pid_t child;
    int ret;
    if ((child = vfork()) == 0) {
        ret = execv(path, argv);
	if (ret < 0)
		printf("exec_cmd errno is %d\n",errno);
        _exit(0);
    }
    waitpid(child, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        printf("%s failed with status %d\n", path, WEXITSTATUS(status));
    }
    return WEXITSTATUS(status);
}

extern "C" int ubiVolumeFormat(char *location)
{
    int ret = -1;
    const char *ubiupdatevol = "/sbin/ubiupdatevol";
    const char* args[] = {"ubiupdatevol", location, "-t", NULL};
    printf("Formatting %s\n",location);

    ret = exec_cmd(ubiupdatevol, (char* const*)args);
    if (ret != 0) {
	printf("format_volume %s failed\n",location);
	return -1;
    }	
  done:
    return ret;
}
