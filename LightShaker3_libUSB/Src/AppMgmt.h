/*
 * AppMgmt.h
 *
 *  Created on: 06.10.2021
 *      Author: ChrLin00
 */

#ifndef APPMGMT_H_
#define APPMGMT_H_

#include "main.h"

uint16_t AppMgmt_Timebase;

void AppMgmt_AppInit();
void AppMgmt_AppExec();
void AppMgmt_CycleApps();
void AppMgmt_LoadApp();

#endif /* APPMGMT_H_ */
