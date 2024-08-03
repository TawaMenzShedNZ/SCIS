 /* 
  *  Board: NodeMCU 1.0 (ESP-12E Module)
  *  
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
 */

 /* Version 1 - feb 2024 */

#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <ESP8266WiFi.h>

IPAddress server(192,168,0,105);

#define RST_PIN         D3          // Configurable, see typical pin layout above
#define SS_PIN          D8         // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance
MFRC522::MIFARE_Key key;

/* Define variables */
const int machine_id = 1; // unique id of machine
const int ledGreen = D2;
const int ledRed = D4;
const int buzzer = D1;

int counter;

/* WiFi variables */
//Menzshed Router
const char ssid[]     = "";
const char password[] = "";
const uint16_t port = 80;

boolean loggedOn = false; //Logged on status
int signin_response;

String cardUID;

void setup() {
  Serial.begin(115200);
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();         // Init MFRC522 card
  
  //initialise LEDs
  pinMode(ledRed, OUTPUT);
  pinMode(ledGreen, OUTPUT);

  // initialise buzzer
  pinMode(buzzer, OUTPUT); // Set buzzer - pin 7 as an output

// We start by connecting to a WiFi network

  Serial.println();
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
  
  delay(10);
  ledGreenOn();
  Serial.println("Setup complete");
}

void loop() {

  RfidScan();

}

void RfidScan(){
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  Serial.println("RFID Card present");
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print(F("\nPICC type: "));
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.println(mfrc522.PICC_GetTypeName(piccType));

  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
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

  //send tag to server
  signin_response = webCall("reception", cardUID);
  Serial.println(signin_response);
  
  //handle response
  if (signin_response == 0){
    //member signed in
    signOn();
  }
  else if (signin_response == 1) {
    //member signed out
    signOff();
  }
  else {
    //member not registered
    Serial.println("\nMember not Registered");
    signalError();
    ledGreenOn();
  }
  delay (1000);
  
  // Halt PICC
  mfrc522.PICC_HaltA();

  // Stop encryption on PCD
  mfrc522.PCD_StopCrypto1();
}

// Log on procedure of machine
void signOn(){
  //make successful sound
  tone(buzzer, 1000);
  //LED off
  ledGreenOff();
  delay(100);
  
  tone(buzzer, 1250);
  //Green LED on
  ledGreenOn(); 
  delay(100);
  
  tone(buzzer, 1500);
  ledGreenOff();
  delay(100);
  noTone(buzzer);
  delay(300);
  //Green LED on
  ledGreenOn();
  
  Serial.println("\nSign on");
}

//Sign off procedure
void signOff(){
  tone(buzzer, 1000);
  ledGreenOff();
  delay(250);
  
  tone(buzzer, 750);
  ledGreenOn();
  delay(250);
  
  tone(buzzer, 500);
  ledGreenOff();
  delay(250);
  
  noTone(buzzer);
  //Green LED on
  ledGreenOn();
 
  Serial.println("\nSign off");
}

/* Web Client procedures */
/*returns true if the member is allowed to use the machine */
int webCall(String page, String tag_id){
  int result = -1;

  Serial.print("connecting to ");
  Serial.print(server);
  Serial.print(':');
  Serial.println(port);

  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  if (!client.connect(server, port)) {
    Serial.println("connection failed");
    delay(5000);
    return -1;
  }

  // This will send a string to the server
  Serial.println("sending data to server");
  if (client.connected()) {
    //encode url
    String url = "/menzshed/api/console/" + page + ".php?tag=" + urlencode(tag_id);
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
      return -1;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  Serial.println("receiving from remote server");
  while (client.available()) {
    char answer = static_cast<char>(client.read());
    //Serial.println(answer);
    //result = answer - 48;
    if (answer=='0'){
      result=0;
    }
    else if (answer=='1'){
      result=1;
    }
    else {
      result=-1;
    }
    Serial.print("Result:");
    Serial.println(result);
  }

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
    char code2;
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
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
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
  ledGreenOff();
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
  delay(300);
}

void ledGreenOn(){
  //turn on Green LED
  digitalWrite(ledGreen, HIGH);
}
void ledGreenOff(){
  //turn off Green LED
  digitalWrite(ledGreen, LOW);
}


void ledRedOn(){
  //turn on Red LED
  digitalWrite(ledRed, HIGH);
}
void ledRedOff(){
  //turn on Red LED
  digitalWrite(ledRed, LOW);
}

//Buzzer procedures
void buzzerSuccessfull(){
  tone(buzzer, 1000);
  delay(100);
  tone(buzzer, 1250);
  delay(100);
  tone(buzzer, 1500);
  delay(100);
  noTone(buzzer);
}

void buzzerLogOff(){
  tone(buzzer, 1000);
  delay(250);
  tone(buzzer, 750);
  delay(250);
  tone(buzzer, 500);
  delay(250);
  noTone(buzzer);
}

void signalError(){
  ledGreenOff();
  tone(buzzer, 400, 250);
  ledRedOn();
  delay(500);

  tone(buzzer, 250);
  ledRedOff();
  delay(300);
  ledRedOn();
  delay(200);
  
  delay(1000);
  ledRedOff();
  noTone(buzzer);
}
