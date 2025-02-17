#ifdef _WIN32
#include <Windows.h>
#endif

#include "EphysSocket.h"
#include "EphysSocketEditor.h"
// #include <curl/curl.h>
#include <curl/curl.h>
#include <string>

using namespace EphysSocketNode;

size_t HDR_LEN = 4;
size_t HDR_TEXT_MASK = 1 << 31;

struct Context {
    uint8_t* buf;
    size_t len;
    size_t max;
    //EphysSocketNode* socket_class;
    EphysSocket* epsock;
    void (EphysSocket::* copyChunkToBuffer)(uint8_t* chunk_buffer, int chunk_len);
    //void* epsock;
};

static size_t write_cb(void* content, size_t size, size_t nmemb, void* userp);

static size_t write_cb(void* content, size_t size, size_t nmemb, void* userp)
{
    /* 16b little endian x 512 channels (max block size)
    * if fewer bytes, then pixel count is less than full-frame
    */
    //size_t realsize = size * nmemb;
    //struct memory* mem = (struct memory*)userp;

    //char* ptr = (char *)realloc(mem->response, mem->size + realsize + 1);
    //if (ptr == NULL)
    //    return 0;  /* out of memory! */

    //mem->response = ptr;
    //memcpy(&(mem->response[mem->size]), data, realsize);
    //mem->size += realsize;
    //mem->response[mem->size] = 0;

    //return realsize;


    size_t byte_size = size * nmemb;
    struct Context* ctx = (struct Context*)userp;
    size_t needed = ctx->len + byte_size;
    // EphysSocket* socket = (EphysSocket*)ctx->epsock;
    EphysSocket* epsock = ctx->epsock;
    // void (EphysSocket::* copyChunkToBuffer)(uint8_t* chunk_buffer, int chunk_len);
    // void copy_method(uint8_t * chunk_buffer, int chunk_len) = ctx->copyChunkToBuffer;

    if (needed > ctx->max) {
        char* ptr = (char*)realloc(ctx->buf, needed);
        if (!ptr) {
            fprintf(stderr, "not enough memory (realloc returned NULL)\n");
            return 0;
        }
        ctx->buf = (uint8_t*)ptr;
        ctx->max = needed;
    }
    memcpy(&(ctx->buf[ctx->len]), content, byte_size);
    ctx->len += byte_size;

    while (ctx->len >= HDR_LEN) {
        size_t chunk_len =
            ctx->buf[0]
            + (ctx->buf[1] << 8)
            + (ctx->buf[2] << 16)
            + (ctx->buf[3] << 24);
        int is_text = (chunk_len & HDR_TEXT_MASK);
        chunk_len &= ~HDR_TEXT_MASK;

        size_t full_len = HDR_LEN + chunk_len;
        if (ctx->len >= full_len) {
            if (is_text) {
                // LOGD("TEXT BLOCK [", chunk_len, "]");
                fprintf(stdout, "TEXT[%ld]: %.*s\n", chunk_len, (int)chunk_len, ctx->buf + HDR_LEN);
            }
            else {
                fprintf(stdout, "BIN[%ld]: ...\n", chunk_len);
                // ctx->epsock.copyChunkToBuffer(ctx->buf + HDR_LEN, chunk_len);
                //socket.copyChunkToBuffer(ctx->buf + HDR_LEN, chunk_len);
                (epsock->*ctx->copyChunkToBuffer)(ctx->buf + HDR_LEN, chunk_len);
            }
            memmove(ctx->buf, &(ctx->buf[full_len]), ctx->len - full_len);
            ctx->len -= full_len;
        }
    }
    return byte_size;
}

DataThread* EphysSocket::createDataThread(SourceNode *sn)
{
    return new EphysSocket(sn);
}


EphysSocket::EphysSocket(SourceNode* sn) : DataThread(sn),
    port(DEFAULT_PORT),
    num_channels(DEFAULT_NUM_CHANNELS),
    num_samp(DEFAULT_NUM_SAMPLES),
    data_offset(DEFAULT_DATA_OFFSET),
    data_scale(DEFAULT_DATA_SCALE),
    sample_rate(DEFAULT_SAMPLE_RATE)
{
    // socket = new DatagramSocket();
    // socket->bindToPort(port);
    // connected = (socket->waitUntilReady(true, 500) == 1); // Try to automatically open, dont worry if it does not work
    sourceBuffers.add(new DataBuffer(num_channels, 10000)); // start with 2 channels and automatically resize
}

GenericEditor* EphysSocket::createEditor(SourceNode* sn)
{
    return new EphysSocketEditor(sn, this);
}



EphysSocket::~EphysSocket()
{
}

void EphysSocket::resizeChanSamp()
{
    sourceBuffers[0]->resize(num_channels, 10000);
    timestamps.resize(num_samp);
    ttlEventWords.resize(num_samp);
}

int EphysSocket::getNumChannels() const
{
    return num_channels;
}

int EphysSocket::getNumDataOutputs(DataChannel::DataChannelTypes type, int subproc) const
{
    if (type == DataChannel::HEADSTAGE_CHANNEL)
        return num_channels;
    else
        return 0; 
}

int EphysSocket::getNumTTLOutputs(int subproc) const
{
    return 0; 
}

float EphysSocket::getSampleRate(int subproc) const
{
    return sample_rate;
}

float EphysSocket::getBitVolts (const DataChannel* ch) const
{
    return 0.195f;
}

bool EphysSocket::foundInputSource()
{
    return connected;
}

bool EphysSocket::startAcquisition()
{
    CURLcode res;
    Context ctx = {};

    std::string api_url = "http://192.168.88.252/api/v_/niob/stream/nitara/chunked";
    std::string post_data = "{\"text\":true,\"binary\":\"None\"}";

    ctx.buf = (uint8_t *) malloc(1);
    ctx.max = 1;
    ctx.epsock = this;
    ctx.copyChunkToBuffer = &EphysSocket::copyChunkToBuffer;

    // Prepare buffer to callback
    resizeChanSamp();

    curl_global_init(CURL_GLOBAL_ALL);
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, api_url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post_data.size());
    curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&ctx);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        CoreServices::sendStatusMessage("Ephys Socket: Curl error");

        //fprintf(stderr, "curl_easy_perform() failed: %s\n",
        //    curl_easy_strerror(res));
    }
    connected = true;

    //if (rc == -1)
    //{
    //    CoreServices::sendStatusMessage("Ephys Socket: Data shape mismatch");
    //    return false;
    //}

    total_samples = 0;

    startTimer(5000);

    startThread();
    return true;
}

void  EphysSocket::tryToConnect()
{
    connected = true;
}

bool EphysSocket::stopAcquisition()
{
    if (isThreadRunning())
    {
        signalThreadShouldExit();
    }

    waitForThreadToExit(600);

    stopTimer();

    sourceBuffers[0]->clear();
    return true;
}

void EphysSocket::copyChunkToBuffer(uint8_t* chunk_buffer, int chunk_len) {
    uint16_t* recvbuf = (uint16_t*) chunk_buffer;
    // int full_frame = 512;
    // assume that we have the right # of channels already set, then there are 2x bytes per short sample
    int num_samples = chunk_len / (num_channels * 2);

    // fprintf("Got a frame, should be 1024 bytes")
    float* convbuf = (float*) malloc(num_samples * num_channels * sizeof(float));
    // Transpose because the chunkSize argument in addToBuffer does not seem to do anything
    if (transpose) {
        int k = 0;
        for (int i = 0; i < num_samples; i++) {
            for (int j = 0; j < num_channels; j++) {
                convbuf[k++] = data_scale * (float)(recvbuf[j * num_samples + i] - data_offset);
            }
        }
    }
    else {
        for (int i = 0; i < num_samples * num_channels; i++)
            convbuf[i] = data_scale * (float)(recvbuf[i] - data_offset);
    }

    // Pretty sure that addToBuffer wants confbuf to have channels indexed on consecutive points,
    // and samples indexed on the strided axis.
    sourceBuffers[0]->addToBuffer(convbuf,
        &timestamps.getReference(0),
        &ttlEventWords.getReference(0),
        num_samples * num_channels,
        1);

    total_samples += num_samples;

    free(convbuf);
    // free(recvbuf);
}



bool EphysSocket::updateBuffer()
{

    //CURL* hnd;
    //CURLcode res;
    //// struct memory chunk;
    //struct Context ctx;
    //ctx.len = 0;
    //ctx.max = 1;
    //ctx.buf = (uint8_t *) malloc(1);
    //hnd = curl_easy_init();
    //int rc = -1;
    //
    //if (hnd) {
    //    struct curl_slist* slist1;
    //    slist1 = NULL;
    //    slist1 = curl_slist_append(slist1, "Content-Type: application/json");
    //    // std::string url = "http://localhost:" + std::to_string(port) + "/api/stream";
    //    std::string url = "http://10.156.5.21/api/v_/niob/stream/nitara/chunked";
    //    std::string postfields = "{\"text\":true,\"binary\":\"None\"}";

    //    curl_easy_setopt(hnd, CURLOPT_BUFFERSIZE, 102400L);
    //    curl_easy_setopt(hnd, CURLOPT_URL,url.c_str());
    //    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
    //    /*
    //    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, "{\"chunkRate\":" + std::to_string(round((float)(sample_rate) / (float)num_samp)) + ","
    //                                             + "\"sampleRate\":" + std::to_string(sample_rate) + ","
    //                                             + "\"cols\":" + std::to_string(num_channels) + ","
    //                                             + "\"format\":\"u16\"}");
    //    */
    //    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, postfields);
    //    curl_easy_setopt(hnd, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)52);
    //    curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
    //    curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.58.0");
    //    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    //    // curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
    //    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
    //    curl_easy_setopt(hnd, CURLOPT_FTP_SKIP_PASV_IP, 1L);
    //    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
    //    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, cb);
    //    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, (void *)&ctx);
    //    res = curl_easy_perform(hnd);
    //    
    //    curl_easy_cleanup(hnd);
    //    if (res == CURLE_OK)
    //        rc = 0;
    //}

    //if (rc == -1)
    //{
    //    CoreServices::sendStatusMessage("Ephys Socket: Data shape mismatch");
    //    return false;
    //}
    // Should this be corrected for 4-byte headers
    //int n = ctx.len / 2;
    //
    //uint16_t* recvbuf = (uint16_t*) ctx.buf;

    //float* convbuf = (float*)malloc(n * sizeof(float));
    //// Transpose because the chunkSize argument in addToBuffer does not seem to do anything
    //if (transpose) {
    //    int k = 0;
    //    for (int i = 0; i < n; i++) {
    //        for (int j = 0; j < num_channels; j++) {
    //            convbuf[k++] = data_scale *  (float)(recvbuf[j*n + i] - data_offset);
    //        }
    //    }
    //} else {
    //    for (int i = 0; i < n * num_channels; i++)
    //        convbuf[i] = data_scale *  (float)(recvbuf[i] - data_offset);
    //}

    //sourceBuffers[0]->addToBuffer(convbuf, 
    //                              &timestamps.getReference(0), 
    //                              &ttlEventWords.getReference(0), 
    //                              n, 
    //                              1);

    //total_samples += num_samp;

    //free(convbuf);
    //free(recvbuf);
    return true;
}

void EphysSocket::timerCallback()
{
    //std::cout << "Expected samples: " << int(sample_rate * 5) << ", Actual samples: " << total_samples << std::endl;
    
    relative_sample_rate = (sample_rate * 5) / float(total_samples);

    total_samples = 0;
}
