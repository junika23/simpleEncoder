// -------------------------------------------------------------------------------------------------
//
// Copyright (C) Juniarto Saputra (juniarto.wongso@gmail.com). All rights reserved.
//
// This software, including documentation, is protected by copyright controlled by
// me. All rights are reserved. Copying, including reproducing, storing,
// adapting or translating, any or all of this material requires the prior written
// consent of Juniarto Saputra.
//
// -------------------------------------------------------------------------------------------------

#include "EncoderMP3.h"
#include "utils/WAVEHelper.h"

#include <lame/lame.h>
#include <iostream>
//#include <chrono>
//#include <iomanip>
#include <sstream>
#include <fstream>

namespace core
{

namespace
{
const std::string LAME = "Lame ";
const std::string OUTPUT_EXT = ".mp3";
static pthread_mutex_t process_mutex = PTHREAD_MUTEX_INITIALIZER;

// -------------------------------------------------------------------------------------------------

std::string
generate_output_file( const std::string& input_file )
{
    std::string output;

    size_t pos = input_file.find_last_of( "." );

    if ( pos == std::string::npos )
    {
        // File has no extension. Just append .mp3 extension.
        output = input_file + OUTPUT_EXT;
    }
    else
    {
        output = input_file.substr( 0, pos );
        output += OUTPUT_EXT;
    }

    return output;
}

void
log( const EncoderMP3::Callback& callback, uint32_t id, const std::string& status )
{
//    auto now = std::chrono::system_clock::now( );
//    auto time = std::chrono::system_clock::to_time_t( now );
//    auto ms = std::chrono::duration_cast< std::chrono::milliseconds >( now.time_since_epoch( ) ) -
//              std::chrono::duration_cast< std::chrono::seconds >( now.time_since_epoch( ) );

//    std::ostringstream oss;
//    oss << std::put_time(std::localtime(&time), "%H:%M:%S." );
//    oss << ms.count();

    callback( "oss.str( )", "Thread id: " + std::to_string( id ) + ", " + status );
}

} // namespace

// -------------------------------------------------------------------------------------------------

EncoderMP3::EncoderMP3( InputAudioType input_type, uint16_t thread_number )
    : Encoder( input_type, OutputEncoderType::OUTPUT_MP3 )
    , m_encoder_version( LAME + get_lame_version( ) )
    , m_thread_number( thread_number )
{
}

// -------------------------------------------------------------------------------------------------

EncoderMP3::~EncoderMP3( )
{
}

// -------------------------------------------------------------------------------------------------

const std::string&
EncoderMP3::get_encoder_version( ) const
{
    return m_encoder_version;
}

// -------------------------------------------------------------------------------------------------

void*
EncoderMP3::processing_files( void* arg )
{
    auto error = common::ErrorCode::ERROR_NONE;
    core::EncoderMP3::EncoderThreadArg* thread_arg = ( core::EncoderMP3::EncoderThreadArg* )arg;
    const uint32_t thread_id = thread_arg->thread_id;
    const auto& callback = thread_arg->callback;

    while ( true )
    {
        std::string input_file;

        pthread_mutex_lock( &process_mutex );

        for ( auto it = thread_arg->input_files->begin( );
              it != thread_arg->input_files->end( ); it++ )
        {
            if ( !it->second )
            {
                input_file = it->first;
                it->second = true;

                break;
            }
        }

        pthread_mutex_unlock( &process_mutex );

        if ( input_file.empty( ) )
        {
            break;
        }

        log( callback, thread_id, "Processing " + input_file );

        std::string output_file = generate_output_file( input_file );

        lame_global_flags* g_lame_flags = lame_init( );
        lame_set_brate( g_lame_flags, 128 );
        lame_set_quality( g_lame_flags, 3 );

        utils::WAVEHelper::WAVEHeader header;
        int16_t* left = NULL;
        int16_t* right = NULL;

        log( callback, thread_id, "reading PCM data from " + input_file );

        if ( !utils::WAVEHelper::get_wave_data( input_file, header, left, right ) )
        {
            error = common::ErrorCode::ERROR_READ_FILE;
            fprintf( stderr, "Error utils::WAVEHelper::get_wave_data() at %s:%d",
                     __FILE__, __LINE__ );
            log( callback, thread_id,
                 "Error while reading PCM data from " + input_file );
            break;
        }

        uint32_t samples = header.data_size / header.block_align;
        lame_set_num_channels( g_lame_flags, header.channels );
        lame_set_num_samples( g_lame_flags, samples );
        lame_set_bWriteVbrTag( g_lame_flags, 0 );

        log( callback, thread_id, "Initializing LAME" );

        auto err = lame_init_params( g_lame_flags );

        if ( err )
        {
            error = common::ErrorCode::ERROR_LAME;
            fprintf( stderr, "Error lame_init_params() returned %d at %s:%d",
                     err, __FILE__, __LINE__ );
            log( callback, thread_id, "Error while initializing LAME" );
            break;
        }

        uint32_t buffer_size = 1.25 * samples + 7200;
        uint8_t* mp3_buffer = new uint8_t[ buffer_size ];

        log( callback, thread_id, "Start encoding ..." );

        auto encoded_size = lame_encode_buffer( g_lame_flags,
                                                left,
                                                right,
                                                samples,
                                                mp3_buffer,
                                                buffer_size );

        if ( encoded_size <= 0 )
        {
            delete [ ] mp3_buffer;
            error = common::ErrorCode::ERROR_LAME;
            fprintf( stderr, "Error lame_encode_buffer() returned %d at %s:%d",
                     encoded_size, __FILE__, __LINE__ );
            log( callback, thread_id,
                 "Error while encoding PCM data from " + input_file );
            break;
        }

        log( callback, thread_id, "Receiving and writing encoded data" );

        FILE* output = fopen( output_file.c_str( ), "wb+" );
        if ( !output )
        {
            error = common::ErrorCode::ERROR_IO;
            fprintf( stderr, "Error fopen() returned at %s:%d",
                     __FILE__, __LINE__ );
            log( callback, thread_id,
                 "Error while writing encoded data to " + output_file );
            break;
        }

        fwrite( ( void* )mp3_buffer, sizeof( uint8_t ), encoded_size, output );

        log( callback, thread_id, "Flushing LAME" );

        uint32_t flush = lame_encode_flush( g_lame_flags, mp3_buffer, buffer_size );

        log( callback, thread_id, "Writing final encoded data" );

        fwrite( ( void* )mp3_buffer, sizeof( uint8_t ), flush, output );

        lame_mp3_tags_fid( g_lame_flags, output );
        fclose( output );
        delete [ ] left;
        delete [ ] right;
        delete [ ] mp3_buffer;

        lame_close( g_lame_flags );

        log( callback, thread_id, "Process done, output file: " + output_file );
    }

    pthread_exit( ( void* )error );
}

// -------------------------------------------------------------------------------------------------

common::ErrorCode
EncoderMP3::start_encoding( )
{
    if ( m_input_files.empty( ) )
    {
        return common::ERROR_NOT_FOUND;
    }

    m_to_be_encoded_files.clear( );

    for( const auto& file : m_input_files )
    {
        m_to_be_encoded_files[ file ] = false;
    }

    pthread_t threads[ m_thread_number ];
    pthread_attr_t thread_attr;
    pthread_attr_init( &thread_attr );
    pthread_attr_setdetachstate( &thread_attr, PTHREAD_CREATE_JOINABLE );

    for ( int i = 0; i < m_thread_number; i++ )
    {
        EncoderThreadArg thread_arg;
        thread_arg.thread_id = ( i + 1 );
        thread_arg.input_files = &m_to_be_encoded_files;

        auto callback = [ this ] ( const std::string& key, const std::string& value )
        {
            on_encoding_status( key, value );
        };

        thread_arg.callback = std::move( callback );

        RETURN_ERR_IF_ERROR( pthread_create( &threads[ i ],
                                             &thread_attr,
                                             EncoderMP3::processing_files,
                                             ( void* )&thread_arg ),
                             common::ErrorCode::ERROR_PTHREAD_CREATE );
    }

    pthread_attr_destroy( &thread_attr );

    for ( int i = 0; i < m_thread_number; i++ )
    {
        RETURN_ERR_IF_ERROR( pthread_join( threads[ i ], NULL ),
                             common::ErrorCode::ERROR_PTHREAD_JOIN );
    }

#ifdef ENCODER_ENABLE_LOG
    std::ofstream ofs( ENCODER_LOG_FILE );
    if ( ofs.is_open( ) )
    {
        for ( const auto& status : m_status )
        {
            ofs << status << std::endl;
        }
    }
    ofs.close( );
#endif

    return common::ErrorCode::ERROR_NONE;
}

// -------------------------------------------------------------------------------------------------

common::ErrorCode
EncoderMP3::cancel_encoding( )
{
    return common::ErrorCode::ERROR_NOT_IMPLEMENTED;
}

// -------------------------------------------------------------------------------------------------

void
EncoderMP3::on_encoding_status( const std::string& key, const std::string& value )
{
    std::lock_guard< std::mutex > guard( m_mutex );

    std::string log = key + " " + value;
    m_status.emplace_back( log );

    std::cout << log << std::endl;
}

// -------------------------------------------------------------------------------------------------

} // core
