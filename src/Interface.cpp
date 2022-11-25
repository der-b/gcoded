#include "Interface.hh"
#include "mqtt_messages/MsgDeviceState.hh"
#include "mqtt_messages/MsgPrint.hh"
#include "mqtt_messages/MsgPrintResponse.hh"
#include "mqtt_messages/MsgPrintProgress.hh"
#include "mqtt_messages/MsgAliases.hh"
#include "mqtt_messages/MsgAliasesSet.hh"
#include "mqtt_messages/MsgAliasesSetProvider.hh"

/*
 * Interface()
 */
Interface::Interface(const Config &conf, Aliases &aliases)
    : m_conf(conf),
      m_mqtt(conf),
      m_aliases(aliases)
{
    //std::cout << "Interface::" << __func__ << "\n";
    {
        const std::lock_guard<std::mutex> guard(m_mutex);
        m_mqtt.register_listener(this);
        std::string topic_print_request = m_conf.mqtt_prefix() + "/clients/" + m_conf.mqtt_client_id() + "/+/print_request";
        std::string topic_alias_set = m_conf.mqtt_prefix() + "/aliases/" + m_conf.mqtt_client_id() + "/set";
        m_mqtt.subscribe(topic_print_request);
        m_mqtt.subscribe(topic_alias_set);
        m_mqtt.start();
    }
    Detector::get(conf).register_on_new_device(this);
    aliases.register_listener(this);
    on_alias_change();
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
        std::string topic_pp = m_conf.mqtt_prefix() + "/clients/" + m_conf.mqtt_client_id() + "/" + dev.name() + "/print_progress";
        m_mqtt.publish_retained(topic.c_str(), NULL, 0);
        m_mqtt.publish_retained(topic_pp.c_str(), NULL, 0);
        const std::lock_guard<std::mutex> guard(m_mutex);
        m_retain_topics.erase(topic);
        m_retain_topics.erase(topic_pp);
        m_mqtt.publish(topic, buf);
    } else {
        m_mqtt.publish_retained(topic, buf);
        const std::lock_guard<std::mutex> guard(m_mutex);
        m_retain_topics.insert(topic);
    }
    if (!dev.is_valid()) {
        const std::lock_guard<std::mutex> guard(m_mutex);
        dev.unregister_listener(this);
    }
}

/*
 * on_message()
 */
void Interface::on_message(const char *topic, const char *payload, size_t payload_len)
{
    const std::string print_prefix = m_conf.mqtt_prefix() + "/clients/" + m_conf.mqtt_client_id() + "/";
    const std::string print_postfix = "/print_request";
    const std::string alias_topic = m_conf.mqtt_prefix() + "/aliases/" + m_conf.mqtt_client_id() + "/set";

    if (alias_topic == topic) {
        const std::vector<char> msg_buf(payload, payload + payload_len);
        try {
            MsgAliasesSetProvider provider_msg;
            provider_msg.decode(msg_buf);
            m_aliases.set_provider_alias(provider_msg.provider_alias());
            return;
        } catch (...) {
        }


        try {
            MsgAliasesSet device_msg;
            device_msg.decode(msg_buf);
            m_aliases.set_alias(device_msg.device_name(), device_msg.device_alias());
        } catch (const std::exception &e) {
            std::cerr << "Faild to set alias: " << e.what() << "\n";
            return;
        }


    } else if (   0 == print_prefix.compare(0, print_prefix.size(), topic, print_prefix.size())
               && 0 <= std::strlen(topic) - print_postfix.size()
               && 0 == print_postfix.compare(0, print_postfix.size(), topic + std::strlen(topic) - print_postfix.size())) {

        const char *first = topic + print_prefix.size();
        const char *last = topic + std::strlen(topic) - print_postfix.size();
        const std::string device(first, last);

        const std::vector<char> msg_buf(payload, payload + payload_len);
        MsgPrint print_msg;
        try {
            print_msg.decode(msg_buf);
        } catch (const std::exception &e) {
            std::cerr << "Could not decode print request message: " << e.what() << "\n";
            return;
        }

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
        std::string response_topic = print_prefix + device + "/print_response";
        m_mqtt.publish(response_topic, response_buf);
    } else {
        std::cerr << "Got message on unknown topic: " << topic << "\n";
    }
}


/*
 * on_build_progress_change()
 */
void Interface::on_build_progress_change(Device &device, unsigned percentage, unsigned remaining_time)
{
    MsgPrintProgress progress(percentage, remaining_time);
    std::vector<char> buf;
    progress.encode(buf);
    std::string topic = m_conf.mqtt_prefix() + "/clients/" + m_conf.mqtt_client_id() + "/" + device.name() + "/print_progress";
    m_mqtt.publish_retained(topic, buf);
    const std::lock_guard<std::mutex> guard(m_mutex);
    m_retain_topics.insert(topic);
}

void Interface::on_alias_change()
{
    std::map<std::string, std::string> aliases;
    std::optional<std::string> provider_alias = m_aliases.provider_alias();
    m_aliases.get_aliases(aliases);
    MsgAliases msg_aliases;

    if (provider_alias) {
        msg_aliases.set_provider_alias(*provider_alias);
    }
    for (const auto &alias: aliases) {
        msg_aliases.add_alias(alias.first, alias.second);
    }

    std::vector<char> buf;
    msg_aliases.encode(buf);
    std::string topic = m_conf.mqtt_prefix() + "/aliases/" + m_conf.mqtt_client_id();
    m_mqtt.publish_retained(topic, buf);
    const std::lock_guard<std::mutex> guard(m_mutex);
    m_retain_topics.insert(topic);
}
