// -------------------------------------------------------------------------------------------------
//
// Copyright (C) all of the contributors. All rights reserved.
//
// This software, including documentation, is protected by copyright controlled by
// contributors. All rights are reserved. Copying, including reproducing, storing,
// adapting or translating, any or all of this material requires the prior written
// consent of all contributors.
//
// -------------------------------------------------------------------------------------------------

#ifndef ENCODER_MP3_H
#define ENCODER_MP3_H

#include <string>
#include <vector>
#include <map>
#include <queue>
#include <pthread.h>
#include <functional>
#include <mutex>

#include "Encoder.h"

namespace core
{

class EncoderMP3 : public Encoder
{
public:

    typedef std::function< void( const std::string&, const std::string& ) > Callback;

    struct EncoderThreadArg
    {
        uint32_t thread_id;
        std::map< std::string, bool >* input_files;
        bool* cancelled;
        Callback callback;
    };

public:

    EncoderMP3( common::AudioFormatType input_type, uint16_t thread_number = 1 );

    ~EncoderMP3( ) override;

    const std::string& get_encoder_version( ) const;

    common::ErrorCode start_encoding( ) override;

    common::ErrorCode cancel_encoding( ) override;

protected:

    void on_encoding_status( const std::string& key, const std::string& value );

private:

    static void* processing_files( void* arg );

private:

    std::string m_encoder_version;
    uint16_t m_thread_number;
    std::map< std::string, bool > m_to_be_encoded_files;
    bool m_cancelled;
    std::deque< std::string > m_status;
    mutable std::mutex m_mutex;
};

} // core

#endif // ENCODER_MP3_H
