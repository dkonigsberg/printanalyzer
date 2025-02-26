/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_CP210X_H
#define USBH_CP210X_H

#include "usb_cdc.h"
#include "usbh_serial_class.h"

/* Requests */
#define CP210X_IFC_ENABLE      0x00
#define CP210X_SET_BAUDDIV     0x01
#define CP210X_GET_BAUDDIV     0x02
#define CP210X_SET_LINE_CTL    0x03 // Set parity, data bits, stop bits
#define CP210X_GET_LINE_CTL    0x04
#define CP210X_SET_BREAK       0x05
#define CP210X_IMM_CHAR        0x06
#define CP210X_SET_MHS         0x07 // Set DTR, RTS
#define CP210X_GET_MDMSTS      0x08 
#define CP210X_SET_XON         0x09
#define CP210X_SET_XOFF        0x0A
#define CP210X_SET_EVENTMASK   0x0B
#define CP210X_GET_EVENTMASK   0x0C
#define CP210X_SET_CHAR        0x0D
#define CP210X_GET_CHARS       0x0E
#define CP210X_GET_PROPS       0x0F
#define CP210X_GET_COMM_STATUS 0x10
#define CP210X_RESET           0x11
#define CP210X_PURGE           0x12
#define CP210X_SET_FLOW        0x13
#define CP210X_GET_FLOW        0x14
#define CP210X_EMBED_EVENTS    0x15
#define CP210X_GET_EVENTSTATE  0x16
#define CP210X_SET_CHARS       0x19
#define CP210X_GET_BAUDRATE    0x1D 
#define CP210X_SET_BAUDRATE    0x1E // Set baudrate
#define CP210X_VENDOR_SPECIFIC 0xFF 

/* VENDOR_SPECIFIC values */
#define CP210X_GET_PARTNUM     0x370B
#define CP210X_WRITE_LATCH     0x37E1
#define CP210X_READ_LATCH      0x00C2

struct usbh_serial_cp210x {
    struct usbh_serial_class base;

    USB_MEM_ALIGNX uint8_t control_buf[64];
    uint8_t partnum;
};

#endif /* USBH_CP210X_H */
