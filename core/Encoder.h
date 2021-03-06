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

#ifndef ENCODER_H
#define ENCODER_H

#include <string>
#include <vector>

#include "common/AudioFormatType.h"
#include "common/ErrorCodes.h"

namespace core
{

class Encoder
{
public:

    Encoder( ) = delete;

    virtual ~Encoder( );

    common::ErrorCode scan_input_directory( const std::string& dir );

    const std::vector< std::string >& get_input_files( ) const;

    virtual common::ErrorCode start_encoding( ) = 0;

    virtual common::ErrorCode cancel_encoding( ) = 0;

protected:

    Encoder( common::AudioFormatType input_type, common::AudioFormatType output_type );

protected:

    common::AudioFormatType m_input_type;
    common::AudioFormatType m_output_type;
    std::string m_input_directory;
    std::vector< std::string > m_input_files;
};

} // core

#endif // ENCODER_H
