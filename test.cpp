#include <chrono>
#include <iostream>
#include <mqtt/async_client.h>
#include <mqtt/client.h>
#include <mqtt/connect_options.h>
#include <mqtt/message.h>
#include <mqtt/ssl_options.h>
#include <mqtt/token.h>
#include <thread>

const std::string TOPIC = "modbus/data";
const std::string BROKER =
    "ssl://ff8d007a082a46b1b696047efe7cb2d4.s1.eu.hivemq.cloud:8883";
const std::string USERNAME = "test01";
const std::string PASSWORD = "Test@123";

class callback : public virtual mqtt::callback {
private:
  mqtt::async_client *cli = nullptr;
  mqtt::connect_options *connopts = nullptr;

public:
  callback(mqtt::async_client *cli, mqtt::connect_options *connopts) {
    this->cli = cli;
    this->connopts = connopts;
  }
  void connection_lost(const std::string &cause) override {
    std::cout << "Connection lost: " << cause << std::endl;
    this->cli->connect(*this->connopts)->wait();
  }

  void connected(const std::string &cause) override {
    std::cout << "Connected to broker with cause: " << cause << std::endl;
  }

  void message_arrived(mqtt::const_message_ptr msg) override {
    std::cout << msg->to_string() << std::endl;
  }

  void delivery_complete(mqtt::delivery_token_ptr token) override {}
};

int main() {
  try {
    mqtt::create_options create_opts(MQTTVERSION_5);
    mqtt::async_client cli(BROKER, "mqtt_ssl_client");

    auto ssl_opts =
        mqtt::ssl_options_builder().enable_server_cert_auth(true).finalize();

    mqtt::connect_options conn_opts;
    conn_opts.set_user_name(USERNAME);
    conn_opts.set_password(PASSWORD);
    conn_opts.set_ssl(ssl_opts);
    conn_opts.set_keep_alive_interval(20);
    conn_opts.set_clean_session(true);
    conn_opts.set_connect_timeout(20); // Increased timeout to 20 seconds

    callback cb(&cli, &conn_opts);
    cli.set_callback(cb);
    std::cout << "Connecting to broker: " << BROKER << std::endl;
    while (!cli.is_connected()) {
      mqtt::token_ptr conntok = cli.connect(conn_opts);
      conntok->wait_for(std::chrono::seconds(5)); // Wait for connection
    }
    std::cout << "Connected. Subscribing to topic: " << TOPIC << std::endl;
    cli.subscribe(TOPIC, 2);

    std::string message = "Hello, HiveMQ SSL!";
    auto msg = mqtt::make_message("neoway", "Hello HiveMQ", 0, false);
    std::cout << "Publishing message: " << message << std::endl;
    cli.publish(msg);

    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  } catch (const mqtt::exception &exc) {
    std::cerr << "MQTT Error " << exc.get_reason_code() << ": " << exc.what()
              << std::endl;
    return 1;
  } catch (const std::exception &exc) {
    std::cerr << "Other Error: " << exc.what() << std::endl;
    return 1;
  }

  return 0;
}
