#include "mqtt.h"
#include "serial.h"
#include "mainwindow.h"
#include <chrono>
#include <mqtt/async_client.h>
#include <mqtt/message.h>
#include <mqtt/ssl_options.h>


#include <iostream>
#include <thread>
#include <fcntl.h>  // File control (open, O_RDWR, etc.)
#include <termios.h> // Terminal I/O (baud rates, parity, etc.)
#include <unistd.h>  // POSIX API (read, write, close)
#include <cstring>
#define UART_PORT "/dev/ttyAMA0"

// GPIO: to be refactored
#include <gpiod.h>
#include <iostream>
#include <thread>
#include <chrono>

#define GPIO_CHIP "gpiochip4"  // The GPIO controller name

#define PHASE_1_PIN 18
#define PHASE_2_PIN 23
#define PHASE_3_PIN 24


gpiod_chip *chip;
gpiod_line *line1;
gpiod_line *line2;
gpiod_line *line3;


    // // Blink the LED 10 times
    // for (int i = 0; i < 10; i++) {
    //     gpiod_line_set_value(line, 1); // LED ON
    //     std::this_thread::sleep_for(std::chrono::milliseconds(500));

    //     gpiod_line_set_value(line, 0); // LED OFF
    //     std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // }

    // Release the GPIO line





std::string TOPIC = "topic8";
std::string BROKER =
    "ssl://e800a45536b84764b7075bdc33165c5a.s1.eu.hivemq.cloud:8883";
std::string USERNAME = "hellomqtt";
std::string PASSWORD = "Hello@123";

class callback : public virtual mqtt::callback {
private:
    MainWindow* window = NULL;
public:
    callback(MainWindow* window){
        this->window = window;
    }
    void connection_lost(const std::string &cause) override {
        std::cout << "Connection lost: " << cause << std::endl;
    }

    void connected(const std::string &cause) override {
        std::cout << "Connected to broker with cause: " << cause << std::endl;
    }

    void message_arrived(mqtt::const_message_ptr msg) override {
        // gpiod_line_set_value(line, 1); // LED ON
        std::string command = msg->to_string();
        if(command=="PHASE1 ON") {
            gpiod_line_set_value(line1, 1);
        } else if(command == "PHASE2 ON") {
            gpiod_line_set_value(line2, 1);
        } else if(command == "PHASE3 ON"){
            gpiod_line_set_value(line3, 1);
        } else if(command == "PHASE1 OFF"){
            gpiod_line_set_value(line1, 0);
        } else if(command == "PHASE2 OFF"){
            gpiod_line_set_value(line2, 0);
        } else if(command == "PHASE3 OFF"){
            gpiod_line_set_value(line3, 0);
        } else if(command == "ALL ON"){
            gpiod_line_set_value(line1, 1);
            gpiod_line_set_value(line2, 1);
            gpiod_line_set_value(line3, 1);
        } else if(command == "ALL OFF"){
            gpiod_line_set_value(line1, 0);
            gpiod_line_set_value(line2, 0);
            gpiod_line_set_value(line3, 0);
        } else {
            std::cout << "Unknown command: " << command << std::endl;
        }

    }

    void delivery_complete(mqtt::delivery_token_ptr token) override {}
};


QJsonObject process_response(uint8_t response[], int device_id) {
    QJsonObject data;
    data["Device"] = device_id;
    data["Voltage"] = static_cast<float>((response[3] << 8 | response[4])) / 10.0;
    data["Current"] = static_cast<float>((response[5] << 8 | response[6] |
                                          response[7] << 24 | response[8] << 16)) / 1000.0;
    data["Power"] = static_cast<float>((response[9] << 8 | response[10] |
                                        response[11] << 24 | response[12] << 16)) / 10.0;
    data["Energy"] = static_cast<float>((response[13] << 8 | response[14] |
                                         response[15] << 24 | response[16] << 16)) / 1000.0;
    data["Frequency"] = static_cast<float>((response[17] << 8 | response[18])) / 10.0;
    data["PowerFactor"] = static_cast<float>((response[19] << 8 | response[20])) / 100.0;
    data["Alarms"] = static_cast<int>(response[21] << 8 | response[22]);

    return data;
}

int mqtt_main(MainWindow* window) {
    try {
        // GPIO: to be refactored
        // Open the GPIO chip
        chip = gpiod_chip_open_by_name(GPIO_CHIP);
        if (!chip) {
            std::cerr << "Failed to open GPIO chip\n";
            return 1;
        }

        // Get the GPIO line (pin)
        line1 = gpiod_chip_get_line(chip, PHASE_1_PIN);
        if (!line1) {
            std::cerr << "Failed to get GPIO line\n";
            gpiod_chip_close(chip);
            return 1;
        }

        line2 = gpiod_chip_get_line(chip, PHASE_2_PIN);
        if (!line2) {
            std::cerr << "Failed to get GPIO line\n";
            gpiod_chip_close(chip);
            return 1;
        }

        line3 = gpiod_chip_get_line(chip, PHASE_3_PIN);
        if (!line3) {
            std::cerr << "Failed to get GPIO line\n";
            gpiod_chip_close(chip);
            return 1;
        }

        // Request the line as output
        if (gpiod_line_request_output(line1, "led_blink1", 0) < 0) {
            std::cerr << "Failed to set GPIO line as output\n";
            gpiod_chip_close(chip);
            return 1;
        }
        if (gpiod_line_request_output(line2, "led_blink2", 0) < 0) {
            std::cerr << "Failed to set GPIO line as output\n";
            gpiod_chip_close(chip);
            return 1;
        }
        if (gpiod_line_request_output(line3, "led_blink3", 0) < 0) {
            std::cerr << "Failed to set GPIO line as output\n";
            gpiod_chip_close(chip);
            return 1;
        }



        // GPIO ends

        SerialHandler serialHandler;
//        mqtt::create_options create_opts(MQTTVERSION_5);
        mqtt::async_client cli(BROKER, "mqtt_ssl_client");

        callback cb(window);
        cli.set_callback(cb);

        auto ssl_opts =
            mqtt::ssl_options_builder().enable_server_cert_auth(true).finalize();

        mqtt::connect_options conn_opts;
        conn_opts.set_user_name(USERNAME);
        conn_opts.set_password(PASSWORD);
        conn_opts.set_ssl(ssl_opts);
        conn_opts.set_keep_alive_interval(20);
        conn_opts.set_clean_session(true);
        conn_opts.set_connect_timeout(20); // Increased timeout to 20 seconds

        std::cout << "Connecting to broker: " << BROKER << std::endl;
        mqtt::token_ptr conntok = cli.connect(conn_opts);
        conntok->wait();
        std::cout << "Connected. Subscribing to topic: " << "neoway" << std::endl;
        cli.subscribe("neoway", 2);

        std::string message = "Hello, HiveMQ SSL!";
        auto msg = mqtt::make_message("topic8", "Hello HiveMQ", 0, false);
        std::cout << "Publishing message: " << message << std::endl;
        cli.publish(msg)->wait_for(100);
        // if (serialHandler.openPort("/dev/ttyAMA0", QSerialPort::Baud9600)) {
        //     qDebug() << "Port opened successfully";
        // }
        // std::vector<std::vector<uint8_t>> commands = {
        //     {0x01, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x70, 0x0D},
        //     {0x02, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x70, 0x3E},
        //     {0x03, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x71, 0xEF}
        // };

        // std::vector<QByteArray> convertedCommands;

        // for (const auto& command : commands) {
        //     QByteArray qCommand(reinterpret_cast<const char*>(command.data()), command.size());
        //     convertedCommands.push_back(qCommand);
        // }


        int uart_fd = open(UART_PORT, O_RDWR | O_NOCTTY | O_NDELAY); // Open UART
        if (uart_fd == -1) {
            std::cerr << "Failed to open UART port\n";
            return 1;
        }
        std::cout << "UART Port Opened Successfully!\n";

        // Configure UART settings
        struct termios options;
        tcgetattr(uart_fd, &options);
        cfsetispeed(&options, B9600); // Set baud rate to 9600
        cfsetospeed(&options, B9600);
        options.c_cflag = CS8 | CREAD | CLOCAL; // 8-bit, enable receiver, local mode
        options.c_iflag = IGNPAR;  // Ignore parity errors
        options.c_oflag = 0;
        options.c_lflag = 0;
        tcflush(uart_fd, TCIFLUSH);
        tcsetattr(uart_fd, TCSANOW, &options);

        const unsigned char commands[3][8] = {
            {0x01, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x70, 0x0D},
            {0x02, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x70, 0x3E},
            {0x03, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x71, 0xEF}
        };
        // Receive data (non-blocking)
        std::vector<std::string> msgs(3);
        while (true) {
            //serialHandler.sendData("Hello Serial Port!");

            // for(auto command : commands){
            //     serialHandler.sendData(QByteArray(reinterpret_cast<const char*>(command), 8));
            // }
            // for (const auto& command : convertedCommands) {
            //     serialHandler.sendData(command);
            // }
            // // Connect signals
            // window->connect(&serialHandler, &SerialHandler::dataReceived,
            //                 [](const QByteArray &data) {
            //                     qDebug() << "Received data:" << data;
            //                 });

            // window->connect(&serialHandler, &SerialHandler::errorOccurred,
            //                 [](const QString &error) {
            //                     qDebug() << "Error:" << error;
            //                 });

            for(int i = 0; i < 3; i++){
                int bytes_written = write(uart_fd, commands[i], 8);
                if (bytes_written < 0) {
                    std::cerr << "UART Write Error!\n";
                } else {
                    std::cout << "Sent: " << msg;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                uint8_t buffer[25];
                int bytesToRead = 25;
                int bytes_read = read(uart_fd, buffer, bytesToRead);
                if (bytes_read == 25) {
                    // buffer[bytes_read] = '\0'; // Null-terminate string
                    QJsonObject jsonObj = process_response(buffer, i+1);
                    QJsonDocument doc(jsonObj);
                    QString jsonString = doc.toJson(QJsonDocument::Compact);

                    std::string stdJSON = jsonString.toStdString();
                    std::cout << stdJSON << std::endl;
                    msgs[i] = stdJSON;

                    auto msg1 = mqtt::make_message("topic8", stdJSON, 0, false);
                    cli.publish(msg1)->wait_for(100);
                } else {
                    std::cout << "No data received.\n";
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(490));
            }

            if(window->chartPageCreated){
                window->updateCharts(msgs);
            }
            // Send some data
            // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        close(uart_fd); // Close UART
        gpiod_line_release(line1);
        gpiod_line_release(line2);
        gpiod_line_release(line3);
        gpiod_chip_close(chip);
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
