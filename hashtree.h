/*
 *  hashtree.h
 *  hashing, Merkle hash trees and data integrity
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#ifndef SWIFT_SHA1_HASH_TREE_H
#define SWIFT_SHA1_HASH_TREE_H
#include "bin64.h"
#include "bins.h"
#include "storage.h"
#include "sha1hash.h"
#include <string.h>
#include <string>


namespace swift {

/** This class controls data integrity of some file; hash tree is put to
    an auxilliary file next to it. The hash tree file is mmap'd for
    performance reasons. Actually, I'd like the data file itself to be
    mmap'd, but 32-bit platforms do not allow that for bigger files. 
 
    There are two variants of the general workflow: either a HashTree
    is initialized with a root hash and the rest of hashes and data is
    spoon-fed later, OR a HashTree is initialized with a data file, so
    the hash tree is derived, including the root hash.
 */
class HashTree {

    /** Merkle hash tree: root */
    Sha1Hash        root_hash_;
    /** Merkle hash tree: peak hashes */
    Sha1Hash        peak_hashes_[64];
    bin64_t         peaks_[64];
    int             peak_count_;
    /** Stores for data and hashes */
    DataStorage*    data_storage_;
    HashStorage*    hash_storage_;
    /** Whether to re-hash files. */
    bool            data_recheck_;
    /** Base size, as derived from the hashes. */
    size_t          size_;
    size_t          sizek_;
    /**    Part of the tree currently checked. */
    size_t          complete_;
    size_t          completek_;
    binmap_t            ack_out_;

protected:
    
    void            Submit();
    void            RecoverProgress();
    Sha1Hash        DeriveRoot();
    bool            OfferPeakHash (bin64_t pos, const Sha1Hash& hash);
    
public:
    
    // @deprecated
    HashTree (const char* file_name, const Sha1Hash& root, 
              const char* hash_filename);
    /// After offering the hash_storage to this constructor, it is governed by the HashTree object and will be deleted when deemed appropriate.
    HashTree (const char* file_name, const Sha1Hash& root=Sha1Hash::ZERO,
              HashStorage* hash_storage=NULL);
    /// After offering the hash_storage to this constructor, it is governed by the HashTree object and will be deleted when deemed appropriate.
    /// Ditto for the data_storage.
    HashTree (DataStorage* data_storage, const Sha1Hash& root=Sha1Hash::ZERO,
              HashStorage* hash_storage=NULL);
    
    /** Offer a hash; returns true if it verified; false otherwise.
     Once it cannot be verified (no sibling or parent), the hash
     is remembered, while returning false. */
    bool            OfferHash (bin64_t pos, const Sha1Hash& hash);
    /** Offer data; the behavior is the same as with a hash:
     accept or remember or drop. Returns true => ACK is sent. */
    bool            OfferData (bin64_t bin, const char* data, size_t length);
    /** For live streaming. Not implemented yet. */
    int             AppendData (char* data, int length) ;
    
    DataStorage*    data_storage () const { return data_storage_; }
    /** Returns the number of peaks (read on peak hashes). */
    int             peak_count () const { return peak_count_; }
    /** Returns the i-th peak's bin number. */
    bin64_t         peak (int i) const { return peaks_[i]; }
    /** Returns peak hash #i. */
    const Sha1Hash& peak_hash (int i) const { return peak_hashes_[i]; }
    /** Return the peak bin the given bin belongs to. */
    bin64_t         peak_for (bin64_t pos) const;
    /** Return a (Merkle) hash for the given bin. */
    const Sha1Hash& hash (bin64_t pos) const {return hash_storage_->getHash(pos);}
    /** Give the root hash, which is effectively an identifier of this file. */
    const Sha1Hash& root_hash () const { return root_hash_; }
    /** Get file size, in bytes. */
    uint64_t        size () const { return size_; }
    /** Get file size in packets (in kilobytes, rounded up). */
    uint64_t        packet_size () const { return sizek_; }
    /** Number of bytes retrieved and checked. */
    uint64_t        complete () const { return complete_; }
    /** Number of packets retrieved and checked. */
    uint64_t        packets_complete () const { return completek_; }
    /** The number of bytes completed sequentially, i.e. from the beginning of
        the file, uninterrupted. */
    uint64_t        seq_complete () ;
    /** Whether the file is complete. */
    bool            is_complete () 
        { return size_ && complete_==size_; }
    /** The binmap of complete packets. */
    binmap_t&           ack_out () { return ack_out_; }
    
    ~HashTree ();

    
};

}

#endif
