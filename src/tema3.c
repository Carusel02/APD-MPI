#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100
#define MAX_CLIENTS 12

// structure for a chunk
typedef struct {
    char hash[HASH_SIZE];   // hash of the chunk
    
    int peers[MAX_CHUNKS];  // peers that have the chunk
    int peers_count;        // number of peers that have the chunk
} chunk_info;

typedef struct {
    int flag;
    int chunk[MAX_CHUNKS];
    int count;
} client;

// structure for a file
typedef struct {
    char filename[MAX_FILENAME];   // name of the file
    int id;                        // id of the file
    int chunks_count;              // number of chunks
    chunk_info chunk[MAX_CHUNKS];  // chunks of the file

    client owned[MAX_CLIENTS];        // peers that have the file
    client needed[MAX_CLIENTS];       // peers that need the file

} file_info;

file_info database[MAX_FILES]; // database of tracker

void *download_thread_func(void *arg)
{
    int rank = *(int*) arg;

    return NULL;
}

void *upload_thread_func(void *arg)
{
    int rank = *(int*) arg;

    return NULL;
}

void print_file(file_info file) {
    printf("\n");
    printf("File %s\n", file.filename);
    printf("Id: %d\n", file.id);
    printf("Chunks: %d\n", file.chunks_count);
    printf("\n");
}

void tracker(int numtasks, int rank) {

    // init database
    for(int i = 0 ; i < MAX_FILES ; i++) {
        database[i].id = -1;
        database[i].chunks_count = 0;
    }

    int receive = 1;

    // receive data from peers
    while(true) {
        
        MPI_Status status;
        int number_of_files_owned;
        int number_of_files_to_download;
        
        // receive number of files owned
        MPI_Recv(&number_of_files_owned, 1, MPI_INT, receive, 0, MPI_COMM_WORLD, &status);
        printf("[TRACKER] Numarul de fisiere detinute de clientul %d este %d\n", status.MPI_SOURCE, number_of_files_owned);
        for(int i = 0; i < number_of_files_owned; i++) {
            file_info file;
            MPI_Recv(&file, sizeof(file_info), MPI_BYTE, receive, 0, MPI_COMM_WORLD, &status);
            if(database[file.id].id == -1)
                database[file.id] = file;
            
            database[file.id].owned[status.MPI_SOURCE].flag = 1;
        }

        // receive number of files to download
        MPI_Recv(&number_of_files_to_download, 1, MPI_INT, receive, 0, MPI_COMM_WORLD, &status);
        printf("[TRACKER DOWNLOAD] clientul %d are de descarcat %d fisiere\n", status.MPI_SOURCE, number_of_files_to_download);
        for(int i = 0; i < number_of_files_to_download; i++) {
            file_info file;
            MPI_Recv(&file, sizeof(file_info), MPI_BYTE, receive, 0, MPI_COMM_WORLD, &status);
            database[file.id].needed[status.MPI_SOURCE].flag = 1;
            database[file.id].needed[status.MPI_SOURCE].count = 0;
            printf("[TRACKER DOWNLOAD] fisierul %s de descarcat de clientul %d are %d chunk-uri\n", file.filename, status.MPI_SOURCE, file.chunks_count);
        }

        receive++;
        if(receive == numtasks)
            break;

    }

    // print the database
    printf("\n");
    for(int i = 0 ; i < MAX_FILES ; i++) {
        printf("[DATABASE] Database file %s -> %d chunk-uri\n", database[i].filename, database[i].chunks_count);
    }

    // send ACK to peers
    char ack[10] = "ACK\0";
    for(int i = 1 ; i < numtasks ; i++) {
        MPI_Send(ack, 10, MPI_CHAR, i, 0, MPI_COMM_WORLD);
    }

    // for every file
    for(int i = 0 ; i < MAX_FILES; i++) {
        // if the file exists
        if(database[i].id != -1) {
            // for every peer
            for(int j = 1 ; j <= numtasks ; j++) {
                // if the peer needs the file
                if(database[i].needed[j].flag == 1) {
                    // print the file
                    printf("[CALCULATION] Peer %d needs file %s\n", j, database[i].filename);
                    // MPI_Send(&database[i], sizeof(file_info), MPI_BYTE, j, 0, MPI_COMM_WORLD);
                }
            }
        }
    }

    // for every file
    for(int i = 0 ; i < MAX_FILES; i++) {
        // if the file exists
        if(database[i].id != -1) {
            // for every peer
            for(int j = 1 ; j <= numtasks ; j++) {
                // if the peer needs the file
                if(database[i].owned[j].flag == 1) {
                    // print the file
                    printf("[CALCULATION] Peer %d owns file %s\n", j, database[i].filename);
                    // MPI_Send(&database[i], sizeof(file_info), MPI_BYTE, j, 0, MPI_COMM_WORLD);
                }
            }
        }
    }


    

}


// for every client
void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    // ************* READ DATA FROM FILE ************* //

    // create vector of files we own
    file_info files[MAX_FILES];
    // create vector of files we want to download
    file_info files_to_download[MAX_FILES];

    // read file from input file
    char client_file[10];
    sprintf(client_file, "in%d.txt", rank);
    FILE *f = fopen(client_file, "r");
    if (f == NULL) {
        printf("Eroare la deschiderea fisierului %s\n", client_file);
        exit(-1);
    }

    // read number of files owned
    int number_of_files_owned = 0;
    fscanf(f, "%d", &number_of_files_owned);
    if (number_of_files_owned > MAX_FILES) {
        printf("Numarul de fisiere este prea mare\n");
        exit(-1);
    } else
        printf("Numarul de fisiere detinute de clientul %d este %d\n", rank, number_of_files_owned);

    // read from files owned
    for (int i = 0; i < number_of_files_owned; i++) {
        fscanf(f, "%s", files[i].filename);

        int nr;
        // read the id 
        if (sscanf(files[i].filename, "file%d", &nr) == 1) {
            printf("Files owned: %d\n", nr);
            files[i].id = nr;
        } else {
            printf("Error file number.\n");
            exit(-1);
        }

        fscanf(f, "%d", &files[i].chunks_count);
        for (int j = 0; j < files[i].chunks_count; j++) {
            fscanf(f, "%s", files[i].chunk[j].hash);
        }

        // initialize owned and needed
        for(int j = 0 ; j < MAX_CLIENTS ; j++) {
            files[i].owned[j].flag = 0;
            files[i].needed[j].flag = 0;
        }

        // mark the file as owned
        files[i].owned[rank].flag = 1;
    }

    // read number of files to download
    int number_of_files_to_download = 0;
    fscanf(f, "%d", &number_of_files_to_download);
    if (number_of_files_to_download > MAX_FILES) {
        printf("Numarul de fisiere este prea mare\n");
        exit(-1);
    }

    // read from files to download
    for (int i = 0 ; i < number_of_files_to_download; i++) {
        fscanf(f, "%s", files_to_download[i].filename);
        printf("File to download: %s\n", files_to_download[i].filename);
        
        int nr;
        // read the id 
        if (sscanf(files_to_download[i].filename, "file%d", &nr) == 1) {
            printf("Files to download: %d\n", nr);
            files_to_download[i].id = nr;
        } else {
            printf("Error file number.\n");
            exit(-1);
        }

        for(int j = 0 ; j < MAX_CLIENTS ; j++) {
            files_to_download[i].owned[j].flag = 0;
            files_to_download[i].needed[j].flag = 0;
        }

        // mark the file as needed
        files_to_download[i].needed[rank].flag = 1;

    }

    fclose(f);

    // ************* SEND DATA TO TRACKER ************* //

    // files owned
    MPI_Send(&number_of_files_owned, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
    for (int i = 0; i < number_of_files_owned; i++) {
        MPI_Send(&files[i], sizeof(file_info), MPI_BYTE, TRACKER_RANK, 0, MPI_COMM_WORLD);
    }

    // files to download
    MPI_Send(&number_of_files_to_download, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
    for (int i = 0; i < number_of_files_to_download; i++) {
        MPI_Send(&files_to_download[i], sizeof(file_info), MPI_BYTE, TRACKER_RANK, 0, MPI_COMM_WORLD);
    }

    // ************* RECEIVE DATA FROM TRACKER ************* //
    
    char ACK[10];
    MPI_Recv(&ACK, 10, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if(strcmp(ACK, "ACK") == 0)
        printf("[PEER] Am primit ACK de la tracker\n");
    else {
        printf("[PEER] Eroare la primire ACK (%s)\n", ACK);
        exit(-1);
    }

    // ************* CREATE THREADS ************* //

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}
