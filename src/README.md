# README 
# MARIN MARIUS DANIEL 332CC
## [Tema3 - APD](https://gitlab.cs.pub.ro/apd/tema3)

### Ideea de baza
Implementarea logicii unui `tracker` ce are ca rol gestionarea
fisierelor intre `clienti` si logica clientilor de a descarca
si uploada fisiere unii de la altii folosind MPI. 

### Implementare
#### Am folosit urmatoarele structuri:
- **`chunk_info`**, structura ce contine informatii despre segmente:
    - *`char hash[HASH_SIZE]`* - hash urile segmentelor
    - *`int peers[MAX_CHUNKS]`* - clientii care detin un anumit segment
- **`file_info`**, structura ce contine informatii despre fisiere:
    - *`char filename[MAX_FILENAME]`* - numele fisierului
    - *`int id`* - id ul fisierului (extras din nume)
    - *`int chunks_count`* - numarul de segmente
    - *`chunk_info chunk[MAX_CHUNKS]`* - informatii despre segmentele fisierelor
    - *`client owned[MAX_CLIENTS]`* - clientii care detin fisierul
    - *`client needed[MAX_CLIENTS]`* - clientii care au nevoie de fisier
- **`client`**, structura ce contine informatii despre clienti:
    - *`int flag`* - pentru marcarea clientilor care au nevoie/detin un fisier
    - *`int chunk[MAX_CHUNKS]`* - segmentele detinute de client
    - *`int count`* - numarul de segmente detinute de client

#### Am folosit urmatoarele enumeratii de tip:
- **`TAG`**, pentru a marca tipul de mesaj trimis
    - *`DOWNLOAD`* - pentru a realiza comunicatia dintre client si server (preluarea listei de fisiere)
    - *`SEND`* - pentru a trimite un mesaj de tip request intre client si server
    - *`CHUNK`* - pentru a trimite un mesaj de tip chunk intre 2 clienti
    - *`DOWN`* - pentru a trimite un mesaj de tip down
    intre client si server
- **`STATE`**, pentru a marca o stare a clientului
    - *`ANSWER`* - pentru a cere unui client un segment
    - *`SHUTDOWN`* - pentru a transmite unui client ca serverul se inchide
    - *`COMPLETE`* - pentru a transmite de la client la tracker ca a terminat de descarcat toate fisierele

#### In functia `main`:
* se initializeaza MPI si se apeleaza functia tracker si peer conform cerintei

#### In functia `peer`:
* se citeste din fisierul corespunzator fiecarui client datele despre fisierele pe care le detine si pe
care doreste sa le descarce
* se trimit catre tracker aceste informatii pe rand folosind MPI_Send
* se asteapta raspunsul de la tracker dupa ce a primit
aceste informatii de la toti clientii
* se creeaza cele 2 thread uri, unul de download si unul de upload

#### In functia `tracker`:
* se creeaza o baza de date cu informatiile despre fisiere si clienti
* se transmite catre toti clientii lista de fisiere
* se asteapta ca toti clientii sa trimita un mesaj
in care sa anunte ca au terminat de descarcat toate fisierele
* se trimite un mesaj de tip shutdown catre toti clientii

#### In thread ul de `download`:
* se receptioneaza lista de fisiere de la tracker
* se cauta pentru fiecare fisier ce trebuie descarcat clientii care detin segmente din acel fisier
* se trimite un mesaj de tip request catre clientii care detin segmente din acel fisier
* se receptioneaza mesajele de tip chunk de la clientii care detin segmente din acel fisier
* se scrie in fisierul de output segmentele receptionate
* la final se trimite un mesaj de tip complete catre tracker

#### In thread ul de `upload`:
* se receptioneaza mesajele de tip request de la clientii care au nevoie de segmente
* se receptioneaza mesajele de tip shutdown de la tracker pentru a se inchide thread ul
* se trimit mesaje de tip chunk catre clientii care au nevoie de segmente

### Mai multe detalii se regasesc in cod.