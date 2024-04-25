#include<stdio.h>
#include<unistd.h>
#include<sys/wait.h>
#include<stdlib.h> // Para usar atoi()

int main() {
    int pfd[2];
    char nombreArchivo[100]; // Suponiendo nombres de archivo de hasta 99 caracteres + terminador null

    if (pipe(pfd) == -1) {
        printf("\nLo sentimos pero hay error al crear el pipe");
        return 1;
    }

    int pid = fork();

    if (pid == 0) { // Código del proceso hijo
        close(pfd[1]); 
        printf("\nHola, soy el hijo %d con padre %d\n", getpid(), getppid());
        printf("\nEsperando recibir los nombres de archivos para respaldar...\n");
        
        int numArchivos;
        read(pfd[0], &numArchivos, sizeof(int)); 
        
        for (int i = 0; i < numArchivos; i++) {
            read(pfd[0], nombreArchivo, sizeof(nombreArchivo)); 
            printf("\nRespaldando archivo %s (%d de %d)\n", nombreArchivo, i + 1, numArchivos);
            //Aqui respaldo de archivos, nada más que no me queda claro como :/
        }

        printf("\nRespaldo completado.\n");
        close(pfd[0]);
    } else { // Código del proceso padre
        close(pfd[0]); 
        int numArchivos = 5; // Se respaldan 5 archivos
        /*Por lo que entiendo, tambien debe de ser válido para cuando el usuario indica el número de archivos asi que:
        int numArchivos;
        printf("\nIndica número de archivos a respaldar:");
        scanf(%d,&numArchivos);
        */
        write(pfd[1], &numArchivos, sizeof(int)); 

        // Suponiendo que esos son los archivos
        //Creo que tambien los archivos deben de ser tecleados por el usuario, pero pues como ellos andan haciendo el padre, lo voy a dejar así
        char *archivos[] = {"archivo1.txt", "archivo2.txt", "archivo3.txt", "archivo4.txt", "archivo5.txt"};
        for (int i = 0; i < numArchivos; i++) {
            write(pfd[1], archivos[i], sizeof(nombreArchivo)); // Envía cada nombre de archivo
        }
        
        close(pfd[1]); 
        wait(NULL); // Esperando a que el hijo termine
    }
    return 0;
}
