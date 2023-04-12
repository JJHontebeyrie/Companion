/******************************************************************************
**************************  Données personnelles  *****************************
****************************** (version 2.30) *********************************
***    Mettez ici toutes vos valeurs personnelles pour le fonctionnement    ***
***             correct de l'afficheur externe Companion                    ***
*******************************************************************************
***     >>>>>  Les valeurs correspondantes à vos branchements               ***
***            et sondes sont à modifier éventuellement                     ***
***            dans le fichier companion.ino                                ***
***            aux lignes 627 et suivantes pour les index                   ***
***            et 674 et suivantes pour les cumuls                          ***
******************************************************************************/

// Vos codes accès au wifi. Remplacez les * par vos valeurs
const char* ssid     = "******";
const char* password = "******";

// Adresse IP du serveur local (ici MSunPV). Il se peut suivant votre réseau
// que vous ayiez besion de remplacer toute la chaine, respectez . entre les chiffres
char serveur[] = "192.168.1.**";

// Boitier horizontal prise à gauche, pour prise à droite mettez rotation = 1
int rotation = 3;

// Les panneaux et le cumulus peuvent avoir une mini conso de veille
// Mettez ici celle que vous estimez pour votre installation
int residuel = 10; // en Watt

// Affichage en nombres entiers
// Affiche aussi les valeurs en kw quand nécessaire
bool nbrentier = true;  // ou false pour chiffres avec virgule

// Eclairage intermédiaire au lancement et en sortie de veille
// Modifiable mais mettez IMPERATIVEMENT des multiples de 50
// (les valeurs min/max sont 0/250)
int dim_choisie = 100;      // Eclairage choisi au lancement

// Modifiez les lignes suivantes en fonction de votre équipement
int puissance = 5000; // production max en watt
int cumulus = 3000; // puissance cumulus en watt

// Si l'alimentation se fait par batterie (true pour oui et false pour non)
bool lipo = false;

// Chauffage par radiateurs électriques, mettez false sinon
bool chauffageElectr = true;

// Affichage de température si vous avez installé une sonde sur le cumulus
// Mettez alors à true et vérifiez ligne 587 dans companion.ino
bool sonde = false;

// Mise en veille affichage écran quand PV=0 (idée de Defaliz)
bool veille = false;

// Localisation de votre ville et décalage horaire pour lever/coucher soleil
String latitude =  "44.8378"; // 90.0000 to -90.0000 negative for Southern hemisphere
String longitude = "-0.594"; // 180.000 to -180.000 negative for West

// Données pour openweathermap.org ( API key et ville)
String api_key = "**************************";
String units = "metric";  // ou "imperial"
String language = "fr";

// Clin d'oeil pour le 1er Avril
// Fait apparaitre un poisson à la place de l'icone météo ce jour là
bool poisson = false;

//*****************************************************************************
//*******************    Fin des données personnelles    **********************
//*****************************************************************************
