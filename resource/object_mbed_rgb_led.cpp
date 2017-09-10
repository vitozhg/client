
/*******************************************************************************
 *
 * Copyright (c) 2013, 2014 Intel Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Simon Bernard
 *******************************************************************************/
#include "liblwm2m.h"

#ifdef ENABLE_RGB_LED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mbed.h"

#define LWM2M_LIGHT_OBJECT_ID   3311

// Resource Id's:
#define RES_COLOUR          5706
#define RES_ON_OFF          5850

typedef struct {
    // state of the light
    bool on;
    // the current color choose by user
    float rv;
    float gv;
    float bv;
} rgb_data_t;

PwmOut * rpw;
PwmOut * gpw;
PwmOut * bpw;

InterruptIn on_button(p13);
InterruptIn off_button(p16);
rgb_data_t * state;

void switchon() {
    printf("LIGHT ON");
    state->on = true;
    *rpw = state->rv;
    *gpw = state->gv;
    *bpw = state->bv;
}

void switchoff() {
    printf("LIGHT OFF");
    state->on = false;
    *rpw = 1.0f;
    *gpw = 1.0f;
    *bpw = 1.0f;
}

// change the led color
static int set_color(const char * htmlcolor, unsigned length, rgb_data_t * rgb) {
    // create color string ended with \0
    char color[length + 1];
    memset(color, 0, length + 1);
    strncpy(color, htmlcolor, length);

    // parse value to extract rgb (255,255,255)
    int r, g, b;
    if (sscanf(color, "#%2x%2x%2x", &r, &g, &b) != 3) {
        printf("failed to parse color string, sscanf failed for %s", color);
        return -1;
    }
    printf("Set Color to rgb(%d, %d, %d)", r, g, b);

    // convert it in  led power and update current color
    rgb->rv = -((r / 255.0f) - 1.0f);
    rgb->gv = -((g / 255.0f) - 1.0f);
    rgb->bv = -((b / 255.0f) - 1.0f);

    // apply color only if the light is ON.
    if (rgb->on) {
        *rpw = rgb->rv;
        *gpw = rgb->gv;
        *bpw = rgb->bv;
    }

    return 0;
}

// read the current color
static char * get_color(rgb_data_t * rgb) {
    // get current value and convert it in rgb(255,255,255)
    int r = (1.0f - rgb->rv) * 255;
    int g = (1.0f - rgb->gv) * 255;
    int b = (1.0f - rgb->bv) * 255;

    // build the corresponding string
    printf("Read Color rgb(%d,%d,%d)", r, g, b);
    char * color = (char *) malloc(8);
    if (0 > sprintf(color, "#%02X%02X%02X", r, g, b)) {
        printf("failed to create color string, sprintf failed");
        free(color);
        return NULL;
    }

    return color;
}

static uint8_t prv_set_value(lwm2m_data_t * dataP, rgb_data_t * devDataP) {
    // a simple switch structure is used to respond at the specified resource asked
    switch (dataP->id) {
    case RES_COLOUR: {
        char * color = get_color(devDataP);
        lwm2m_data_encode_int((int64_t)(*color),dataP);
        return COAP_205_CONTENT ;
    }
    case RES_ON_OFF: {
        bool on = (rpw->read() < 1.0f || gpw->read() < 1.0f || bpw->read() < 1.0f);
        lwm2m_data_encode_bool(on,dataP);
        return COAP_205_CONTENT ;
    }
    default:
        return COAP_404_NOT_FOUND ;
    }
}


void lwm2m_data_encode_string(const char * string, lwm2m_data_t * dataP);
void lwm2m_data_encode_nstring(const char * string, size_t length, lwm2m_data_t * dataP);
void lwm2m_data_encode_opaque(uint8_t * buffer, size_t length, lwm2m_data_t * dataP);
void lwm2m_data_encode_int(int64_t value, lwm2m_data_t * dataP);
int lwm2m_data_decode_int(const lwm2m_data_t * dataP, int64_t * valueP);
void lwm2m_data_encode_float(double value, lwm2m_data_t * dataP);
int lwm2m_data_decode_float(const lwm2m_data_t * dataP, double * valueP);
void lwm2m_data_encode_bool(bool value, lwm2m_data_t * dataP);
int lwm2m_data_decode_bool(const lwm2m_data_t * dataP, bool * valueP);
void lwm2m_data_encode_objlink(uint16_t objectId, uint16_t objectInstanceId, lwm2m_data_t * dataP);
void lwm2m_data_encode_instances(lwm2m_data_t * subDataP, size_t count, lwm2m_data_t * dataP);
void lwm2m_data_include(lwm2m_data_t * subDataP, size_t count, lwm2m_data_t * dataP);


static uint8_t prv_rgb_read(uint16_t instanceId, int * numDataP, lwm2m_data_t ** dataArrayP, lwm2m_object_t * objectP) {
    uint8_t result;
    int i;

    // this is a single instance object
    if (instanceId != 0) {
        return COAP_404_NOT_FOUND ;
    }

    // is the server asking for the full object ?
    if (*numDataP == 0) {

        uint16_t resList[] = {
        RES_COLOUR,
        RES_ON_OFF, };
        int nbRes = sizeof(resList) / sizeof(uint16_t);

        *dataArrayP = lwm2m_data_new(nbRes);
        if (*dataArrayP == NULL)
            return COAP_500_INTERNAL_SERVER_ERROR ;
        *numDataP = nbRes;
        for (i = 0; i < nbRes; i++) {
            (*dataArrayP)[i].id = resList[i];
        }
    }

    i = 0;
    do {
        result = prv_set_value((*dataArrayP) + i, (rgb_data_t*) (objectP->userData));
        i++;
    } while (i < *numDataP && result == COAP_205_CONTENT );

    return result;
}

static uint8_t prv_rgb_write(uint16_t instanceId, int numData, lwm2m_data_t * dataArray, lwm2m_object_t * objectP) {
    int i;
    uint8_t result;

    // this is a single instance object
    if (instanceId != 0) {
        return COAP_404_NOT_FOUND ;
    }

    i = 0;

    do {
        switch (dataArray[i].id) {
        case RES_COLOUR:
            if (-1 != set_color((char*) dataArray[i].value.asBuffer.buffer, dataArray[i].value.asBuffer.length, (rgb_data_t*) (objectP->userData))) {
                result = COAP_204_CHANGED;
            } else {
                result = COAP_500_INTERNAL_SERVER_ERROR;
            }
            break;
        case RES_ON_OFF:
            bool on;
            if (1 == lwm2m_data_decode_bool(dataArray + i, &on)) {
                if (on) {
                    switchon();
                } else {
                    switchoff();
                }
                result = COAP_204_CHANGED;
            } else {
                result = COAP_400_BAD_REQUEST;
            }
            break;
        default:
            result = COAP_405_METHOD_NOT_ALLOWED;
        }

        i++;
    } while (i < numData && result == COAP_204_CHANGED );

    return result;
}

static void prv_rgb_close(lwm2m_object_t * objectP) {
    if (NULL != objectP->userData) {
        lwm2m_free(objectP->userData);
        objectP->userData = NULL;
    }
    if (NULL != objectP->instanceList) {
        lwm2m_free(objectP->instanceList);
        objectP->instanceList = NULL;
    }
}

void display_rgb_object(lwm2m_object_t * object) {
#ifdef WITH_LOGS
    rgb_data_t * data = (rgb_data_t *) object->userData;
    fprintf(stdout, "  /%u: rgb object:\r\n", object->objID);
    if (NULL != data) {
        fprintf(stdout, "    previous x: %f, previous y: %f, previous z: %f\r\n", data->rv, data->gv, data->bv);
    }
#endif
}

lwm2m_object_t * get_object_rgb_led() {

    /*
     * The get_object_rgb_led function create the object itself and return a pointer to the structure that represent it.
     */
    lwm2m_object_t * rgbObj;

    rgbObj = (lwm2m_object_t *) lwm2m_malloc(sizeof(lwm2m_object_t));

    if (NULL != rgbObj) {
        memset(rgbObj, 0, sizeof(lwm2m_object_t));

        /*
         * It assigns his unique ID
         * The 3313 is the standard ID for the mandatory object "IPSO Light Control".
         */
        rgbObj->objID = LWM2M_LIGHT_OBJECT_ID;

        /*
         * there is only one instance of RGB LED on the Application Board for mbed NXP LPC1768.
         *
         */
        rgbObj->instanceList = (lwm2m_list_t *) lwm2m_malloc(sizeof(lwm2m_list_t));
        if (NULL != rgbObj->instanceList) {
            memset(rgbObj->instanceList, 0, sizeof(lwm2m_list_t));
        } else {
            lwm2m_free(rgbObj);
            return NULL;
        }

        /*
         * And the private function that will access the object.
         * Those function will be called when a read/write/execute query is made by the server. In fact the library don't need to
         * know the resources of the object, only the server does.
         */
        rgbObj->readFunc = prv_rgb_read;
        rgbObj->writeFunc = prv_rgb_write;
        rgbObj->executeFunc = NULL;
        state = (rgb_data_t *) lwm2m_malloc(sizeof(rgb_data_t));
        rgbObj->userData = state;

        /*
         * Also some user data can be stored in the object with a private structure containing the needed variables 
         */
        if (NULL != rgbObj->userData) {

            rpw = new PwmOut(p23);
            gpw = new PwmOut(p24);
            bpw = new PwmOut(p25);

            // light off to start
            ((rgb_data_t*) rgbObj->userData)->on = false;
            *rpw = 1.0f;
            *gpw = 1.0f;
            *bpw = 1.0f;

            // blue as default color
            ((rgb_data_t*) rgbObj->userData)->rv = 1.0f;
            ((rgb_data_t*) rgbObj->userData)->gv = 1.0f;
            ((rgb_data_t*) rgbObj->userData)->bv = 0.0f;
        } else {
            lwm2m_free(rgbObj);
            rgbObj = NULL;
        }

        on_button.rise(&switchon);
        off_button.rise(&switchoff);
    }

    return rgbObj;
}
#endif