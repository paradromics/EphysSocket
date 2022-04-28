#pragma once
/*
  ==============================================================================

   This file is part of the juce_core module of the JUCE library.
   Copyright (c) 2015 - ROLI Ltd.

   Permission to use, copy, modify, and/or distribute this software for any purpose with
   or without fee is hereby granted, provided that the above copyright notice and this
   permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
   TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
   NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
   DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
   IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

   ------------------------------------------------------------------------------

   NOTE! This permissive ISC license applies ONLY to files within the juce_core module!
   All other JUCE modules are covered by a dual GPL/commercial license, so if you are
   using any other modules, be sure to check that you also comply with their license.

   For more details, visit www.juce.com

  ==============================================================================
*/
#include "JuceHeader.h"
#include <curl/curl.h>

class WebInputStream : public InputStream
{
public:
    WebInputStream(const String& address, bool isPost, const MemoryBlock& postData,
        URL::OpenStreamProgressCallback* progressCallback, void* progressCallbackContext,
        const String& headers, int timeOutMs, StringPairArray* responseHeaders,
        const int maxRedirects, const String& httpRequest);

    ~WebInputStream();

    //==============================================================================
    // Input Stream overrides
    bool isError();
    bool isExhausted();
    int64 getPosition();
    int64 getTotalLength();

    int read(void* buffer, int bytesToRead) override;

    bool setPosition(int64 wantedPos) override;

    //==============================================================================
    int statusCode;

private:
    //==============================================================================
    bool init();

    void cleanup();

    //==============================================================================
    bool setOptions(const String& address, int timeOutMs, bool wantsHeaders,
        const int maxRedirects, const String& headers,
        bool isPost, const String& httpRequest, size_t postSize);
 

    void connect(StringPairArray* responseHeaders, bool isPost, const MemoryBlock& postData,
        URL::OpenStreamProgressCallback* progressCallback, void* progressCallbackContext);

    void finish();

    //==============================================================================
    void singleStep();

    int readOrSkip(void* buffer, int bytesToRead, bool skip);


    //==============================================================================
    void parseHttpHeaders(StringPairArray& responseHeaders);


    //==============================================================================
    // CURL callbacks
    size_t curlWriteCallback(char* ptr, size_t size, size_t nmemb);

    size_t curlReadCallback(char* ptr, size_t size, size_t nmemb);

    size_t curlHeaderCallback(char* ptr, size_t size, size_t nmemb);

    //==============================================================================
    // Static method wrappers
    static size_t StaticCurlWrite(char* ptr, size_t size, size_t nmemb, void* userdata);

    static size_t StaticCurlRead(char* ptr, size_t size, size_t nmemb, void* userdata);

    static size_t StaticCurlHeader(char* ptr, size_t size, size_t nmemb, void* userdata);

private:
    CURLM* multi;
    CURL* curl;
    struct curl_slist* headerList;
    int lastError;

    //==============================================================================
    // internal buffers and buffer positions
    int64 contentLength, streamPos;
    MemoryBlock curlBuffer;
    String curlHeaders;
    bool finished;
    size_t skipBytes;

    //==============================================================================
    // Http POST variables
    const MemoryBlock* postBuffer;
    size_t postPosition;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WebInputStream)
};
