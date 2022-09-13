#include "Interface.hh"
#include "mqtt_messages/MsgDeviceState.hh"
#include "mqtt_messages/MsgPrint.hh"
#include "mqtt_messages/MsgPrintResponse.hh"

/*
 * Interface()
 */
Interface::Interface(const Config &conf)
    : m_conf(conf),
      m_mqtt(conf)
{
    //std::cout << "Interface::" << __func__ << "\n";
    {
        const std::lock_guard<std::mutex> guard(m_mutex);
        m_mqtt.register_listener(this);
        std::string topic = m_conf.mqtt_prefix() + "/clients/" + m_conf.mqtt_client_id() + "/+/print_request";
        m_mqtt.subscribe(topic);
        m_mqtt.start();
    }
    Detector::get(conf).register_on_new_device(this);
}

/*
 * ~Interface()
 */
Interface::~Interface()
{
    const std::lock_guard<std::mutex> guard(m_mutex);
    Detector::get(m_conf).unregister_on_new_device(this);
    Detector::get(m_conf).for_each_device([this](const std::shared_ptr<Device> &dev) {
        dev->unregister_listener(this);
    });
    // delete all retained messages
    for (const auto &r_topic: m_retain_topics) {
        m_mqtt.publish_retained(r_topic.c_str(), NULL, 0);
    }
    m_mqtt.stop();
}


/*
 * on_new_device()
 */
void Interface::on_new_device(const std::shared_ptr<Device> &dev)
{
    {
        const std::lock_guard<std::mutex> guard(m_mutex);
        dev->register_listener(this);
    }
    on_state_change(*dev, dev->state());
}


/*
 * on_state_changed()
 */
void Interface::on_state_change(Device &dev, enum Device::State new_state)
{
    std::string topic = m_conf.mqtt_prefix() + "/clients/" + m_conf.mqtt_client_id() + "/" + dev.name() + "/state";
    std::vector<char> buf;
    MsgDeviceState msg_state(new_state);
    msg_state.encode(buf);
    if (Device::State::DISCONNECTED == new_state) {
        m_mqtt.publish_retained(topic.c_str(), NULL, 0);
        m_retain_topics.erase(topic);
        m_mqtt.publish(topic, buf);
    } else {
        m_mqtt.publish_retained(topic, buf);
        m_retain_topics.insert(topic);
    }
}

/*
 * on_message()
 */
void Interface::on_message(const char *topic, const char *payload, size_t payload_len)
{
    // TODO: The client ID is already known, therefore the logic here can be simplified
    const std::string prefix = m_conf.mqtt_prefix() + "/clients/";
    const std::string print_postfix = "/print_request";
    
    ssize_t res;
    if (0 != (res = prefix.compare(0, prefix.size(), topic, prefix.size()))) {
        std::cerr << "Unexpected mqtt topic prefix\n";
        return;
    }


    if (   0 <= std::strlen(topic) - print_postfix.size()
        && 0 == print_postfix.compare(0, print_postfix.size(), topic + std::strlen(topic) - print_postfix.size())) {

        const char *first = topic + prefix.size();
        const char *last = topic + std::strlen(topic) - print_postfix.size();
        const char *pos = std::find(first, last, '/');
        if (pos >= last) {
            std::cerr << "Unexpected topic format: " << topic << "\n";
            return;
        }
        const std::string provider(first, pos);
        const std::string device(pos+1, last);

        const std::vector<char> msg_buf(payload, payload + payload_len);
        MsgPrint print_msg;
        print_msg.decode(msg_buf);

        // std::cout << "Got MQTT message on topic: " << topic << "\n";
        // std::cout << "part2: " << std::hex << std::setfill('0') << std::setw(16) << print_msg.request_code_part1() << "\n";
        // std::cout << "part1: " << std::hex << std::setfill('0') << std::setw(16) << print_msg.request_code_part2() << "\n";
        // std::cout << "size : " << std::dec << print_msg.gcode().size() << "\n";

        Device::PrintResult result = Device::PrintResult::NET_ERR_NO_DEVICE;
        Detector &detector = Detector::get(m_conf);
        // TODO: Implemente and use something like find_device(...)
        detector.for_each_device([&](const std::shared_ptr<Device> &dev) {
                if (dev->name() == device) {
                    result = dev->print(print_msg.gcode());
                }
        });

        MsgPrintResponse response_msg(print_msg, result);
        std::vector<char> response_buf;
        response_msg.encode(response_buf);
        std::string response_topic = prefix + m_conf.mqtt_client_id() + "/" + device + "/print_response";
        m_mqtt.publish(response_topic, response_buf);
    }
}
