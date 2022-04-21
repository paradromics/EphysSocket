#ifndef __EPHYSSOCKETH__
#define __EPHYSSOCKETH__

#include <DataThreadHeaders.h>
//#include "Utils.h"

const int DEFAULT_PORT = 9001;
const float DEFAULT_SAMPLE_RATE = 30000.0f;
const float DEFAULT_DATA_SCALE = 0.195f;
const uint16_t DEFAULT_DATA_OFFSET = 32768;
const int DEFAULT_NUM_SAMPLES = 256;
// const int DEFAULT_NUM_CHANNELS = 64;
const int DEFAULT_NUM_CHANNELS = 512;

namespace EphysSocketNode
{
    class EphysSocket : public DataThread, public Timer
    {

    public:
        EphysSocket(SourceNode* sn);
        ~EphysSocket();

        // Interface fulfillment
        bool foundInputSource() override;
        int getNumDataOutputs(DataChannel::DataChannelTypes type, int subProcessor) const override;
        int getNumTTLOutputs(int subprocessor) const override;
        float getSampleRate(int subprocessor) const override;
        float getBitVolts(const DataChannel* chan) const override;
        int getNumChannels() const;

        // User defined
        int port;
        float sample_rate;
        float data_scale;
        uint16_t data_offset;
        bool transpose = true;
        int num_samp;
        int num_channels;

        int total_samples;
        float relative_sample_rate;

        void resizeChanSamp();
        void tryToConnect();

        GenericEditor* createEditor(SourceNode* sn);
        static DataThread* createDataThread(SourceNode* sn);

    private:

        bool updateBuffer() override;
        bool startAcquisition() override;
        bool stopAcquisition()  override;
        void timerCallback() override;
        // The curl callback
        // static size_t write_cb(void* content, size_t size, size_t nmemb, void* userp);
        // Within the callback, write blocks to the buffer
        void copyChunkToBuffer(uint8_t* chunk_buffer, int chunk_len);

        bool connected = false;

       ScopedPointer<DatagramSocket> socket;

        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EphysSocket);
    };
}
#endif