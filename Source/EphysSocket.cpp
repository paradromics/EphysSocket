#ifdef _WIN32
#include <Windows.h>
#endif

#include "EphysSocket.h"
#include "EphysSocketEditor.h"
#include <curl.h>
#include <string>

using namespace EphysSocketNode;

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
    socket = new DatagramSocket();
    socket->bindToPort(port);
    connected = (socket->waitUntilReady(true, 500) == 1); // Try to automatically open, dont worry if it does not work
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
    resizeChanSamp();

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

    waitForThreadToExit(500);

    stopTimer();

    sourceBuffers[0]->clear();
    return true;
}

struct memory {
    char* response;
    size_t size;
};

static size_t cb(void* data, size_t size, size_t nmemb, void* userp)
{
    size_t realsize = size * nmemb;
    struct memory* mem = (struct memory*)userp;

    char* ptr = (char *)realloc(mem->response, mem->size + realsize + 1);
    if (ptr == NULL)
        return 0;  /* out of memory! */

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), data, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;

    return realsize;
}
bool EphysSocket::updateBuffer()
{
    CURL* hnd;
    CURLcode res;
    struct memory chunk;
    chunk.size = 0;
    chunk.response = NULL;
    hnd = curl_easy_init();
    int rc = -1;
    
    if (hnd) {
        struct curl_slist* slist1;
        slist1 = NULL;
        slist1 = curl_slist_append(slist1, "Content-Type: application/json");
        std::string url = "http://localhost:" + std::to_string(port) + "/api/stream";

        curl_easy_setopt(hnd, CURLOPT_BUFFERSIZE, 102400L);
        curl_easy_setopt(hnd, CURLOPT_URL,url.c_str());
        curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, "{\"chunkRate\":" + std::to_string(round((float)(sample_rate) / (float)num_samp)) + ","
                                                 + "\"sampleRate\":" + std::to_string(sample_rate) + ","
                                                 + "\"cols\":" + std::to_string(num_channels) + ","
                                                 + "\"format\":\"u16\"}");
        curl_easy_setopt(hnd, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)52);
        curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
        curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.58.0");
        curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
        curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
        curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(hnd, CURLOPT_FTP_SKIP_PASV_IP, 1L);
        curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, cb);
        curl_easy_setopt(hnd, CURLOPT_WRITEDATA, (void*)&chunk);
        res = curl_easy_perform(hnd);
        
        curl_easy_cleanup(hnd);
        if (res == CURLE_OK)
            rc = 0;
    }
    if (rc == -1)
    {
        CoreServices::sendStatusMessage("Ephys Socket: Data shape mismatch");
        return false;
    }
    int n = chunk.size/2;
    
    uint16_t* recvbuf = (uint16_t*)chunk.response;

    float* convbuf = (float*)malloc(n * sizeof(float));
    // Transpose because the chunkSize argument in addToBuffer does not seem to do anything
    if (transpose) {
        int k = 0;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < num_channels; j++) {
                convbuf[k++] = data_scale *  (float)(recvbuf[j*n + i] - data_offset);
            }
        }
    } else {
        for (int i = 0; i < n * num_channels; i++)
            convbuf[i] = data_scale *  (float)(recvbuf[i] - data_offset);
    }

    sourceBuffers[0]->addToBuffer(convbuf, 
                                  &timestamps.getReference(0), 
                                  &ttlEventWords.getReference(0), 
                                  n, 
                                  1);

    total_samples += num_samp;

    free(convbuf);
    free(recvbuf);
    return true;
}

void EphysSocket::timerCallback()
{
    //std::cout << "Expected samples: " << int(sample_rate * 5) << ", Actual samples: " << total_samples << std::endl;
    
    relative_sample_rate = (sample_rate * 5) / float(total_samples);

    total_samples = 0;
}
