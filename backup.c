//Sistemas Operativos: Examen practico de procesos
//Carmona Ayala Mariana Zoe
//Carranza Paula Jose Carlos
//Gonzalez Nava Alicia Aislinn
//Quintero Montero Francisco Joshua

//Librerias a utilizar
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>


//Prototipos de funcion
// Prototipo de la función para copiar un archivo o directorio
void copiar(const char *origen, const char *destino_base);

// Prototipo de la función para obtener rutas de origen y destino
int obtener_rutas(int argc, char *argv[], char *srcDir, char *destDir);

// Prototipo de la función para crear un archivo de texto con los nombres de archivos a respaldar
int crear_lista_archivos(const char *srcDir, const char *fileListPath);

// Prototipo de la función para agregar un sufijo al directorio de destino
void agregarbackup(char *destDir, const char *anadir, size_t destSize);

// Prototipo de la función para copiar archivos específicamente
void copiar_archivo(const char *origen, const char *destino);

// Prototipo de la función para copiar directorios específicamente
void copiar_directorio(const char *origen, const char *destino);

// Prototipo de la función para vaciar un directorio
void vaciar_directorio(const char *path);

// Prototipo de la función para enviar una ruta a través de un pipe
void enviar_ruta(int fd, const char *ruta);

// Prototipo de la función para listar y enviar archivos y directorios a través de un pipe
void listar_y_enviar(int pipe_envio, int pid_padre, const char *ruta_directorio);

char globalDestDir[100];
int globalTotArchivos;

int obtener_rutas(int argc, char *argv[], char *srcDir, char *destDir) {
    if (argc != 3) {
        int opcion;
        do {
            printf("Seleccione una opcion: 0 para ingresar datos, 1 para leer desde un archivo\n");
            scanf("%d", &opcion);
            getchar(); // consume the newline character left in the input buffer by scanf

            switch (opcion) {
                case 0:
                    printf("Indica el directorio de origen de los archivos a respaldar: ");
                    scanf("%99s", srcDir); // Leyendo directorio de origen
                    getchar(); // consume the newline character left in the input buffer

                    printf("Indica el directorio de destino: ");
                    scanf("%99s", destDir); // Leyendo directorio de destino
                    strcpy(globalDestDir, destDir);
                    getchar(); // consume the newline character left in the input buffer
                    return 0;

                case 1: {
                    char nombreArchivo[100];
                    printf("Ingrese el nombre del archivo: ");
                    fgets(nombreArchivo, sizeof(nombreArchivo), stdin);
                    nombreArchivo[strcspn(nombreArchivo, "\n")] = 0; // Remove newline
                    
                    FILE *rutas = fopen(nombreArchivo, "r");
                    if (rutas == NULL) {
                        perror("Error al abrir el archivo");
                        return 1;
                    }

                    char linea[200];
                    while (fgets(linea, sizeof(linea), rutas) != NULL) {
                        if (strstr(linea, "ruta de origen:") != NULL) {
                            sscanf(linea, "ruta de origen: %99[^\n]", srcDir);
                        } else if (strstr(linea, "ruta de destino:") != NULL) {
                            sscanf(linea, "ruta de destino: %99[^\n]", destDir);
                            strcpy(globalDestDir, destDir);
                        }
                    }
                    fclose(rutas);
                    return 0;
                }

                default:
                    printf("Por favor, ingrese una opcion valida.\n");
                    break;
            }
        } while (opcion != 0 && opcion != 1);
    } else {
        strncpy(srcDir, argv[1], sizeof(srcDir) - 1);
        srcDir[sizeof(srcDir) - 1] = '\0';
        strncpy(destDir, argv[2], sizeof(destDir) - 1);
        destDir[sizeof(destDir) - 1] = '\0';
        return 0;
    }
    return 0;
}


int crear_lista_archivos(const char *srcDir, const char *fileListPath) {
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(srcDir))) {
        perror("No se puede abrir el directorio");
        return EXIT_FAILURE;
    }

    FILE *fileList = fopen(fileListPath, "w");
    if (!fileList) {
        perror("No se puede abrir el archivo de lista de archivos");
        closedir(dir);
        return EXIT_FAILURE;
    }

    int numArchivos = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {  // Es un archivo regular
            fprintf(fileList, "FILE:%s\n", entry->d_name);
            numArchivos++;
        } else if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {  // Es un directorio
            fprintf(fileList, "DIR:%s\n", entry->d_name);
            numArchivos++;
        }
    }
    fprintf(fileList, "Total de archivos: %d\n", numArchivos);
    fclose(fileList);
    closedir(dir);
    return EXIT_SUCCESS;
}

void agregarbackup(char *destDir, const char *anadir, size_t destSize) {
    //printf("Debug: Entrando a agregarbackup\n"); // Depuración
    if (strlen(destDir) + strlen(anadir) < destSize - 1) {
        strcat(destDir, anadir);  // Concatena 'anadir' al final de 'destDir'
        //printf("\nRuta actualizada: %s\n", destDir); // Muestra la ruta actualizada
    } else {
        printf("No hay suficiente espacio en el buffer para agregar la ruta.\n");
    }
    //printf("Debug: Saliendo de agregarbackup\n"); // Depuración
}


// Función para copiar archivos
void copiar_archivo(const char *origen, const char *destino) {
    FILE *src, *dst;
    src = fopen(origen, "rb");
    if (src == NULL) {
        perror("Error al abrir el archivo de origen");
        return;
    }

    dst = fopen(destino, "wb");
    if (dst == NULL) {
        perror("Error al abrir el archivo de destino");
        fclose(src);
        return;
    }

    char buffer[1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }

    fclose(src);
    fclose(dst);
}

// Función para copiar directorios
void copiar_directorio(const char *origen, const char *destino) {
    mkdir(destino, 0777);  // Crear el directorio destino
    DIR *dir;
    struct dirent *entrada;
    dir = opendir(origen);
    if (dir == NULL) {
        perror("Error al abrir el directorio de origen");
        return;
    }

    while ((entrada = readdir(dir)) != NULL) {
        if (strcmp(entrada->d_name, ".") == 0 || strcmp(entrada->d_name, "..") == 0) {
            continue;
        }

        char path_origen[1024];
        snprintf(path_origen, sizeof(path_origen), "%s/%s", origen, entrada->d_name);
        char path_destino[1024];
        snprintf(path_destino, sizeof(path_destino), "%s/%s", destino, entrada->d_name);

        struct stat statbuf;
        if (stat(path_origen, &statbuf) == -1) {
            perror("Error al obtener información de archivo/directorio");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            copiar_directorio(path_origen, path_destino);
        } else {
            copiar_archivo(path_origen, path_destino);
        }
    }

    closedir(dir);
}

// Función principal para copiar archivo o directorio
void copiar(const char *origen, const char *destino_base) {
    struct stat statbuf;
    if (stat(origen, &statbuf) == -1) {
        perror("Error al obtener información del archivo/directorio");
        return;
    }

    char destino_final[1024];
    snprintf(destino_final, sizeof(destino_final), "%s/%s", destino_base, strrchr(origen, '/'));

    if (S_ISDIR(statbuf.st_mode)) {
        copiar_directorio(origen, destino_final);
    } else {
        copiar_archivo(origen, destino_final);
    }
}



void vaciar_directorio(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        printf("Directorio de respaldo '%s' ya limpio.\n", path);
        mkdir(path, 0777);  // Intenta crear el directorio si no existe
        return;
    }

    struct dirent *entry;
    int found,bandera = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if(bandera==0){
          printf("PADRE(pid=%d): borrando respaldo viejo...\n",getpid());
          bandera++;
        }
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            found = 1;
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
            remove(full_path);
            printf("removed '%s'\n", entry->d_name);
        }
    }
    closedir(dir);
    if (found) {
        printf("Directorio de respaldo '%s' limpiado y recreado.\n", path);
    } else {
        printf("Respaldando en directorio '%s' existente.\n", path);
    }
}

void enviar_ruta(int fd, const char *ruta) {
    int longitud = strlen(ruta) + 1;  // +1 para incluir el caracter nulo
    write(fd, &longitud, sizeof(longitud));
    write(fd, ruta, longitud);
}

void listar_y_enviar(int pipe_envio, int pid_padre, const char *ruta_directorio) {
    DIR *dir = opendir(ruta_directorio);
    if (!dir) {
        perror("No se puede abrir el directorio");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    int numArchivos = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG || (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)) {
            numArchivos++;
        }
    }

    rewinddir(dir);
    write(pipe_envio, &numArchivos, sizeof(numArchivos));
    globalTotArchivos = numArchivos;
    printf("======== RESPALDANDO %d ARCHIVOS ========\n", numArchivos);
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG || (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)) {
            const char *fileName = entry->d_name;
            int longitud = strlen(fileName) + 1;
            write(pipe_envio, &longitud, sizeof(longitud));
            write(pipe_envio, fileName, longitud);
            printf("(PADRE --> %s)\n", fileName);
        }
    }


    closedir(dir);
}

int main(int argc, char *argv[])  {
    int pipe_padre_hijo[2], pipe_hijo_padre[2];
    char ruta_lista_archivos[] = "filelist.txt";
    const char *anadir = "/backup";
    if (pipe(pipe_padre_hijo) == -1 || pipe(pipe_hijo_padre) == -1) {
        perror("Error al crear los pipes");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("Error al hacer fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { // Proceso hijo
        close(pipe_padre_hijo[1]); // Cierra la escritura del pipe padre-hijo
        close(pipe_hijo_padre[0]); // Cierra la lectura del pipe hijo-padre

        int longitud_origen, longitud_destino;
        char origen[1024], destino[1024];

        read(pipe_padre_hijo[0], &longitud_origen, sizeof(longitud_origen));
        read(pipe_padre_hijo[0], origen, longitud_origen);
        read(pipe_padre_hijo[0], &longitud_destino, sizeof(longitud_destino));
        read(pipe_padre_hijo[0], destino, longitud_destino);

        int total_archivos;
        read(pipe_padre_hijo[0], &total_archivos, sizeof(total_archivos));
        globalTotArchivos = total_archivos;
        printf("HIJO (pid=%d) recibe el mensaje de su padre y el total de archivos a respaldar es de %d\n", getpid(), total_archivos);
        for (int i = total_archivos - 1; i > -1; i--) {
            int longitud;
            read(pipe_padre_hijo[0], &longitud, sizeof(longitud));
            char *nombre_archivo = malloc(longitud);
            read(pipe_padre_hijo[0], nombre_archivo, longitud);

            char fullPath[2048];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", origen, nombre_archivo);
            copiar(fullPath, destino);

            printf("\tHijo(pid=%d), respaldando el archivo: %s\tpendientes: %d/%d\n", getpid(), nombre_archivo, i, total_archivos);
            free(nombre_archivo);
        }

        //printf("Hijo (pid=%d) espera mensaje de su padre...\n", getpid());

        // Recibe e imprime mensaje del padre
        //int msg_len;
        //char *msg = malloc(1024);
        //read(pipe_padre_hijo[0], &msg_len, sizeof(msg_len));
        //read(pipe_padre_hijo[0], msg, msg_len);
        //printf("HIJO(pid=%d), Mensaje del padre: <--- %s\n", getpid(), msg);

        // Envia mensaje de respuesta al padre
        //const char *mensaje_respuesta = "Cuantos archivos?";
        //int respuesta_len = strlen(mensaje_respuesta) + 1;
        //write(pipe_hijo_padre[1], &respuesta_len, sizeof(respuesta_len));
        //write(pipe_hijo_padre[1], mensaje_respuesta, respuesta_len);

        //free(msg);
        close(pipe_padre_hijo[0]); // Cierra la lectura del pipe padre-hijo
        close(pipe_hijo_padre[1]); // Cierra la escritura del pipe hijo-padre
        exit(0);
    } else { // Proceso padre
        close(pipe_padre_hijo[0]); // Cierra la lectura del pipe padre-hijo
        close(pipe_hijo_padre[1]); // Cierra la escritura del pipe hijo-padre

        char ruta_directorio[1024], ruta_destino[1024];

        if (obtener_rutas(argc, argv, ruta_directorio, ruta_destino) != 0) {
            fprintf(stderr, "Error obteniendo las rutas necesarias.\n");
            return EXIT_FAILURE;
        }

        agregarbackup(ruta_destino, anadir, sizeof(ruta_destino));

        printf("\nPADRE(pid=%d): generando LISTA DE ARCHIVOS A RESPALDAR\n", getpid());

        if (crear_lista_archivos(ruta_directorio, ruta_lista_archivos) != EXIT_SUCCESS) {
            fprintf(stderr, "Error al crear el archivo de lista de archivos\n");
            exit(EXIT_FAILURE);
        }

        vaciar_directorio(ruta_destino);

        enviar_ruta(pipe_padre_hijo[1], ruta_directorio);
        enviar_ruta(pipe_padre_hijo[1], ruta_destino);

        listar_y_enviar(pipe_padre_hijo[1], getpid(), ruta_directorio);

        // Enviar mensaje al hijo
        //const char *mensaje_hijo = "Hola hijo, realiza el respaldo de archivos";
        //int mensaje_len = strlen(mensaje_hijo) + 1;
        //write(pipe_padre_hijo[1], &mensaje_len, sizeof(mensaje_len));
        //write(pipe_padre_hijo[1], mensaje_hijo, mensaje_len);

        // Esperar respuesta del hijo
        //int respuesta_len;
        //char *respuesta = malloc(1024);
        //read(pipe_hijo_padre[0], &respuesta_len, sizeof(respuesta_len));
        //read(pipe_hijo_padre[0], respuesta, respuesta_len);
        //printf("PADRE(pid=%d), mensaje de mi hijo: %s\n", getpid(), respuesta);

        //free(respuesta);

        close(pipe_padre_hijo[1]); // Cierra la escritura del pipe padre-hijo
        close(pipe_hijo_padre[0]); // Cierra la lectura del pipe hijo-padre

        printf("\nPADRE(pid=%d) comprobando respaldo:\n=========================================================\n", getpid());
        char comando[200];
        sprintf(comando, "cd %s/backup && ls -l", globalDestDir);
        system(comando);
        printf("%d\n  ARCHIVOS RESPALDADOS\n=========================================================\n", globalTotArchivos);

        printf("Termino el proceso padre...\n");
    }

    return 0;
}
