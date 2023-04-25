#ifndef OUTILS_H
#define OUTILS_H

//Liste des types des messages possibles 
#define INVALIDATION    -1      //si type = invalidation indique que celui qui recoit ce type de message doit invalider la page reçue
#define LOCK_OK         1       //si esclave recoit message de type VERROU_OK cela signifie qu'il a obtenu le verrou
#define LOCK_READ       2       //esclave envoie message de type LOCK_READ au maître avec numéro de page s'il souhaite avoir accès en lecture à une certaine page
#define LOCK_WRITE      3       //esclave envoie message de type LOCK_WRITE au maître avec numéro de page s'il souhaite avoir accès en lecture/ecriture à une certaine page
#define GET_PAGE        4       //ce type de message est envoyé par l'esclave au mettre lors d'un défaut de page pour récupérer la page souhaitée

typedef struct attente {
    int id;                 //identitfiant (socket) de l'esclave
    char mode;              //r = lecture et w = ecriture
    struct attente* next;
    struct attente* prev;
} attente;            //Cette structure permet d'inserer les esclaves dans une liste d'attente en connaissant le mode avec lequel ils veulent accéder à la page


void push(attente** liste, int id, char mode);
attente* pop(attente** liste);
int isEmpty(attente* liste);



#endif