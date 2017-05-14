#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

//-----------------------------------------------------------
//Entrego el codigo de la parte obligatoria ya que  tengo hechas la de atencion al cliente y la de introducir el numero de cajeros
//y clientes al ejecutar, pero me hace cosas raras a la hora de atender al cliente. Dejo la de introducir el numero de cajeros y
//clientes comentada.
//Tambien, la parte obligatoria se me queda colgada a los 7 clientes, y no se si es cosa de mi sistema operativo, espero que si.
//............................................................
//semaforos
pthread_mutex_t semaforo_cola;
pthread_mutex_t semaforo_fichero;
pthread_mutex_t semaforo_iniciar_variables;
pthread_mutex_t semaforo_reponedor;

//variables de condicion
pthread_cond_t condicionReponedor;

//structs
struct cliente{
	pthread_t thread;
	int ID;
	int atendido;	//1 si esta atendido, 0 si no.
	int terminado;	//1 si ha terminado de atenderle, 0 si no.

};

struct cajero{
	int ID;
	int atendiendo; //1 si si, 0 si no
	int clientesAtendidos;
	int descansando;
	pthread_t thread;
};

//otras variables globales
FILE *logFile;
char *logFileName;
int numeroClientes;
int numeroCajeros;
int contadorClientes;

struct cliente cliente[20];
struct cajero cajero[3];

//struct cliente *cliente;
//struct cajero *cajero;

//Funciones

int comprobarSitio();

void escribirLog(char *msg1);

int CalculaAleatorio (int min, int max, int semilla);

void terminar(int sig);

int buscarClienteSiguiente();

void *HiloCliente(void *idCliente);

void *HiloCajero(void *arg);

void *HiloReponedor(void *arg);

void nuevoCajero(int id);

void nuevoCliente(int sig);



//int main(int argc, char  *argv[]){

int main(void){
	//variables locales
	int i;
	int j;
	pthread_t reponedor;
	
	//asignacion de manejadores
	if(signal(SIGUSR1, nuevoCliente) == SIG_ERR){
			perror("Error en la llamada a signal.\n");
			exit(-1);
	}  
	if(signal(SIGINT, terminar) == SIG_ERR){
			perror("Error en la llamada a signal.\n");
			exit(-1);
	}

	pthread_mutex_init(&semaforo_iniciar_variables, NULL);

	//Inicializacion de variables-------------------
	pthread_mutex_lock(&semaforo_iniciar_variables);
	
	
	pthread_mutex_init(&semaforo_cola, NULL);
	pthread_mutex_init(&semaforo_fichero, NULL);
	pthread_mutex_init(&semaforo_reponedor, NULL);
	pthread_cond_init(&condicionReponedor, NULL);

	contadorClientes = 0;
	numeroClientes = 20;
	numeroCajeros = 3;
	
	//numeroCajeros = atoi(argv[1]);
	//numeroClientes = atoi(argv[2]);
	//cajero = (struct cajero*) malloc(sizeof(struct cajero) * numeroCajeros);
	//cliente = (struct cliente*) malloc(sizeof(struct cliente) * numeroClientes);
	
	//Creacion del hilo del reponedor
	pthread_create(&reponedor, NULL, HiloReponedor, NULL);
	
	pthread_mutex_unlock(&semaforo_iniciar_variables);
	//------------------------------------------------

	//inicializacion del fichero----------
	pthread_mutex_lock(&semaforo_fichero);

	logFileName = "./registroCola.log";
	logFile = fopen(logFileName, "w");
	fclose(logFile);

	pthread_mutex_unlock(&semaforo_fichero);
	//-----------------------------------


	//bloqueo el acceso a la cola para inicializarla
	pthread_mutex_lock(&semaforo_cola);
	for(i = 0; i < numeroClientes; i++){
		cliente[i].ID = 0;
		cliente[i].atendido = 0;
		cliente[i].terminado = 0;
	} 
	pthread_mutex_unlock(&semaforo_cola);
	//-----------------------------------

	//Creacion de cajeros
	for(j = 0; j < numeroCajeros; j++){
		nuevoCajero(j);
	} 

	//Se queda a la espera de seÒales	
	while(1){
		pause();	
	}

	return 0;
}




//Funcion que comprueba el sitio en la cola, si hay sitio devuelve el sitio, si no -1
int comprobarSitio(){

	int sitio = -1;
	int i;
	for(i = 0; i < 20; i++){
		if(cliente[i].ID == 0){
			sitio = i;
			return sitio;
		}
	}
	return sitio;
};


//Funcion que escribe en el log
void escribirLog(char *msg1){
	//bloquea el acceso al fichero para que solo uno pueda acceder al mismo tiempo

	//Calculamos la hora actual
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	char stnow[19];
	strftime(stnow, 19, "%d/%m/%y %H:%M:%S", tlocal);
	logFile = fopen(logFileName, "a");
	fprintf(logFile, "[%s] : %s \n", stnow, msg1);
	fclose(logFile);
	
};

//Funcion qye devuelve un numero aleatorio entre los dos argumentos que se pasan+1, la semilla permite que se creen aleatorios distintos
int CalculaAleatorio (int min, int max, int semilla){
	int numeroAleatorio;    
	time_t seconds;
	seconds = time(NULL);
	srand(seconds*semilla);
	numeroAleatorio = rand() % (max - min + 1) + min + 1;
	return numeroAleatorio;
}




//Funcion manejadora de la seÒal SIGINT para terminar el programa
void terminar(int sig){
	char totalClientes[60];
	char clientesParticulares[60];
	int i;
	if(signal(SIGINT, terminar) == SIG_ERR){
			perror("Error en la llamada a signal.\n");
			exit(-1);
		} 
	sprintf(totalClientes, "El numero total de clientes que han llegado al supermercado han sido: %d", contadorClientes);
	
	pthread_mutex_lock(&semaforo_fichero);	
	escribirLog(totalClientes);
	pthread_mutex_unlock(&semaforo_fichero);

	for(i = 0; i < numeroCajeros; i++){
		sprintf(clientesParticulares, "El numero de clientes atendidos por el cajero %d ha sido %d.", cajero[i].ID, cajero[i].clientesAtendidos);

		pthread_mutex_lock(&semaforo_fichero);		
		escribirLog(clientesParticulares);
		pthread_mutex_unlock(&semaforo_fichero);
	}
	//free(cajero);
	//free(cliente);

	exit(0);
}

//Funcion que devuelve la posicion del siguinte cliente, es decir, el de mas bajo ID
//el semaforo sirve para que al mirar quien es el cliente siguiente, ninguno lo este modificando
int buscarClienteSiguiente(){
	
	int posicion = -1;
	int idMasBajo = contadorClientes;
	int i;
	for(i = 0; i < numeroClientes; i++){
		if(cliente[i].ID <= idMasBajo &&  cliente[i].ID != 0 && cliente[i].atendido == 0){
			posicion = i;
			idMasBajo = cliente[i].ID;
		}
	}	
	return posicion;
}

//Acciones que hace el cliente
void *HiloCliente(void *arg) {
	char clienteCreado[50];
	char mensajeClienteCansado[70];
	char mensajeTerminado[70];
	int seCansa;
	int tiempoEsperando;
	int esAtendido;
	
	//El argumento pasado es el ID del cajero, que empieza en 1, y que es igual a la posicion en el array +1
	int idCliente = *(int *)arg;
	int posicionEnArray = idCliente-1;
	int haTerminado;
	
	
	//Creacion de mensajes
	sprintf(mensajeClienteCansado, "El cliente %d se ha cansado de esperar, se va del supermercado.", idCliente);
	sprintf(clienteCreado, "El cliente %d ha llegado, y se ha puesto en la cola.", idCliente);
	sprintf(mensajeTerminado, "El cliente %d ha terminado, y se va del supermercado.", idCliente);

	pthread_mutex_lock(&semaforo_fichero);	
	escribirLog(clienteCreado);
	pthread_mutex_unlock(&semaforo_fichero);
	
	seCansa =  CalculaAleatorio(0,9, idCliente);	
	
	haTerminado = 0;
	//Semaforo para mirar el estado de un cliente, por si alguien lo esta modificando.	
	pthread_mutex_lock(&semaforo_cola);	
	esAtendido = cliente[posicionEnArray].atendido;
	pthread_mutex_unlock(&semaforo_cola);
	//-----------------------------------
	//printf("El cliente %d ha saido del mutex para comprobar su estado.\n", idCliente);


	

	while(esAtendido != 1){
		if(tiempoEsperando >= 10 && seCansa == 1){
			//Si el cliente se cansa, se ponen a 0 todos sus atributos para liberar el espacio de la cola, luego se sale del hilo    
			pthread_mutex_lock(&semaforo_fichero);	   		
			escribirLog(mensajeClienteCansado);
			pthread_mutex_unlock(&semaforo_fichero);

			//---------------------------------
			pthread_mutex_lock(&semaforo_cola);			
			cliente[posicionEnArray].ID = 0;
   			cliente[posicionEnArray].atendido = 0 ;	//1 si esta atendido, 0 si no.
			cliente[posicionEnArray].terminado = 0 ;
			pthread_mutex_unlock(&semaforo_cola);
			//----------------------------------
			pthread_exit(NULL);
		}else{
			sleep(1);
			//printf("El cliente %d duerme un segundo.\n", idCliente);
			tiempoEsperando++;
			//--------------------------
			pthread_mutex_lock(&semaforo_cola);	
			esAtendido = cliente[posicionEnArray].atendido;
			pthread_mutex_unlock(&semaforo_cola);	
			//printf("El cliente %d ha saido del mutex para comprobar su estado (%d).\n", idCliente, esAtendido);	
		}
	}
	while(esAtendido == 1){
		pthread_mutex_lock(&semaforo_cola);	
		haTerminado = cliente[posicionEnArray].terminado;
		pthread_mutex_unlock(&semaforo_cola);
		
		if(haTerminado){
			pthread_mutex_lock(&semaforo_fichero);	
			escribirLog(mensajeTerminado);
			pthread_mutex_unlock(&semaforo_fichero);

			//---------------------------------
			pthread_mutex_lock(&semaforo_cola);			
			cliente[posicionEnArray].ID = 0;
   			cliente[posicionEnArray].atendido = 0 ;	//1 si esta atendido, 0 si no.
			cliente[posicionEnArray].terminado = 0 ;
			pthread_mutex_unlock(&semaforo_cola);
			//----------------------------------
			
			pthread_exit(NULL);		
		}else{
			sleep(1);	
		}
	}
}

//Acciones que hace el cajero
void *HiloCajero(void *arg) {
    //variables locales
	int posicionCliente;
	int tiempoAtencion;
	int precio;
	int problemas;
	int clientesAtendidos;
	char cajeroAtiende[50];
	
	char seVaAlCafe[50];
	char vuelveDelCafe[50];
	char tiketCompra[60];
	char finReponedor[60];
	//El argumento pasado es el ID del cajero, que empieza en 1, y que es igual a la posicion en el array +1
	int idCajero = *(int *)arg;
	int posicionCajeroEnArray = idCajero-1;
	
	//printf("Acabo de crear el hilo del cajero %d.\n", idCajero);

	sprintf(seVaAlCafe, "El cajero %d se va a tomar el cafe.", idCajero);
	sprintf(vuelveDelCafe, "El cajero %d ha vuelto de tomar el cafe.", idCajero);
	


	while(1){
			//Si el numero de clientes atendidos por el cajero es multiplo de 10, y aun no esta descansando, se va a tomar un cafe
			pthread_mutex_lock(&semaforo_cola);			
			clientesAtendidos = cajero[posicionCajeroEnArray].clientesAtendidos;
			posicionCliente = buscarClienteSiguiente();
			pthread_mutex_unlock(&semaforo_cola);
			//
			
			if(clientesAtendidos != 0 &&  clientesAtendidos%10 == 0 && cajero[idCajero].descansando == 0){
				pthread_mutex_lock(&semaforo_fichero);	
				escribirLog(seVaAlCafe);
				pthread_mutex_unlock(&semaforo_fichero);
				
				pthread_mutex_lock(&semaforo_cola);	
				cajero[idCajero].descansando = 1;
				pthread_mutex_unlock(&semaforo_cola);
				
				sleep(20);

				
				pthread_mutex_lock(&semaforo_fichero);	
				escribirLog(vuelveDelCafe);
				pthread_mutex_unlock(&semaforo_fichero);

			}else if(posicionCliente == -1){
				sleep(1);		//Si la posicion es -1 significa que la cola esta vac√≠a, por lo que duerme y vuelve a mirar si hay alguien
			}else if(posicionCliente != -1){
				pthread_mutex_lock(&semaforo_cola);	
				cajero[idCajero].descansando = 1;
				pthread_mutex_unlock(&semaforo_cola);
				
				
				//si la posicion del cliente es -1 significa que la cola de clientes esta vacia, duerme un segundo y vuelve a buscar
				pthread_mutex_lock(&semaforo_cola);
				cliente[posicionCliente].atendido = 1;
				cajero[posicionCajeroEnArray].clientesAtendidos++;
				sprintf(cajeroAtiende, "El cajero %d atiende al cliente %d.", idCajero, cliente[posicionCliente].ID);
				pthread_mutex_unlock(&semaforo_cola);

				pthread_mutex_lock(&semaforo_fichero);	
				escribirLog(cajeroAtiende);
				pthread_mutex_unlock(&semaforo_fichero);

				problemas = CalculaAleatorio(0, 99, idCajero);
				
				tiempoAtencion = CalculaAleatorio(1, 5, idCajero);
				sleep(tiempoAtencion);

				//El 70% de los clientes no tiene ningun problema con la compra.
				if(problemas <= 70){
					precio = CalculaAleatorio(1, 100, idCajero);
					
					pthread_mutex_lock(&semaforo_cola);
					sprintf(tiketCompra, "El cajero %d ha terminado de atender al cliente %d, con un precio total de %d ‚Ç¨, sin ningun problema.", idCajero, cliente[posicionCliente].ID, precio);
					pthread_mutex_unlock(&semaforo_cola);

					pthread_mutex_lock(&semaforo_fichero);	
					escribirLog(tiketCompra);	
					pthread_mutex_unlock(&semaforo_fichero);

					pthread_mutex_lock(&semaforo_cola);				
					cliente[posicionCliente].terminado = 1;
					pthread_mutex_unlock(&semaforo_cola);	

				}else if(problemas <= 95){

					pthread_mutex_lock(&semaforo_cola);
					sprintf(tiketCompra, "El cliente %d tiene problemas con la compra, el cajero %d llama al reponedor.", cliente[posicionCliente].ID, idCajero);
					pthread_mutex_unlock(&semaforo_cola);

					pthread_mutex_lock(&semaforo_fichero);	
					escribirLog(tiketCompra);
					pthread_mutex_unlock(&semaforo_fichero);

					//llama al reponedor				
					pthread_cond_signal(&condicionReponedor);
					pthread_mutex_lock(&semaforo_reponedor);   
					//Espera a que el reponedor le devuelva la seÒal de condicion
					pthread_cond_wait(&condicionReponedor, &semaforo_reponedor);
					pthread_mutex_unlock(&semaforo_reponedor); 
		
					pthread_mutex_lock(&semaforo_cola);
					sprintf(finReponedor, "El reponedor ha terminado con el cliente %d, y se va.", cliente[posicionCliente].ID);
					pthread_mutex_unlock(&semaforo_cola);

					pthread_mutex_lock(&semaforo_fichero);	
					escribirLog(finReponedor);
					pthread_mutex_unlock(&semaforo_fichero);
					
					pthread_mutex_lock(&semaforo_cola);
					cliente[posicionCliente].terminado = 1;
					pthread_mutex_unlock(&semaforo_cola);
					//llamar al reponedor

				}else if(problemas <= 100){
					pthread_mutex_lock(&semaforo_cola);
					sprintf(tiketCompra,"El cliente %d no tiene dinero suficiente para realizar la compra.", cliente[posicionCliente].ID);
					pthread_mutex_unlock(&semaforo_cola);	

					pthread_mutex_lock(&semaforo_fichero);	
					escribirLog(tiketCompra);
					pthread_mutex_unlock(&semaforo_fichero);

					pthread_mutex_lock(&semaforo_cola);
					cliente[posicionCliente].terminado = 1;
					pthread_mutex_unlock(&semaforo_cola);				
				}
						
				}		
			}
}

//Acciones que hace el reponedor
void *HiloReponedor(void *arg) {
	int tiempoQueTarda;
	while(1){
		pthread_mutex_lock(&semaforo_reponedor);   
		//Espera a que le manden la seÒal de condicion
		pthread_cond_wait(&condicionReponedor, &semaforo_reponedor);
		pthread_mutex_unlock(&semaforo_reponedor); 
		tiempoQueTarda = CalculaAleatorio(0, 4, getpid());
		sleep(tiempoQueTarda);
		pthread_cond_signal(&condicionReponedor);

	}	
}




//Funcion que crea nuevos cajeros e inicia sus hilos
void nuevoCajero(int id){
	char creaCajero[60];
	cajero[id].ID = id+1;
	cajero[id].atendiendo = 0;
	cajero[id].clientesAtendidos = 0;
	cajero[id].descansando = 0;
	//printf("Acabo de crear el cajero %d.\n", cajero[id].ID);
	sprintf(creaCajero, "El cajero %d ha llegado al trabajo.", cajero[id].ID);
	
	pthread_mutex_lock(&semaforo_fichero);	
	escribirLog(creaCajero);
	pthread_mutex_unlock(&semaforo_fichero);

	pthread_create(&cajero[id].thread, NULL, HiloCajero, &cajero[id].ID);
}

//Funcion que crea nuevos clientes e inicia sus hilos
void nuevoCliente(int sig){
	char mensaje[80];
	char colaLLena[80];
	char mensajeError[50];
	int sitio;
	int error;
	if(signal(SIGUSR1, nuevoCliente) == SIG_ERR){
			sprintf(mensaje, "Error en la llamada a signal.\n");

			pthread_mutex_lock(&semaforo_fichero);	
			escribirLog(mensaje);
			pthread_mutex_unlock(&semaforo_fichero);

			exit(-1);
		} 
	
	//printf("Ha llegado un nuevo cliente.\n");
	pthread_mutex_lock(&semaforo_cola);
	sitio = comprobarSitio();
	pthread_mutex_unlock(&semaforo_cola);
	//printf("Su sitio va a ser %d.\n", sitio);
	if(sitio == -1){
		sprintf(colaLLena, "Ha llegado un nuevo cliente, pero no hay sitio en la cola, as√≠ que se va");

		pthread_mutex_lock(&semaforo_fichero);	
		escribirLog(colaLLena);
		pthread_mutex_unlock(&semaforo_fichero);

	}else{
		pthread_mutex_lock(&semaforo_cola);
		contadorClientes++;
		cliente[sitio].ID = contadorClientes;
		cliente[sitio].atendido = 0;
		cliente[sitio].terminado = 0;
		pthread_mutex_unlock(&semaforo_cola);
		pthread_create(&cliente[sitio].thread, NULL, HiloCliente, &cliente[sitio].ID);
		
	}
}


