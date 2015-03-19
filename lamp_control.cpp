#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <stdint.h>

#include "hidapi.h"

const static int GREEN = ~(1 << 0);
const static int RED   = ~(1 << 1);
const static int BLUE  = ~(1 << 2);

// commands
const static uint8_t MAJOR_COMMAND_8_BYTE = 101;
const static uint8_t MAJOR_COMMAND_16_BYTE = 102;

const static uint8_t MINOR_COMMAND_PORT_1 = 2;
const static uint8_t MINOR_COMMAND_BUZZER = 70;
const static uint8_t MINOR_COMMAND_PULSE = 76;


struct hid_message {
    uint8_t major_cmd;
    uint8_t minor_cmd;
    uint8_t data_lsb;
    uint8_t data_msb;
    uint8_t data_HID[4];
};

struct extended_hid_message {
    hid_message message;
    uint8_t data_ext[8];
};

static int send_message(hid_device* device, hid_message &message) {
    message.major_cmd = MAJOR_COMMAND_8_BYTE;
    int ret = hid_send_feature_report(device, (unsigned char *)&message, sizeof(message));
    if (ret < 0) {
        fprintf(stderr, "hid_send_feature_report failed(): %d\n", ret);
        hid_close(device);
        exit(-1);
    }
}

static int send_message(hid_device* device, extended_hid_message &message) {
    message.message.major_cmd = MAJOR_COMMAND_16_BYTE;
    int ret = hid_send_feature_report(device, (unsigned char *)&message, sizeof(message));
    if (ret < 0) {
        fprintf(stderr, "hid_send_feature_report failed(): %d\n", ret);
        hid_close(device);
        exit(-1);
    }
}
static void enable_buzzer(hid_device* device, int state) {
    extended_hid_message message = {
        .message = {
            .major_cmd = 0,
            .minor_cmd = MINOR_COMMAND_BUZZER,
            .data_lsb = state,
            .data_msb = 0x20,
        },
        .data_ext = {10, 2, 2}
    };
    send_message(device, message);
}

static void blink(hid_device* device, uint8_t color) {
    extended_hid_message message = {
        .message = {
            .major_cmd = 0,
            .minor_cmd = MINOR_COMMAND_PULSE,
            .data_lsb = 1 << 7 | 0x7f, // port 1, prescaler 1
            .data_msb = color,
        },
        .data_ext = {0xff, color, 0xff, color, 0xff, color, 0xff, color}
    };
    send_message(device, message);
}


int main(int argc, char* argv[]) {

    // First, grab the user's options.
    static int blue_flag;
    static int green_flag;
    static int help_flag;
    static int off_flag;
    static int red_flag;
    static int buzzer_flag;
    static int blink_flag;
    static int test_flag;

    int c;
    while (1) {
        static struct option long_options[] = {
            // These options set a flag.
            {"blue",  no_argument,  &blue_flag,  1},
            {"green", no_argument,  &green_flag, 1},
            {"help",  no_argument,  &help_flag,  1},
            {"off",   no_argument,  &off_flag,   1},
            {"red",   no_argument,  &red_flag,   1},
            {"buzzer",no_argument,  &buzzer_flag,1},
            {"blink", no_argument,  &blink_flag, 1},
            {"test",  no_argument,  &test_flag,  1},
        };

        int option_index = 0;
        c = getopt_long(argc, argv, "", long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {
            case 0:
                if (long_options[option_index].flag != 0)
                    break;
        }
    }

    // Print usage if the user didn't supply a valid option or asked for help.
    if (help_flag) {
        printf(
            "Usage: %s [ --blue | --red | --green | --off | --buzzer ]\n",
            argv[0]
        );

        exit(1);
    }

    unsigned short vendor_id  = 0x0fc5;
    unsigned short product_id = 0xb080;

    hid_device* device = hid_open(vendor_id, product_id, NULL);
    if (device == NULL) {
        fprintf(stderr, "hid_open failed\n");
        return 1;
    }

    // We are doing an 8 byte write feature to set the active LED.
    hid_message message = {
        .major_cmd = 0,
        .minor_cmd = MINOR_COMMAND_PORT_1,
        .data_lsb = 7
    };

    if (green_flag) {
        message.data_lsb &= GREEN;
    }
    if (red_flag) {
        message.data_lsb &= RED;
    }
    if (blue_flag) {
        message.data_lsb &= BLUE;
    }
    if (buzzer_flag) {
        enable_buzzer(device, 1);
    }
    if (off_flag) {
        message.data_lsb = 0;
        enable_buzzer(device, 0);
    }
    if (blink_flag) {
        char color = message.data_lsb;
        while(true) {
            send_message(device, message);
            message.data_lsb = 0;
            usleep(10000);
            send_message(device, message);
            message.data_lsb = color;
            usleep(20000);
        }
    }
    else {
        send_message(device, message);
    }
    hid_close(device);
}
