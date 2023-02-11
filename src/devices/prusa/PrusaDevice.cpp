#include "PrusaDevice.hh"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <regex>
#include <string>
#include <fstream>

// example for temperature readings: "T:21.6 /0.0 B:21.8 /0.0 T0:21.6 /0.0 @:0 B@:0 P:0.0 A:23.0"
#define __TEMP_REGEX "((T[[:digit:]]*)|(B[[:digit:]]*)|(B@)|@|P|A):[[:digit:]]+(\\.[[:digit:]]+)?( /[[:digit:]]+\\.[[:digit:]])?[[:space:]]*"
const std::regex __temp_regex(__TEMP_REGEX);
const std::regex __full_temp_regex("(" __TEMP_REGEX ")+");


// example for position readings: "X:0.00 Y:0.00 Z:0.15 E:0.00 Count X: 0.00 Y:0.00 Z:0.15 E:0.00"
//                                                                     ^ (space!)
#define __POS_REGEX "(([XYZE]:[[:space:]]?[[:digit:]]+(\\.[[:digit:]]+))|Count)[[:space:]]?"
const std::regex __pos_regex(__POS_REGEX);
const std::regex __full_pos_regex("(" __POS_REGEX "){9}");


// example for fan readings: "E0:0 RPM PRN1:0 RPM E0@:0 PRN1@:0"
#define __FAN_REGEX "((E)|(PRN))[[:digit:]]@?:[[:digit:]]+( RPM)?[[:space:]]?"
const std::regex __fan_regex(__FAN_REGEX);
const std::regex __full_fan_regex("(" __FAN_REGEX ")+");


// example: NORMAL MODE: Percent done: 0; print time remaining in mins: 24; Change in mins: -1
// example: SILENT MODE: Percent done: 1; print time remaining in mins: 24; Change in mins: -1
#define __PROGRESS_REGEX "^NORMAL MODE: Percent done: ([[:digit:]]+); print time remaining in mins: ([[:digit:]]+);.*$"
const std::regex __progress_regex(__PROGRESS_REGEX);



/*
 * PrusaDevice()
 */
PrusaDevice::PrusaDevice(const std::string &file, const std::string &name, const Config &conf)
    : m_device(file),
      m_name(name),
      m_ev(EventLoop::get_realtime_event_loop(conf.use_realtime_scheduler())),
      m_fd(-1),
      m_send_lines(),
      m_sended_lines(),
      m_send_buf_helper(m_mutex, m_send_lines, m_sended_lines),
      m_conf(conf)
{
    initialize();
}


/*
 * ~PrusaDevice()
 */
PrusaDevice::~PrusaDevice()
{
    on_shutdown();
}


/*
 * initialize()
 */
void PrusaDevice::initialize()
{
    if (!is_valid() && state() != Device::State::UNINITIALIZED) {
        return;
    }
    const std::lock_guard<std::mutex> guard(m_mutex);
    m_sensor_readings.clear();
    m_send_lines.clear();
    m_sended_lines.clear();
    m_curr_print.clear();

    set_state(State::INIT_DEVICE);
    m_pstate = DEVICE_NOT_READY;

    // skip the rest, if we have allready an connection to the printer
    if (0 <= m_fd) {
        return;
    }

    m_print_helper = [this](const std::string &line) {
        const std::lock_guard<std::mutex> guard(m_mutex);
        if (m_curr_print.empty()) {
            update_progress(100, 0);
            set_state(State::OK);
            return;
        }
        send_command_nl(m_curr_print.front(), nullptr, m_print_helper);
        m_curr_print.pop_front();
    };

    m_fd = open(m_device.c_str(), O_RDWR | O_SYNC | O_NOCTTY | O_NONBLOCK);
    if (0 > m_fd) {
        if (EBUSY == errno) {
            set_state(State::BUSY);
        } else {
            set_state(State::ERROR);
        }
        return;
    }

    if (0 > ioctl(m_fd, TIOCEXCL)) {
        return;
    }

    struct termios tty;
    if (0 != tcgetattr(m_fd, &tty)) {
        close(m_fd);
        m_fd = -1;
        set_state(State::ERROR);
        return;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_lflag = ICANON;
    tty.c_oflag = 0;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (0 != tcsetattr(m_fd, TCSANOW, &tty)) {
        set_state(State::ERROR);
        return;
    }

    m_read_helper.pd = this;
    m_read_helper.set_state = [this](enum State state){ set_state(state); };

    // this have to be the last call in this function!
    m_ev.register_read_cb(m_fd, [](int fd, void *arg) -> bool {
            struct read_helper *rh = static_cast<struct read_helper *>(arg);
            char buf[1000];
            ssize_t n;
            while(0 < (n = read(fd, buf, 1000))) {
                if (!n) {
                    continue;
                }
                buf[n-1] = 0;
                rh->pd->onReadedLine(buf);

            }

            if (0 == n) {
                rh->set_state(State::DISCONNECTED);
                return false;
            }

            if (errno != EAGAIN) {
                std::cerr << "Error while reading from fd: " << strerror(errno) << "\n";
                std::cerr << "fd: " << fd << "\n";
                rh->set_state(State::ERROR);
                return false;
            }
            return true;
        }, &m_read_helper);
}


/*
 * onReadedLine()
 */
void PrusaDevice::onReadedLine(const std::string &readed_line)
{
    if (!is_valid()) {
        return;
    }
    if (m_conf.verbose()) {
        std::cout << readed_line.length() << " > " << readed_line << "\n";
    }
    if (readed_line == "start") {
        initialize();
        return;
    }

    // we got an ok for an command, call the callback for finished
    if (readed_line == "ok") {
        struct send_buf finished_buf;
        {
            const std::lock_guard<std::mutex> guard(m_mutex);
            if (!m_sended_lines.empty()) {
                finished_buf = m_sended_lines.front();
                m_sended_lines.pop_front();
            } else {
                std::cerr << "Got ok, but didn't have commands in the queue!";
            }
        }
        if (finished_buf.finished) {
            finished_buf.finished(finished_buf.line);
        }
        return;
    // we send an invalid command
    } else if (0 == readed_line.compare(0, std::strlen("echo:Unknown command:"), "echo:Unknown command:")) {
        const std::lock_guard<std::mutex> guard(m_mutex);
        std::cerr << "Error: Command is not known by printer: " << m_sended_lines.front().line << std::endl;
        // TODO: Sending an invalid gcode command kills gocoded completely ... fix this!
        throw std::runtime_error("Sended an command to printer, which the printer does not know!");
    }

    struct send_buf curr_buf;
    {
        const std::lock_guard<std::mutex> guard(m_mutex);
        if (!m_sended_lines.empty()) {
            curr_buf = m_sended_lines.front();
        }
    }
    if (curr_buf.parse_line) {
        curr_buf.parse_line(readed_line);
    }

    if (state() == State::INIT_DEVICE) {
        switch(m_pstate) {
            case DEVICE_NOT_READY:
                if (readed_line == "LCD status changed") {
                    const std::lock_guard<std::mutex> guard(m_mutex);
                    change_pstate(DEVICE_ACCEPTS_COMMANDS);
                }
                break;
            default:
                break;
        };
    } else if (state() == State::OK || state() == State::PRINTING) {
        if (std::regex_match(readed_line, __full_temp_regex)) {
            parse_temp(readed_line);
        } else if (std::regex_match(readed_line, __full_pos_regex)) {
            parse_pos(readed_line);
        } else if (std::regex_match(readed_line, __full_fan_regex)) {
            parse_fan(readed_line);
        } else {
            parse_progress(readed_line);
        }
    }
}


/*
 * change_pstate
 */
void PrusaDevice::change_pstate(enum prusa_state pstate)
{
    if (!is_valid()) {
        return;
    }
    enum prusa_state old_pstate = m_pstate;
    m_pstate = pstate;

    switch (m_pstate) {
        case DEVICE_ACCEPTS_COMMANDS:
            // get firmware version and capabilities
            send_command_nl("M115",
                            [this](const std::string &read_line) {
                                if (0 != read_line.compare(0, 4, "Cap:")) {
                                    return;
                                }
                                const size_t second_colon = read_line.find(':', 5);
                                if (std::string::npos == second_colon) {
                                    return;
                                }
                                if ('1' != read_line[second_colon+1]) {
                                    return;
                                }
                                const std::lock_guard<std::mutex> guard(m_mutex);
                                m_capabilities.push_back(read_line.substr(4, second_colon-4));
                            },
                            [this](const std::string &bla){
                                unsigned int bitmap = 0;
                                {
                                    const std::lock_guard<std::mutex> guard(m_mutex);
                                    for (const auto &cap: m_capabilities) {
                                        if ("AUTOREPORT_TEMP" == cap) {
                                            bitmap |= 0x1 << 0;
                                        } else if ("AUTOREPORT_FANS" == cap) {
                                            bitmap |= 0x1 << 1;
                                        } else if ("AUTOREPORT_POSITION" == cap) {
                                            bitmap |= 0x1 << 2;
                                        }
                                    }
                                }
                                // enable autoreporting of senser readings
                                std::string command = "M155 S2 C";
                                command += std::to_string(bitmap);
                                send_command_nl(command,
                                                nullptr,
                                                [this](const std::string &) {
                                                    change_pstate(DEVICE_READY);
                                                });
                            });
            break;
        case DEVICE_READY:
            set_state(State::OK);
            start_print();
            break;
        default:
            break;
    }
}


/*
 * send_command_nl()
 */
void PrusaDevice::send_command_nl(const std::string &command,
                                  std::function<void(const std::string &line)> parse_line,
                                  std::function<void(const std::string &line)> finished)
{
    const bool is_empty = m_send_lines.empty();
    struct send_buf &sb = m_send_lines.emplace_back();
    sb.line = command + "\n";
    //std::cout << "send_command: " << command << "\n";
    sb.finished = finished;
    sb.parse_line = parse_line;

    // it is important, that we only register the write callback if we didn't use an already
    // registered write callback. If we would register a new one, while using the old one,
    // than we mess up some memory (stack or heap) if the usage and registering happens on
    // different threads.
    if (!is_empty) {
        return;
    }
    m_ev.register_write_cb(m_fd, [](int fd, void *arg) -> bool {
            struct send_buf_helper *sb_helper = static_cast<struct send_buf_helper *>(arg);
            std::lock_guard<std::mutex> guard(sb_helper->mutex);

            ssize_t n = 0;
            while(-1 != n) {
                {
                    if (sb_helper->send_lines.empty()) {
                        break;
                    }
                    struct send_buf &curr = sb_helper->send_lines.front();
                    size_t to_send = curr.line.size() - curr.sended;
                    if (to_send > 0) {
                        n = write(fd, curr.line.data(), to_send);
                        if (n < 0) {
                            break;
                        }
                        curr.sended += n;
                    }
                    sb_helper->sended_lines.push_back(curr);
                    sb_helper->send_lines.pop_front();
                }
            }
            if (   n == -1
                && errno != EAGAIN) {
                return false;
            }

            if (sb_helper->send_lines.empty()) {
                return false;
            }
            return true;

        }, &m_send_buf_helper);
}


/*
 * parse_temp()
 */
void PrusaDevice::parse_temp(const std::string &line)
{
    std::smatch m;
    auto begin = line.begin();
    const auto end = line.end();
    while (std::regex_search(begin, end, m, __temp_regex)) {
        begin = begin + m.position() + m[0].length();
        struct SensorValue readings;

        std::string::size_type colon_pos = m[0].str().find(':');
        std::string::size_type slash_pos = m[0].str().find('/', colon_pos);

        std::string type = m[0].str().substr(0, colon_pos);
        readings.current_value = std::stod(m[0].str().substr(colon_pos+1));
        readings.unit = "°C";

        if (std::string::npos != slash_pos) {
            readings.set_point = std::stod(m[0].str().substr(slash_pos+1));
        }

        const std::lock_guard<std::mutex> guard(m_mutex);
        if ("T" == type) {
            m_sensor_readings["temp_extruder"] = readings;
        } else if ("B" == type) {
            m_sensor_readings["temp_bed"] = readings;
        } else if ("A" == type) {
            m_sensor_readings["temp_ambient"] = readings;
        }
    }
}



/*
 * parse_pos()
 */
void PrusaDevice::parse_pos(const std::string &line)
{
    std::smatch m;
    auto begin = line.begin();
    const auto end = line.end();
    while (std::regex_search(begin, end, m, __pos_regex)) {
        begin = begin + m.position() + m[0].length();
        struct SensorValue readings;

        // After "Count" there are only debug values
        if ("Count " == m[0]) {
            break;
        }

        std::string::size_type colon_pos = m[0].str().find(':');

        std::string name = m[0].str().substr(0, colon_pos);
        std::string::size_type space = name.find(' ');
        if (std::string::npos != space) {
            name = name.substr(0, space);
        }
        readings.current_value = std::stod(m[0].str().substr(colon_pos+1));
        readings.unit = "mm";

        const std::lock_guard<std::mutex> guard(m_mutex);
        if ("X" == name) {
            m_sensor_readings["pos_X"] = readings;
        } else if ("Y" == name) {
            m_sensor_readings["pos_Y"] = readings;
        } else if ("Z" == name) {
            m_sensor_readings["pos_Z"] = readings;
        } else if ("E" == name) {
            m_sensor_readings["pos_E"] = readings;
        }
    }
}


/*
 * parse_fan()
 */
void PrusaDevice::parse_fan(const std::string &line)
{
    std::smatch m;
    auto begin = line.begin();
    const auto end = line.end();
    while (std::regex_search(begin, end, m, __fan_regex)) {
        begin = begin + m.position() + m[0].length();

        struct SensorValue readings;

        // ignor power readings
        if (std::string::npos != m[0].str().find('@')) {
            continue;
        }

        std::string::size_type colon_pos = m[0].str().find(':');

        std::string name = m[0].str().substr(0, colon_pos);
        readings.current_value = std::stod(m[0].str().substr(colon_pos+1));
        readings.unit = "rpm";

        const std::lock_guard<std::mutex> guard(m_mutex);
        m_sensor_readings["fan_" + name] = readings;
    }

    /*
    for (const auto &read: m_sensor_readings) {
        std::cout << read.first << " = " << read.second.current_value;
        if (read.second.set_point) {
            std::cout << " / " << *read.second.set_point;
        }
        std::cout << "\n";
    }
    */
}


/*
 * parse_progress()
 */
void PrusaDevice::parse_progress(const std::string &line)
{
    std::smatch match;
    // TODO: distinguise between normal mode and silent mode ...
    if (std::regex_match(line, match, __progress_regex)) {
        if (match.size() != 3) {
            throw std::runtime_error("This is unexpected and if it happends, than this is a programming error!");
        }
        const unsigned percentage = std::atoi(match[1].str().c_str());
        const unsigned remaining = std::atoi(match[2].str().c_str());
        update_progress(percentage, remaining);
    }
}


/*
 * print_file()
 */
Device::PrintResult PrusaDevice::print_file(const std::string &file_path)
{
    if (state() != Device::State::OK) {
        return PrintResult::ERR_INVALID_STATE;
    }
    const std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_curr_print.empty()) {
        return PrintResult::ERR_PRINTING;
    }
    std::ifstream file(file_path);

    auto trim = [](std::string &s) {
        s.erase(0, s.find_first_not_of(" \n\r\t\f\v"));
        s.erase(s.find_last_not_of(" \n\r\t\f\v") + 1);
    };

    while (!file.eof()) {
        std::string line;
        std::getline(file, line);
        std::string::size_type pos = line.find(';');
        if (std::string::npos != pos) {
            line = line.substr(0, pos);
        }
        if (0 == line.size()) {
            continue;
        }
        trim(line);
        if (0 == line.size()) {
            continue;
        }
        m_curr_print.push_back(line);
    }

    start_print();

    return PrintResult::OK;
}


/*
 * print()
 */
Device::PrintResult PrusaDevice::print(const std::string &gcode)
{
    if (state() != Device::State::OK) {
        return PrintResult::ERR_INVALID_STATE;
    }
    const std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_curr_print.empty()) {
        return PrintResult::ERR_PRINTING;
    }
    std::istringstream gcode_stream(gcode);

    auto trim = [](std::string &s) {
        s.erase(0, s.find_first_not_of(" \n\r\t\f\v"));
        s.erase(s.find_last_not_of(" \n\r\t\f\v") + 1);
    };

    while (!gcode_stream.eof()) {
        std::string line;
        std::getline(gcode_stream, line);
        std::string::size_type pos = line.find(';');
        if (std::string::npos != pos) {
            line = line.substr(0, pos);
        }
        if (0 == line.size()) {
            continue;
        }
        trim(line);
        if (0 == line.size()) {
            continue;
        }
        m_curr_print.push_back(line);
    }

    start_print();

    return PrintResult::OK;
}


/**
 * on_shutdown()
 */
void PrusaDevice::on_shutdown()
{
    if (0 >= m_fd) {
        m_ev.unregister_read_cb(m_fd);
        m_ev.unregister_write_cb(m_fd);
        close(m_fd);
        m_fd = -1;
    }
}


/*
 * start_print()
 */
void PrusaDevice::start_print()
{
    if (state() != State::OK) {
        return;
    }

    if (m_curr_print.empty()) {
        return;
    }

    set_state(State::PRINTING);

    for (int i = 0; i < 2; ++i) {
        if (m_curr_print.empty()) {
            update_progress(100, 0);
            set_state(State::OK);
            return;
        }
        send_command_nl(m_curr_print.front(), nullptr, m_print_helper);
        m_curr_print.pop_front();
    }
}


/*
 * set_state()
 */
void PrusaDevice::set_state(enum State state)
{
    if (state == this->state()) {
        return;
    }
    Device::set_state(state);
    if (!is_valid() && 0 <= m_fd) {
        m_ev.unregister_read_cb(m_fd);
        m_ev.unregister_write_cb(m_fd);
        close(m_fd);
        m_fd = -1;
    }
}
