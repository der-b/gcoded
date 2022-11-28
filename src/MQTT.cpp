#include "MQTT.hh"
#include <iostream>

/*
 * on_connect() is a helper function for the MQTT class since we want to hide the c callbacks.
 */
void on_connect(struct mosquitto *mosq, void *_data, int reason_code)
{
    struct MQTT::callback_data *data = static_cast<struct MQTT::callback_data *>(_data);
    // this is only for easier reading of the code since now we can access
    // the members in the same way as in the class methods
    struct MQTT::callback_data &m_cb_data = *data;

    const std::lock_guard<std::mutex> guard(m_cb_data.mutex);
    data->connection_try++;

    if (reason_code) {
        std::cerr << "Faild to connect("
                  << m_cb_data.connection_try
                  << "): "
                  << mosquitto_connack_string(reason_code)
                  << "\n";
        if (m_cb_data.conf.mqtt_connect_retries() &&
            data->connection_try >= *m_cb_data.conf.mqtt_connect_retries()) {
            throw std::runtime_error("Limit of connection retries reached. Giving up.");
        }
        return;
    }

    for (const std::string &topic: m_cb_data.topics)
    {
        //std::cout << "subscribe: " << topic << "\n";
        mosquitto_subscribe(m_cb_data.mosq, NULL, topic.c_str(), 0);
    }

    m_cb_data.connected = true;
}


/*
 * on_disconnect() is a helper function for the MQTT class since we want to hide the c callbacks.
 */
void on_disconnect(struct mosquitto *mosq, void *_data, int reason_code)
{
    struct MQTT::callback_data *data = static_cast<struct MQTT::callback_data *>(_data);
    // this is only for easier reading of the code since now we can access
    // the members in the same way as in the class methods
    struct MQTT::callback_data &m_cb_data = *data;

    const std::lock_guard<std::mutex> guard(m_cb_data.mutex);
    // TODO: Mosquitto dosn't send 0x8e in cases of session takeovers
    // as required by the MQTT specification in section "3.1.4 CONNECT Actions"
    // github issue: https://github.com/eclipse/mosquitto/issues/2607
    if (0x8E == reason_code) {
        std::cout << "Session takeover!\n";
    }
    //std::cout << "MQTT disconnect: " << mosquitto_reason_string(reason_code) << "(" << std::hex << reason_code << ")\n";
    m_cb_data.connected = false;
}


/*
 * on_message() is a helper function for the MQTT class since we want to hide the c callbacks.
 */
void on_message(struct mosquitto *mosq, void *_data, const struct mosquitto_message *message)
{
    struct MQTT::callback_data *data = static_cast<struct MQTT::callback_data *>(_data);
    // this is only for easier reading of the code since now we can access
    // the members in the same way as in the class methods
    struct MQTT::callback_data &m_cb_data = *data;

    const std::lock_guard<std::mutex> guard(m_cb_data.mutex);
    for (auto &listener: m_cb_data.listener) {
        listener->on_message(message->topic, (const char *)message->payload, message->payloadlen);
    }
}


/*
 * MQTT()
 */
MQTT::MQTT(const MQTTConfig &conf)
    : m_cb_data(conf)
{
    const std::lock_guard<std::mutex> guard(m_cb_data.mutex);

    if (MOSQ_ERR_SUCCESS != mosquitto_lib_init()) {
        throw std::runtime_error("Failed to initialize libmosquitto.");
    }

    if (conf.mqtt_client_id().size()) {
        m_cb_data.mosq = mosquitto_new(conf.mqtt_client_id().c_str(), true, &m_cb_data);
    } else {
        m_cb_data.mosq = mosquitto_new(NULL, true, &m_cb_data);
    }
    if (NULL == m_cb_data.mosq) {
        throw std::runtime_error("Failed to to initialize mosquitto object.");
    }
    //mosquitto_int_option(m_cb_data.mosq, MOSQ_OPT_PROTOCOL_VERSION, MQTT_PROTOCOL_V5);

    mosquitto_connect_callback_set(m_cb_data.mosq, on_connect);
    mosquitto_disconnect_callback_set(m_cb_data.mosq, on_disconnect);
    mosquitto_message_callback_set(m_cb_data.mosq, on_message);

    if (conf.mqtt_user()) {
        int ret = MOSQ_ERR_SUCCESS;
        if (conf.mqtt_password()) {
            ret = mosquitto_username_pw_set(m_cb_data.mosq, conf.mqtt_user()->c_str(), conf.mqtt_password()->c_str());
        } else {
            ret = mosquitto_username_pw_set(m_cb_data.mosq, conf.mqtt_user()->c_str(), NULL);
        }
        if (MOSQ_ERR_SUCCESS != ret) {
            throw std::runtime_error("Failed to set username and password for mqtt connection.");
        }
    }

    if (MOSQ_ERR_SUCCESS != mosquitto_connect_async(m_cb_data.mosq,
                                                    m_cb_data.conf.mqtt_broker().c_str(),
                                                    m_cb_data.conf.mqtt_port(),
                                                    60)) {
        throw std::runtime_error("Failed to connect to mqtt broker.");
    }
}


/*
 * ~MQTT()
 */
MQTT::~MQTT()
{
    stop();
    const std::lock_guard<std::mutex> guard(m_cb_data.mutex);
    mosquitto_destroy(m_cb_data.mosq);
    m_cb_data.mosq = NULL;
    if (MOSQ_ERR_SUCCESS != mosquitto_lib_cleanup()) {
        std::cerr << "Failed to cleanup libmosquitto.\n";
    }
}


/*
 * start()
 */
void MQTT::start()
{
    const std::lock_guard<std::mutex> guard(m_cb_data.mutex);
    if (m_cb_data.running) {
        return;
    }
    if (MOSQ_ERR_SUCCESS != mosquitto_loop_start(m_cb_data.mosq)) {
        throw std::runtime_error("Failed to start mosquitto loop.");
    }
    m_cb_data.running = true;
}


/*
 * stop()
 */
void MQTT::stop()
{
    //std::cout << "stop()\n";
    if (m_cb_data.connected) {
        if (MOSQ_ERR_SUCCESS != mosquitto_disconnect(m_cb_data.mosq)) {
            std::cerr << "Failed to disconnect from MQTT broker.\n";
        }
    }
    if (m_cb_data.running) {
        if (MOSQ_ERR_SUCCESS != mosquitto_loop_stop(m_cb_data.mosq, false)) {
            std::cerr << "Failed to stop mosquitto loop.\n";
        }
        m_cb_data.running = false;
    }
}


/*
 *  publish()
 */
void MQTT::publish(const char *topic, const char *payload, size_t payload_length)
{
    if (MOSQ_ERR_SUCCESS != mosquitto_publish(m_cb_data.mosq, NULL, topic, payload_length, payload, 0, false)) {
        if (m_cb_data.conf.verbose()) {
            std::cout << "MQTT: Failed to publish message on topic: " << topic << "\n";
        }
    }
}


/*
 *  publish_retained()
 */
void MQTT::publish_retained(const char *topic, const char *payload, size_t payload_length)
{
    if (MOSQ_ERR_SUCCESS != mosquitto_publish(m_cb_data.mosq, NULL, topic, payload_length, payload, 0, true)) {
        if (m_cb_data.conf.verbose()) {
            std::cout << "MQTT: Failed to publish retained message on topic: " << topic << "\n";
        }
    }
}


/*
 * subscribe()
 */
void MQTT::subscribe(const std::string &topic)
{
    const std::lock_guard<std::mutex> guard(m_cb_data.mutex);
    auto inserted = m_cb_data.topics.insert(topic);

    if (   m_cb_data.connected
        && inserted.second) {
        //std::cout << "subscribe: " << topic << "\n";
        mosquitto_subscribe(m_cb_data.mosq, NULL, topic.c_str(), 0);
    }
}


/*
 * unsubscribe()
 */
void MQTT::unsubscribe(const std::string &topic)
{
    const std::lock_guard<std::mutex> guard(m_cb_data.mutex);
    auto erased = m_cb_data.topics.erase(topic);

    if (   m_cb_data.connected
        && 0 != erased) {
        //std::cout << "unsubscribe: " << topic << "\n";
        mosquitto_unsubscribe(m_cb_data.mosq, NULL, topic.c_str());
    }
}


/*
 * register_listener()
 */
void MQTT::register_listener(Listener *listener)
{
    const std::lock_guard<std::mutex> guard(m_cb_data.mutex);
    m_cb_data.listener.insert(listener);
}


/*
 * unregister_listener()
 */
void MQTT::unregister_listener(Listener *listener)
{
    const std::lock_guard<std::mutex> guard(m_cb_data.mutex);
    m_cb_data.listener.erase(listener);
}
