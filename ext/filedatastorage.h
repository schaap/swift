#ifndef FILEDATASTORAGE_H
#define FILEDATASTORAGE_H

#include "../bin64.h"
#include "../compat.h"
#include "../storage.h"

namespace swift {

    class FileDataStorage : public DataStorage {
    private:
        int fd_;
        char* filename_;

    public:
        FileDataStorage( const char* filename );
        ~FileDataStorage();
        virtual size_t read( off_t pos, char* buf, size_t len );
        virtual size_t write( off_t pos, const char* buf, size_t len );
        virtual size_t read( bin64_t pos, char* buf, size_t len ); 
        virtual size_t write( bin64_t pos, const char* buf, size_t len ); 
        virtual size_t read( char* buf, size_t len );
        virtual size_t write( const char* buf, size_t len );
        virtual size_t size();
        virtual bool setSize( size_t len );
        virtual bool valid();
        const char* filename() { return filename_; }
    };

}

#endif
