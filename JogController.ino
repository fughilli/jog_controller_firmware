/*
 *  This sketch sends data via HTTP GET requests to data.sparkfun.com service.
 *
 *  You need to get streamId and privateKey at data.sparkfun.com and paste them
 *  below. Or just customize this script to talk to other HTTP servers.
 *
 */

#include "extmain.h"

void setup() { jog_controller::ExtMain(); }
void loop() { jog_controller::ExtLoop(); }
