#include <LiquidCrystal_I2C.h>  // LCD-Bibliothek
#include <Wire.h>               // LCD-Bibliothek
#include <require_cpp11.h>      // RFID-Bibliothek
#include <MFRC522.h>            // RFID-Bibliothek
#include <deprecated.h>         // RFID-Bibliothek
#include <MFRC522Extended.h>    // RFID-Bibliothek
#include <SPI.h>                // RFID-Bibliothek

//Debugging | Gruß an Herrn Sülünkü fürs Erklären <3
#define DEBUG_ULTRASCHALL_ABSTAND   false
#define DEBUG_ULTRASCHALL_ON_OFF    false
#define DEBUG_IR_SENSOR_LINKS       false
#define DEBUG_IR_SENSOR_RECHTS      false
#define DEBUG_drive                false
#define DEBUG_MOTOREN               false






// Ultraschallsensorik
const int trigger = A3; // Port vom Trigger
const int echo = A2; // Port vom Echo

long laufzeit = 0; // Laufzeit deklarieren
long abstand = 0; // Abstand deklarieren
bool ultraschall = false; // 0 = Freie Fahrt; 1 = Stehen bleiben, Objekt zu nah


// Infrarotsensorik
const bool links = true;
const bool rechts = false;

int sensorLinks;
int sensorRechts;
int sensorLinksPin = A0;
int sensorRechtsPin = A1;


// Motorpins
const int motor_links_rueckwaerts = 2;
const int motor_links_vorwaerts = 3;
const int motor_rechts_rueckwaerts = 4;
const int motor_rechts_vorwaerts = 5;

// Richtungen
const int nachHinten  = 0b0011;
const int nachVorne   = 0b1100;
const int nachRechts  = 0b1001;
const int nachLinks   = 0b0110;
const int stoppen      = 0b0000;

// RFID Definitionen
const uint8_t RST_PIN = 9;
const uint8_t SS_PIN = 10;
uint8_t bufferSize = 255;
byte trailerBlock = 7;
MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

// LCD Definition
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup()
{
  // Sensoren
  pinMode(A0, INPUT);   // IR Sensor links
  pinMode(A1, INPUT);   // IR Sensor rechts

  pinMode(echo, INPUT); // Ultraschall Sensor
  pinMode(trigger, OUTPUT); // Ultraschall Sender

  // Motorpins schalten
  for (int i = 2; i < 6; i++) {
    pinMode(i, OUTPUT);
  }

  // Initialisierung für LCD
  lcd.init();
  lcd.begin(0x27, 16, 2);
  lcd.backlight();
  mfrc522.PCD_Init();

  // Serielle Schnittstelle konfigurieren
  SPI.begin();
  Serial.begin(9600);
}

void loop() {
  drive();
  scanRFID();
}
void scanRFID()
{
  mfrc522.PCD_Init();

  for (byte i = 0; i < MFRC522::MF_KEY_SIZE; i++) {
    key.keyByte[i] = 0xFF;
  }

  if (mfrc522.PICC_IsNewCardPresent()) {
    mfrc522.PICC_ReadCardSerial();

    // Authentifizierung mit Chip:
    mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));

    // Block zum auslesen festlegen:
    uint16_t blockAddress = 6; // Wert zwischen [0,63] gültig

    // Array mit 16 Slots deklarieren:
    char speicher_abfrage[16];

    // Block auslesen und in vorher deklariertem Array speichern:
    mfrc522.MIFARE_Read(blockAddress, speicher_abfrage, &bufferSize );
    Serial.println(speicher_abfrage);
    setLcdMsg(speicher_abfrage);
  }
}

void refreshData() {
  sensorLinks =  digitalRead(sensorLinksPin);
  sensorRechts = digitalRead(sensorRechtsPin);

  // Sensoren abfragen
  if (DEBUG_IR_SENSOR_RECHTS) {
    Serial.println(sensorRechts);
  }

  if (DEBUG_IR_SENSOR_LINKS) {
    Serial.println(sensorLinks);
  }

  // Ultraschallfunktion
  digitalWrite(trigger, LOW);
  delay(5);
  digitalWrite(trigger, HIGH);
  delay(10);
  digitalWrite(trigger, LOW);

  laufzeit = pulseIn(echo, HIGH, 2000);

  abstand = (laufzeit / 2.0) * 0.03432; // Abstand in cm umrechnen

  if (abstand <= 10 && abstand != 0) { // Wenn Abstand unter 10cm & größer 0cm
    ultraschall = true; // Hinderniss
  } else {
    ultraschall = false; // Kein Hinderniss
  }

  if (DEBUG_ULTRASCHALL_ABSTAND) {
    Serial.println(abstand);
  }

  if (DEBUG_ULTRASCHALL_ON_OFF) {
    Serial.println(ultraschall);
  }
}

void drive() {

  // Richtung Vorwärts
  while (!infrarotMessung(links) && !infrarotMessung(rechts) && !ultraschall) {
    motor(nachVorne);
    refreshData();
    if (DEBUG_drive) {
      Serial.println("Vorwaerts");
    }
  }

  // Richtung Rückwärts
  while (false) {
    motor(nachHinten);
    refreshData();
    if (DEBUG_drive) {
      Serial.println("Rueckwaerts");
    }
  }

  // Richtung Links
  while (infrarotMessung(links) && !infrarotMessung(rechts) && !ultraschall) {
    motor(nachLinks);
    refreshData();
    if (DEBUG_drive) {
      Serial.println("Links");
    }
  }

  // Richtung Rechts
  while (!infrarotMessung(links) && infrarotMessung(rechts) && !ultraschall) {
    motor(nachRechts);
    refreshData();
    if (DEBUG_drive) {
      Serial.println("Rechts");
    }
  }

  // Stoppen durch Infrarot-Meldung
  while (infrarotMessung(links) && infrarotMessung(rechts) && !ultraschall) {
    motor(stoppen);
    refreshData();
    if (DEBUG_drive) {
      Serial.println("Stopp durch IR");
    }
  }

  // Stoppen durch Ultraschall-Hinderniss
  while (ultraschall) {
    motor(stoppen);
    refreshData();
    if (DEBUG_drive) {
      Serial.println("Stopp durch US");
    }
  }
}

bool infrarotMessung(boolean linkerSensor) {

  if ((linkerSensor && sensorLinks != 0) || (!linkerSensor && sensorRechts != 0))
    return true;
  else
    return false;
}

// Methode zur Motorsteuerung
void motor(uint8_t value)
{
  if(DEBUG_MOTOREN){
    char string[100];
    sprintf(string, "Links Vorwärts: %d \t Rechts Vorwärts: %d \t Links Rückwärts: %d \t Rechts Rückwärts: %d", value & 0b1000 ? 1 : 0, value & 0b0100 ? 1 : 0, value & 0b0010 ? 1 : 0, value & 0b0001 ? 1 : 0 );
    Serial.println(string);
  }

  // Motorpins ansteuern
  if(value & 0b1000){
    digitalWrite(motor_links_vorwaerts, HIGH);
  } else {
    digitalWrite(motor_links_vorwaerts, 0);
  }
  
  if(value & 0b0100){
    digitalWrite(motor_rechts_vorwaerts, HIGH);
  } else {
    digitalWrite(motor_rechts_vorwaerts, 0);
  }
  
  if(value & 0b0010){
    digitalWrite(motor_links_rueckwaerts, HIGH);
  } else {
    digitalWrite(motor_links_rueckwaerts, 0);
  }
  
  if(value & 0b0001){
    digitalWrite(motor_rechts_rueckwaerts, HIGH);
  } else {
    digitalWrite(motor_rechts_rueckwaerts, 0);
  }
}


// LCD Methoden
void setLcdMsg(String firstLine) {
  firstLine = fillLCD(firstLine);
  lcd.setCursor(0, 0);
  lcd.print(firstLine);
  lcd.setCursor(0, 1);
  lcd.print(fillLCD(""));
}

void setLcdMsg(String firstLine, String secondLine) {
  firstLine = fillLCD(firstLine);
  secondLine = fillLCD(secondLine);

  lcd.setCursor(0, 0);
  lcd.print(firstLine);
  lcd.setCursor(0, 1);
  lcd.print(secondLine);
}

String fillLCD(String var) {
  if (var.length() < 16) {
    for (int i = var.length(); i < 16; i++)
    {
      var = var + " ";
    }
  }
  return var;
}
