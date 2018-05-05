/*
 * The MIT License
 * 
 * Copyright 2018 Davide (mail4davide@gmail.com)
 * 
 * Permission is hereby granted, free of charge, to any person 
 * obtaining a copy of this software and associated 
 * documentation files (the "Software"), to deal in the 
 * Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, 
 * sublicense, and/or sell copies of the Software, and to 
 * permit persons to whom the Software is furnished to do so, 
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall 
 * be included in all copies or substantial portions of the 
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY 
 * KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE 
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
 * PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN 
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF 
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 * 
 * ============================================================
 * 
 * level2pir - Centralina livello 2 per sensori PIR
 * 
 * Legge trasmissioni da attiny85 (PIR) ed invia i dati
 * alla centralina "level1".
 * 
 * Questo programma utilizza le librerie:
 * - Manchester
 * http://mchr3k.github.io/arduino-libs-manchester/
 * https://github.com/mchr3k/arduino-libs-manchester
 * - Ethernet (originale arduino)
 * - MQTT
 * https://github.com/256dpi/arduino-mqtt
 * 
 * Nelle varie prove, la velocita` corretta di funzionamento
 * e` stata 600, quella impostata negli sketches ovviamente.
 * 
 * Il programma in origine aveva dei "blocchi" inspiegabili.
 * Non ho ancora aggiunto il watchdog, ho messo qualche
 * "Serial.print" per vedere se riesco a capire DOVE !
 * 
 * Il buffer di lettura puo`/deve essere >= al buffer di scrittura 
 * (usato nelle antenne di trasmissione)
 * Adesso e` ad 8.
 * 
 * Allo stato attuale ci sono ancora le accensioni dei leds,
 * servono come debug visuale, in funzionamento si possono
 * togliere (anche dal programma), specialmente dall' attiny
 * 
 * L'ethernet usa il DHCP client, solo se non gli viene
 * assegnato l'indirizzo dal server si assegna quello
 * statico preimpostato.
 * 
 * Eliminazione/scarto valore vuoto
 * 
 * Memorizzazione dato inviato alla level1 per non
 * inviarlo 10 volte
 * 
 * Attualmente il programma gestisce fino a 10 PIR,
 * da 0 a 9 (una sola cifra numerica).
 */

#include "Manchester.h"
#include <Ethernet.h>
#include <MQTT.h>

#define RX_PIN 2
#define LED_PIN 13

#define BUFFER_SIZE 8
uint8_t buffer[BUFFER_SIZE];
// Sinceramente non ho capito perche` devo dichiararle stringhe, ma funziona ;)
String strReceived;  // Stringa ricevuta
String memReceived;  // Stringa ricevuta


// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network.
// gateway and subnet are optional:

//Ethernet
byte mac[] = {
  0x90, 0xA2, 0xDA, 0x00, 0x00, 0x01
};
IPAddress ip(192, 168, 2, 101);
IPAddress myDns(192, 168, 2, 1);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);

EthernetClient net;
MQTTClient client;
const char* mqtt_server = "level1.home.local"; // Ho dovuto mettere l'indirizzo completo
String TopicBase = "I/Test/Test/Test/";  // Stringa Topic MQTT (Tipo/Casa/Piano/Stanza)
String TopicType = "PIR";  // Stringa Topic (type: PIR, ST, RH, ...)
String msg;  // Stringa da inviare


void connect() {
  Serial.print("connecting...");
  while (!client.connect("arduino", "try", "try")) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nconnected!");

  //client.subscribe("/hello");
  //client.unsubscribe("/hello");
}

void setup() 
{
  pinMode(LED_PIN, OUTPUT);  
  digitalWrite(LED_PIN, HIGH);
  Serial.begin(9600);

  // start the Ethernet connection:
  Serial.println("Trying to get an IP address using DHCP");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // initialize the Ethernet device not using DHCP:
    Ethernet.begin(mac, ip, myDns, gateway, subnet);
  }
  // print your local IP address:
  Serial.print("My IP address: ");
  ip = Ethernet.localIP();
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(ip[thisByte], DEC);
    Serial.print(".");
  }
  Serial.println();

  // mqtt
  client.begin(mqtt_server, 1883, net);
  connect();

  //manchester
  man.setupReceive(RX_PIN, MAN_600);
  man.beginReceiveArray(BUFFER_SIZE, buffer);
  Serial.println(".. begin ..");
  
  /*
   * Watchdog
   * Disattiva e riattiva a 8 secondi (il massimo)
   */
  //wdt_disable();
  //wdt_enable(WDTO_8S);

}

void loop() 
{
  // mqtt
  client.loop();

  if (!client.connected()) {
    connect();
  }

  //manchester
  if (man.receiveComplete()) 
  {
    digitalWrite(LED_PIN, HIGH);
    uint8_t receivedSize = 0;

    //do something with the data in 'buffer' here before you start receiving to the same buffer again
    receivedSize = buffer[0];
    for(uint8_t i=1; i<receivedSize; i++) {
      //Serial.write(buffer[i]);
      //Serial.println(char(buffer[i]));
      strReceived += char(buffer[i]);
    }

    Serial.println(strReceived);
    Serial.println(memReceived);
    // Ho cambiato la stringa, ora mando: "PIR1,0"
    // Devo estrarre e ricomporre la stringa da inviare ad mqtt
    //Serial.println(strReceived.substring(0,4));
    //Serial.println(strReceived.substring(5,6));

    /*
     * Pubblico il valore solo se: 
     * - la stringa e` diversa dalla precedente
     *   (qua ci sara` l'inghippo quando saranno presenti piu` sensori,
     *   ma non posso riservere tutta la "memoria" per gestire 10 sensori,
     *   ad ora ho infatti previsto i PIR da 0 a 9)
     * - c'e` una stringa/non e` vuota
     * - controllo se c'e` PIR
     * - controllo se c'e` 0 o 1
     * tutto questo (anche) per eliminare errori che arrivano da una errata trasmissione
     */ 
    if (memReceived != strReceived && strReceived != "" && strReceived.substring(0,3) == "PIR" && (strReceived.substring(5,6) == "0" || strReceived.substring(5,6) == "1")) {
      msg="{ \"ID\" : \""+strReceived.substring(0,4)+"\", \"Valore\" : \""+strReceived.substring(5,6)+"\" }";
      Serial.println(msg);
      client.publish(TopicBase+TopicType,msg);  // invia mqtt
      // Copia/Memo
      //strcpy (memReceived, strReceived); // non va perche` non sono piu` stringhe :O
      memReceived = strReceived;
    }
    // Azzero lettura
    strReceived = String("");
    // msg = String(""); // non era qua il problema

    man.beginReceiveArray(BUFFER_SIZE, buffer);
    Serial.println(".. new begin ..");

    //wdt_reset();
  }
  digitalWrite(LED_PIN, LOW);
}
