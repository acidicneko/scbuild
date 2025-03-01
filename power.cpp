#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <modbus/modbus.h>
#include <sstream>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

#define PIN_ON 0
#define PIN_OFF 1

const std::string MQTT_BROKER =
    "ff8d007a082a46b1b696047efe7cb2d4.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;
const std::string MQTT_USERNAME = "test01";
const std::string MQTT_PASSWORD = "Test@123";
const std::string MQTT_TOPIC = "modbus/data";
const char *LTE_DEVICE = "/dev/myUSBLTE";
const int BAUD_RATE = 115200;

// Function prototypes
void publish_to_mqtt(int fd, const std::string &payload);
int configureSerialPort(int fd);
std::string sendATCommand(int fd, const std::string &command,
                          float wait_time = 2);
void reconnect_mqtt(int fd);
bool initialize_lte(int fd);
float convert_to_float(uint16_t low, uint16_t high);
void check_and_reconnect(int fd);

// Function to convert Modbus register values to float
float convert_to_float(uint16_t low, uint16_t high) {
  uint32_t combined = (static_cast<uint32_t>(high) << 16) | low;
  float result;
  std::memcpy(&result, &combined, sizeof(result));
  return result;
}

// Function to publish data to MQTT with reconnection logic
void publish_to_mqtt(int fd, const std::string &payload) {
  std::cout << "Publishing to MQTT: " << payload << std::endl;
  std::string response = sendATCommand(
      fd, "AT+MQTTPUB=1,1,\"" + MQTT_TOPIC + "\",\"" + payload + "\"", 1);

  if (response.find("ERROR") != std::string::npos) {
    std::cerr << "MQTT publish failed. Checking network..." << std::endl;
    check_and_reconnect(fd);
    std::cout << "Retrying MQTT publish..." << std::endl;
    sendATCommand(fd,
                  "AT+MQTTPUB=1,1,\"" + MQTT_TOPIC + "\",\"" + payload + "\"");
  }
}

// Function to check network and reconnect if needed
void check_and_reconnect(int fd) {
  std::string net_status = sendATCommand(fd, "AT+CGATT?");
  if (net_status.find("0") != std::string::npos) {
    std::cerr << "Network is disconnected. Reinitializing LTE..." << std::endl;
    initialize_lte(fd);
  }
  reconnect_mqtt(fd);
}

// Function to configure the serial port
int configureSerialPort(int fd) {
  struct termios tty;
  if (tcgetattr(fd, &tty) != 0) {
    std::cerr << "Error getting serial port attributes" << std::endl;
    return -1;
  }
  cfsetospeed(&tty, B115200);
  cfsetispeed(&tty, B115200);
  tty.c_cflag &= ~PARENB;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;
  tty.c_cflag &= ~CRTSCTS;
  tty.c_cflag |= CREAD | CLOCAL;
  tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);
  tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | BRKINT | IGNPAR);
  tty.c_oflag &= ~OPOST;
  tty.c_cc[VTIME] = 10;
  tty.c_cc[VMIN] = 0;
  return tcsetattr(fd, TCSANOW, &tty);
}

// Function to send AT commands and read responses
std::string sendATCommand(int fd, const std::string &command, float wait_time) {
  std::string response;
  std::string cmd = command + "\r\n";
  write(fd, cmd.c_str(), cmd.length());
  usleep(wait_time * 1000000);
  char buffer[256];
  memset(buffer, 0, sizeof(buffer));
  int n = read(fd, buffer, sizeof(buffer) - 1);
  response = (n > 0) ? std::string(buffer, n) : "";
  std::cout << "AT Command: " << command << " Response: " << response
            << std::endl;
  return response;
}

// Function to initialize LTE module
bool initialize_lte(int fd) {
  std::cout << "Initializing LTE module..." << std::endl;
  sendATCommand(fd, "AT");
  sendATCommand(fd, "AT");
  sendATCommand(fd, "AT+CGDCONT=1,\"IP\",\"airtelgprs.com\"");
  sendATCommand(fd, "AT+XIIC=1");
  // sendATCommand(fd, "AT+MQTTDISCONN");
  sendATCommand(fd, "AT+MQTTTLS=authmode,0");
  sendATCommand(fd, "AT+MQTTTLS=sslmode,1");
  sendATCommand(fd, "AT+MQTTTLS=sslversion,0");
  sendATCommand(fd, "AT+MQTTCONNPARAM=\"meterdata\",\"test01\",\"Test@123\"");
  std::cout << "LTE module initialized." << std::endl;
  return true;
}

// Function to connect to MQTT broker with retries
void reconnect_mqtt(int fd) {
  std::cout
      << "Reinitializing LTE and attempting to reconnect to MQTT broker..."
      << std::endl;
  initialize_lte(fd);

  int retry_count = 0;
  while (true) {
    // Check if LTE is attached to the network
    std::string net_status = sendATCommand(fd, "AT+CGATT?");
    if (net_status.find("0") != std::string::npos) {
      std::cerr << "Network is disconnected. Reinitializing LTE..."
                << std::endl;
      initialize_lte(fd);
    }

    // Check signal strength (optional but helpful)
    std::string signal_strength = sendATCommand(fd, "AT+CSQ");
    std::cout << "Signal Strength: " << signal_strength << std::endl;

    // Attempt MQTT connection
    std::string response =
        sendATCommand(fd, "AT+MQTTCONN=\"" + MQTT_BROKER + ":8883\",0,60", 15);
    if (response.find("OK") != std::string::npos) {
      std::cout << "Successfully connected to MQTT broker!" << std::endl;
      break;
    }

    std::cerr << "MQTT connection failed. Retrying..." << std::endl;
    retry_count++;

    // If multiple failures occur, restart LTE module
    if (retry_count >= 5) {
      std::cerr << "Multiple MQTT failures. Restarting LTE module..."
                << std::endl;
      initialize_lte(fd);
      retry_count = 0; // Reset retry count
    }

    sleep(10); // Use exponential backoff (increase delay if failures persist)
  }
}

void msg_read() {}

int main() {

  auto mqtt_worker = [&]() { msg_read(); };

  std::vector<std::thread> threads;
  // threads.emplace_back(std::thread(mqtt_worker));

  // Modbus Configuration
  const char *device = "/dev/modbusUSB";
  modbus_t *ctx = modbus_new_rtu(device, 9600, 'N', 8, 1);
  if (!ctx || modbus_connect(ctx) == -1) {
    std::cerr << "Modbus connection failed" << std::endl;
    return -1;
  }
  modbus_set_slave(ctx, 1);
  std::cout << "Modbus connection established." << std::endl;
  std::cout << "Starting Modbus-LTE MQTT Integration..." << std::endl;

  int fd = open(LTE_DEVICE, O_RDWR | O_NOCTTY | O_NDELAY);
  if (fd == -1 || configureSerialPort(fd) == -1) {
    std::cerr << "Failed to initialize LTE module" << std::endl;
    return -1;
  }
  initialize_lte(fd);
  reconnect_mqtt(fd);
  uint16_t tab_reg[30];
  while (true) {
    std::cout << "Reading Modbus data..." << std::endl;
    if (modbus_read_input_registers(ctx, 0x00, 30, tab_reg) > 0) {
      float v1 = convert_to_float(tab_reg[0], tab_reg[1]);
      float v2 = convert_to_float(tab_reg[2], tab_reg[3]);
      float v3 = convert_to_float(tab_reg[4], tab_reg[5]);
      float c1 = convert_to_float(tab_reg[16], tab_reg[17]);
      float c2 = convert_to_float(tab_reg[18], tab_reg[19]);
      float c3 = convert_to_float(tab_reg[20], tab_reg[21]);
      float p1 = convert_to_float(tab_reg[24], tab_reg[25]);
      float p2 = convert_to_float(tab_reg[26], tab_reg[27]);
      float p3 = convert_to_float(tab_reg[28], tab_reg[29]);

      std::ostringstream payload;
      payload << " v1: " << v1 << " v2: " << v2 << " v3: " << v3
              << " c1: " << c1 << " c2: " << c2 << " c3: " << c3
              << " p1: " << p1 << " p2: " << p2;
      publish_to_mqtt(fd, payload.str());
    } else {
      std::cerr << "Failed to read Modbus registers." << std::endl;
    }

    // std::ostringstream payload;
    // payload << "test data";
    // publish_to_mqtt(fd, payload.str());
    sleep(0.1);
  }
  close(fd);
  return 0;
}
