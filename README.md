# Processing_System_Heasca_Sirbu

1. Processing System:

Server centralizat care primeste requesturi cu sarcini de executie (prin socket)
Agent - se instaleaza pe o statie, se înregistrează în server, primește sarcini (prin socketsi).


Proiect PSO 2024

 #Server:
    - gestionarea conexiunilor cu clientii si agentii (conexiune prin socketsi)
    -primirea si stocarea task-urilor intr-o coada de task-uri
    -alocarea task-urilor catre agentii disponibili
    -colectarea raspunsurilor de la agenti
    -raspuns catre client

Client:
    -initiaza conexiunea cu serverul
    -creaza si trimite un task catre server(executabil+argumente+nivelul resurselor necesitare)
    -asteptarea si receptionarea raspunsului de la server

Agent:
    -initiaza conexiunea cu serverul
    -asteptarea si receptionarea task-ului de la server
    -executarea task-ului
    -trimiterea raspunsului catre server

