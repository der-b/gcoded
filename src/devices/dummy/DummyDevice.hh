#ifndef __DUMMY_DEVICE_HH__
#define __DUMMY_DEVICE_HH__

#include "../Device.hh"

class DummyDevice : public Device {
    public:
        DummyDevice() = delete;
        DummyDevice(const DummyDevice &) = delete;
        DummyDevice(DummyDevice &&) = delete;
        DummyDevice &operator=(const DummyDevice &) = delete;

        DummyDevice(const std::string &name);
        ~DummyDevice();

        virtual PrintResult print_file(const std::string &file_path) override;
        virtual PrintResult print(const std::string &gcode) override;
        virtual const std::string &name() const override 
        {
            return m_device;
        }
    private:
        std::string m_device;
};

#endif
