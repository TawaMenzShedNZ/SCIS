 /* 
 *  Board: NodeMCU 1.0 (ESP-12E Module)
 *  Program Name: machine_box_WWN_High

 *  Typical pin layout used:
 * ------------------------------------------------
 *             MFRC522      Arduino       Arduino   
 *             Reader/PCD   Uno/101       Mini     
 * Signal      Pin          Pin           Pin       
 * ------------------------------------------------
 * RST/Reset   RST          9             D3        
 * SPI SS      SDA(SS)      10            D8        
 * SPI MOSI    MOSI         11 / ICSP-4   D7        
 * SPI MISO    MISO         12 / ICSP-1   D6       
 * SPI SCK     SCK          13 / ICSP-3   D5
 * Buzzer      Blue                       D1
 * LED Green                              D2
 * LED Red                                D4
 * Relay                                  D0
 */

/* Version Control
* ----------------
* 5.2 - 5 Dec 2023 - Wait period after successful swipe on and off
* 5.3 - 27 Jan 2024 - Long ID (WWN) for registration 
* 5.3.1 - 22 Jun 2024 - Relay signal for activation of HIGH
*                     - swap initial signal and output setting
*/
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <ESP8266WiFi.h>

//Menzshed ip address
IPAddress server(192,168,0,105);//address of mapped IP address

//Home IP Address
//IPAddress server(192,168,0,111);//address of mapped IP address

#define RST_PIN         D3          // Configurable, see typical pin layout above
#define SS_PIN          D8         // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance
MFRC522::MIFARE_Key key;

/* Define variables */
const String machine_id = "7"; // unique id of machine
String str_machine_wwn = "";

/* assign ports to component */
const int ledGreen = D2;
const int ledRed = D4;
//const int buzzer = D1;
const int relay = D0;
int counter;

/* WiFi variables */
const char ssid[]     = "insert_ssid_here";
const char password[] = "password";
const uint16_t port = 80;

boolean mach_in_use = false; //Machine in use status

unsigned long delayTime = 1800000; // 30 minutes in milliseconds

unsigned long timerMarker = 0;
unsigned long readerMarker = 0;

String cardUID;

void setup() {
  Serial.begin(115200);
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();         // Init MFRC522 card

  //initialise LEDs
  pinMode(ledRed, OUTPUT);
  pinMode(ledGreen, OUTPUT);

  // initialise buzzer
 // pinMode(buzzer, OUTPUT); // Set buzzer - pin 7 as an output
  
  //initialise Relay - sequence swapped
  digitalWrite(relay, LOW);
  pinMode(relay, OUTPUT);
  
//initiate the timer marker
  timerMarker = millis();
  readerMarker = millis();
  
// We start by connecting to a WiFi network

  Serial.println();
  Serial.println("Version: WWN_High");
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    flash();
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Gather wwn machine_wwn
  byte myMac[6];
  WiFi.macAddress(myMac);
  // Loop around appending to strFull
  for (byte intX = 0; intX < 6; intX++) {
    // strWork=String(myMac[intX], HEX);
    // strFull.concat(strWork);
    // append 0 if only single digit
    if (myMac[intX] < 17)
    { str_machine_wwn+="0";}
        str_machine_wwn += String(myMac[intX], HEX);
     // Serial.printf("MYMAC:%i %i\n", intX, myMac[intX]);
    // Serial.printf("MYMAC:%i :%s:\n", intX, strWork);
     
    // Serial.printf("MYMAC_prog: %s\n", strFull);
  }
  Serial.printf("str_machine_wwn:");
  Serial.println(str_machine_wwn);
  Serial.printf("\n");
  //Ensure machine is logged out and ready for use
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  Serial.println("Closing off machine " + str_machine_wwn);
  client.connect(server, port);
  if (client.connected()) {
    //encode url
    String url = "/menzshed/api/machine/close_machine_long.php?machine_wwn=" + str_machine_wwn;
    Serial.println(url);
    client.println("GET " + url);
  }
  delay(100);
  ledRedOn();
  Serial.println("Setup complete for machine " + str_machine_wwn);
}


void loop() {

  RfidScan();
  //Green LED On (static) when in use
  if (mach_in_use){
    ledGreenOn();
  }
  //Red LED on when ready
  else {ledRedOn();}

  if ((millis()-timerMarker > delayTime) and mach_in_use){
    //force log off after 30 mins
    Serial.println("Forcing logoff");
    logOff("0");
  }

  //restart reader - this fixes the issue that it becomes unresponsive after a while
  if (millis()-readerMarker > 30000){
    Serial.println("Forcing erader reset");
    mfrc522.PCD_SoftPowerDown();
    delay(10);
    mfrc522.PCD_SoftPowerUp();
    mfrc522.PCD_Init();
    delay(4);
    readerMarker = millis();
  }
}

void RfidScan(){
    // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    Serial.println("Card detected");
    return;
  }

  Serial.print(F("\nPICC type: "));
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.println(mfrc522.PICC_GetTypeName(piccType));

  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your q                                                                                         / is not of type MIFARE Classic."));
    return;
  }
  
  Serial.println(F("**Card Detected:**"));
  //printHex(mfrc522.uid.uidByte, mfrc522.uid.size);

  //save UID value
  cardUID = String(mfrc522.uid.uidByte[0],HEX) + " " + String(mfrc522.uid.uidByte[1],HEX) + " " + String(mfrc522.uid.uidByte[2],HEX) + " " + String(mfrc522.uid.uidByte[3],HEX);
  //upper case string
  cardUID.toUpperCase();
  //print Card UID
  Serial.println("\nCard UID: " + cardUID);

  //check if already logged on to machine
  if (!mach_in_use){
      //check member status against database
      if(webCallLong("check_long",cardUID)){
        //member is allowed to use the machine
        Serial.println("\nAccess allowed");
        logOn(cardUID);
      }
      else{
        //member is not allowed to use the machine
        Serial.println("\nMachine Access Denied");
        buzzerFailed();
      }
  }
  else{
    //machine is already in use on so can be logged off
    // will disable use of machine if user forgot to sign off and another comes along and tries to sign on
    logOff(cardUID);
  }
  
  // Wait so card can be removed- 1 second
  delay (1000);
  
  // Halt PICC
  mfrc522.PICC_HaltA();

  // Stop encryption on PCD
  mfrc522.PCD_StopCrypto1();
}

// Log on procedure of machine
void logOn(String tag_id){
  //set logged on flag
  mach_in_use = true;
  
  // add machine details to machine log table
  webCallLong("log_machine_long", tag_id);
  
  //open relay
  Serial.println("\nOpening Relay....");
  digitalWrite(relay, HIGH);

  //make successful sound
  //buzzerSuccessfull();
  //Turn Green LED On
  ledGreenOn();

  // reset timer
  timerMarker = millis();
  
  Serial.println("\nLogged on");
}

//Log off procedure
void logOff(String tag_id){
  //Log off
  mach_in_use = false;
  //close relay
  digitalWrite(relay, LOW);

  // log machine off in machine log table
  webCallLong("log_machine_long", tag_id);
  
  ledRedOn();
 
  Serial.println("\nLogged off");
}

/* Web Client procedures */
/*returns true if the member is allowed to use the machine */
boolean webCall(String page, String tag_id){
  boolean result = false;



  Serial.setTimeout(100);
  Serial.print("connecting to ");
  Serial.print(server);
  Serial.print(':');
  Serial.println(port);

  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  if (!client.connect(server, port)) {
    Serial.println("connection failed");
    delay(5000);
    return false;
  }

  // This will send a string to the server
  Serial.println("sending data to server");
  if (client.connected()) {
    //encode url
    // String url = "/menzshed/api/machine/" + page + ".php?tag=" + urlencode(tag_id) + "&machinewwn=" + str_machine_wwn;
    String url = "/menzshed/api/machine/" + page + ".php?tag=" + urlencode(tag_id) + "&machine_id=" + machine_id;
    Serial.println(url);
    client.println("GET " + url);
  }

  // wait for data to be available
  unsigned long timeout = millis();
    while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      delay(1000);
      return false;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  Serial.println("receiving from remote server");
 // String result=client.readString();
 // Serial.print("string received:%s\n", result);

  while (client.available()) {
     char answer = static_cast<char>(client.read());
     Serial.print(answer);
     if (answer == '1'){
      result = true;
     }
  }

  // Close the connection
  Serial.println();
  Serial.println("closing connection");
  client.stop();
  
  return result;
}

boolean webCallLong(String page, String tag_id){
  boolean result = false;
  Serial.setTimeout(100);
  Serial.print("connecting to ");
  Serial.print(server);
  Serial.print(':');
  Serial.println(port);

  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  if (!client.connect(server, port)) {
    Serial.println("connection failed");
    delay(5000);
    return false;
  }

  // This will send a string to the server
  Serial.println("sending data to server");
  if (client.connected()) {
    //encode url
    // String url = "/menzshed/api/machine/" + page + ".php?tag=" + urlencode(tag_id) + "&machine_wwn=" + str_machine_wwn;
    String url = "/menzshed/api/machine/" + page + ".php?tag=" + urlencode(tag_id) + "&machine_wwn="+ str_machine_wwn;
    Serial.println(url);
    client.println("GET " + url);
  }

  // wait for data to be available
  unsigned long timeout = millis();
  // unsigned long timeout = micros();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      delay(1000);
      return false;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  Serial.println("receiving from remote server");

   // trial 1 - readsting
  // String resultBack=client.readString();
 // delay(100);
 
  // trial 2 - serial.read
  //String resultBack="";
  //while (Serial.available()) {
  //  char c = Serial.read();  //gets one byte from serial buffer
  //  resultBack += c; //makes the String readString
  //  delay(2);  //slow looping to allow buffer to fill with next character
  //}

  // trial 3 
  //     String resultBack[20];
  //      unsigned long timeout2 = millis() + 1000;
  //      uint8_t inIndex = 0;
  //      while ( ((int32_t)(millis() - timeout2) < 0) && (inIndex < 2)) {
  //          if (Serial.available() > 0) {
  //              // read the incoming byte:
  //              // resultBack[inIndex] = Serial.read();
  //              char c = Serial.read();
  //              if ((c == '\n') || (c == '\r')) {
  //                  break;
  //              }
  //              printf(".%c", c);
  //              resultBack[inIndex++]=c;
  //              Serial.write(c);
  //          }
  //      }

  // trial 4 - readstring with timeout
  //  String resultBack=client.readString();
  // delay(100);
 
 // trial 5 - use read but read from "client."" not "Serial."
     String resultBack[20];
     unsigned long timeout2 = millis() + 1000;
     uint8_t inIndex = 0;
     while ( ((int32_t)(millis() - timeout2) < 0) && (inIndex < 2)) {
         if (client.available() > 0) {
                // read the incoming byte:
               // resultBack[inIndex] = Serial.read();
              char c = client.read();
              if ((c == '\n') || (c == '\r')) {
                   break;
              }
              printf(".%c", c);
              resultBack[inIndex++]=c;
              Serial.write(c);
            }
        }
 
  Serial.printf("string received:");
  Serial.printf("%s", resultBack);
//  Serial.printf("%s", inData);
  Serial.printf(":\n");
   
  // Send back first character
  if ( resultBack[0] == "1")
  {
   result = true;
  } else
  { result = false;
  }
 // while (client.available()) {
 //    char answer = static_cast<char>(client.read());
 //    Serial.print(answer);
 //   if (answer == '1'){
 //     result = true;
 //    }
 // }
  
  // Close the connection
  Serial.println();
  Serial.println("closing connection");
  client.stop();
  
  return result;
}
// URL encoding procedure
String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        
      }
      yield();
    }
    return encodedString;
}

/**
 * Helper routine to dump a byte array as hex values to Serial.
 */
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

// LED procedures
void flash(){
  //Red light on
  digitalWrite(ledRed, HIGH);
  delay(300);
  //Red light off
  digitalWrite(ledRed, LOW);
  //Green light on
  digitalWrite(ledGreen, HIGH);
  delay(300);
  //Green light off
  digitalWrite(ledGreen, LOW);
}

void ledGreenOn(){
  //turn off Red LED
  digitalWrite(ledRed, LOW);
  //turn on Green LED
  digitalWrite(ledGreen, HIGH);
}

void ledGreenFlash(){
  static unsigned long lastBlinkTime = 0;
  if (millis() - lastBlinkTime >= 500UL)
  {
    digitalWrite(ledGreen, !digitalRead(ledGreen)); //toggles the led
    lastBlinkTime = millis();
  }
}

void ledRedOn(){
  //turn off Green LED
  digitalWrite(ledGreen, LOW);
  //turn on Red LED
  digitalWrite(ledRed, HIGH);
}

//Buzzer procedures
void buzzerFailed(){
  //sound buzzer and flash red LED
  //tone(buzzer, 400, 250);
  digitalWrite(ledGreen, LOW);
  digitalWrite(ledRed, HIGH);
  delay(100);
  digitalWrite(ledRed, LOW);
  delay(100);
  digitalWrite(ledRed, HIGH);
  delay(100);
  digitalWrite(ledRed, LOW);
  
  //tone(buzzer, 250);
  delay(100);
  digitalWrite(ledRed, HIGH);
  delay(100);
  digitalWrite(ledRed, LOW);
  delay(100);
  digitalWrite(ledRed, HIGH);
  delay(100);
  digitalWrite(ledRed, LOW);
  delay(100);
  digitalWrite(ledRed, HIGH);
  //noTone(buzzer);
}
