# Companion
**Afficheur déporté pour MSunPV**

Cet afficheur est conçu pour être utilisé avec le LILYGO T-Display S3 et certaines bibliothèques sont nécessaire pour un fonctionnement correct sous Arduino IDE.

**TFT_eSPI.h par Bodmer**

Cette bibliothèque doit être installée dans Documents/Arduino/libraries. Dans le github https://github.com/Xinyuan-LilyGO/T-Display-S3, ce fichier est contenu dans le répertoire lib. Il suffit de la copier pour la coller dans libraries.

**WiFiManager par tzapu**

A rechercher grace à l'onglet bibliothèque sous Arduino IDE.

Cette version permer l'affichage de la production des panneaux photovoltaiques, de la recharge d'un cumulus et de la consommation totale.
Le rafraichissement des informations se fait toutes les 15 secondes.

Le bouton du T-Diplay situé en bas, si vous avez branché le module par la gauche affiche les cumuls de la consommation, de la production des panneaux, de la réinjection, et de la charge du cumulus.

Le bouton haut permet de régler la luminosité de l'écran avec effet de va-et-vient et rendu graphique. La charge de la batterie éventuellement installée est visualisée également.

**MSunPV**

C'est un routeur solaire permettant d'utiliser le courant normalement réinjecté dans le réseau et de l'utiliser pour recharger un cumulus, lancer une climatisation, faire fonctionner un radiateur, un moteur ou une pompe par exemple. Tous les détails sont sur le site https://ard-tek.com
