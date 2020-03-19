#include <LiquidCrystal_I2C.h>  // LCD-Bibliothek
#include <Wire.h>               // LCD-Bibliothek
#include <require_cpp11.h>      // RFID-Bibliothek
#include <MFRC522.h>            // RFID-Bibliothek
#include <deprecated.h>         // RFID-Bibliothek
#include <MFRC522Extended.h>    // RFID-Bibliothek
#include <SPI.h>                // RFID-Bibliothek

//Debug ausgaben
#define DEBUG_ULTRASCHALL_ABSTAND   false
#define DEBUG_ULTRASCHALL_ON_OFF    false
#define DEBUG_IR_SENSOR_LINKS       false
#define DEBUG_IR_SENSOR_RECHTS      false
#define DEBUG_FAHREN                false
#define DEBUG_MOTOREN               false


// Ultraschallsensor Einstellungen
const int trigger = A3; // Port vom Trigger
const int echo = A2; // Port vom Echo
long laufzeit = 0; // Laufzeit deklarieren
long abstand = 0; // Abstand deklarieren
bool ultraschall = false; // 0 = Freie Fahrt; 1 = Stehen bleiben, Objekt zu nah

// Infrarotsensor Einstellungen
int sensorLinks;
int sensorRechts;
int sensorLinksPin = A0;
int sensorRechtsPin = A1;
const bool links = true; 
const bool rechts = false;

// Motor Einstellungen

//mlh = links nach hinten
//mlv = links nach vorne
//mrh = rechts nach hinten
//mrv = rechts nach vorne
    
const int mlh = 2;  // A-1A
const int mlv = 3;  // A-1B
const int mrh = 4;  // B-1A
const int mrv = 5;  // B-1B

const int nachHinten  = 0b0011;
const int nachVorne   = 0b1100;
const int nachRechts  = 0b1001;
const int nachLinks   = 0b0110;
const int nichts      = 0b0000; //stehen bleiben 

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// RFID
const uint8_t RST_PIN = 9; // Resetpin
const uint8_t SS_PIN = 10; // serieller Datenpin
uint8_t bufferSize = 255; //Buffergröße: Kommandobyte+Adressbyte+16 Bytes Daten
byte trailerBlock = 7; // Verwendeter Trailerblock
MFRC522 mfrc522(SS_PIN, RST_PIN); // erzeugt MFRC522 Instanz
MFRC522::MIFARE_Key key; // access key


void setup()
{
  //ULTRASCHALL
  pinMode(trigger, OUTPUT); // Trigger als OUTPUT
  pinMode(echo, INPUT); // Echo als INPUT

  pinMode(mlh, OUTPUT); // Motor A 1
  pinMode(mlv, OUTPUT); // Motor A 2
  pinMode(mrh, OUTPUT); // Motor B 1
  pinMode(mrv, OUTPUT); // Motor B 2

  pinMode(A0, INPUT);   // Linker Sensor
  pinMode(A1, INPUT);   // Rechter Sensor

  lcd.init();
  lcd.begin(0x27, 16, 2);
  lcd.backlight();

  SPI.begin();        // startet den SPI bus (RFID)
  mfrc522.PCD_Init(); // initialisiert RFID Modul
  
  Serial.begin(9600);


}

void loop(){
  
    fahren();
    rfidLesen();
    
}
void rfidLesen()
{
  mfrc522.PCD_Init(); // initialisiert RFID Modul
  
  for (byte i = 0; i < MFRC522::MF_KEY_SIZE; i++)
  {
    key.keyByte[i] = 0xFF;
  }
  
  if (mfrc522.PICC_IsNewCardPresent())
  {
      mfrc522.PICC_ReadCardSerial();
      // Authentifizierung:
      mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));

      //Auszulesenden Block festlegen:
      uint16_t blockAddress = 6; // Wert zwischen [0,63]

      // 16 Byte großen Datentyp anlegen:
      char text[16];

      // Angegebenen Block auslesen und in "text" speichern:
      mfrc522.MIFARE_Read(blockAddress, text, &bufferSize );
      Serial.println(text);
      setLcdMsg(text);
   } 
}
void datenErneuern()
{
  sensorLinks =  digitalRead(sensorLinksPin);
  sensorRechts = digitalRead(sensorRechtsPin);
  
  if(DEBUG_IR_SENSOR_LINKS)
    Serial.println(sensorLinks);
  if(DEBUG_IR_SENSOR_RECHTS)
    Serial.println(sensorRechts);
  
  //ULTRASCHALL
  digitalWrite(trigger, LOW); // Trigger ausschalten
  delay(5);
  digitalWrite(trigger, HIGH); // Trigger einschalten
  delay(10);
  digitalWrite(trigger, LOW);// Trigger ausschalten

  laufzeit = pulseIn(echo, HIGH, 2000); // Laufzeit = Echo pulse Input
  
  abstand = (laufzeit / 2.0) * 0.03432; // Abstand berechnen
  if (abstand <= 10 && abstand != 0) // Wenn Abstand zwischen 1-10
  {
    ultraschall = true; // true = Hinderniss erkannt
  } else {
    ultraschall = false; // false = freie Bahn
  }
  if(DEBUG_ULTRASCHALL_ABSTAND)
    Serial.println(abstand);
  if(DEBUG_ULTRASCHALL_ON_OFF)
    Serial.println(ultraschall);
}
void fahren() {
  
  while (!infrarotSchwarz(links) && !infrarotSchwarz(rechts) && !ultraschall) {
    motor(nachVorne);
    datenErneuern();
    if(DEBUG_FAHREN)
      Serial.println("Nach vorne");
  }
  
  while (infrarotSchwarz(links) && !infrarotSchwarz(rechts) && !ultraschall) {
    motor(nachLinks);
    datenErneuern();
    if(DEBUG_FAHREN)
      Serial.println("Nach links");
  }
  
  while (!infrarotSchwarz(links) && infrarotSchwarz(rechts) && !ultraschall) {
    motor(nachRechts);
    datenErneuern();
    if(DEBUG_FAHREN)
      Serial.println("Nach rechts");
  }
  
  while (infrarotSchwarz(links) && infrarotSchwarz(rechts) && !ultraschall) {
    motor(nichts);
    datenErneuern();
    if(DEBUG_FAHREN)
      Serial.println("Stehen bleiben, Infrarot.");
  }

  while (ultraschall) {
    motor(nichts);
    datenErneuern();
    if(DEBUG_FAHREN)
      Serial.println("Stehen bleiben, Ultraschall");
  }

  while (false) {
    motor(nachHinten);
    datenErneuern();
    if(DEBUG_FAHREN)
      Serial.println("Nach hinten");
  }  
}
bool infrarotSchwarz(boolean links){
  // Hier wird abgefragt ob der Sensor das Schwarze Band erkennt
  // Parameter links == true -> Linken Sensor abfragen
  // Parameter links == false -> Rechten Sensor abfragen

  if ((links && sensorLinks != 0) || (!links && sensorRechts != 0))
  {
    return true;
  }
  else
  {
    return false;
  }
  
}

void setLcdMsg(String firstLine)
{
  firstLine = fillLCD(firstLine);
  lcd.setCursor(0,0);
  lcd.print(firstLine);
  lcd.setCursor(0,1);
  lcd.print(fillLCD(""));
}

void setLcdMsg(String firstLine, String secondLine)
{
  firstLine = fillLCD(firstLine);
  secondLine = fillLCD(secondLine);
  
  lcd.setCursor(0,0);
  lcd.print(firstLine);
  lcd.setCursor(0,1);
  lcd.print(secondLine);
}
String fillLCD(String var)
{
  if(var.length() < 16){
    for(int i = var.length(); i < 16; i++)
    {
      var = var + " ";
    }
  }
  return var;
}
void motor(uint8_t value)
{
    if(DEBUG_MOTOREN)
    {
      char string[100];
      sprintf(string, "Links Vorwärts: %d \t Rechts Vorwärts: %d \t Links Rückwärts: %d \t Rechts Rückwärts: %d", value & 0b1000 ? 1 : 0,value & 0b0100 ? 1 : 0,value & 0b0010 ? 1 : 0,value & 0b0001 ? 1 : 0 );
      Serial.println(string);
    }
    
   // Hier werden die Motoren angesteuert und werden zum laufen gebracht
   if(value & 0b1000){
    digitalWrite(mlv, HIGH);
   }else{
    digitalWrite(mlv, 0);
   }
   if(value & 0b0100){
    digitalWrite(mrv, HIGH);
   }else{
    digitalWrite(mrv, 0);
   }
   if(value & 0b0010){
    digitalWrite(mlh, HIGH);
   }else{
    digitalWrite(mlh, 0);
   }
   if(value & 0b0001){
    digitalWrite(mrh, HIGH);
   }else{
    digitalWrite(mrh, 0);
   }

}
