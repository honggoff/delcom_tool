#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "hidapi.h"

/**
 * Control program for a delcom USB lamp with buzzer.
 * Data sheet is included in the doc/ directory.
 */


// transmit message format, see page 14 of datasheet
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

class LampControl {
private:
    hid_device* device;

    uint8_t mColors[3];
    int mOnTime;
    int mOffTime;
    int mFrequencyIndex;

    const static unsigned short VENDOR_ID  = 0x0fc5;
    const static unsigned short PRODUCT_ID = 0xb080;

    // Commands. See page 15 of data sheet
    const static uint8_t MAJOR_COMMAND_8_BYTE = 101;
    const static uint8_t MAJOR_COMMAND_16_BYTE = 102;

    const static uint8_t MINOR_COMMAND_PORT_1 = 2;
    const static uint8_t MINOR_COMMAND_PWM = 34;
    const static uint8_t MINOR_COMMAND_BUZZER = 70;
    const static uint8_t MINOR_COMMAND_PULSE = 76;

    void resetState() {
        mColors[0] = mColors[1] = mColors[2] = 0;
        mOnTime = mOffTime = 200;
        mFrequencyIndex = 0;
    }

    void initDevice() {
        if (device == NULL) {
            device = hid_open(VENDOR_ID, PRODUCT_ID, NULL);
        }
        if (device == NULL) {
            fprintf(stderr, "hid_open failed\n");
            exit(-1);
        }
    }

    int sendMessage(hid_message &message) {
        initDevice();
        message.major_cmd = MAJOR_COMMAND_8_BYTE;
        int ret = hid_send_feature_report(device, (unsigned char *)&message, sizeof(message));
        if (ret < 0) {
            fprintf(stderr, "hid_send_feature_report failed(): %d\n", ret);
            hid_close(device);
            exit(-1);
        }
    }

    int sendMessage(extended_hid_message &message) {
        initDevice();
        message.message.major_cmd = MAJOR_COMMAND_16_BYTE;
        int ret = hid_send_feature_report(device, (unsigned char *)&message, sizeof(message));
        if (ret < 0) {
            fprintf(stderr, "hid_send_feature_report failed(): %d\n", ret);
            hid_close(device);
            exit(-1);
        }
    }

    void enableBuzzer(uint8_t frequencyIndex, int duration = 100, bool wait = false) {
        extended_hid_message message = {
            .message = {
                .major_cmd = 0,
                .minor_cmd = MINOR_COMMAND_BUZZER,
                // lsb is 1 to enable buzzer, 0 to disable
                .data_lsb = frequencyIndex != 0,
                // msb is index in frequency table (see page 10 of data sheet)
                .data_msb = frequencyIndex,
            },
            .data_ext = {1, duration / 50, 0}
        };
        sendMessage(message);
        if (wait) {
            usleep(1000 * duration);
        }
    }

    void turnLampOff() {
        hid_message message = {
            .major_cmd = 0,
            .minor_cmd = MINOR_COMMAND_PORT_1,
            .data_lsb = 0x07,
        };
        sendMessage(message);
    }

    void setColor() {
        // This is the value for the write port command. Enabled ports are set to 0.
        uint8_t enable_mask = 0x07;
        // colors are wired as follows:
        // port 0: green
        // port 1: red
        // port 2: blue
        uint8_t device_colors[] = { mColors[1], mColors[0], mColors[2] };
        hid_message message = {
            .major_cmd = 0,
            .minor_cmd = MINOR_COMMAND_PWM,
        };
        for (int i = 0; i < 3; i++) {
            if (device_colors[i]) {
                // lsb is the port index
                message.data_lsb = i;
                // msb is the duty cycle in percent
                message.data_msb = (uint8_t)(100 * device_colors[i] / 255.0);
                sendMessage(message);
                enable_mask &= ~(1 << i);
            }
        }
        sendMessage(message);
        message.minor_cmd = MINOR_COMMAND_PORT_1;
        message.data_lsb = enable_mask;
        sendMessage(message);
    }

    // plays a little fanfare
    void tada() {
        const int duration = 120;
        enableBuzzer(15, duration);
        usleep(duration * 1000);
        enableBuzzer(12, duration);
        usleep(duration * 1000);
        enableBuzzer(10, duration);
        usleep(duration * 1000);
        enableBuzzer(7, duration);
        usleep(2 * duration * 1000);
        enableBuzzer(15, duration);
        usleep(duration * 1000);
        enableBuzzer(7, 3 * duration);
        usleep(3 * duration * 1000);
    }

    void showHelp(char* name) {
        printf(
            "Usage examlpe: %s --on 1000 --blue --new --on 100 --color dead00 --buzzer 5 ]\n",
            name
            );
    }

    void parseColors(const char* string) {
        if (strlen(string) != 6) {
            fprintf(stderr, "Illegal format string: %s\n", string);
            exit(-1);
        }
        for (int i = 0; i < 3; i++) {
            unsigned int c;
            sscanf(string + 2 * i, "%02x", &c);
            mColors[i] = (uint8_t)c;
        }
    }

    void play() {
        enableBuzzer(mFrequencyIndex, mOnTime);
        setColor();
        usleep(1000 * mOnTime);
        turnLampOff();
        usleep(1000 * mOffTime);
    }
public:
    LampControl() {
        resetState();
    }

    int run(int argc, char* argv[]) {
        const struct option long_options[] = {
            // These options set a flag.
            {"blue",  no_argument,       NULL, 'b'},
            {"green", no_argument,       NULL, 'g'},
            {"red",   no_argument,       NULL, 'r'},
            {"help",  no_argument,       NULL, 'h'},
            {"test",  no_argument,       NULL, 't'},
            {"new",   no_argument,       NULL, 'n'},
            {"off",   required_argument, NULL, 'f'},
            {"on",    required_argument, NULL, 'o'},
            {"repeat",required_argument, NULL, 'r'},
            {"buzzer",required_argument, NULL, 'z'},
            {"color", required_argument, NULL, 'c'},
        };
        bool done = false;
        int currentOption, lastOption;
        // First, grab the user's options.
        while (!done) {
            int optionIndex = 0;
            lastOption = currentOption;
            currentOption = getopt_long(argc, argv, "bgrthnf:o:r:z:c:", long_options, &optionIndex);

            switch (currentOption) {
                case -1:
                    done = true;
                    break;
                case 'h':
                    showHelp(argv[0]);
                    exit(1);
                case 'r':
                    parseColors("ff0000");
                    break;
                case 'g':
                    parseColors("00ff00");
                    break;
                case 'b':
                    parseColors("0000ff");
                    break;
                case 'c':
                    parseColors(optarg);
                    break;
                case 'o':
                    mOnTime = atoi(optarg);
                    break;
                case 'f':
                    mOffTime = atoi(optarg);
                    break;
                case 'z':
                    mFrequencyIndex = atoi(optarg);
                    break;
                case 'n':
                    play();
                    break;
                case 't':
                    tada();
                    break;

                default:
                    printf("Unknown option: %c", currentOption);
            }
        }
        if (lastOption != 'n') {
            play();
        }
        hid_close(device);
    }
};

int main(int argc, char* argv[]) {
    LampControl lamp;
    return lamp.run(argc, argv);
}