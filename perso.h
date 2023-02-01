//*****************************************************************************
//************************  Données personnelles  *****************************
//*****************************************************************************
//     Mettez ici toutes vos valeurs personnelles pour le fonctionnement    ***
//              correct de l'afficheur externe Companion                    ***
//*****************************************************************************

// Vos codes accès au wifi et adresse de votre MSunPV dans votre réseau
// Remplacez les * par vos valeurs
const char* ssid     = "******";
const char* password = "******";

// Adresse IP du serveur local (ici MSunPV). Il se peut suivant votre réseau
// que vous ayiez besion de remplacer toute la chaine, respectez . entre les chiffres
char server[] = "192.168.1.**";

// Boitier horizontal prise à gauche, pour prise à droite mettez rotation = 1
int rotation = 3;

// Les panneaux et le cumulus peuvent avoir une mini conso de veille
// Mettez ici celle que vous estimez pour votre installation
int residuel = 10; // en Watt

// Modifiez les lignes suivantes en fonction de votre équipement
int puissance = 5000; // production max en watt
int cumulus = 3000; // puissance cumulus en watt

// Chauffage par radiateurs électriques, mettez false sinon
bool chauffageE = true;

// Affichage de température si vous avez installé une sonde sur le cumulus
// Mettez alors à true et vérifiez dans la routine 'decrypte' la bonne sonde
bool sonde = false;

// Localisation de votre ville et décalage horaire pour lever/coucher soleil
double latitude = 44.8378;
double longitude = -0.594;
int utc_offset = +1;