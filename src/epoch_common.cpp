//
// Created by daoful on 18-7-27.
//

#include <src/epoch_common.h>


germ::epoch_counts::epoch_counts()
        :count(0)
{
}

size_t germ::epoch_counts::sum()
{
    return count;
}

//germ::epoch_infos::epoch_infos ()
//        :timestamp(0),
//         prev(0),
//         merkle(0)
//{
//    signature.clear();
//}
//
//germ::epoch_infos::epoch_infos (MDB_val const & val_a)
//{
//    assert (val_a.mv_size == sizeof (*this));
//    static_assert (sizeof (timestamp) + sizeof (prev) + sizeof (merkle) + sizeof (signature) == sizeof (*this), "Packed class");
//    std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
//}
//
//germ::epoch_infos::epoch_infos (uint64_t const timestamp_r, germ::epoch_hash const & prev_r, germ::epoch_hash const & merkle_r, germ::signature const & signature_r)
//        :timestamp(timestamp_r),
//         prev(prev_r),
//         merkle(merkle_r),
//         signature(signature_r)
//{
//}
//
//void germ::epoch_infos::serialize (germ::stream & stream_a) const
//{
//    germ::write (stream_a, timestamp);
//    germ::write (stream_a, prev.bytes);
//    germ::write (stream_a, merkle);
//    germ::write (stream_a, signature.bytes);
//}
//
//bool germ::epoch_infos::deserialize (germ::stream & stream_a)
//{
//    auto error (germ::read (stream_a, timestamp));
//    if (error)
//        return false;
//
//    error = germ::read (stream_a, prev.bytes);
//    if (error)
//        return false;
//
//    error = germ::read (stream_a, merkle.bytes);
//    if (error)
//        return false;
//
//    error = germ::read (stream_a, signature.bytes);
//
//    return error;
//}
//
//bool germ::epoch_infos::operator== (germ::epoch_infos const & other_a) const
//{
//    return timestamp == other_a.timestamp && prev == other_a.prev && merkle == other_a.merkle && signature == other_a.signature;
//}
//
//germ::mdb_val germ::epoch_infos::val () const
//{
//    return germ::mdb_val (sizeof (*this), const_cast<germ::epoch_infos *> (this));
//}