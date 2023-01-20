#ifndef __PRUSA_DEVICE_HH__
#define __PRUSA_DEVICE_HH__

#include "../Device.hh"
#include <iostream>
#include <mutex>
#include <functional>
#include <list>
#include "../../Config.hh"
#include "../../EventLoop.hh"

class PrusaDevice : public Device {
    public:
        PrusaDevice() = delete;
        PrusaDevice(const PrusaDevice &) = delete;
        PrusaDevice(PrusaDevice &&) = delete;
        PrusaDevice &operator=(const PrusaDevice &) = delete;

        PrusaDevice(const std::string &file, const std::string &name, const Config &conf);
        virtual ~PrusaDevice();

        void onReadedLine(const std::string &readedLine);

        virtual PrintResult print_file(const std::string &file_path) override;
        virtual PrintResult print(const std::string &gcode) override;
        virtual const std::string &name() const override 
        {
            return m_name;
        }

        std::map<std::string, struct SensorValue> sensor_readings() override
        {
            const std::lock_guard<std::mutex> guard(m_mutex);
            return m_sensor_readings;
        }
    protected:
        virtual void set_state(enum State new_state) override;

    private:
        /**
         * tries to initialize the device.
         */
        void initialize();

        enum prusa_state {
            UNINITIALIZED = 0,
            DEVICE_NOT_READY,
            DEVICE_ACCEPTS_COMMANDS,
            DEVICE_READY
        };

        void change_pstate(enum prusa_state pstate);
        
        /**
         * Sends a G-code command to the 3D printer. This is an asynchronous interface: All commands are buffered on the
         * host and send as soon as the used file descriptor is ready for writing. This could overload the printer.
         * Therefore, an implementation using this interface have to wait for the printer acknowledgement before sending the
         * next command.
         *
         * The function "finished" is called, when the 3d printer acknowledged the command and the function "parse_line" is
         * called for each received output line of the printer between sending the command and receiving the acknowledgment.
         *
         * Be aware, that depending on the firmware implementation, not all lines provided to parse_line are related to
         * the send command. If the printer supports an auto report feature, "parse_line" also get these auto reported lines,
         * if they happen to occur between the sending and the acknowledgement of the command.
         */
        void send_command_nl(const std::string &command,
                             std::function<void(const std::string &line)> parse_line,
                             std::function<void(const std::string &line)> finished);

        void parse_temp(const std::string &line);
        void parse_pos(const std::string &line);
        void parse_fan(const std::string &line);
        void parse_progress(const std::string &line);

        void start_print();

    public:
        struct read_helper {
            std::function<void(enum State)> set_state;
            PrusaDevice *pd;
        };

    private:

        struct send_buf {
            std::string line;
            size_t sended;
            std::function<void(const std::string &line)> parse_line;
            std::function<void(const std::string &line)> finished;

            send_buf()
                : sended(0)
            {};
        };

        struct send_buf_helper {
            std::mutex &mutex;
            std::list<struct send_buf> &send_lines;
            std::list<struct send_buf> &sended_lines;
            send_buf_helper(std::mutex &m, std::list<struct send_buf> &sl, std::list<struct send_buf> &sdl)
                : mutex(m),
                  send_lines(sl),
                  sended_lines(sdl)
            {};
        };


    private:
        std::string m_device;
        std::string m_name;
        int m_fd;
        enum prusa_state m_pstate;
        EventLoop &m_ev;

        std::mutex m_mutex;
        struct send_buf_helper m_send_buf_helper;
        std::list<struct send_buf> m_send_lines;
        std::list<struct send_buf> m_sended_lines;
        std::list<std::string> m_capabilities;
        std::map<std::string, struct SensorValue> m_sensor_readings;
        std::list<std::string> m_curr_print;
        std::function<void(const std::string &line)> m_print_helper;
        struct read_helper m_read_helper;
        const Config &m_conf;
};

#endif
