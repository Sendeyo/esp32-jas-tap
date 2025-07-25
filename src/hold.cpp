// #include <HardwareSerial.h>

// HardwareSerial sim800(2); // Using Serial2: RX=16, TX=17

// void sendAT(String command, int waitMs = 1000) {
//   sim800.println(command);
//   delay(waitMs);
//   while (sim800.available()) {
//     Serial.write(sim800.read());
//   }
// }

// void setup() {
//   Serial.begin(115200);                    // Debug monitor
//   sim800.begin(9600, SERIAL_8N1, 16, 17);  // SIM800L RX, TX

//   Serial.println("Initializing SIM800L...");

//   delay(1000);
//   sendAT("AT");                   // Check module
//   sendAT("AT+CSQ");               // Signal quality
//   sendAT("AT+CREG?");             // Network registration
//   sendAT("AT+CGATT?");            // Attach to GPRS
//   sendAT("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");         // Bearer profile
//   sendAT("AT+SAPBR=3,1,\"APN\",\"internet\"");         // Set APN — change as needed
//   sendAT("AT+SAPBR=1,1", 3000);   // Open bearer
//   sendAT("AT+SAPBR=2,1");         // Query bearer

//   Serial.println("Sending GET request to google.com...");
//   sendAT("AT+HTTPINIT");                                     // Init HTTP
//   sendAT("AT+HTTPPARA=\"CID\",1");                           // Use profile 1
//   sendAT("AT+HTTPPARA=\"URL\",\"http://35.202.151.232:8000/\"");
//   sendAT("AT+HTTPACTION=0", 5000);                           // GET request
//   sendAT("AT+HTTPREAD", 5000);                               // Read the content
//   sendAT("AT+HTTPTERM");                                     // End HTTP
// }

// void loop() {
//   // Empty loop
// }




// // 