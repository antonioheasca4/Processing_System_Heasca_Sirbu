# Processing_System_Heasca_Sirbu

Processing System:

Server centralizat care primeste requesturi cu sarcini de executie (prin socket)
Agent - se instaleaza pe o statie, se înregistrează în server, primește sarcini (prin socketsi).


Proiect PSO 2024

 ## Server
    - gestionarea conexiunilor cu clientii si agentii (conexiune prin socketsi)
    -primirea si stocarea task-urilor intr-o coada de task-uri
    -alocarea task-urilor catre agentii disponibili
    -colectarea raspunsurilor de la agenti
    -raspuns catre client

## Client
    -initiaza conexiunea cu serverul
    -creaza si trimite un task catre server(executabil+argumente+nivelul resurselor necesitare)
    -asteptarea si receptionarea raspunsului de la server

## Agent
    -initiaza conexiunea cu serverul
    -asteptarea si receptionarea task-ului de la server
    -executarea task-ului
    -trimiterea raspunsului catre server


## 13.11.2024

-Deoarece aplicatia noastra trebuie sa poata executa mai multe sarcini repetitive, am decis sa  folosind mecanismul thread pool pentru a gestionam intr-un mod eficient thread-urile. In loc sa se creeze un thread nou la fiecare conexiune, server-ul va refolosi thread-urile existente(coada de conexiuni clienti/agenti).
Avand in vedere ca server-ul trebuie sa gestioneze si clientii si agentii, am decis sa implementam doua thread pool-uri.

-Referitor la transmisia task-urilor si avand in vedere ca executabilele vor rula pe sisteme diferite, va trebui sa trimitem continutul fisierelor, urmand ca destinatarii sa creeeze noi fisiere local.

-Am definit sablonul pe care ne-am propus sa il respectam in implementarea codului. Obs. Poate suferi modificari masive. :)  



