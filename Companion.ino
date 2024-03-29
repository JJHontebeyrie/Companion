//*************************************************
//                  COMPANION                    **
String          Version = "2.54";                
//                @jjhontebeyrie                 **
/**************************************************
**               Affichage déporté               **
**            de consommation solaire            ** 
**                pour MSunPV                    **
***************************************************
**                   ATTENTION                   **
**            Le code est prévu pour             ** 
**              LILYGO T-Display S3              **   
***************************************************
**           Repository du Companion             **
**  https://github.com/JJHontebeyrie/Companion   **
***************************************************
**          Bibliothèques nécessaires            **
**                                               **
**  https://github.com/PaulStoffregen/Time       **
**  https://github.com/JChristensen/Timezone     **
**  https://github.com/Bodmer/JSON_Decoder       **
**  https://github.com/Bodmer/OpenWeather        **
**                                               ** 
**  Ces bibliothèques doivent être décompactées  **
**  et les dossiers obtenus sont ensuite collés  **
**  dans /Documents/Arduino/libraries            **
***************************************************
** Les valeurs correspondantes à vos branchements**
** et sondes sont à modifier éventuellement      **
** aux lignes 627 et suivantes pour les index    **
** et 674 et suivantes pour les cumuls           **
**************************************************/

#include <TFT_eSPI.h>     // Voir pdf du github pour son installation
#include <WiFi.h>         // Voir pdf du github pour son installation
#include <JSON_Decoder.h> // https://github.com/Bodmer/JSON_Decoder
#include <OpenWeather.h>  // Latest here: https://github.com/Bodmer/OpenWeather
#include "NTP_Time.h"     // Attached to this sketch, see that tab for library needs
#include <OneButton.h>    // OneButton par Matthias Hertel (dans le gestionnaire de bibliothèque)
#include "perso.h"       // Données personnelles à modifier dans le fichier (voir en haut de cet écran)
#include "logo.h"         // Logo de départ
#include "images.h"       // Images affichées sur l'écran
#include "meteo.h"        // Icones météo
#include <ESPmDNS.h>      //Pour le mDNS
// Watchdog (relance le Companion si perte de signal) Idée géniale de Bellule!
#include <esp_task_wdt.h>  //watchdog en cas de déconnexion
#define WDT_TIMEOUT 10     //délai d'activation du watchdog en secondes

TFT_eSPI lcd = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&lcd);   // Tout l'écran
TFT_eSprite voyant = TFT_eSprite(&lcd);   // Sprite voyant
TFT_eSprite depart = TFT_eSprite(&lcd);   // Sprite écran d'accueil
TFT_eSprite sun = TFT_eSprite(&lcd);      // Sprite Hors service & soleil
TFT_eSprite Chauffe = TFT_eSprite(&lcd);  // Sprite indicateur chauffage électrique
TFT_eSprite fond = TFT_eSprite(&lcd);     // Sprite contour affichage cumuls
TFT_eSprite wifi = TFT_eSprite(&lcd);     // Sprite wifi
TFT_eSprite batterie = TFT_eSprite(&lcd); // Sprite batterie
TFT_eSprite meteo = TFT_eSprite(&lcd);    // Sprite meteo

// Couleurs bargraph
#define color0 0x10A2   //Sombre 
#define color00 0x7FB8  //Vert clair
#define color1 0x07E0   //Vert 
#define color2 0x26FB   //Bleu
#define color3 0xF780   //Jaune
#define color4 0xFD8C   //Orange
#define color45 0xFC10  //Rouge leger
#define color5 0xF9A6   //Rouge
// Couleurs pour affichage cumuls
#define color6 0xE6D8
#define color7 0xEF5D
// Couleur Température cumulus
#define color8 0x16DA 
unsigned long couleur;

// Chemin acces au fichier de données MSunPV
char path[] = "/status.xml";

// Variables pour programme
long lastTime = 0;
long lastMSunPV = 0;
String Months[13]={"Mois","Jan","Fev","Mars","Avril","Mai","Juin","Juill","Aout","Sept","Oct","Nov","Dec"};
String IP;  // Adresse IP de connexion du Companion
String RSSI; // Puissance signal WiFi
uint32_t volt ; // Voltage batterie
int vertical = 15;
bool wink = false;

// Boutons, pour les inverser, inversez 0 et 14
OneButton button(0, true); // Bouton éclairage
int BTNCumuls = 14; // Bouton cumuls

// Pointeurs pour relance recherche valeurs
bool awaitingArrivals = true;
bool arrivalsRequested = false;

// Variables pour dimmer
const int PIN_LCD_BL = 38;
const int freq = 1000;
const int ledChannel = 0;
const int resolution = 8;
bool inverse = true;
int dim, x;

// Variables affichant les valeurs reçues depuis le MSunPV
String PV,CU,CO,TEMPCU; // Consos et températures. Il y a 16 valeurs, on en récupère que 3 ou 4
String CUMCO,CUMINJ,CUMPV,CUMBAL; // Cumuls. Il y a 8 valeurs, on en récupère que 4

// Wifi
int status = WL_IDLE_STATUS;
WiFiClient client;

// Chaines pour decryptage
String matchString = "";
String  MsgContent;
String MsgSplit[16]; // 16 valeurs à récupérer pour les consos et températures
String MsgSplit2[8]; // 8 valeurs à récupérer pour les compteurs cumul

// Données de openweather
#define TIMEZONE euCET // Voir NTP_Time.h tab pour d'autres "Zone references", UK, usMT etc
String lever, coucher, tempExt, icone, ID;
OW_Weather ow; // Weather forecast librairie instance
// Update toutes les 15 minutes, jusqu'à 1000 requêtes par jour gratuit (soit ~40 par heure)
const int UPDATE_INTERVAL_SECS = 15 * 60UL; // 15 minutes
bool booted = true;
long lastDownloadUpdate = millis();
String timeNow = "";
String dateNow = "";

// Variables pour serveur web
WiFiServer server(80);
String header; // Variable to store the HTTP request
unsigned long currentTime = millis(); // Current time
unsigned long previousTime = 0; // Previous time
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 5000;

// transforme les W en kW (par Bellule, encore lui!)
float wh_to_kwh(float wh) {return wh / 1000.0;}

///////////////////////////////////////////////////////////////////////////////////////
//                                 Routine SETUP                                     //
/////////////////////////////////////////////////////////////////////////////////////// 
void setup(){

  // Activation du port batterie interne
  if (lipo) {pinMode(15,OUTPUT); digitalWrite(15,1);}

  //Initialisation du bouton pour cumuls
  pinMode(BTNCumuls, INPUT_PULLUP);
  // Initialisation ecran   
  lcd.init();
  lcd.setRotation(rotation); 
  lcd.setSwapBytes(true);
  // Affichage logo depart
  lcd.fillScreen(TFT_WHITE);
  depart.createSprite(300,146);
  depart.setSwapBytes(true);
  depart.pushImage(0,0,300,146,image);
  depart.setTextColor(TFT_BLUE,TFT_WHITE);
  depart.drawString("Version " + (Version),100,0,2);
  depart.pushSprite(10,20,TFT_WHITE);
  
  // Creation des sprites affichage
  sprite.createSprite(320, 170);              // Tout l'ecran
  sprite.setTextColor(TFT_WHITE,TFT_BLACK);   // Ecriture blanc sur fonf noir
  sprite.setTextDatum(4);                     // Alignement texte au centre du rectangle le contenant
  voyant.createSprite(68,68);                 // Voyant rouge, vert ou bleu indiquant si on peut lancer un truc
  voyant.setSwapBytes(true);                  // (Pour affichage correct d'une image)
  sun.createSprite(220,29);                   // Texte Hors service et soleil
  sun.setSwapBytes(true);
  Chauffe.createSprite(40,40);                // Chauffage en cours 
  Chauffe.setSwapBytes(true);
  fond.createSprite(158,77);                  // Image de fond des cumuls
  fond.setSwapBytes(true);
  wifi.createSprite(24,23);                   // Image wifi
  wifi.setSwapBytes(true);
  batterie.createSprite(24,24);               // Image batterie
  batterie.setSwapBytes(true);
  meteo.createSprite(50,50);                  // Icone meteo
  meteo.setSwapBytes(true);

 //Initialisation port série et attente ouverture
  Serial.begin(115200);
  while (!Serial) {
    delay(100); // wait for serial port to connect. Needed for native USB port only
  }  

  Serial.println("Configuraton du WDT...");
  esp_task_wdt_init(WDT_TIMEOUT, true);  //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);                //add current thread to WDT watch

  // delete old config et vérif de deconnexion
  WiFi.disconnect(true);
  delay(1000);

  // Etablissement connexion wifi
  WiFi.mode(WIFI_STA);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  //*****************************************************************************
  // Si difficultés à se connecter >  wpa minimal (décommenter la ligne suivante)
  //WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);
  //*****************************************************************************
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("."); 
    delay(100);   
    }
  Serial.println("WiFi connected.");
  IP=WiFi.localIP().toString();
  RSSI = String(WiFi.RSSI());
  // Récupération de l'heure
  udp.begin(localPort);
  syncTime();
  esp_task_wdt_reset();

  // Paramètres pour dimmer
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(PIN_LCD_BL, ledChannel);

  // Affichage texte sur écran de départ
  depart.setTextColor(TFT_RED,TFT_WHITE);
  depart.setTextDatum(4);
  depart.drawString("CONNEXION OK (" + (IP) + ") " + (RSSI) + "dB",150,126,2);
  depart.pushSprite(10,20);
  // Tamisage écran (dim varie de 0 à 250)
  dim = dim_choisie;
  ledcWrite(ledChannel, dim);
  if (veille){
    delay(5000);
    depart.drawString("             MISE EN VEILLE              ",150,126,2);
    depart.pushSprite(10,20);
    delay(2000); 
  }

  // Activation du simple et double clic (by Felvic)
  button.attachClick(handleClick);
  button.attachDoubleClick(doubleClick);

  // Activation du mDNS responder: encore idée de Bellule
  // il suffit de taper dans l'invite du navigateur web
  // cette simple commande http://companion pour y accéder
  if (!MDNS.begin("companion")) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {delay(1000);}
  }
  Serial.println("mDNS responder started");
  // Lancement serveur web
  server.begin();
  // Ajout duservice to MDNS-SD
  MDNS.addService("http", "tcp", 80);

  esp_task_wdt_reset();
}

///////////////////////////////////////////////////////////////////////////////////////
//                                 Routine LOOP                                      //
/////////////////////////////////////////////////////////////////////////////////////// 
void loop(){

  // Etat batterie
  volt = (analogRead(4) * 2 * 3.3 * 1000) / 4096;

  // Lit heure
  if (lastTime + 1000 < millis()){
    drawTimeDate();    
    lastTime = millis();
  }

  // Données météo
  // Teste pour voir si un rafraissement est nécessaire
  if (booted || (millis() - lastDownloadUpdate > 1000UL * UPDATE_INTERVAL_SECS)){
    donneesmeteo();
    lastDownloadUpdate = millis();
  }

  // Affiche données
  if (awaitingArrivals) {
   if (!arrivalsRequested) {
      arrivalsRequested = true;
      getArrivals();
      decrypte();
      Affiche();         
    }  
  }

  // Relance de lecture des données (15 sec)
  if(lastMSunPV + 15000 < millis()){
    Serial.println("poll again");  
    resetCycle();
    lastMSunPV = millis();
  }

 // Teste si veille demandée
  if (veille) {
    dim = dim_choisie; // Pour la sortie de veille
    if (PV.toInt() <= 0) dim = 0; // on met l'écran en arrêt si pv = 0
    ledcWrite(ledChannel, dim);
  }

  // Si bouton pressé, affiche cumuls momentanément
  if (digitalRead(BTNCumuls) == 0) AfficheCumul();

  // Modification intensité lumineuse sous forme va  & vient
  // et gestion double clic (by Felvic encore !)                   
  button.tick();
  
  // Teste si demande lecture serveur web
  serveurweb();

  booted = false;
  esp_task_wdt_reset();
} 

/***************************************************************************************
**                      Serveur web (idée et conception Bellule)
***************************************************************************************/
void serveurweb() {
  // Activation de la fonction serveur Web (superbe idée de Bellule)
  // Ceci permet une lecture sur un téléphone par exemple mais aussi
  // à distance si l'adresse du companion est fixe. Commencez par
  // vous connecter sur l'adresse affichée sur l'écran d'accueil
 WiFiClient clientweb = server.available();  // Listen for incoming clients

  if (clientweb) {                  // If a new client connects,
    Serial.println("New Client.");  // print a message out in the serial port
    String currentLine = "";        // make a String to hold incoming data from the client
    currentTime = millis();
    previousTime = currentTime;
    while (clientweb.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (clientweb.available()) {  // if there's bytes to read from the client,
        char c = clientweb.read();  // read a byte, then
        Serial.write(c);            // print it out the serial monitor
        header += c;
        if (c == '\n') {  // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            clientweb.println("HTTP/1.1 200 OK");
            clientweb.println("Content-type:text/html");
            clientweb.println("Connection: close");
            clientweb.println();

            clientweb.println("<!DOCTYPE html><html>");
            clientweb.println("<html lang=\"fr\">");
            clientweb.println("<head>");
            clientweb.println("<meta charset=\"UTF-8\" />");
            clientweb.println("<title>MSunPV Companion</title>");
            clientweb.println("<link rel=\"stylesheet\" href=\"https://www.w3schools.com/w3css/4/w3.css\">");
            clientweb.println("<link rel=\"stylesheet\" href=\"https://fonts.googleapis.com/css?family=Allerta+Stencil\">");
            clientweb.println("<script src=\"https://code.jquery.com/jquery-3.6.4.js\" integrity=\"sha256-a9jBBRygX1Bh5lt8GZjXDzyOB+bWve9EiO7tROUtj/E=\" crossorigin=\"anonymous\"></script>");

            clientweb.println("<script>");
            clientweb.println("$( document ).ready(function() {");
            clientweb.println("$('#div_refresh').load(document.URL +  ' #div_refresh');");
            clientweb.println("setInterval(function() {");
            clientweb.println("$('#div_refresh').load(document.URL +  ' #div_refresh');");
            clientweb.println("},5000);");
            clientweb.println("});");
            clientweb.println("</script>");

            clientweb.println("</head>");
            clientweb.println("<body>");
            //script             
            clientweb.println("<script>");
            clientweb.println("function toggleBottom() {");
            clientweb.println("var bottomDiv = document.getElementById(\"bottom\");");
            clientweb.println("if (bottomDiv.style.display === \"none\") {");
            clientweb.println("bottomDiv.style.display = \"block\";");
            clientweb.println("} else {");
            clientweb.println("bottomDiv.style.display = \"none\";");
            clientweb.println("}");
            clientweb.println("}");
            clientweb.println("</script>");

            // Web Page Heading
            clientweb.println("<div class= \"w3-container w3-black w3-center w3-allerta\">");
            clientweb.println("<h1>MSunPV Companion</h1>");
            clientweb.println("</div>");
            clientweb.println("<div id=\"div_refresh\">");

            // <<<<<<<<<<<<<<<<<<<<<<<< Affichage des données MSunPV  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
            clientweb.println("<div class=\"w3-card-4 w3-teal w3-padding-16 w3-xxxlarge w3-center\">");
            clientweb.println("<p>Production Solaire</p>");
            clientweb.print(PV);  // Valeur Panneaux Photovoltaiques
            clientweb.println(" w");
            clientweb.println("</div>");

            clientweb.println("<div class=\"w3-card-4 w3-blue-grey w3-padding-16 w3-xxxlarge w3-center\">");
            clientweb.println("<p>Routage vers le ballon</p>");
            clientweb.print(CU);  // Valeur Recharge Cumulus
            clientweb.println(" w");
            clientweb.println("</div>");

            clientweb.println("<div class=\"w3-card-4 w3-padding-16 w3-xxxlarge w3-center\">");
            clientweb.println("<p>Consommation EDF</p>");
            clientweb.print(CO);  // Valeur Consommation EDF
            clientweb.println(" w");
            clientweb.println("</div>");
            clientweb.println("</div>");

            clientweb.println("<center>");
            clientweb.println("<button class=\"w3-bar w3-black w3-button w3-large w3-hover-white\" onclick=\"toggleBottom()\">Informations journalières</button>");
            clientweb.println("</center>");
            clientweb.println("<div id=\"bottom\" style=\"display:none;\">");
            
            clientweb.println("<div class=\"w3-card-4 w3-teal w3-padding-16 w3-xxxlarge w3-center\">");
            clientweb.println("<p>Consommation EDF journalière</p>");
            clientweb.print(CUMCO);  // Cumul Panneaux Photovoltaiques
            if (nbrentier) clientweb.println(" kWh"); else clientweb.println(" Wh");
            clientweb.println("</div>");

            clientweb.println("<div class=\"w3-card-4  w3-blue-grey w3-padding-16 w3-xxxlarge w3-center\">");
            clientweb.println("<p>Production Solaire journalière</p>");
            clientweb.print(CUMPV);  // Valeur cumul recharge cumulus
            if (nbrentier) clientweb.println(" kWh"); else clientweb.println(" Wh");
            clientweb.println("</div>");
            
            clientweb.println("<div class=\"w3-card-4 w3-dark-grey w3-padding-16 w3-xxxlarge w3-center\">");
            clientweb.println("<p>Recharge Cumulus journalière</p>");
            clientweb.print(CUMBAL);  // Cumul Consommation EDF
            if (nbrentier) clientweb.println(" kWh"); else clientweb.println(" Wh");
            clientweb.println("</div>");

            clientweb.println("<div class=\"w3-card-4 w3-padding-16 w3-xxxlarge w3-center\">");
            clientweb.println("<p>Injection sur EDF</p>");
            clientweb.print(CUMINJ);  // Cumul Injection EDF
            if (nbrentier) clientweb.println(" kWh"); else clientweb.println(" Wh");
            clientweb.println("</div>");
            // <<<<<<<<<<<<<<<<<<<<<<<< Affichage des données MSunPV  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
            clientweb.println("</div>");
            clientweb.println("</body></html>");
            // The HTTP response ends with another blank line
            clientweb.println();
            // Break out of the while loop
            break;
          } else {  // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    clientweb.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
    esp_task_wdt_reset();
  }
}

/***************************************************************************************
**                             Affichage principal
***************************************************************************************/ 
void Affiche(){

  //test(); // teste les affichages

  //  Dessin fenêtre principale
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextColor(TFT_WHITE,TFT_BLACK);

  // Panneaux principaux
  sprite.drawRoundRect(0,0,226,55,3,TFT_WHITE);
  sprite.drawString("PANNEAUX PV",115,10,2); 
  sprite.drawRoundRect(0,57,226,55,3,TFT_WHITE);
  sprite.drawString("CUMULUS",115,68,2);
  sprite.drawRoundRect(0,114,226,55,3,TFT_WHITE);
  //Affichage titre conso/injection dans routine indic

  //Panneau de droite sur l'écran : heure, date, dimer, batterie
  sprite.drawRoundRect(234,0,80,31,3,TFT_WHITE);
  sprite.drawRoundRect(234,33,80,20,3,TFT_WHITE);
  if (lipo){
    vertical = 0;
    batterie.pushImage(0,0,24,24,pile);
    batterie.pushToSprite(&sprite,296,121,TFT_BLACK);}

  // Affichage Météo
  meteo.pushImage(0,0,50,50,unknown);
  if (icone == "01d") {meteo.pushImage(0,0,50,50,clear_day); goto suite;}
  if (icone == "01n") {meteo.pushImage(0,0,50,50,clear_night); goto suite;}
  if (icone == "02d") {meteo.pushImage(0,0,50,50,partly_cloudy_day); goto suite;}
  if (icone == "02n") {meteo.pushImage(0,0,50,50,partly_cloudy_night); goto suite;}
  if ((icone == "03d") or (icone == "03n")) {meteo.pushImage(0,0,50,50,few_clouds); goto suite;}
  if ((icone == "04d") or (icone == "04n")) {meteo.pushImage(0,0,50,50,cloudy); goto suite;}
  if ((icone == "09d") or (icone == "09n")) {meteo.pushImage(0,0,50,50,rain); goto suite;}
  if ((icone == "10d") or (icone == "10n")) {meteo.pushImage(0,0,50,50,lightRain); goto suite;}
  if ((icone == "11d") or (icone == "11n")) {meteo.pushImage(0,0,50,50,thunderstorm); goto suite;}
  if ((icone == "13d") or (icone == "13n")) {meteo.pushImage(0,0,50,50,snow); goto suite;}
  if ((icone == "50d") or (icone == "50n")) {meteo.pushImage(0,0,50,50,fog); goto suite;}
  if (icone == "80d") {meteo.pushImage(0,0,50,50,splash);  goto suite;}
  if (ID == "301") {meteo.pushImage(0,0,50,50,drizzle); goto suite;} 
  if (ID == "221") meteo.pushImage(0,0,50,50,wind);

  suite: // Affiche icone metéo et température extérieure
  sprite.setTextDatum(5); // centre droit                                                                                         
  sprite.drawString(tempExt, 306, 160,2);   
  sprite.drawCircle(310,154,2,TFT_WHITE); // pour °
  sprite.setTextDatum(4); // retour au centre milieu
  sprite.setTextColor(TFT_CYAN,TFT_BLACK);
  if (tempExt.toInt() <= 3) sprite.drawString("*", 285, 168,4);  
  sprite.setTextColor(TFT_WHITE,TFT_BLACK); // Retour texte normal
  meteo.pushToSprite(&sprite,235,120,TFT_BLACK);

  // Affichage heure et date
    sprite.drawString(String(timeNow),272,17,4);
    sprite.drawString(String(dateNow),272,43,2);
  
  // Affichage éventuel de la température si sonde validée
  if (sonde) {
    sprite.drawString(TEMPCU,26,85,2);
    //sprite.drawCircle(39,79,2,TFT_WHITE); // pour ° 
    couleur = color1;
    if (TEMPCU.toInt() >= 40) couleur = color8;
    if (TEMPCU.toInt() >= 50) couleur = color4;
    if (TEMPCU.toInt() >= 60) couleur = color5;
    sprite.drawCircle(25,84,20,couleur);
    sprite.drawCircle(25,84,19,couleur);
  } 

  // Affichage des valeurs des compteurs
  sprite.setFreeFont(&Orbitron_Light_24); // police d'affichage
  // Affichage valeur PV
  if (PV.toInt() >= residuel) sprite.drawString(PV +" w",115,35);
  else {
    // Hors service & lever/coucher 
    sun.pushImage(0,0,220,29,Soleil); 
    sun.pushToSprite(&sprite,3,20,TFT_BLACK);
    sprite.setTextColor(TFT_YELLOW,TFT_BLACK);
    sprite.drawString(lever, 23, 15,2);
    sprite.drawString(coucher, 203, 15,2);
    sprite.setTextColor(TFT_WHITE,TFT_BLACK);}   
  // Affichage valeur Cumulus
  sprite.drawString(CU +" w",115,92); 
  // Affichage valeur Consommation
  sprite.drawString(CO +" w",115,150);

  //Voyant assistant de consommation 
  if (PV.toInt() < 1000) voyant.pushImage(0,0,68,68,BtnO); else voyant.pushImage(0,0,68,68,BtnV);
  if (PV.toInt() < 500) voyant.pushImage(0,0,68,68,BtnR); 
  if ((CO.toInt() > 0) and (PV.toInt() < 1000)) voyant.pushImage(0,0,68,68,BtnO); 
  if (CO.toInt() > 500) voyant.pushImage(0,0,68,68,BtnR);  
  if (CO.toInt() < -500) voyant.pushImage(0,0,68,68,BtnO);
  if (CO.toInt() < -1000) voyant.pushImage(0,0,68,68,BtnV);
  if (CO.toInt() > PV.toInt()) voyant.pushImage(0,0,68,68,BtnR);
  if (CU.toInt() > 1000) voyant.pushImage(0,0,68,68,BtnV);  
  if (PV.toInt() < residuel) voyant.pushImage(0,0,68,68,BtnR);
             
  // En cas de chauffage électrique
  if (chauffageElectr) {               
    if ((CO.toInt() + PV.toInt() > 3000) and (CU.toInt() < 100)){  
      Chauffe.pushImage(0,0,40,40,chauffage);
      Chauffe.pushToSprite(&sprite,6,122,TFT_BLACK);
    }
  }
  
  // Appel routine affichage des graphes latéraux
  indic(); 
  // Dessin du voyant
  voyant.pushToSprite(&sprite,230,55,TFT_BLACK);
  // Le rafraichissement de l'affichage de tout l'écran est fait dans Barlight()
  Barlight();
  esp_task_wdt_reset();
}

/***************************************************************************************
**                        Affichage de la page des cumuls
***************************************************************************************/ 
void AfficheCumul(){
  //  Dessin fenêtre noire et titre
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextColor(TFT_WHITE,TFT_BLACK);
  if (nbrentier) sprite.drawString("CUMULS (en kWh)",160,10,2); else sprite.drawString("CUMULS (en Wh)",160,10,2); 

  // Affichage des rectangles graphiques
  fond.pushImage(0,0,158,77,pano);
  fond.pushToSprite(&sprite,0,20,TFT_BLACK);
  fond.pushToSprite(&sprite,160,20,TFT_BLACK);
  fond.pushToSprite(&sprite,0,96,TFT_BLACK);
  fond.pushToSprite(&sprite,160,96,TFT_BLACK);

  // Affichage des légendes
  sprite.setTextColor(TFT_BLACK,color6);
  sprite.drawString("Consommation",80,36,2);
  sprite.drawString("Cumulus",80,112,2);
  sprite.drawString("Panneaux",240,36,2);
  sprite.drawString("Injection",240,112,2); 

  // Affichage des valeurs cumuls
  sprite.setTextColor(TFT_BLACK,color7);
  if (!nbrentier) sprite.setFreeFont(&Roboto_Thin_24);
  sprite.drawString(CUMCO,80,68);
  sprite.drawString(CUMBAL,80,144);
  sprite.drawString(CUMPV,240,68);
  sprite.drawString(CUMINJ,240,144);

  // Rafraichissement écran (indispensable après avoir tout dessiné)
  sprite.pushSprite(0,0);
}   

/***************************************************************************************
**                    Decryptage des valeurs lues dans le xml
***************************************************************************************/ 
void decrypte(){
  delay(1000);
  // Mise en mémoire des consos du fichier xml
  int startValue = matchString.indexOf("<inAns>");
  int endValue = matchString.indexOf("</inAns>");
  MsgContent = matchString.substring(startValue + 7, endValue);    // + 7 depart index après la valeur du tag

  // MsgContent : chaine avec toutes les valeurs recherchées 
  Serial.println("Compteurs >> " + MsgContent); 

  // Mise en variables des valeurs trouvées MsgSplit[0 à 15]
  split(MsgSplit,16,MsgContent,';');

  // Mise en mémoire des cumuls du fichier xml
  startValue = matchString.indexOf("<cptVals>");
  endValue = matchString.indexOf("</cptVals>");
  MsgContent = matchString.substring(startValue + 9, endValue);

  // Mise en variables des cumuls trouvés MsgSplit2[0 à 7]
  split(MsgSplit2,8,MsgContent,';');

  /***************************************************************************************
  **                               MODIFICATION DES INDEX
  ****************************************************************************************
  **   Suivant les modifications que vous avez apporté au MSunPV
  **   il est possible que les valeurs souhaitées ne soient pas
  **   celles affichées. Vérifiez sur le moniteur série le bon index 
  ***************************************************************************************/  
  CO = MsgSplit[0];     // Consommation
  PV = MsgSplit[1];     // Panneaux PV 
  CU = MsgSplit[2];     // Cumulus
  TEMPCU = MsgSplit[5]; // Sonde température cumulus
  //*************************************************************************************

  // Formatage des valeurs pour affichage sur l'écran 
  PV = String(abs(PV.toInt()));  // (avec prod en + ou en -)

  // Affichage en entiers si demandé dans perso.h
  if (nbrentier) {
    CO = String(CO.toInt()); 
    PV = String(PV.toInt());
    CU = String(CU.toInt());
  }

  // hysteresis des panneaux et cumulus
  if (PV.toInt() <= residuel) PV = "0"; // Légère consommation due aux onduleurs
  if (CU.toInt() <= residuel) CU = "0"; // Légère consommation due au thermostat du cumulus

  //  >>>>>>>>>>>>>  Routine made by Patrick (MSunPV), mon sauveur !  <<<<<<<<<<<<<<<
  for(int i = 0; i < 8; i++) {              //à remplacer par nbre de compteurs à afficher
    char xx[16];
    int yy[16];
    float zz[16];
        
    MsgSplit2[i].toCharArray(xx,16);
    sscanf(xx, "%x", &yy[i]);
    //Serial.println(yy[i]);
    zz[i] = yy[i];
    zz[i] /= 10;                     //à remplacer par decim
    MsgSplit2[i] = String(zz[i],1);  //à remplacer par autre unité si besoin
    MsgSplit2[i].replace('.', ',');
  }

  // Affichage des cumuls dans le Moniteur Série
  for(int j = 0;j<8;j++) {Serial.print("Cumul "); Serial.print (j); Serial.println(" >> " + MsgSplit2[j]);}

  /***************************************************************************************
  **                               MODIFICATION DES CUMULS
  ****************************************************************************************
  **   Suivant les modifications que vous avez apporté au MSunPV
  **   il est possible que les cumuls souhaités ne soient pas
  **   ceux affichés. Vérifiez sur le moniteur série le bon index
  ***************************************************************************************/   
  CUMCO = MsgSplit2[0];   // Cumul Conso
  CUMINJ = MsgSplit2[1];  // Cumul Injection
  CUMPV = MsgSplit2[2];   // Cumul Panneaux
  CUMBAL = MsgSplit2[3];  // Cumul Ballon cumulus
  //*************************************************************************************
  
  // Affichage en entiers si demandé dans perso.h (par etienneroussel)
  // Les Wh sont transformés aussi en kWh (par Bellule)
  if (nbrentier) {
    CUMCO  = String(wh_to_kwh(CUMCO.toInt()));
    CUMINJ = String(wh_to_kwh(CUMINJ.toInt()));
    CUMPV  = String(wh_to_kwh(CUMPV.toInt()));
    CUMBAL = String(wh_to_kwh(CUMBAL.toInt()));

    // Si votre MSunPV envoie déjà les données en kWh décommentez ces 4 lignes
    //CUMCO = String(CUMCO);
    //CUMINJ = String(CUMINJ);
    //CUMPV = String(CUMPV);
    //CUMBAL = String(CUMBAL);
  }  
  esp_task_wdt_reset();
}

/***************************************************************************************
**                      Affichage des bargraphs verticaux
***************************************************************************************/          
void indic(){
  // Panneaux Photovoltaiques
  int valeur,i;
  valeur = PV.toInt();
  int pmax = (puissance/10)*9; // On prend 90% de la prod théorique
  int pmin = puissance / 20;   // Pour min on prend 1/20 de la prod
  int ecart = (pmax - pmin) / 8; // steps ecart entre min et max
  int nbbarres = (valeur/ecart); // combien de steps dans prod en cours
  if (nbbarres > 8) nbbarres = 8; // on bloque les steps à 8

  if (valeur > residuel) {
    for(i = 0;i<8;i++) sprite.fillRect(200,(44-(i*5)),20,4,color0);
    // Permet l'affichage d'au moins une barre si on produit
    sprite.fillRect(200,44,20,4,color3);                   
    // Affichage de barres supplémentaires fonction de la prod
    for(i = 0;i<nbbarres;i++) sprite.fillRect(200,(44-(i*5)),20,4,color3);
  }

 // Cumulus
  valeur = CU.toInt();
  ecart = cumulus/ 8;
  nbbarres = (valeur/ecart); 
  if (nbbarres > 8) nbbarres = 8;
  for(i = 0;i<8;i++) sprite.fillRect(200,(100-(i*5)),20,4,color0);   
  if (valeur > residuel) {
    sprite.fillRect(200,100,20,4,color4);
    for(i = 0;i<nbbarres;i++) sprite.fillRect(200,(100-(i*5)),20,4,color4); 
  }   
   
  // Consommation
  valeur = CO.toInt();
  for(i = 0;i<8;i++) sprite.fillRect(200,(158-(i*5)),20,4,color0); 
  if (valeur != 0){
    // Affichage 1 barre dès conso ou injection
    sprite.fillRect(200,158,20,4,color1);
    if (valeur < 0) { // On injecte
      sprite.drawString("INJECTION",115,126,2);
      nbbarres = abs(valeur/500);
      if (nbbarres > 8) nbbarres = 8;
      for(i = 0;i<nbbarres;i++) sprite.fillRect(200,(158-(i*5)),20,4,color1); 
      }
    else { // On consomme, bargraph multicolore
      sprite.drawString("CONSOMMATION EDF",115,126,2);
      sprite.fillRect(200,158,20,4,color00);
      if (valeur > 500) sprite.fillRect(200,153,20,4,color1);
      if (valeur > 1000) sprite.fillRect(200,148,20,4,color2);
      if (valeur > 1500) sprite.fillRect(200,143,20,4,TFT_CYAN);
      if (valeur > 2000) sprite.fillRect(200,138,20,4,color3);
      if (valeur > 2500) sprite.fillRect(200,133,20,4,color4);
      if (valeur > 3000) sprite.fillRect(200,128,20,4,color45);
      if (valeur > 4000) sprite.fillRect(200,123,20,4,color5);
    }
  }
  else sprite.drawString("CONSOMMATION NULLE",115,126,2);
}

/***************************************************************************************
**      Fait varier l'intensité d'éclairage de 50 à 250 dans un sens et l'inverse
***************************************************************************************/
void Eclairage(){
  if (!inverse) {
    dim = dim - 50;
    if (dim <= 50) {dim = 50; inverse = true;}
  }
  else
  {
    dim = dim + 50;
    if (dim >= 250) {dim = 250; inverse = false;}
  }
  delay(300);
  Barlight();
}

/***************************************************************************************
**                      Affichage du graphe intensité lumineuse
***************************************************************************************/
void Barlight(){
  x = dim/50; // steps de 50
  for(int i = 0;i<5;i++) sprite.fillRect(302,85+vertical+(i*6),12,5,color0);
  for(int i = 0;i<x;i++) sprite.fillRect(302,109+vertical-(i*6),12,5,color3);
  // Gestion batterie
  if (lipo) batt();
  // Puissance du signal WiFi
  signalwifi();
  // Rafraichissement de tout l'écran
  sprite.pushSprite(0,0);
  // Réglage de la luminosité
  ledcWrite(ledChannel, dim);
}

/***************************************************************************************
**                            Gestion de la batterie
***************************************************************************************/
void batt(){
  // Voltage pour batterie, les chiffres sont à modifier suivant votre batterie
  if (volt < 4) sprite.fillRect(302,127,12,3,color0);
  if (volt < 3.5) sprite.fillRect(302,132,12,3,color0);
  if (volt < 3) sprite.fillRect(302,137,12,3,color0);
}

/***************************************************************************************
**                            Gestion du signal Wifi
***************************************************************************************/
void signalwifi(){
  // Etat du signal WiFi
  RSSI = String(WiFi.RSSI());
  // Attention à -65dB on a toutes les barres, c'est adapté au Companion !
  wifi.pushImage(0,0,24,23,signal4); // Signal max par défaut
  if (RSSI.toInt() <= -70) wifi.pushImage(0,0,24,23,signal3);
  if (RSSI.toInt() <= -75) wifi.pushImage(0,0,24,23,signal2);
  if (RSSI.toInt() <= -80) wifi.pushImage(0,0,24,23,signal1);
  if (RSSI.toInt() <= -90) wifi.pushImage(0,0,24,23,signal0);
  wifi.pushToSprite(&sprite,296,(57 + vertical),TFT_BLACK);
}

/***************************************************************************************
**                           Réception des données météo
**                 Valeurs issues de Open Weather (Gestion Forecast)
***************************************************************************************/
void donneesmeteo(){
  // Valeurs issues de Open Weather (Gestion Forecast)
  OW_forecast  *forecast = new OW_forecast;
  ow.getForecast(forecast, api_key, latitude, longitude, units, language);

  Serial.println("");
  Serial.println("############### Données météo ###############");
  Serial.print("Latitude            : "); Serial.println(ow.lat);
  Serial.print("Longitude           : "); Serial.println(ow.lon);
  Serial.print("Timezone            : "); Serial.println(forecast->timezone);
  Serial.print("Heure actuelle      : "); Serial.println(strTime(forecast->dt[0]));
  Serial.print("Lever soleil        : "); Serial.println(strTime(forecast->sunrise));
  Serial.print("Coucher soleil      : "); Serial.println(strTime(forecast->sunset));
  Serial.print("temperature         : "); Serial.println(forecast->temp[0]);
  Serial.print("description         : "); Serial.println(forecast->description[0]);
  Serial.print("icone               : "); Serial.println(forecast->icon[0]);
  Serial.print("ID                  : "); Serial.println(forecast->id[0]);

  lever = strTime(forecast->sunrise);
  coucher = strTime(forecast->sunset);
  tempExt = String(forecast->temp[0], 0);  // Température sans décimale
  if (tempExt.length() < 2) tempExt = " " + tempExt; //et sur 2 caractères
  icone = (forecast->icon[0]);
  ID  = (forecast->id[0]);
  if (poisson) {if (wink) icone ="80d";}

  // Effacement des chaines pour libérer la mémoire
  delete forecast;
  esp_task_wdt_reset();
}

/***************************************************************************************
**                         Relancement du cycle de lecture
***************************************************************************************/
void getArrivals() {
 // Use WiFiClient class to create TLS connection
  Serial.println("\nInitialisation de la connexion au serveur...");

 // Connexion au serveur web
  Serial.print("Connexion à ");
  Serial.println(serveur);

  if (!client.connect(serveur, 80)) {
    Serial.println("Connexion échouée");
    return;
  }
    
    // Make a HTTP request:
    client.print("GET "); client.print(path); client.println(" HTTP/1.1");
    client.print("Host: "); client.println(serveur);
    client.println();
  
    while(!client.available()); //wait for client data to be available
    Serial.println("Attente de la réponse serveur...");
    delay(5);

    while(client.available()) {   
      String line = client.readStringUntil('\r');
      matchString = line; // Mise en mémoire du xml complet
    }

    Serial.println("requete validée");  
    awaitingArrivals = false;
    client.stop();
    esp_task_wdt_reset();
}
  

void resetCycle() {
 awaitingArrivals = true;
 arrivalsRequested = false;
 Serial.println("et montre l'écran...");    
}

/***************************************************************************************
**         Découpe la partie du xml voulue en autant de valeurs que trouvées
***************************************************************************************/
void split(String * vecSplit, int dimArray,String content,char separator){
  if(content.length()==0)
    return;
  content = content + separator;
  int countVec = 0;
  int posSep = 0;
  int posInit = 0;
  while(countVec<dimArray){
    posSep = content.indexOf(separator,posSep);
    if(posSep<0) return;       
    String splitStr = content.substring(posInit,posSep);
    posSep = posSep+1; 
    posInit = posSep;
    vecSplit[countVec] = splitStr;
    countVec++;    
  } 
}

/***************************************************************************************
**             Conversion Unix time vers "time" time string "12:34"
***************************************************************************************/
String strTime(time_t unixTime)
{
  String localTime = "";
  time_t local_time = TIMEZONE.toLocal(unixTime, &tz1_Code);

  if (hour(local_time) < 10) localTime += "0";
  localTime += hour(local_time);
  localTime += ":";
  if (minute(local_time) < 10) localTime += "0";
  localTime += minute(local_time);

  return localTime;
}

/***************************************************************************************
**                        Decryptage de l'heure et Date
***************************************************************************************/
void drawTimeDate() {
  // Convert UTC to local time, returns zone code in tz1_Code, e.g "GMT"
  time_t local_time = TIMEZONE.toLocal(now(), &tz1_Code);

  timeNow = "";
  if (hour(local_time) < 10) timeNow += "0";
  timeNow += hour(local_time);
  timeNow += ":";
  if (minute(local_time) < 10) timeNow += "0";
  timeNow += minute(local_time);

  dateNow = "";
  dateNow += day(local_time);
  dateNow += " ";
  dateNow += String(Months[month(local_time)]);
  if (poisson) {if (dateNow == "1 Avril") wink = true;}
}

/***************************************************************************************
**     Routine de test pour afficher toutes les icones sur l'écran
**               Décommenter la ligne 446 pour l'activer
***************************************************************************************/
void test(){
  PV = "0";
  CO = "4500";
  CU = "90";
  TEMPCU = "55";
  tempExt = "3";
  sonde = true;
  chauffageElectr = true;
  veille = false;
}

/***************************************************************************************
**                               Gestion bouton éclairage
***************************************************************************************/
// Simple clic
void handleClick() {
  if ((veille) and (PV.toInt() <= 0)) { // Si on clique bouton, veille annulée momentanément
    dim = dim_choisie;
    Barlight(); 
    delay(5000);} // affichage de 5 secondes
  Eclairage();  
}

// Double clic
void doubleClick() {
  if (veille) {veille = false; dim = dim_choisie;}
  Eclairage();
}