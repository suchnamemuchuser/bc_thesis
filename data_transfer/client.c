#ifndef CLIENT_C
#define CLIENT_C

#include "client.h"
#include "client_behaviours.h"

void writeBufferStatusToFile(const char* filepath, BufferRegistry* registry);

BufferSession* getBufferById(BufferRegistry* registry, int id);

char* readFileToString(const char* filename);

BufferRegistry* parseConfigBuffers(cJSON* root);

void parseConfigClients(cJSON* root, BufferRegistry* registry);

int main(int argc, char* argv[]){
    const char* config_filename = (argc > 1) ? argv[1] : "clientconfig.json";

    char statusFile[256];

    char* argBuf = readFileToString(config_filename);

    cJSON *root = cJSON_Parse(argBuf);
    free(argBuf);
    if (!root) return 1;

    cJSON* statusFileJson = cJSON_GetObjectItemCaseSensitive(root, "buffer_status_file");
    if (cJSON_IsString(statusFileJson) && (statusFileJson->valuestring != NULL)) {
        printf("Buffer status file: %s\n", statusFileJson->valuestring);
        strncpy(statusFile, statusFileJson->valuestring, sizeof(statusFile) - 1);
    }

    BufferRegistry* registry = parseConfigBuffers(root);

    parseConfigClients(root, registry);

    cJSON_Delete(root);

    while(true)
    {
        writeBufferStatusToFile(statusFile, registry);
        sleep(60);
    }

    return 0;
}

void writeBufferStatusToFile(const char* filepath, BufferRegistry* registry) 
{
    // w for overwrite
    FILE* file = fopen(filepath, "w");
    if (file == NULL) 
    {
        fprintf(stderr, "Failed to open buffer status file for writing: %s\n", filepath);
        return;
    }

    fprintf(file, "Circular Buffer Status\n\n");

    for (int i = 0; i < registry->count; i++) 
    {
        BufferSession* session = registry->sessions[i];
        
        fprintf(file, "Buffer [%d]: %s\n", 
                session->bufferId,
                session->description ? session->description : "N/A");

        // lock for recordingInfo
        pthread_mutex_lock(&session->buffer_lock);

        if (session->buffer.recordingActive) 
        {
            fprintf(file, "  Status: RECORDING\n");
            fprintf(file, "  Target: %s (ID: %d)\n", 
                    session->recordingInfo.object_name, 
                    session->recordingInfo.id);
            fprintf(file, "  From: %d\n", session->recordingInfo.rec_start_time);
            fprintf(file, "  To: %d\n", session->recordingInfo.end_time);
        } 
        else 
        {
            fprintf(file, "  Status: IDLE\n");
        }

        // buffer stats
        size_t capacity = session->buffer.data_len;
        fprintf(file, "  Capacity: %zu bytes\n", capacity);
        
        int active_readers = 0;
        for (int r = 0; r < session->buffer.reader_cnt; r++) 
        {
            // unused readers
            if (session->buffer.readerOffset[r] >= capacity) 
            {
                continue;
            }
            active_readers++;
            
            size_t unread_bytes = circularBufferAvailableData(&session->buffer, r);

            float fill_percentage = ((float)unread_bytes / capacity) * 100.0f;

            fprintf(file, "    Reader %d: %zu bytes unread (%.2f%% full)\n", 
                    r, unread_bytes, fill_percentage);
        }
        
        if (active_readers == 0) 
        {
            fprintf(file, "    No active readers registered.\n");
        }

        fprintf(file, "\n");

        pthread_mutex_unlock(&session->buffer_lock);
    }

    fclose(file);
}

BufferSession* getBufferById(BufferRegistry* registry, int id)
{
    for (int i = 0; i < registry->count; i++)
    {
        if (registry->sessions[i]->bufferId == id)
        {
            return registry->sessions[i];
        }
    }

    return NULL; 
}

char* readFileToString(const char* filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) exit(1);

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buffer = malloc(size + 1);
    fread(buffer, 1, size, fp);
    buffer[size] = '\0';
    fclose(fp);

    return buffer;
}

BufferRegistry* parseConfigBuffers(cJSON* root)
{
    cJSON* buffers_array = cJSON_GetObjectItemCaseSensitive(root, "buffers");

    if (!cJSON_IsArray(buffers_array))
    {
        fprintf(stderr, "Error: 'buffers' is missing or not an array.\n");
        return NULL;
    }

    int buffer_count = cJSON_GetArraySize(buffers_array);
    
    BufferRegistry* registry = malloc(sizeof(BufferRegistry));
    if (!registry) return NULL;
    
    registry->count = 0;
    registry->sessions = malloc(sizeof(BufferSession*) * buffer_count);

    if (!registry->sessions) {
        free(registry);
        return NULL;
    }

    cJSON* buf_json = NULL;
    cJSON_ArrayForEach(buf_json, buffers_array)
    {
        cJSON* id_item = cJSON_GetObjectItemCaseSensitive(buf_json, "id");
        cJSON* size_item = cJSON_GetObjectItemCaseSensitive(buf_json, "size");
        cJSON* desc_item = cJSON_GetObjectItemCaseSensitive(buf_json, "description");

        if (!cJSON_IsNumber(id_item) || !cJSON_IsNumber(size_item))
        {
            fprintf(stderr, "Warning: Skipping invalid buffer entry in JSON.\n");
            continue;
        }

        BufferSession* session = malloc(sizeof(BufferSession));
        if (!session) return NULL;

        session->bufferId = id_item->valueint;
        
        // valuedouble for size! valueint can overflow if size > 2GB.
        size_t buffer_size = (size_t)size_item->valuedouble; 
        
        if (cJSON_IsString(desc_item) && desc_item->valuestring != NULL)
        {
            strncpy(session->description, desc_item->valuestring, sizeof(session->description) - 1);
            session->description[sizeof(session->description) - 1] = '\0';
        }
        else
        {
            session->description[0] = '\0';
        }

        pthread_mutex_init(&session->buffer_lock, NULL);
        pthread_cond_init(&session->data_available, NULL);

        circularBufferInit(&session->buffer, buffer_size);

        registry->sessions[registry->count] = session;
        registry->count++;
    }
    
    return registry;
}

void parseConfigClients(cJSON* root, BufferRegistry* registry) {
    cJSON* clients_array = cJSON_GetObjectItemCaseSensitive(root, "clients");

    if (!cJSON_IsArray(clients_array))
    {
        fprintf(stderr, "Error: 'clients' array missing.\n");
        exit(1);
    }

    cJSON* clientJson = NULL;
    cJSON_ArrayForEach(clientJson, clients_array)
    {
        ClientContext* ctx = calloc(1, sizeof(ClientContext));
        if (!ctx) continue;

        ctx->clientId = cJSON_GetObjectItemCaseSensitive(clientJson, "id")->valueint;
        
        cJSON* desc = cJSON_GetObjectItemCaseSensitive(clientJson, "description");
        if (cJSON_IsString(desc))
        {
            strncpy(ctx->description, desc->valuestring, sizeof(ctx->description) - 1);
        }

        cJSON* outBufId = cJSON_GetObjectItemCaseSensitive(clientJson, "output_buffer");
        if (cJSON_IsNumber(outBufId) && outBufId->valueint != -1)
        {
            ctx->outputBuffer = getBufferById(registry, outBufId->valueint);
        }

        cJSON* in_bufs = cJSON_GetObjectItemCaseSensitive(clientJson, "input_buffers");
        if (cJSON_IsArray(in_bufs))
        {
            ctx->inputBufferCount = cJSON_GetArraySize(in_bufs);
            if (ctx->inputBufferCount > 0)
            {
                ctx->inputBuffers = malloc(sizeof(BufferSession*) * ctx->inputBufferCount);
                for (int i = 0; i < ctx->inputBufferCount; i++)
                {
                    int b_id = cJSON_GetArrayItem(in_bufs, i)->valueint;
                    ctx->inputBuffers[i] = getBufferById(registry, b_id);
                }
            }
        }

        // parse client specific arguments
        char* func_name = cJSON_GetObjectItemCaseSensitive(clientJson, "function")->valuestring;
        cJSON* args_json = cJSON_GetObjectItemCaseSensitive(clientJson, "args");
        
        pthread_t thread_id;
        void* (*thread_func)(void*) = NULL;

        if (strcmp(func_name, "network_producer") == 0)
        {
            NetworkProducerArgs* p_args = malloc(sizeof(NetworkProducerArgs));
            strncpy(p_args->database, cJSON_GetObjectItemCaseSensitive(args_json, "database")->valuestring, 255);
            strncpy(p_args->source, cJSON_GetObjectItemCaseSensitive(args_json, "source")->valuestring, 255);
            p_args->port = cJSON_GetObjectItemCaseSensitive(args_json, "port")->valueint;
            
            ctx->customArgs = p_args;
            thread_func = networkProducerThread;
        } 
        else if (strcmp(func_name, "network_consumer") == 0)
        {
            NetworkConsumerArgs* n_args = malloc(sizeof(NetworkConsumerArgs));
            n_args->port = cJSON_GetObjectItemCaseSensitive(args_json, "port")->valueint;
            printf("Parsing port: %d\n", cJSON_GetObjectItemCaseSensitive(args_json, "port")->valueint);
            
            ctx->customArgs = n_args;
            thread_func = bufferNetworkConsumerThread;
        }
        else if (strcmp(func_name, "file_consumer") == 0)
        {
            FileConsumerArgs* c_args = malloc(sizeof(FileConsumerArgs));
            strncpy(c_args->dataDir, cJSON_GetObjectItemCaseSensitive(args_json, "data_dir")->valuestring, 255);
            strncpy(c_args->ext, cJSON_GetObjectItemCaseSensitive(args_json, "ext")->valuestring, 63);
            
            ctx->customArgs = c_args;
            thread_func = bufferFileConsumerThread;
        }
        else if (strcmp(func_name, "data_processor") == 0)
        {
            ctx->customArgs = NULL;
            thread_func = dataProcessorThread;
        }
        else if (strcmp(func_name, "data_averager") == 0)
        {
            DataAveragerArgs* a_args = malloc(sizeof(DataAveragerArgs));
            a_args->ratio = cJSON_GetObjectItemCaseSensitive(args_json, "ratio")->valueint;
            
            ctx->customArgs = a_args;
            thread_func = dataAveragerThread;
        }
        else if (strcmp(func_name, "image_archiver") == 0)
        {
            ImageArchiverArgs* a_args = malloc(sizeof(ImageArchiverArgs));
            strncpy(a_args->dataDir, cJSON_GetObjectItemCaseSensitive(args_json, "output_dir")->valuestring, 255);
            strncpy(a_args->sourceImagePath, cJSON_GetObjectItemCaseSensitive(args_json, "source_image_path")->valuestring, 255);

            ctx->customArgs = a_args;
            thread_func = imageArchiverThread;
        }
        else
        {
            fprintf(stderr, "Unknown function: %s\n", func_name);
            free(ctx->inputBuffers);
            free(ctx);
            continue;
        }


        if (pthread_create(&thread_id, NULL, thread_func, ctx) == 0)
        {
            pthread_detach(thread_id);
            printf("Launched %s (ID: %d)\n", ctx->description, ctx->clientId);
        }
    }
}

#endif // CLIENT_C