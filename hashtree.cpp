/*
 *  hashtree.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */

#include "hashtree.h"
//#include <openssl/sha.h>
#include "sha1.h"
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include "compat.h"
#include "ext/filehashstorage.h"

#ifdef _WIN32
#define OPENFLAGS         O_RDWR|O_CREAT|_O_BINARY
#else
#define OPENFLAGS         O_RDWR|O_CREAT
#endif


using namespace swift;

#define HASHSZ 20
const size_t Sha1Hash::SIZE = HASHSZ;
const Sha1Hash Sha1Hash::ZERO = Sha1Hash();

void SHA1 (const void *data, size_t length, unsigned char *hash) {
    blk_SHA_CTX ctx;
    blk_SHA1_Init(&ctx);
    blk_SHA1_Update(&ctx, data, length);
    blk_SHA1_Final(hash, &ctx);
}

Sha1Hash::Sha1Hash(const Sha1Hash& left, const Sha1Hash& right) {
    char data[HASHSZ*2];
    memcpy(data,left.bits,SIZE);
    memcpy(data+SIZE,right.bits,SIZE);
    SHA1((unsigned char*)data,SIZE*2,bits);
}

Sha1Hash::Sha1Hash(const char* data, size_t length) {
    if (length==-1)
        length = strlen(data);
    SHA1((unsigned char*)data,length,bits);
}

Sha1Hash::Sha1Hash(const uint8_t* data, size_t length) {
    SHA1(data,length,bits);
}

Sha1Hash::Sha1Hash(bool hex, const char* hash) {
    if (hex) {
        char hx[3]; hx[2]=0;
        int val;
        for(int i=0; i<SIZE; i++) {
            strncpy(hx,hash+i*2,2);
            if (sscanf(hx, "%x", &val)!=1) {
                memset(bits,0,20);
                return;
            }
            bits[i] = val;
        }
        assert(this->hex()==std::string(hash));
    } else
        memcpy(bits,hash,SIZE);
}

std::string    Sha1Hash::hex() const {
    char hex[HASHSZ*2+1];
    for(int i=0; i<HASHSZ; i++)
        sprintf(hex+i*2, "%02x", (int)(unsigned char)bits[i]);
    return std::string(hex,HASHSZ*2);
}



/**     H a s h   t r e e       */

HashTree::HashTree (const char* filename, const Sha1Hash& root_hash, const char* hash_filename) :
root_hash_(root_hash), fd_(0), data_recheck_(true),
peak_count_(0), size_(0), sizek_(0),
complete_(0), completek_(0), hash_storage_(NULL)
{
    fd_ = open(filename,OPENFLAGS,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (fd_<0) {
        fd_ = 0;
        print_error("cannot open the file");
        return;
    }
    char hfn[1024];
    strcpy(hfn,hash_filename);
    hash_storage_ = new FileHashStorage(hfn);
    if( !hash_storage_->valid() )
        return;
    if (root_hash_==Sha1Hash::ZERO) { // fresh submit, hash it
        assert(file_size(fd_));
        Submit();
    } else {
        RecoverProgress();
    } // else  LoadComplete()
}

HashTree::HashTree (const char* filename, const Sha1Hash& root_hash, HashStorage* hash_storage) :
root_hash_(root_hash), fd_(0), data_recheck_(true),
peak_count_(0), size_(0), sizek_(0),
complete_(0), completek_(0), hash_storage_(NULL)
{
    fd_ = open(filename,OPENFLAGS,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (fd_<0) {
        fd_ = 0;
        print_error("cannot open the file");
        return;
    }
    if( !hash_storage ) {
        char hfn[1024] = "";    // Construct a filename for the hash storage and use that
        strcat(hfn, filename);
        strcat(hfn, ".mhash");
        hash_storage_ = new FileHashStorage(hfn);
        if( !hash_storage_->valid() )
            return;
    }
    else {
        if( !hash_storage->valid() ) {
            delete hash_storage;
            print_error( "Invalid hash storage" ); // FIXME: Won't work
            return;
        }
        hash_storage_ = hash_storage;
    }
    if (root_hash_==Sha1Hash::ZERO) { // fresh submit, hash it
        assert(file_size(fd_));
        Submit();
    } else {
        RecoverProgress();
    } // else  LoadComplete()
}

void            HashTree::Submit () {
    size_ = file_size(fd_);
    sizek_ = (size_ + 1023) >> 10;
    peak_count_ = bin64_t::peaks(sizek_,peaks_);
    if( !hash_storage_->setHashCount( sizek_ ) ) {
        size_ = sizek_ = complete_ = completek_ = 0;
        return;
    }
    for (size_t i=0; i<sizek_; i++) {
        char kilo[1<<10];
        size_t rd = read(fd_,kilo,1<<10);
        if (rd<(1<<10) && i!=sizek_-1) {
            delete hash_storage_;
            hash_storage_ = NULL;
            return;
        }
        bin64_t pos(0,i);
        hash_storage_->setHash(pos,Sha1Hash(kilo,rd));
        ack_out_.set(pos);
        complete_+=rd;
        completek_++;
    }
    for (int p=0; p<peak_count_; p++) {
        if (!peaks_[p].is_base())
            for(bin64_t b=peaks_[p].left_foot().parent(); b.within(peaks_[p]); b=b.next_dfsio(1))
                hash_storage_->hashLeftRight(b);
        peak_hashes_[p] = hash_storage_->getHash(peaks_[p]);
    }

    root_hash_ = DeriveRoot();
}


/** Basically, simulated receiving every single packet, except
 for some optimizations. */
void            HashTree::RecoverProgress () {
    size_t size = file_size(fd_);
    size_t sizek = (size + 1023) >> 10;
    bin64_t peaks[64];
    int peak_count = bin64_t::peaks(sizek,peaks);
    for(int i=0; i<peak_count; i++) {
        Sha1Hash peak_hash;
        peak_hash = hash_storage_->getHash(peaks[i]);
        OfferPeakHash(peaks[i], peak_hash);
    }
    if (!this->size())
        return; // if no valid peak hashes found
    // at this point, we may use mmapd hashes already
    // so, lets verify hashes and the data we've got
    char zeros[1<<10];
    memset(zeros, 0, 1<<10);
    Sha1Hash kilo_zero(zeros,1<<10);
    for(int p=0; p<packet_size(); p++) {
        char buf[1<<10];
        bin64_t pos(0,p);
        if(hash_storage_->getHash(pos)==Sha1Hash::ZERO)
            continue;
        size_t rd = read(fd_,buf,1<<10);
        if (rd!=(1<<10) && p!=packet_size()-1)
            break;
        if (rd==(1<<10) && !memcmp(buf, zeros, rd) &&
                hash_storage_->getHash(pos)!=kilo_zero) // FIXME
            continue;
        if ( data_recheck_ && !OfferHash(pos, Sha1Hash(buf,rd)) )
            continue;
        ack_out_.set(pos);
        completek_++;
        complete_+=rd;
        if (rd!=(1<<10) && p==packet_size()-1)
            size_ = ((sizek_-1)<<10) + rd;
    }
}


bool            HashTree::OfferPeakHash (bin64_t pos, const Sha1Hash& hash) {
    assert(!size_);
    if (peak_count_) {
        bin64_t last_peak = peaks_[peak_count_-1];
        if ( pos.layer()>=last_peak.layer() ||
            pos.base_offset()!=last_peak.base_offset()+last_peak.width() )
            peak_count_ = 0;
    }
    peaks_[peak_count_] = pos;
    peak_hashes_[peak_count_] = hash;
    peak_count_++;
    // check whether peak hash candidates add up to the root hash
    Sha1Hash mustbe_root = DeriveRoot();
    if (mustbe_root!=root_hash_)
        return false;
    for(int i=0; i<peak_count_; i++)
        sizek_ += peaks_[i].width();

    // bingo, we now know the file size (rounded up to a KByte)

    size_ = sizek_<<10;
    completek_ = complete_ = 0;
    sizek_ = (size_ + 1023) >> 10;

    size_t cur_size = file_size(fd_);
    if ( cur_size<=(sizek_-1)<<10  || cur_size>sizek_<<10 ) {
        if (file_resize(fd_, size_)) {
            print_error("cannot set file size\n");
            size_=0; // remain in the 0-state
            return false;
        }
    }

    // tell the storage how big the file really is, for more efficient usage
    hash_storage_->setHashCount( sizek_ );

    for(int i=0; i<peak_count_; i++)
        hash_storage_->setHash(peaks_[i],peak_hashes_[i]);
    return true;
}


Sha1Hash        HashTree::DeriveRoot () {
    int c = peak_count_-1;
    bin64_t p = peaks_[c];
    Sha1Hash hash = peak_hashes_[c];
    c--;
    while (p!=bin64_t::ALL) {
        if (p.is_left()) {
            p = p.parent();
            hash = Sha1Hash(hash,Sha1Hash::ZERO);
        } else {
            if (c<0 || peaks_[c]!=p.sibling())
                return Sha1Hash::ZERO;
            hash = Sha1Hash(peak_hashes_[c],hash);
            p = p.parent();
            c--;
        }
    }
    return hash;
}


/** For live streaming: appends the data, adjusts the tree.
    @ return the number of fresh (tail) peak hashes */
int         HashTree::AppendData (char* data, int length) {
    return 0;
}


bin64_t         HashTree::peak_for (bin64_t pos) const {
    int pi=0;
    while (pi<peak_count_ && !pos.within(peaks_[pi]))
        pi++;
    return pi==peak_count_ ? bin64_t(bin64_t::NONE) : peaks_[pi];
}


bool            HashTree::OfferHash (bin64_t pos, const Sha1Hash& hash) {
    if (!size_)  // only peak hashes are accepted at this point
        return OfferPeakHash(pos,hash);
    bin64_t peak = peak_for(pos);
    if (peak==bin64_t::NONE)
        return false;
    if (peak==pos)
        return hash == hash_storage_->getHash(pos);
    if (ack_out_.get(pos.parent())!=binmap_t::EMPTY)
        return hash==hash_storage_->getHash(pos); // have this hash already, even accptd data
    hash_storage_->setHash( pos, hash );
    if (!pos.is_base())
        return false; // who cares?
    bin64_t p = pos;
    Sha1Hash uphash = hash;
    while ( p!=peak && ack_out_.get(p)==binmap_t::EMPTY ) {
        hash_storage_->setHash(p, uphash);
        p = p.parent();
        uphash = Sha1Hash(hash_storage_->getHash(p.left()),hash_storage_->getHash(p.right())) ;
    }// walk to the nearest proven hash
    return uphash==hash_storage_->getHash(p);
}


bool            HashTree::OfferData (bin64_t pos, const char* data, size_t length) {
    if (!size())
        return false;
    if (!pos.is_base())
        return false;
    if (length<1024 && pos!=bin64_t(0,sizek_-1))
        return false;
    if (ack_out_.get(pos)==binmap_t::FILLED)
        return true; // to set data_in_
    bin64_t peak = peak_for(pos);
    if (peak==bin64_t::NONE)
        return false;

    Sha1Hash data_hash(data,length);
    if (!OfferHash(pos, data_hash)) {
        //printf("invalid hash for %s: %s\n",pos.str(),data_hash.hex().c_str()); // paranoid
        return false;
    }

    //printf("g %lli %s\n",(uint64_t)pos,hash.hex().c_str());
    ack_out_.set(pos,binmap_t::FILLED);
    if( pwrite(fd_,data,length,pos.base_offset()<<10) < 0 )
        print_error( strerror( errno ) );
    complete_ += length;
    completek_++;
    if (pos.base_offset()==sizek_-1) {
        size_ = ((sizek_-1)<<10) + length;
        if (file_size(fd_)!=size_)
            file_resize(fd_,size_);
    }
    return true;
}


uint64_t      HashTree::seq_complete () {
    uint64_t seqk = ack_out_.seq_length();
    if (seqk==sizek_)
        return size_;
    else
        return seqk<<10;
}

HashTree::~HashTree () {
    if (fd_)
        close(fd_);
    if (hash_storage_)
        delete hash_storage_;
}

