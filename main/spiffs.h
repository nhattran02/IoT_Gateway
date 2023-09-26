/**
********************************************************************************
* @file         spiffs.h
* @brief        Header file for spiffs.c
* @since        Created on 2023-09-26
********************************************************************************
*/

#ifndef SPIFFS_H_
#define SPIFFS_H_

#include "esp_spiffs.h"


esp_err_t mountSPIFFS(char * path, char * label, int max_files);


#endif /* SPIFFS_H_ */


