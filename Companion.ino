//////////////////////////////////////
/////          COMPANION         /////
/////   Version 09/01/23 12:30   /////
/////        @jjhontebeyrie      /////
//////////////////////////////////////
/////      Affichage déporté     /////
/////    consommation solaire    /////
/////        pour MSunPV         /////
//////////////////////////////////////
/////          ATTENTION         /////
/////  Le code est prévu pour    /////
/////    LILYGO T-Display S3     /////
//////////////////////////////////////

#include <TFT_eSPI.h>
#include <WiFi.h>
#include "time.h"
#include "Boutons.h"
#include "logo.h"
#include "divers.h"

TFT_eSPI lcd = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&lcd);   // Tout l'écran
TFT_eSprite voyant = TFT_eSprite(&lcd);   // Sprite voyant
TFT_eSprite depart = TFT_eSprite(&lcd);   // Sprite écran d'accueil
TFT_eSprite HS = TFT_eSprite(&lcd);       // Sprite hors service
TFT_eSprite Chauffe = TFT_eSprite(&lcd);  // Sprite idicateur chauffage électrique
TFT_eSprite fond = TFT_eSprite(&lcd);     // Sprite contour affichage cumuls
TFT_eSprite light = TFT_eSprite(&lcd);    // Sprite ampoule
TFT_eSprite batterie = TFT_eSprite(&lcd); // Sprite batterie

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

//*****************************************************************************
//************************  Données personnelles  *****************************
// Vos codes accès au wifi et adresse de votre MSunPV dans votre réseau
// Remplacez les * par vos valeurs
const char* ssid     = "******";
const char* password = "******";
// Adresse IP du serveur local (ici MSunPV). Il se peut suivant votre réseau
// que vous ayiez besion de remplacer toute la chaine, respectez . entre les chiffres
char server[] = "192.168.1.**";    
// Les panneaux et le cumulus peuvent avoir une mini conso de veille
// Mettez ici celle que vous estimez pour votre installation
int residuel = 10; // en Watt
// Modifiez les lignes suivantes en fonction de votre équipement
int puissance = 5000; // production max en watt
int cumulus = 3000; // puissance cumulus en watt
//*****************************************************************************
//*****************************************************************************
char path[]   = "/status.xml";
// Serveur pour heure
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec =3600;   //time zone * 3600 , Europe time zone est  +1 GTM
const int   daylightOffset_sec = 3600;
long t,tt=0;
char timeHour[3];
char timeMin[3];
char day[3];
char month[6];
// Adresse IP de connexion du Companion sur écran
String IP;  
// Variables affichant les valeurs reçues depuis le MSunPV
String PV,CU,CO; // Consos et températures. Il y a 16 valeurs, on en récupère que 3
String CUMCO,CUMINJ,CUMPV,CUMBAL; // Cumuls. Il y a 8 valeurs, on en récupère que 4

// Wifi
int status = WL_IDLE_STATUS;
WiFiClient client;
// Chaines pour decryptage
String matchString = "";
String  MsgContent;
String MsgSplit[16]; // 16 valeurs à récupérer pour les consos et températures
String MsgSplit2[8]; // 8 valeurs à récupérer pour les compteurs cumul
// Pointeurs pour relance recherche valeurs
bool awaitingArrivals = true;
bool arrivalsRequested = false;
// Voltage batterie
uint32_t volt ;

// Variables pour dimmer
const int PIN_LCD_BL = 38;
const int freq = 1000;
const int ledChannel = 0;
const int resolution = 8;
int dim = 250;
bool inverse = false;
int x;

void setup(void)
{ 
  // Allume écran (optionnel)
  pinMode(15,OUTPUT);
  digitalWrite(15,1);
  //Initialisation des 2 boutons
  pinMode(0, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
  // Initialisation ecran   
  lcd.init();
  // **********************************************************************************
  // **********************************************************************************
  // Boitier horizontal prise à gauche, pour prise à droite mettez lcd.setRotation(1); 
  lcd.setRotation(3); 
  // **********************************************************************************
  // **********************************************************************************
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
  voyant.setSwapBytes(true);                  // Pour affichage correct d'une image
  HS.createSprite(173,22);                    // Texte Hors service
  HS.setSwapBytes(true);
  Chauffe.createSprite(40,40);                // Chauffage en cours 
  Chauffe.setSwapBytes(true);
  fond.createSprite(158,77);                  // Image de fond des cumuls
  fond.setSwapBytes(true);
  light.createSprite(24,24);                  // Image ampoule
  light.setSwapBytes(true);
  batterie.createSprite(24,24);               // Image batterie
  batterie.setSwapBytes(true);

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
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Paramètres pour dimmer
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(PIN_LCD_BL, ledChannel);

  depart.setTextColor(TFT_RED,TFT_WHITE);
  depart.setTextDatum(4);
  depart.drawString("       CONNEXION OK        ",160,126,2);
  depart.pushSprite(10,20);
  // Tamisage écran dim 200 (va de 0 à 255)
  ledcWrite(ledChannel, dim);
}   

void loop()
{
  // Etat batterie
    volt = (analogRead(4) * 2 * 3.3 * 1000) / 4096;

  // Lit heure
  if(t+1000<millis()){
    printLocalTime();    
    t=millis();
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
  if(tt+15000<millis()){
    Serial.println("poll again");  
    resetCycle();
    tt=millis();
  }

  // Si un bouton pressé, affiche cumuls momentanément
  if (digitalRead(14) == 0) AfficheCumul();

  // Modification intensité lumineuse sous forme va  & vient
  if (digitalRead(0) == 0) Eclairage();
}   

void printLocalTime()
  {
  struct tm timeinfo; 
  if(!getLocalTime(&timeinfo)){
    return;
  }
  strftime(timeHour,3, "%H", &timeinfo);
  strftime(timeMin,3, "%M", &timeinfo);
  strftime(day,3, "%d", &timeinfo);
  strftime(month,6, "%b", &timeinfo); 
  }
  
void Affiche()
{
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

  //Panneau de droite sur l'écran : heure, date, voyant, etc...
  sprite.drawRoundRect(234,4,80,31,3,TFT_WHITE);
  sprite.drawRoundRect(234,40,80,20,3,TFT_WHITE);

  light.pushImage(0,0,24,24,bulb);
  light.pushToSprite(&sprite,233,139,TFT_BLACK);
  batterie.pushImage(0,0,24,24,pile);
  batterie.pushToSprite(&sprite,290,141,TFT_BLACK);

  // Affichage heure et date
  sprite.drawString(String(timeHour)+":"+String(timeMin),272,21,4); 
  sprite.drawString(String(day)+" "+String(month),272,50,2);
  
  // Affichage valeur PV    
  sprite.setFreeFont(&Orbitron_Light_24);
  if (PV.toInt() >= residuel) sprite.drawString(PV +" W",115,35);   
  // Affichage valeur Cumulus
  sprite.drawString(CU +" W",115,92); 
  // Affichage valeur Consommation
  sprite.drawString(CO +" W",115,150);

  // Affichage IP wifi et batterie
  //sprite.drawString(String(volt)+" mV",275,150,2);
  //sprite.setTextColor(TFT_DARKGREEN);  
  //sprite.setTextFont(0);
  //sprite.drawString(IP,275,160); 

  //Voyant de consommation
  if (CO.toInt() > PV.toInt()) voyant.pushImage(0,0,68,68,BtnR);
  if (PV.toInt() > CO.toInt()) voyant.pushImage(0,0,68,68,BtnB);  
  if (CO.toInt() < 0) voyant.pushImage(0,0,68,68,BtnV);
  if (PV.toInt() < residuel) { HS.pushImage(0,0,173,22,hs);
                      HS.pushToSprite(&sprite,30,25,TFT_BLACK);
                      voyant.pushImage(0,0,68,68,BtnR);
                      }
  //****************************************************************                    
  // En cas de chauffage électrique, sinon effacez les lignes du if
  //****************************************************************                     
  if ((CO.toInt() + PV.toInt() > 3000) and (CU.toInt() < 100)){  
    Chauffe.pushImage(0,0,40,40,chauffage);
    Chauffe.pushToSprite(&sprite,3,122,TFT_BLACK);
  }
  //**************************************************************** 
  
  // Appel routine affichage des graphes latéraux
  indic(); 
  // Dessin du voyant
  voyant.pushToSprite(&sprite,240,66,TFT_BLACK);
  // Le rafraichissement de l'affichage de tout l'écran est fait dans Barlight()
  Barlight();
}

void AfficheCumul()
{
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
  sprite.setFreeFont(&Orbitron_Light_24);
  sprite.drawString(CUMCO,80,68);
  sprite.drawString(CUMINJ,80,144);
  sprite.drawString(CUMPV,240,68);
  sprite.drawString(CUMBAL,240,144);

  // Rafraichissement écran (indispensable après avoir tout dessiné)
  sprite.pushSprite(0,0);
}

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

  // hysteresis des panneaux et cumulus
  if (PV.toInt() <= residuel) PV = "0"; // Légère consommation due aux onduleurs
  if (CU.toInt() <= residuel) CU = "0"; // Légère consommation due au thermostat du cumulus

  //  Routine made by Patrick, mon sauveur !
  //	String MsgSplit2[8] = {"48bfd", "fffff000", "7849", "2b5d"};

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
        //Serial.println(MsgSplit2[i]);
    }

  for(int j = 0;j<8;j++) { Serial.print("Cumul ");
                           Serial.print (j);
                           Serial.println(" >> " + MsgSplit2[j]);}

  //**************************************************************
  // Suivant les modifications que vous avez apporté au MSunPV
  // il est possible que les cumuls souhaités ne soient pas
  // ceux affichés. Vérifiez sur le moniteur série le bon index
  CUMCO = MsgSplit2[0];   // Cumul Conso
  CUMINJ = MsgSplit2[1];  // Cumul Injection
  CUMPV = MsgSplit2[2];   // Cumul Panneaux
  CUMBAL = MsgSplit2[3];  // Cumul Ballon cumulus
  //**************************************************************
}

// Cette routine découpe la partie du xml voulue en autant de valeurs que trouvées
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

// Reconnexion wifi en cas de perte
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
           
// Affichage des bargraphs verticaux
void indic()
{
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

// Fait varier le dimmage de 50 à 250 dans un sens et l'inverse
void Eclairage()
{
  if (inverse == false) {
    dim = dim - 50;
    if (dim <= 50) {dim = 50;
                   inverse = true;}
  }
  else
  {
    dim = dim + 50;
    if (dim >= 250) {dim = 250;
                   inverse = false;}
  }
    delay(300);
    Barlight();
}

// Affichage du graphe intensité lumineuse
void Barlight()
{
  x = dim/50; // steps de 50
  for(int i = 0;i<5;i++) sprite.fillRect(256+(i*6),147,5,12,color0);
  for(int i = 0;i<x;i++) sprite.fillRect(256+(i*6),147,5,12,color3);
  // Gestion batterie
  batt();
  // Rafraichissement de tout l'écran
  sprite.pushSprite(0,0);
  // Réglage de la luminosité
  ledcWrite(ledChannel, dim); 
}

void batt()
{
// Voltage pour batterie, les chiffres sont à modifier suivant votre batterie
if (volt < 3.5) sprite.fillRect(296,147,12,3,color0);
if (volt < 3) sprite.fillRect(296,152,12,3,color0);
if (volt < 2.5) sprite.fillRect(296,157,12,3,color0);
// Contours
sprite.drawRoundRect(289,138,26,29,3,TFT_WHITE);
sprite.drawRoundRect(234,138,56,29,3,TFT_WHITE);
}