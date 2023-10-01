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


esp_err_t initSPIFFS(char * path, char * label, int max_files);
esp_err_t closeSPIFFS(const char* partition_label);

#endif /* SPIFFS_H_ */

