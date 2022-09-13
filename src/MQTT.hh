#ifndef __MQTT_HH__
#define __MQTT_HH__

#include <mutex>
#include <vector>
#include <set>
#include <functional>
#include <mosquitto.h>
#include "MQTTConfig.hh"

/**
 * MQTT interface to gcoded.
 */
class MQTT {
    public:
        class Listener {
            public:
                virtual void on_message(const char *topic, const char *payload, size_t payload_len) = 0;
                virtual ~Listener() {};
        };

    public:
        MQTT() = delete;
        MQTT(const MQTT &) = delete;
        MQTT(const MQTT &&) = delete;
        MQTT &operator=(const MQTT &) = delete;
        MQTT(const MQTTConfig &conf);

        ~MQTT();

        void start();
        void stop();

        /**
         * Publish a message via MQTT. This is a fire and forgett. That means, the message 
         * will be send with QOS 0 and will not be retained.
         */
        void publish(const std::string &topic, const std::vector<char> &payload)
        {
            publish(topic.c_str(), payload.data(), payload.size());
        }


        /**
         * Publish a message via MQTT. This is a fire and forgett. That means, the message 
         * will be send with QOS 0 and will not be retained.
         */
        void publish(const char *topic, const char *payload, size_t payload_length);

        /**
         * Publish a message via MQTT. The message 
         * will be send with QOS 0 and will be retained.
         */
        void publish_retained(const std::string &topic, const std::vector<char> &payload)
        {
            publish_retained(topic.c_str(), payload.data(), payload.size());
        }


        /**
         * Publish a message via MQTT. The message 
         * will be send with QOS 0 and will be retained.
         */
        void publish_retained(const char *topic, const char *payload, size_t payload_length);

        void subscribe(const std::string &topic);
        void subscribe(const char *topic)
        {
            subscribe(std::string(topic));
        }
        void unsubscribe(const std::string &topic);
        void unsubscribe(const char *topic)
        {
            unsubscribe(std::string(topic));
        }

        void register_listener(Listener *listener);
        void unregister_listener(Listener *listener);


    public:
        struct callback_data {
            std::mutex mutex;
            bool running;
            bool connected;
            const MQTTConfig &conf;
            struct mosquitto *mosq;
            std::set<Listener *> listener;
            std::set<std::string> topics;

            callback_data(const MQTTConfig &_conf) 
                : conf(_conf),
                  running(false),
                  connected(false),
                  mosq(NULL)
            { }
        };
    private:
       struct callback_data m_cb_data;
};

#endif
