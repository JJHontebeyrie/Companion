/*******************************************
**               COMPANION                **
**              Version 2.0               **
**             @jjhontebeyrie             **
********************************************
**            Affichage déporté           **
**         de consommation solaire        ** 
**             pour MSunPV                **
********************************************
**                ATTENTION               **
**         Le code est prévu pour         ** 
**           LILYGO T-Display S3          **   
********************************************

********************************************
**        Bibliothèques nécessaires       **
********************************************

https://github.com/PaulStoffregen/Time
https://github.com/JChristensen/Timezone
https://github.com/Bodmer/JSON_Decoder
https://github.com/Bodmer/OpenWeather

*******************************************/

#include <TFT_eSPI.h>
#include <WiFi.h>
#include <JSON_Decoder.h> // https://github.com/Bodmer/JSON_Decoder
#include <OpenWeather.h>  // Latest here: https://github.com/Bodmer/OpenWeather
#include "NTP_Time.h"     // Attached to this sketch, see that tab for library needs
#include "perso.h"
#include "logo.h"
#include "images.h"
#include "meteo.h"


TFT_eSPI lcd = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&lcd);   // Tout l'écran
TFT_eSprite voyant = TFT_eSprite(&lcd);   // Sprite voyant
TFT_eSprite depart = TFT_eSprite(&lcd);   // Sprite écran d'accueil
TFT_eSprite sun = TFT_eSprite(&lcd);      // Sprite Hors service & soleil
TFT_eSprite Chauffe = TFT_eSprite(&lcd);  // Sprite indicateur chauffage électrique
TFT_eSprite fond = TFT_eSprite(&lcd);     // Sprite contour affichage cumuls
TFT_eSprite light = TFT_eSprite(&lcd);    // Sprite ampoule
TFT_eSprite batterie = TFT_eSprite(&lcd); // Sprite batterie
TFT_eSprite meteo = TFT_eSprite(&lcd);    // Sprite meteo

// Couleurs bargraph
#define color0 0x10A2   //Sombre 
#define color1 0x07E0   //Blanc 
#define color2 0x26FB   //Vert
#define color3 0xF780   //Bleu
#define color4 0xFC00   //Orange
#define color5 0xF9A6   //Rouge
// Couleurs pour affichage cumuls
#define color6 0xE6D8
#define color7 0xEF5D
#define color8 0x16DA // Température cumulus

// Chemin acces au fichier de données MSunPV
char path[]   = "/status.xml";

// Variables pour programme
long lastTime = 0;
long lastMSunPV = 0;
String Months[13]={"Mois","Jan","Fev","Mars","Avril","Mai","Juin","Juill","Aout","Sept","Oct","Nov","Dec"};
String IP;  // Adresse IP de connexion du Companion
uint32_t volt ; // Voltage batterie
bool wink = false;

// Pointeurs pour relance recherche valeurs
bool awaitingArrivals = true;
bool arrivalsRequested = false;

// Variables pour dimmer
const int PIN_LCD_BL = 38;
const int freq = 1000;
const int ledChannel = 0;
const int resolution = 8;
int dim = 150; // Eclairage intermédiaire au lancement
bool inverse = false;
int x;

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
String lever, coucher, date, tempExt, icone, ID;
OW_Weather ow; // Weather forecast librairie instance
// Update toutes les 15 minutes, jusqu'à 1000 requêtes par jour gratuit (soit ~40 par heure)
const int UPDATE_INTERVAL_SECS = 15 * 60UL; // 15 minutes
boolean booted = true;
long lastDownloadUpdate = millis();
String timeNow = "";

///////////////////////////////////////////////////////////////////////////////////////
//                                 Routine SETUP                                     //
/////////////////////////////////////////////////////////////////////////////////////// 
void setup(){ 
  // Allume écran (optionnel)
  //pinMode(15,OUTPUT);
  //digitalWrite(15,1);

  //Initialisation des 2 boutons
  pinMode(0, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
  // Initialisation ecran   
  lcd.init();
  lcd.setRotation(rotation); 
  lcd.setSwapBytes(true);
  // Affichage logo depart
  lcd.fillScreen(TFT_WHITE);
  depart.createSprite(300,146);
  depart.setSwapBytes(true);
  depart.pushImage(0,0,300,146,image);
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
  light.createSprite(24,24);                  // Image ampoule
  light.setSwapBytes(true);
  batterie.createSprite(24,24);               // Image batterie
  batterie.setSwapBytes(true);
  meteo.createSprite(50,50);                  // Image meteo
  meteo.setSwapBytes(true);

 //Initialisation port série et attente ouverture
  Serial.begin(115200);
  while (!Serial) {
    delay(100); // wait for serial port to connect. Needed for native USB port only
  }  
   // delete old config et vérif de deconnexion
  WiFi.disconnect(true);
  delay(1000);

  // Etablissement connexion wifi
  WiFi.mode(WIFI_STA);
  //*****************************************************************************
  // Si difficultés à se connecter >  wpa minimal (décommenter la ligne suivante)
  //WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);
  //*****************************************************************************
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("."); 
    delay(1000);   
    }
  Serial.println("WiFi connected.");
  IP=WiFi.localIP().toString();

  // Récupération de l'heure
  udp.begin(localPort);
  syncTime();

  // Paramètres pour dimmer
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(PIN_LCD_BL, ledChannel);
  depart.setTextColor(TFT_RED,TFT_WHITE);
  depart.setTextDatum(4);
  depart.drawString("CONNEXION OK (" + (IP) + ")",150,126,2);
  depart.pushSprite(10,20);
  // Tamisage écran dim 200 (va de 0 à 255)
  ledcWrite(ledChannel, dim);
}

///////////////////////////////////////////////////////////////////////////////////////
//                                 Routine LOOP                                      //
/////////////////////////////////////////////////////////////////////////////////////// 
void loop(){
  // Etat batterie
    volt = (analogRead(4) * 2 * 3.3 * 1000) / 4096;

  // Lit heure
  if(lastTime + 1000 < millis()){
    drawTime();    
    lastTime = millis();
   }

  // Données météo
    // Teste pour voir si un rafraissement est nécessaire
  if (booted || (millis() - lastDownloadUpdate > 1000UL * UPDATE_INTERVAL_SECS))
  {
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

  // Si un bouton pressé, affiche cumuls momentanément
  if (digitalRead(14) == 0) AfficheCumul();

  // Modification intensité lumineuse sous forme va  & vient
  if (digitalRead(0) == 0) Eclairage();

  booted = false;
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
  sprite.drawString("CONSOMMATION",115,126,2);

  //Panneau de droite sur l'écran : heure, date, dimer, batterie
  sprite.drawRoundRect(234,0,80,31,3,TFT_WHITE);
  sprite.drawRoundRect(234,33,80,20,3,TFT_WHITE);
  light.pushImage(0,0,24,24,bulb);
  light.pushToSprite(&sprite,296,57,TFT_BLACK);
  batterie.pushImage(0,0,24,24,pile);
  batterie.pushToSprite(&sprite,296,121,TFT_BLACK);

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
  if (icone == "13d") {meteo.pushImage(0,0,50,50,sleet); goto suite;}
  if (ID == "621") {meteo.pushImage(0,0,50,50,snow); goto suite;}
  if (ID == "741") {meteo.pushImage(0,0,50,50,fog); goto suite;}
  if (icone == "50d") {meteo.pushImage(0,0,50,50,wind); goto suite;}
  if (icone == "80d") meteo.pushImage(0,0,50,50,splash);

  suite:
  // Affiche icone metéo et température extérieure
  sprite.setTextDatum(5); // centre gauche                                                                                         
  sprite.drawString(tempExt, 306, 160,2);   
  sprite.drawCircle(310,154,2,TFT_WHITE); // pour °
  sprite.setTextDatum(4); // retour au centre milieu
  sprite.setTextColor(TFT_CYAN,TFT_BLACK);
  if (tempExt.toInt() <= 3) sprite.drawString("*", 285, 168,4);  
  sprite.setTextColor(TFT_WHITE,TFT_BLACK);
  meteo.pushToSprite(&sprite,235,120,TFT_BLACK);

  // Affichage heure et date
    sprite.drawString(String(timeNow),272,17,4);
    sprite.drawString(String(date),272,43,2);
  
  // Affichage éventuel de la température si sonde validée
  if (sonde) {
    sprite.setTextDatum(5); // centre gauche 
    sprite.drawString(TEMPCU,33,85,2);
    sprite.drawCircle(36,79,2,TFT_WHITE); // pour °
    sprite.setTextDatum(4); // retour au centre milieu
    sprite.drawCircle(25,84,20,color1);
    sprite.drawCircle(25,84,19,color1);
    if (TEMPCU.toInt() > 30) {
        sprite.drawCircle(25,84,20,color8);
        sprite.drawCircle(25,84,19,color8);
      }
    if (TEMPCU.toInt() > 50) {
        sprite.drawCircle(25,84,20,color4);
        sprite.drawCircle(25,84,19,color4);
      }
    if (TEMPCU.toInt() > 60) {
        sprite.drawCircle(25,84,20,color5);
        sprite.drawCircle(25,84,19,color5);
      }
  } 

  // Police d'affichage 
  sprite.setFreeFont(&Orbitron_Light_24);
  // Affichage valeur PV
  if (PV.toInt() >= residuel) sprite.drawString(PV +" w",115,35);   
  // Affichage valeur Cumulus
  sprite.drawString(CU +" w",115,92); 
  // Affichage valeur Consommation
  sprite.drawString(CO +" w",115,150);

  //Voyant de consommation
  if (CO.toInt() > PV.toInt()) voyant.pushImage(0,0,68,68,BtnR);
  if (PV.toInt() > CO.toInt()) voyant.pushImage(0,0,68,68,BtnB); 
  if ((PV.toInt() > CO.toInt()) and (PV.toInt() > 1200)) voyant.pushImage(0,0,68,68,BtnV);  
  if (CO.toInt() < 0) voyant.pushImage(0,0,68,68,BtnV);
  if (PV.toInt() < residuel) {
    // Hors service & lever/coucher 
    sun.pushImage(0,0,220,29,Soleil); 
    sun.pushToSprite(&sprite,3,20,TFT_BLACK);
    sprite.setTextColor(TFT_YELLOW,TFT_BLACK);
    sprite.drawString(lever, 23, 15,2);
    sprite.drawString(coucher, 203, 15,2);
    sprite.setTextColor(TFT_WHITE,TFT_BLACK);
    // Voyant
    voyant.pushImage(0,0,68,68,BtnR);
  }
                  
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
}

/***************************************************************************************
**                        Affichage de la page des cumuls
***************************************************************************************/ 
void AfficheCumul(){
  //  Dessin fenêtre noire et titre
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextColor(TFT_WHITE,TFT_BLACK);
  sprite.drawString("CUMULS (en Wh)",160,10,2); 

  // Affichage des rectangles graphiques
  fond.pushImage(0,0,158,77,pano);
  fond.pushToSprite(&sprite,0,20,TFT_BLACK);
  fond.pushToSprite(&sprite,160,20,TFT_BLACK);
  fond.pushToSprite(&sprite,0,96,TFT_BLACK);
  fond.pushToSprite(&sprite,160,96,TFT_BLACK);

  // Affichage des légendes
  sprite.setTextColor(TFT_BLACK,color6);
  sprite.drawString("Consommation",80,36,2);
  sprite.drawString("Injection",80,112,2);
  sprite.drawString("Panneaux",240,36,2);
  sprite.drawString("Cumulus",240,112,2); 

  // Affichage des valeurs avec grande police
  sprite.setTextColor(TFT_BLACK,color7);
  sprite.setFreeFont(&Roboto_Thin_24);
  sprite.drawString(CUMCO,80,68);
  sprite.drawString(CUMINJ,80,144);
  sprite.drawString(CUMPV,240,68);
  sprite.drawString(CUMBAL,240,144);

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
  
  // Récupération des données à afficher
  CO = MsgSplit[0];  // Consommation
  PV = MsgSplit[1];  // Panneaux PV 
  PV = String(abs(PV.toInt()));  // (avec prod en + ou en -)
  CU = MsgSplit[2];  // Cumulus
  TEMPCU = MsgSplit[5];  // Sonde température cumulus
  TEMPCU = TEMPCU.toInt();
  if (TEMPCU.length() < 2) TEMPCU = " " + TEMPCU; // 2 caractères
  if (nbrentier) {
    CO = String(CO.toInt()); 
    PV = String(PV.toInt());
    CU = String(CU.toInt());
  }

  // hysteresis des panneaux et cumulus
  if (PV.toInt() <= residuel) PV = "0"; // Légère consommation due aux onduleurs
  if (CU.toInt() <= residuel) CU = "0"; // Légère consommation due au thermostat du cumulus

  //  >>>>>>>>>>>>>  Routine made by Patrick, mon sauveur !  <<<<<<<<<<<<<<<
    for(int i = 0; i < 8; i++)                  //à remplacer par nbre de compteurs à afficher
    {
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
  for(int j = 0;j<8;j++) { 
    Serial.print("Cumul "); Serial.print (j); Serial.println(" >> " + MsgSplit2[j]);}

  //**************************************************************
  // Suivant les modifications que vous avez apporté au MSunPV
  // il est possible que les cumuls souhaités ne soient pas
  // ceux affichés. Vérifiez sur le moniteur série le bon index
  CUMCO = MsgSplit2[0];   // Cumul Conso
  CUMINJ = MsgSplit2[1];  // Cumul Injection
  CUMPV = MsgSplit2[2];   // Cumul Panneaux
  CUMBAL = MsgSplit2[3];  // Cumul Ballon cumulus
  //**************************************************************

  if (nbrentier) {
    CUMCO = String(CUMCO.toInt()); 
    CUMINJ = String(CUMINJ.toInt()); 
    CUMPV = String(CUMPV.toInt());
    CUMBAL = String(CUMBAL.toInt());
  }
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
  if (valeur > residuel) 
    { // Permet l'affichage d'au moins une barre si on produit
      for(i = 0;i<8;i++) sprite.fillRect(200,(44-(i*5)),20,4,color0); 
      sprite.fillRect(200,44,20,4,color1);                   
    }
  // Affichage de barres supplémentaires fonction de la prod
  for(i = 0;i<nbbarres;i++) sprite.fillRect(200,(44-(i*5)),20,4,color1);

 // Cumulus
  valeur = CU.toInt();
  ecart = cumulus/ 8;
  nbbarres = (valeur/ecart); 
  if (nbbarres > 8) nbbarres = 8;
  for(i = 0;i<8;i++) sprite.fillRect(200,(100-(i*5)),20,4,color0);   
  if (valeur > residuel) sprite.fillRect(200,100,20,4,color4);
  for(i = 0;i<nbbarres;i++) sprite.fillRect(200,(100-(i*5)),20,4,color4);    
   
  // Consommation
  valeur = CO.toInt();
  for(i = 0;i<8;i++) sprite.fillRect(200,(158-(i*5)),20,4,color0);
  if (valeur < 0) sprite.fillRect(200,153,20,9,TFT_WHITE); else sprite.fillRect(200,158,20,4,color1);
  if (valeur > 500) sprite.fillRect(200,153,20,4,color1);
  if (valeur > 1000) sprite.fillRect(200,148,20,4,color2);
  if (valeur > 1500) sprite.fillRect(200,143,20,4,color2);
  if (valeur > 2000) sprite.fillRect(200,138,20,4,color3);
  if (valeur > 2500) sprite.fillRect(200,133,20,4,color3);
  if (valeur > 3000) sprite.fillRect(200,128,20,4,color4);
  if (valeur > 4000) sprite.fillRect(200,123,20,4,color5);
}

/***************************************************************************************
**      Fait varier l'intensité d'éclairage de 50 à 250 dans un sens et l'inverse
***************************************************************************************/
void Eclairage(){
  if (inverse == false) {
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
  for(int i = 0;i<5;i++) sprite.fillRect(302,85+(i*6),12,5,color0);
  for(int i = 0;i<x;i++) sprite.fillRect(302,85+(i*6),12,5,color3);
  // Gestion batterie
  batt();
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
if (volt < 3.5) sprite.fillRect(302,127,12,3,color0);
if (volt < 3) sprite.fillRect(302,132,12,3,color0);
if (volt < 2.5) sprite.fillRect(302,137,12,3,color0);
}

/***************************************************************************************
**                           Réception des données météo
***************************************************************************************/
void donneesmeteo(){
  // Create the structures that hold the retrieved weather
  OW_current *current = new OW_current;
  OW_hourly *hourly = new OW_hourly;
  OW_daily  *daily = new OW_daily;

  ow.getForecast(current, hourly, daily, api_key, latitude, longitude, units, language);
  Serial.println("");

  // Valeurs issues de Open Weather
  Serial.println("############### Données météo ###############");
  Serial.print("Latitude            : "); Serial.println(ow.lat);
  Serial.print("Longitude           : "); Serial.println(ow.lon);
  Serial.print("Timezone            : "); Serial.println(ow.timezone);
  Serial.print("Heure actuelle      : "); Serial.println(strTime(current->dt));
  Serial.print("Lever soleil        : "); Serial.println(strTime(current->sunrise));
  Serial.print("Coucher soleil      : "); Serial.println(strTime(current->sunset));
  Serial.print("temperature         : "); Serial.println(current->temp);
  Serial.print("description         : "); Serial.println(current->description);
  Serial.print("icone               : "); Serial.println(current->icon);
  Serial.print("ID                  : "); Serial.println(current->id);
  lever = strLocalTime(current->sunrise);
  coucher = strLocalTime(current->sunset);
  date = strDate(current->dt);
  tempExt = String(current->temp, 0);  // Température sans décimale
  if (tempExt.length() < 2) tempExt = " " + tempExt; //et sur 2 caractères
  icone = (current->icon);
  ID  = (current->id);
  if (wink) icone ="80d";
  
  // Effacement des chaines pour libérer la mémoire
  delete current;
  delete hourly;
  delete daily;
}

/***************************************************************************************
**                         Relancement du cycle de lecture
***************************************************************************************/
void getArrivals() {
 // Use WiFiClient class to create TLS connection
  Serial.println("\nInitialisation de la connexion au serveur...");
  // if you get a connection, report back via serial.connect(server, 80))
  if (client.connect(server, 80)) {
    Serial.println("Connecté au serveur");
    
    // Make a HTTP request:
    client.print("GET "); client.print(path); client.println(" HTTP/1.1");
    client.print("Host: "); client.println(server);
    client.println();
  
    while(!client.available()); //wait for client data to be available
    Serial.println("Attente de la réponse serveur...");

    while(client.available()) {   
      String line = client.readStringUntil('\r');
      matchString = line; // Mise en mémoire du xml complet
    }

    Serial.println("requete validée");  
    awaitingArrivals = false;
    client.stop();
  }
}  

void resetCycle() {
 awaitingArrivals = true;
 arrivalsRequested = false;
 Serial.println("et montre l'écran...");    
}

/***************************************************************************************
**                          Reconnexion wifi en cas de perte
***************************************************************************************/
void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  depart.setTextColor(TFT_RED,TFT_WHITE);
  depart.setTextDatum(4);
  depart.drawString("   CONNEXION PERTURBEE   ",160,118,2);
  depart.drawString("    VEUILLEZ PATIENTER...    ",160,133,2);
  depart.pushSprite(10,20);
  Serial.println(info.wifi_sta_disconnected.reason);
  Serial.println("Trying to Reconnect");
  WiFi.begin(ssid, password);
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
    if(posSep<0){
      return;
    }        
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
**             Conversion Unix time to vers "local time" time string "12:34"
***************************************************************************************/
String strLocalTime(time_t unixTime)
{
  unixTime += TIME_OFFSET;

  String localTime = "";

  if (hour(unixTime) < 10) localTime += "0";
  localTime += hour(unixTime);
  localTime += ":";
  if (minute(unixTime) < 10) localTime += "0";
  localTime += minute(unixTime);

  return localTime;
}

/***************************************************************************************
**                            Decryptage de l'heure
***************************************************************************************/
void drawTime() {
  // Convert UTC to local time, returns zone code in tz1_Code, e.g "GMT"
  time_t local_time = TIMEZONE.toLocal(now(), &tz1_Code);
  timeNow = "";
  if (hour(local_time) < 10) timeNow += "0";
  timeNow += hour(local_time);
  timeNow += ":";
  if (minute(local_time) < 10) timeNow += "0";
  timeNow += minute(local_time);
}

/***************************************************************************************
**  Conversion Unix time vers local date + time string "Oct 16 17:18", fin avec nvle ligne
***************************************************************************************/
String strDate(time_t unixTime)
{
  time_t local_time = TIMEZONE.toLocal(unixTime, &tz1_Code);

  String localDate = "";
  String localDate2 = "";
  localDate += day(local_time);
  localDate2 = localDate;
  localDate += " ";
  localDate += String(Months[month(local_time)]);
  localDate2 += month(local_time);
  if (localDate2 == "14") wink = true;
  return localDate;
}

/***************************************************************************************
**     Routine de test pour afficher toutes les icones sur l'écran
**               Décommenter la ligne 239 pour l'activer
***************************************************************************************/
void test(){
  PV = "0";
  CO = "4500";
  CU = "90";
  TEMPCU = "55";
  tempExt = "3";
  sonde = true;
  chauffageElectr = true;
}
