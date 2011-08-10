#ifndef FILEOFFSETDATASTORAGE_H
#define FILEOFFSETDATASTORAGE_H

#include "../bin64.h"
#include "../compat.h"
#include "../storage.h"
#include "../ext/filedatastorage.h"

namespace swift {

    class FileOffsetDataStorage : public DataStorage {
    protected:
        size_t offset_;
        size_t size_;
        size_t cur_;
        char* filename_;
        size_t fd_;

    public:
        FileOffsetDataStorage( const char* filename, size_t offset );
        ~FileOffsetDataStorage();
        virtual size_t read( off_t pos, char* buf, size_t len );
        virtual size_t write( off_t pos, const char* buf, size_t len );
        virtual size_t read( bin64_t pos, char* buf, size_t len ); 
        virtual size_t write( bin64_t pos, const char* buf, size_t len ); 
        virtual size_t read( char* buf, size_t len );
        virtual size_t write( const char* buf, size_t len );
        virtual size_t size();
        virtual bool setSize( size_t len );
        virtual bool valid();
        virtual const char* filename() { return filename_; }
    };

}

#endif
