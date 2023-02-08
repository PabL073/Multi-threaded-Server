# Tema 3 Server multi-threded


### Descriere

	Taskul pentru aceasta tema a fost implementarea unei aplicatii te tip server care sa permita conectarea
	mai multor clienti. Exista un trhead care asteapta conexiunile, apoi pentru fiecare conexiune realizata, se creeaza un thread independent. Functionalitatile serverului: 
	
* LIST - Clientul va primi o listă cu fișierele disponibile pe server. 
        IN: operația
        OUT: status; număr octeți răspuns; lista de fișiere, separate prin ‘\0’

   		void list_files(const char *path, char *buffer, int *bytes_written);


* GET - Clientul va putea descărca un fișier pe baza numelui. 
        IN: operația; nr. octeți nume fișier; numele fișierului
        OUT: status; număr octeți răspuns; conținut fișier



* PUT - Clientul va putea încărca un nou fișier
        IN: operația; nr. octeți nume fișier; numele fișierului; nr. octeți conținut; conținut fișier*
        OUT: status
		
		void put_operation(int client_socket, char *message);

* DELETE - Clientul va putea șterge un fișier existent
        IN: operația; nr. octeți nume fișier; numele fișierului
        OUT: status

		void delete_operation(char *message, int new_socket_fd);


* UPDATE - Clientul va putea schimba conținutul unui fișier.*
        IN: operația; nr. octeți nume fișier; numele fișierului; octet start; dimensiune; caracterele noi
        OUT: status

		void updateFile(int new_socket_fd, char *message);


* SEARCH - Clientul va putea căuta un cuvânt în toate fișierele expuse de server și va primi o listă cu 	numele fișierelor ce conțin acea secvență în conținutul lor*
        IN: operația; nr. octeți cuvânt; cuvânt
        OUT: status; listă de fișiere separate prin ‘\0’

		
		void searchFiles(char *message,int sock);

La nivelul aplicatiei am creat 2 structuri:

	typedef struct pthread_arg_t {
    int new_socket_fd;
    struct sockaddr_in client_address;
    char message[1024];
    char *dynamicBuffer;
    char filename[256];
    int clientNo;

		} pthread_arg_t;

* argumentele pentru fiecare thread 


		typedef struct fileList {
    		char fullPath[1024];
    		int bytes;
		} fileList;


* retin o lista cu fisierele disponibile pe server

	
### Graceful temination

	In cazul in care serverul primeste unul dintre semnalele SIGTERM, SIGINT, acesta asteapta pana cand 
	toti clienti conectati la acel moment se deconecteaza. Astfel, clientii conectati vor putea trimite comenzi catre server.

	Aceasta functionalitate a fost implementata cu handlerul de semnal:
	void signal_handler(int signal_number);
	
	* Am inregistrat semnalele:
		signal(SIGINT, signal_handler)
		signal(SIGTERM, signal_handler)
	
	* Astept terminarea threadurilor in handlerul de semnal

	
### Log file

	void logOperation( char *operation,  char *filename, char *searchWord);
	
	Cu ajutorul acestei functii creez un fisier disponibil global la nivelul aplicatiei.
	Functia este thread-safe, ptotejata de un mutex.


### Testare

	make 

	./server

	Introduceti numarul portului: 
	(trebuie sa fie mai mare decat 1024)

	Deschidem alt terminal si scriem comanda:
	nc localhost <port> 

	Pentru a conecta mai multi clienti, deschidem terminale diferite si scriem aceeasi comanda.
	
	Pentru a deconecta un Client apsam Ctrl+C.

	

	


