/*
 * Copyright (C) 2012 The Android Open Source Project
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
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>

#include "ui.h"
#include "cutils/properties.h"
#include "install.h"
#include "common.h"
#include "adb_install.h"
#include "minadbd/fuse_adb_provider.h"
#include "fuse_sideload.h"

static RecoveryUI* ui = NULL;

static void
stop_adbd() {
    /* For 4.1 kernel:
     * "start adbd" will trigger sys.usb.ffs.ready=1 to bind ffs driver;
     * "stop adbd" will unbind ffs driver, need below process */
    property_set("ctl.stop", "adbd");
    property_set("sys.usb.ffs.ready", "0");
}

static void
maybe_restart_adbd() {
    if (is_ro_debuggable()) {
        ui->Print("Restarting adbd...\n");
        property_set("ctl.start", "adbd");
    }
}

// How long (in seconds) we wait for the host to start sending us a
// package, before timing out.
#define ADB_INSTALL_TIMEOUT 300

int
apply_from_adb(RecoveryUI* ui_, bool* wipe_cache, const char* install_file) {
    modified_flash = true;

    ui = ui_;

    stop_adbd();

    ui->Print("\n\nNow send the package you want to apply\n"
              "to the device with \"adb sideload <filename>\"...\n");

    pid_t child;
    if ((child = fork()) == 0) {
        execl("/sbin/recovery", "recovery", "--adbd", NULL);
        _exit(-1);
    }

    // FUSE_SIDELOAD_HOST_PATHNAME will start to exist once the host
    // connects and starts serving a package.  Poll for its
    // appearance.  (Note that inotify doesn't work with FUSE.)
    int result;
    int status;
    bool waited = false;
    struct stat st;
    for (int i = 0; i < ADB_INSTALL_TIMEOUT; ++i) {
        if (waitpid(child, &status, WNOHANG) != 0) {
            result = INSTALL_ERROR;
            waited = true;
            break;
        }

        if (stat(FUSE_SIDELOAD_HOST_PATHNAME, &st) != 0) {
            if (errno == ENOENT && i < ADB_INSTALL_TIMEOUT-1) {
                sleep(1);
                continue;
            } else {
                ui->Print("\nTimed out waiting for package.\n\n");
                result = INSTALL_ERROR;
                kill(child, SIGKILL);
                break;
            }
        }
        result = install_package(FUSE_SIDELOAD_HOST_PATHNAME, wipe_cache, install_file, false);
        break;
    }

    if (!waited) {
        // Calling stat() on this magic filename signals the minadbd
        // subprocess to shut down.
        stat(FUSE_SIDELOAD_HOST_EXIT_PATHNAME, &st);

        // TODO(dougz): there should be a way to cancel waiting for a
        // package (by pushing some button combo on the device).  For now
        // you just have to 'adb sideload' a file that's not a valid
        // package, like "/dev/null".
        waitpid(child, &status, 0);
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        if (WEXITSTATUS(status) == 3) {
            ui->Print("\nYou need adb 1.0.32 or newer to sideload\nto this device.\n\n");
        } else if (!WIFSIGNALED(status)) {
            ui->Print("\n(adbd status %d)\n", WEXITSTATUS(status));
        }
    }

    maybe_restart_adbd();

    return result;
}
