#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

int main() {
    char srcDir[100]; // Directorio de origen
    char backupDir[] = "backup"; // Directorio de respaldo
    char fileListPath[] = "filelist.txt"; // Archivo que contendrá la lista de nombres de archivo
    char buf[100];  // Buffer para almacenar el mensaje recibido del hijo (para el padre)

    //Creando pipe y comprobando que se cree correctamente
    int pfd[2];
    if(pipe(pfd)==-1){
        printf("\nLo sentimos pero hay error");
        return 1;
    }


    //Se supone que sto va en el padre pero para poder trabajar con el hijo lo puse (chatgpt lo hizo:/)
    printf("Indica el directorio de origen de los archivos a respaldar: ");
    scanf("%99s", srcDir); //Leyendo directorio

    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(srcDir))) {
        perror("No se puede abrir el directorio");
        return EXIT_FAILURE;
    }

    FILE *fileList = fopen(fileListPath, "w");
    if (!fileList) {
        perror("No se puede abrir el archivo de lista de archivos");
        return EXIT_FAILURE;
    }

    int numArchivos = 0;
    //Aqui identifica archivos y carpetas, escribiendo FILE: antes del nombre para identificar que es un archivo, y escribe DIR: para identificar el directorio
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            fprintf(fileList, "FILE:%s\n", entry->d_name); 
            numArchivos++;
        } else if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            fprintf(fileList, "DIR:%s\n", entry->d_name); 
            numArchivos++;
        }
    }
    fprintf(fileList, "Total de archivos: %d\n", numArchivos); 
    fclose(fileList);
    closedir(dir);

    int pid = fork(), i=numArchivos-1; //i empieza con un numero de archivos menor pq cuenta desde el que está procesando (si tienen dudas nada mas preguntenme jajja)

    //Esto es lo que hace el HIJO
    if (pid == 0) { 
        // Crear o limpiar el directorio de respaldo
        char command[512];

        printf("\n========= RESPALDANDO %d ARCHIVOS Y/O DIRECTORIOS ==========\n", numArchivos);

        snprintf(command, sizeof(command), "rm -rf %s && mkdir %s", backupDir, backupDir);
        system(command);

        //Para verificar la lista donde están todos los archivos y directorios escritos fue creado bien o no (solo lo lee)
        FILE *fileList = fopen(fileListPath, "r");
        if (!fileList) {
            perror("No se puede abrir el archivo de lista de archivos");
            exit(EXIT_FAILURE);
        }

        char nombreArchivo[256];
        while (fgets(nombreArchivo, sizeof(nombreArchivo), fileList)) {
            if (strncmp(nombreArchivo, "Total de archivos:", 18) == 0) {
                break; //Detiene la lectura al llegar a la línea de total de archivos
            }
            nombreArchivo[strcspn(nombreArchivo, "\n")] = 0; //Elimina el salto de línea

            char *tipo = strtok(nombreArchivo, ":"); //Divide una cadena en partes, donde el delimitador son los : que ve, se le llama tipo debido a que puede ser FILE o DIR (esto ayuda a poder realizar los comandos de copiado)
            char *nombre = strtok(NULL, ":"); //Esta parte continua de donde se quedóy guarda lo que ve en nombre (guardadndo el nombre real de los archivos)

            //Aqui viene el porque se guarda en tipo
            if (strcmp(tipo, "FILE") == 0) { //Estamos comparando dos cadenas, la cadena que se guardó en tipo con la cadena FILE, si estas dos son iguales (==0) entra al ciclo y utiliza los comandos apropiados solo para copiar un archivo
                snprintf(command, sizeof(command), "cp '%s/%s' '%s/'", srcDir, nombre, backupDir);
            } else if (strcmp(tipo, "DIR") == 0) { //En otro caso, si nuestra cadena de tipo es DIR, utiliza el comando para copiar directorios
                snprintf(command, sizeof(command), "cp -r '%s/%s' '%s/'", srcDir, nombre, backupDir);
            }
            system(command);

            printf("\n(PADRE--> %s)", nombre);

            if (strcmp(tipo, "FILE") == 0) { 
                printf("\n     Hijo(pid=%d), respaldando el archivo: %s   pendientes:%d/%d\n", getpid(),nombre,i, numArchivos);
                i--;
            } else if (strcmp(tipo, "DIR") == 0) { 
                printf("\n     Hijo(pid=%d), respaldando el directorio: %s   pendientes:%d/%d\n", getpid(),nombre,i, numArchivos);
                i--;
            }

            //Mandando mensaje al padre (la parte donde el padre recibe este mensaje está en el padre)
            if(i==-1){
                //printf("\nSI ENTRA?");
                close(pfd[0]);
                char *message = "Adios padre, termine el respaldo......";
                write(pfd[1], message, strlen(message));
                close(pfd[1]);
            }
        }
        fclose(fileList);
        //exit(0);

    } else { //Pdre
        wait(NULL); // Espera a que el hijo termine
        //printf("Respaldo completado. %d archivos procesados.\n", numArchivos);

        //Nada más para ver si el padre si recibe mensaje
        close(pfd[1]); 
        read(pfd[0], buf, sizeof(buf));
        close(pfd[0]); 
        printf("\nPADRE(pid=%d), Mensaje del hijo: <--- %s\n",getppid(), buf);
    }

    return 0;
}