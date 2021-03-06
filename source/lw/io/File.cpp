
#include <cstdlib>
#include <cstring>
#include <uv.h>

#include "lw/io/File.hpp"

namespace lw {
namespace io {

inline FileError _wrap_uv_error( int err_code ){
    return FileError(
        err_code,
        (std::string)uv_err_name( err_code ) + ": " + uv_strerror( err_code )
    );
}

// -------------------------------------------------------------------------- //

File::File( event::Loop& loop ):
    m_loop( loop ),
    m_handle( (uv_fs_s*)std::malloc( sizeof( uv_fs_s ) ) ),
    m_promise( nullptr ),
    m_file_descriptor( -1 ),
    m_uv_buffer( (uv_buf_t*)std::malloc( sizeof( uv_buf_t ) ) )
{
    m_handle->data = (void*)this;
}

// -------------------------------------------------------------------------- //

File::~File( void ){
    if( m_file_descriptor ){
        uv_fs_close( m_loop.lowest_layer(), m_handle, m_file_descriptor, nullptr );
    }

    uv_fs_req_cleanup( m_handle );

    std::free( m_uv_buffer );
    std::free( m_handle );
}

// -------------------------------------------------------------------------- //

event::Future<> File::open( const std::string& path, const std::ios::openmode mode ){
    int flags = O_CREAT
        | (mode & std::ios::app     ? O_APPEND  : 0)
        | (mode & std::ios::trunc   ? O_TRUNC   : 0)
    ;
    int permissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    if( mode & std::ios::in && mode & std::ios::out ){
        flags |= O_RDWR;
    }
    else if( mode & std::ios::in ){
        flags |= O_RDONLY;
    }
    else if( mode & std::ios::out ){
        flags |= O_WRONLY;
    }

    uv_fs_open(
        m_loop.lowest_layer(),
        m_handle,
        path.c_str(),
        flags,
        permissions,
        &File::_open_cb
    );

    return _reset_promise();
}

// -------------------------------------------------------------------------- //

void File::_open_cb( uv_fs_s* handle ){
    int result = handle->result;
    File* file = (File*)handle->data;
    if( result < 0 ){
        file->m_promise->reject( _wrap_uv_error( result ) );
    }
    else {
        file->m_file_descriptor = result;
        file->m_promise->resolve();
    }
}

// -------------------------------------------------------------------------- //

event::Future<> File::close( void ){
    uv_fs_close(
        m_loop.lowest_layer(),
        m_handle,
        m_file_descriptor,
        &File::_close_cb
    );
    return _reset_promise();
}

// -------------------------------------------------------------------------- //

void File::_close_cb( uv_fs_s* handle ){
    int result = handle->result;
    File* file = (File*)handle->data;
    if( result < 0 ){
        file->m_promise->reject( _wrap_uv_error( result ) );
    }
    else {
        file->m_file_descriptor = -1;
        file->m_promise->resolve();
    }
}

// -------------------------------------------------------------------------- //

event::Future< int > File::read( memory::Buffer& data ){
    *m_uv_buffer = uv_buf_init( (char*)data.data(), data.size() );

    uv_fs_read(
        m_loop.lowest_layer(),
        m_handle,
        m_file_descriptor,
        m_uv_buffer,
        1,
        -1,
        &File::_read_cb
    );

    auto handlePtr = m_handle;
    return _reset_promise()
        .then< int >([ handlePtr ]( event::Promise< int >&& promise ){
            promise.resolve( handlePtr->result );
        })
    ;
}

// -------------------------------------------------------------------------- //

event::Future< memory::Buffer > File::read( const std::size_t bytes ){
    auto dataPtr = std::make_shared< memory::Buffer >( bytes );
    return read( *dataPtr )
        .then([ dataPtr ]( int size ) mutable {
            return memory::Buffer( std::move( *dataPtr ), size );
        })
    ;
}

// -------------------------------------------------------------------------- //

void File::_read_cb( uv_fs_s* handle ){
    int result = handle->result;
    File* file = (File*)handle->data;
    if( result < 0 ){
        file->m_promise->reject( _wrap_uv_error( result ) );
    }
    else {
        file->m_promise->resolve();
    }
}

// -------------------------------------------------------------------------- //

event::Future<> File::write( const memory::Buffer& data ){
    *m_uv_buffer = uv_buf_init( (char*)data.data(), data.size() );

    uv_fs_write(
        m_loop.lowest_layer(),
        m_handle,
        m_file_descriptor,
        m_uv_buffer,
        1,
        -1,
        &File::_write_cb
    );

    return _reset_promise();
}

// -------------------------------------------------------------------------- //

void File::_write_cb( uv_fs_s* handle ){
    int result = handle->result;
    File* file = (File*)handle->data;
    if( result < 0 ){
        file->m_promise->reject( _wrap_uv_error( result ) );
    }
    else {
        file->m_promise->resolve();
    }
}

// -------------------------------------------------------------------------- //

event::Future<> File::_reset_promise( void ){
    m_promise = std::make_unique< event::Promise<> >();
    return m_promise->future();
}

// -------------------------------------------------------------------------- //

event::Future< std::shared_ptr< File > > open(
    event::Loop& loop,
    const std::string& path,
    const std::ios::openmode mode
){
    auto file = std::make_shared< File >( loop );
    return file->open( path, mode )
        .then([ file ](){
            return file;
        })
    ;
}

}
}
