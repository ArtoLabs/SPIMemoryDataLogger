#include<SPIMemory.h>
#include <Time.h>
#include <SimpleDHT.h>

#define LOG_BUFFER_SIZE 17 // Unix timestamp (10 digits) + : (1 digit) + temp (2 digits) + : (1 digit) + humidity (2 digits) + newline (1 digit)
#define CS_PIN 10
#define CURRENT_UNIX_TIMESTAMP 1550447210 // This is only used for demonstration purposes and should be replaced (Approx Feb 17 16:50)
#define DHT11_PIN A0
#define LED_PIN 3

const long timerInterval = 8000;  // Check every 15 seconds
unsigned long logTimer = 0;
char logDataBuffer[LOG_BUFFER_SIZE];
unsigned long currentTimestamp = CURRENT_UNIX_TIMESTAMP;
bool pausePolling = true;
byte temp = 0;
byte hum = 0;
int temperature = 0;
int humidity = 0;
char tempChar[3];
char humChar[3];
char humanTime[16];
float Tf = 0;

SPIFlash flash(CS_PIN);    
SimpleDHT11 dht11;

void setup() {
    Serial.begin(115200);
    flash.begin();
    flash.setClock(1000); // A slow 1Khz to prevent reflections and distortions in the voltage dividers 
    pinMode(LED_PIN, OUTPUT);
    delay(1000);
    Serial.println(F("_____ SPI Flash Memory Data logger Tutorial _____"));
    Serial.println();
    printMenu();
}

void loop() {
    uint8_t command = 0;
    while (Serial.available() > 0) {
      command = Serial.parseInt();
    }
    if (command == 1) {
      printLog();
    }
    else if (command == 2) {
      pausePolling = false;
    }
    else if (command == 3) {
      pausePolling = true;
      Serial.println(F("Logging paused"));
    }
    else if (command == 4) {
      printChipStats();
    }
    else if (command == 5) {
      Serial.print(F("Erasing flash chip..."));
      flash.eraseChip();
      Serial.println(F("erased."));
      flash.begin();
    }
    else if (command == 6) {
      printPage(0, 1);
    }
    else if (command == 7) {
      Serial.print(F("The next available address: "));
      uint32_t gaddr = flash.getAddress(0);
      Serial.println(gaddr);
    }
    if (command == 8) {
      printMenu();
    }
    unsigned long currentMillis = millis();
    if ((currentMillis - logTimer >= timerInterval) && (!pausePolling)){
        digitalWrite(LED_PIN, HIGH);
        logTimer = currentMillis;
        currentTimestamp += int(currentMillis / 1000);
        logData();
        delay(200);
        digitalWrite(LED_PIN, LOW);
    }
}

void printLog() {
    uint32_t paddr = 0x00; // Start at the very beginning of the chip
    while(1) {
      if (endOfData(paddr)) { break; }
      flash.readCharArray(paddr, logDataBuffer, sizeof(logDataBuffer));
      Serial.print(logDataBuffer);
      delay(3);
      paddr += LOG_BUFFER_SIZE;
    }
}

void clearDataBuffer() {
    for(int i=0; i<LOG_BUFFER_SIZE; i++) {
      logDataBuffer[i] = char(0);
    }
}

void printMenu() {
    Serial.println(F("__ Command Menu __\n8 = this menu\n1 = print log\n2 = start logging\n3 = pause logging\n4 = print chip id and capacity\n5 = erase flash chip\n6 = print raw data\n7 = print next address"));
}

void logData() {
    uint32_t Myaddr = 0;
    int err = SimpleDHTErrSuccess;
    if ((err = dht11.read(DHT11_PIN, &temp, &hum, NULL)) != SimpleDHTErrSuccess) {
        // If the DHT11 encounters an error print it to the LCD
        Serial.println(err);
        return;
    }
    
    humidity = int(hum);

    Tf = (int(temp) * 9.0)/ 5.0 + 32.0;  // Convert temperature to fahrenheit
    temperature = int(Tf);

    ultoa(currentTimestamp, logDataBuffer, 10);

    itoa(temperature, tempChar, 10);
    itoa(humidity, humChar, 10);

    strcat(logDataBuffer, ":");

    strcat(logDataBuffer, tempChar);
    strcat(logDataBuffer, ":");
    strcat(logDataBuffer, humChar);
    strcat(logDataBuffer, "\n");

    Myaddr = flash.getAddress(sizeof(char));

    //Serial.print(F("Next available address: "));
    //Serial.println(addr);

    flash.writeCharArray(Myaddr, logDataBuffer, sizeof(logDataBuffer));

    makeHumanTime(currentTimestamp);
    Serial.print(F("Temperature: "));
    Serial.print(tempChar);
    Serial.print(F("F Humidity: "));
    Serial.print(humidity);
    Serial.println(F("%"));

}



void printChipStats() {
    uint32_t JEDEC = flash.getJEDECID();
    Serial.print(F("JEDEC ID: "));
    Serial.println(JEDEC);
    uint32_t Cap = flash.getCapacity() / 1048576;
    Serial.print(F("Capacity: "));
    Serial.print(Cap);
    Serial.println(F(" MB"));
}

void makeHumanTime(unsigned long timestamp) {
    setTime(timestamp);
    sprintf(humanTime,"%02d/%02d/%04d %02d:%02d", month(), day(), year(), hour(), minute());
    Serial.print(F("Polled at: "));
    Serial.println(humanTime);
}

// Find the end of our data log. All the bytes of the chip are set to High 0xFF (255)
// When the chip is erased. Since no data entered by the data logger is likely to
// have for bytes in a row set high, it is considered the end of the data.
bool endOfData(uint32_t thisaddr) {
    int s = 0;
    uint8_t thisByte = 0x00;
    uint32_t newaddr = 0x00;
    for (uint32_t i=0; i<8; i++) {
      newaddr = thisaddr + i;
      thisByte = flash.readByte(newaddr);
      if ((thisByte == 0xFF) || (thisByte == 255)){
        s++; // If 6 bytes of 8 bytes are all 0xFF (255) then there is no more data
      }
    }
    if (s > 6) {
      return true;
    }
    else {
      return false;
    }
}

//Prints hex/dec formatted data from page reads - for debugging
void _printPageBytes(uint8_t *data_buffer, uint8_t outputType) {
  char buffer[10];
  for (int a = 0; a < 16; ++a) {
    for (int b = 0; b < 16; ++b) {
      if (outputType == 1) {
        sprintf(buffer, "%02x", data_buffer[a * 16 + b]);
        Serial.print(buffer);
      }
      else if (outputType == 2) {
        uint8_t x = data_buffer[a * 16 + b];
        if (x < 10) Serial.print("0");
        if (x < 100) Serial.print("0");
        Serial.print(x);
        Serial.print(',');
      }
    }
    Serial.println();
  }
}

//Reads a page of data and prints it to Serial stream. Make sure the sizeOf(uint8_t data_buffer[]) == 256.
void printPage(uint32_t _addy, uint8_t outputType) {

  char buffer[24];
  sprintf(buffer, "Reading address 0x(%04x)", _addy);
  Serial.println(buffer);

  uint8_t data_buffer[SPI_PAGESIZE];
  flash.readByteArray(_addy, &data_buffer[0], SPI_PAGESIZE);
  _printPageBytes(data_buffer, outputType);
}
