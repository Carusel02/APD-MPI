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

typedef enum {
    DOWNLOAD = 1,
    SEND = 2,
    CHUNK = 3,
    DOWN = 4
} TAG;

typedef enum {
    ANSWER = 900,
    SHUTDOWN = 901,
    COMPLETE = 902
} STATE;

// structure for a chunk
typedef struct {
    char hash[HASH_SIZE];   // hash of the chunk
    
    int peers[MAX_CHUNKS];  // peers that have the chunk
    int peers_count;        // number of peers that have the chunk
} chunk_info;

typedef struct {
    STATE state;
    int file_id;
    int chunk_id;
} request;

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

// create vector of files we own
file_info files[MAX_FILES];
int number_of_files_owned = 0;

// create vector of files we want to download
file_info files_to_download[MAX_FILES];
int number_of_files_to_download = 0;

file_info database[MAX_FILES]; // database of tracker

void *download_thread_func(void *arg)
{
    int rank = *(int*) arg;
    int requests = 0;

    printf("DOWNLOAD FROM PEER %d STARTED!\n", rank);

    // database of the client
    file_info my_database[MAX_FILES];
    // take the database from the tracker
    MPI_Recv(&my_database, sizeof(file_info) * MAX_FILES, MPI_BYTE, TRACKER_RANK, DOWNLOAD, MPI_COMM_WORLD, MPI_STATUS_IGNORE);


    // for every file
    for(int i = 0 ; i < number_of_files_to_download ; i++) {
        
        file_info file_to_complete;
        file_to_complete.id = files_to_download[i].id;
        file_to_complete.chunks_count = 0;
        int found = 0;
        
        char name[100];
        sprintf(name, "client%d_%s", rank, files_to_download[i].filename);
        printf("[OUTPUT] File to complete: %s\n", name);
        FILE *f = fopen(name, "w");
        
        // look for the file in the database
        for(int j = 0 ; j < MAX_FILES && found == 0; j++) {
            if(my_database[j].id == files_to_download[i].id) {
                // send request to a client that has the file
                for(int k = 0 ; k < MAX_CLIENTS && found == 0; k++)
                    if(my_database[j].owned[k].flag == 1) {
                        // send request for every chunk
                        for(int nr_chunk = 0 ; nr_chunk < my_database[j].chunks_count ; nr_chunk++) {
                                // send request
                                request req;
                                req.state = ANSWER;
                                req.file_id = my_database[j].id;
                                req.chunk_id = nr_chunk;
                                MPI_Send(&req, sizeof(request), MPI_BYTE, k, SEND, MPI_COMM_WORLD);
                                MPI_Recv(&file_to_complete.chunk[nr_chunk], sizeof(chunk_info), MPI_BYTE, k, CHUNK, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                                
                                printf("Chunk %d received from peer %d\n", nr_chunk, k);
                                printf("Chunk %d hash: %s\n", nr_chunk, file_to_complete.chunk[nr_chunk].hash);
                                fprintf(f, "%s\n", file_to_complete.chunk[nr_chunk].hash);
                                
                                file_to_complete.chunks_count++;
                                if(file_to_complete.chunks_count == my_database[j].chunks_count) {
                                    printf("File %s completed!\n", file_to_complete.filename);
                                    my_database[j].needed[rank].flag = 0;
                                    found = 1;
                                }
                        }
                    }
            }
        }


    }

    printf("FINISH ALL FROM DOWNLOAD!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

    request req;
    req.state = COMPLETE;
    MPI_Send(&req, sizeof(request), MPI_BYTE, TRACKER_RANK, SEND, MPI_COMM_WORLD);


    return NULL;
}

void *upload_thread_func(void *arg)
{
    int rank = *(int*) arg;

    while(true) {
        MPI_Status status;
        // receive request
        request req;
        MPI_Recv(&req, sizeof(request), MPI_BYTE, MPI_ANY_SOURCE, SEND, MPI_COMM_WORLD, &status);
        
        // if the request is fot shutdown, exit
        if(req.state == SHUTDOWN)
            break;

        // if the request is for an answer
        if(req.state == ANSWER) {
            // look for the file in the database
            for(int i = 0 ; i < number_of_files_owned ; i++) {
                if(files[i].id == req.file_id) {
                    // send the chunk
                    MPI_Send(&files[i].chunk[req.chunk_id], sizeof(chunk_info), MPI_BYTE, status.MPI_SOURCE, CHUNK, MPI_COMM_WORLD);
                }
            }
        }
    }

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

    // **************** FOR VERIFICATION **************** //

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

    // **************** RESPONSE TO REQUEST **************** //
    for(int i = 1 ; i < numtasks ; i++)
        MPI_Send(&database, sizeof(file_info) * MAX_FILES, MPI_BYTE, i, DOWNLOAD, MPI_COMM_WORLD);

    // **************** WAIT TO COMPLETE **************** //
    for(int i = 1 ; i < numtasks ; i++) {
        request req;
        MPI_Recv(&req, sizeof(request), MPI_BYTE, i, SEND, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if(req.state == COMPLETE)
            printf("[TRACKER] Peer %d has completed the download\n", i);
    }

    // **************** SHUTDOWN UPLOAD FROM EVERY CLIENT **************** //
    for(int i = 1 ; i < numtasks ; i++) {
        request req;
        req.state = SHUTDOWN;
        MPI_Send(&req, sizeof(request), MPI_BYTE, i, SEND, MPI_COMM_WORLD);
    }

}


// for every client
void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    // ************* READ DATA FROM FILE ************* //

    // init structures
    for(int i = 0 ; i < MAX_FILES ; i++) {
        files[i].id = -1;
        files[i].chunks_count = 0;
        files_to_download[i].id = -1;
        files_to_download[i].chunks_count = 0;
    }

    // read file from input file
    char client_file[10];
    sprintf(client_file, "in%d.txt", rank);
    FILE *f = fopen(client_file, "r");
    if (f == NULL) {
        printf("Eroare la deschiderea fisierului %s\n", client_file);
        exit(-1);
    }

    // read number of files owned
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
