/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

static bool SetTTYParams(int fd, const char *params) {
    speed_t speed = B115200;
    if (params[0] == 'b' || params[0] == 'B')
        params = params + 1;
    if (*params) {
        int speed_number = atoi(params);
        switch (speed_number) {
        case 9600:   speed = B9600; break;
        case 19200:  speed = B19200; break;
        case 38400:  speed = B38400; break;
        case 57600:  speed = B57600; break;
        case 115200: speed = B115200; break;
        case 230400: speed = B230400; break;
        case 460800: speed = B460800; break;
        default:
            fprintf(stderr, "Invalid speed '%s'; valid speeds are "
                    "[9600, 19200, 38400, 57600, 115200, 230400, 460800]\n",
                    params);
            return false;
            break;
        }
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) < 0) {
        perror("tcgetattr() failed. Is this a tty ?");
        return false;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag |= (CLOCAL | CREAD);  // no modem controls
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;               // 8
    tty.c_cflag &= ~PARENB;           // N
    tty.c_cflag &= ~CSTOPB;           // 1
    tty.c_cflag &= ~CRTSCTS;          // No hardware flow-control

    // terminal magic. Non-canonical mode
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return false;
    }
    return true;
 }

int OpenMachineConnection(const char *descriptor) {
    if (descriptor == nullptr) return -1;
    const char *comma = strchrnul(descriptor, ',');
    const std::string path(descriptor, comma);
    int fd = open(path.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "Opening %s: %s\n", path.c_str(), strerror(errno));
        return -1;
    }
    if (!SetTTYParams(fd, *comma ? comma+1 : "")) {
        return -1;
    }
    return fd;
}
