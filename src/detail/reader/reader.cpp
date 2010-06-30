/******************************************************************************
 * $Id$
 *
 * Project:  libLAS - http://liblas.org - A BSD library for LAS format data.
 * Purpose:  LAS 1.0 reader implementation for C++ libLAS 
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2008, Mateusz Loskot
 *
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following 
 * conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright 
 *       notice, this list of conditions and the following disclaimer in 
 *       the documentation and/or other materials provided 
 *       with the distribution.
 *     * Neither the name of the Martin Isenburg or Iowa Department 
 *       of Natural Resources nor the names of its contributors may be 
 *       used to endorse or promote products derived from this software 
 *       without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT 
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 ****************************************************************************/

#include <liblas/detail/reader/reader.hpp>

#include <liblas/detail/utility.hpp>
#include <liblas/liblas.hpp>
#include <liblas/lasheader.hpp>
#include <liblas/laspoint.hpp>

// std
#include <fstream>
#include <istream>
#include <iostream>
#include <stdexcept>
#include <cstddef> // std::size_t
#include <cstdlib> // std::free
#include <cassert>

namespace liblas { namespace detail { 

ReaderImpl::ReaderImpl(std::istream& ifs) :
    m_ifs(ifs), m_size(0), m_current(0),
    m_point_reader(0),     
    m_header_reader(new reader::Header(m_ifs))

{
}

ReaderImpl::~ReaderImpl()
{

    delete m_point_reader;
    delete m_header_reader;
}

std::istream& ReaderImpl::GetStream() const
{
    return m_ifs;
}

void ReaderImpl::Reset(liblas::Header const& header)
{
    m_ifs.clear();
    m_ifs.seekg(0);

    // Reset sizes and set internal cursor to the beginning of file.
    m_current = 0;
    m_size = header.GetPointRecordsCount();
    
    // If we reset the reader, we're ready to start reading points, so 
    // we'll create a point reader at this point.
    if (m_point_reader == 0) {
        m_point_reader = new reader::Point(m_ifs, header);
    } 
}

liblas::Header const& ReaderImpl::ReadHeader()
{
    m_header_reader->read();
    const liblas::Header& header = m_header_reader->GetHeader();
    
    Reset(header);
    
    return header;
}

liblas::Point const& ReaderImpl::ReadNextPoint(const liblas::Header& header)
{
    if (0 == m_current)
    {
        m_ifs.clear();
        m_ifs.seekg(header.GetDataOffset(), std::ios::beg);

    }

    if (m_current < m_size)
    {
        m_point_reader->read();
        const liblas::Point& point = m_point_reader->GetPoint();
        ++m_current;
        return point;

    } else if (m_current == m_size ){
        throw std::out_of_range("file has no more points to read, end of file reached");
    } else {
        throw std::runtime_error("ReadNextPoint: m_current > m_size, something has gone extremely awry");
    }

}

liblas::Point const& ReaderImpl::ReadPointAt(std::size_t n, const liblas::Header& header)
{
    if (m_size == n) {
        throw std::out_of_range("file has no more points to read, end of file reached");
    } else if (m_size < n) {
        std::ostringstream output;
        output << "ReadPointAt:: Inputted value: " << n << " is greater than the number of points: " << m_size;
        std::string out(output.str());
        throw std::runtime_error(out);
    } 

    std::streamsize pos = (static_cast<std::streamsize>(n) * header.GetDataRecordLength()) + header.GetDataOffset();    

    m_ifs.clear();
    m_ifs.seekg(pos, std::ios::beg);

    m_point_reader->read();
    const liblas::Point& point = m_point_reader->GetPoint();
    
    return point;
}

void ReaderImpl::Seek(std::size_t n, const liblas::Header& header)
{
    if (m_size == n) {
        throw std::out_of_range("file has no more points to read, end of file reached");
    } else if (m_size < n) {
        std::ostringstream output;
        output << "Seek:: Inputted value: " << n << " is greater than the number of points: " << m_size;
        std::string out(output.str());
        throw std::runtime_error(out);
    } 

    std::streamsize pos = (static_cast<std::streamsize>(n) * header.GetDataRecordLength()) + header.GetDataOffset();    

    m_ifs.clear();
    m_ifs.seekg(pos, std::ios::beg);
    
    m_current = n+1;
}

CachedReaderImpl::CachedReaderImpl(std::istream& ifs , std::size_t size) :
    ReaderImpl(ifs), m_cache_size(size), m_cache_start_position(0), m_cache_read_position(0)
{
}


liblas::Header const& CachedReaderImpl::ReadHeader()
{
    const liblas::Header& header = ReaderImpl::ReadHeader();
    
    // If we were given no cache size, try to cache the whole thing
    if (m_cache_size == 0) {
        m_cache_size = header.GetPointRecordsCount();
    }

    if (m_cache_size > header.GetPointRecordsCount()) {
        m_cache_size = header.GetPointRecordsCount();
    }
    m_cache.resize(m_cache_size);
    
    // Mark all positions as uncached and build up the mask
    // to the size of the number of points in the file
    for (uint32_t i = 0; i < header.GetPointRecordsCount(); ++i) {
        m_mask.push_back(0);
    }

    
    return header;
}

void CachedReaderImpl::CacheData(liblas::uint32_t position, const liblas::Header& header) 
{
        int32_t old_cache_start_position = m_cache_start_position;
        m_cache_start_position = position;

    std::vector<uint8_t>::size_type header_size = static_cast<std::vector<uint8_t>::size_type>(header.GetPointRecordsCount());
    std::vector<uint8_t>::size_type left_to_cache = std::min(m_cache_size, header_size - m_cache_start_position);

    std::vector<uint8_t>::size_type to_mark = std::max(m_cache_size, left_to_cache);
        for (uint32_t i = 0; i < to_mark; ++i) {
            m_mask[old_cache_start_position + i] = 0;
        }

        // if these aren't equal, we've hopped around with ReadPointAt
        // and we need to seek to the proper position.
        if (m_current != position) {
            CachedReaderImpl::Seek(position, header);
            m_current = position;
        }
        m_cache_read_position =  position;

        for (uint32_t i = 0; i < left_to_cache; ++i) 
        {
            try {
                m_mask[m_current] = 1;
                m_cache[i] = ReaderImpl::ReadNextPoint(header);
            } catch (std::out_of_range&) {
                // cached to the end
                break;
            }
        }

}

liblas::Point const& CachedReaderImpl::ReadCachedPoint(liblas::uint32_t position, const liblas::Header& header) {
    
    int32_t cache_position = position - m_cache_start_position ;

    // std::cout << "MASK: ";
    // std::vector<bool>::iterator it;
    // for (it = m_mask.begin(); it != m_mask.end(); ++it) {
    //     std::cout << *it << ",";
    // }
    // std::cout << std::endl;

    if (m_mask[position] == 1) {
        m_cache_read_position = position;
        return m_cache[cache_position];
    } else {

        CacheData(position, header);
        
        // At this point, we can't have a negative cache position.
        // If we do, it's a big error or we'll segfault.
        cache_position = position - m_cache_start_position ;
        if (cache_position < 0) {
            std::ostringstream output;
            output  << "ReadCachedPoint:: cache position: " 
                    << cache_position 
                    << " is negative. position or m_cache_start_position is invalid "
                    << "position: " << position << " m_cache_start_position: "
                    << m_cache_start_position;
            std::string out(output.str());
            throw std::runtime_error(out);   
        }
            
        if (m_mask[position] == 1) {
            if (static_cast<uint32_t>(cache_position) > m_cache.size()) {
                std::ostringstream output;
                output  << "ReadCachedPoint:: cache position: " 
                        << position 
                        << " greater than cache size: " << m_cache.size() ;
                std::string out(output.str());
                throw std::runtime_error(out);                
            }
            return m_cache[cache_position];
        } else {
            std::ostringstream output;
            output << "ReadCachedPoint:: unable to obtain cached point"
                      " at position: " << position 
                   << " cache_position was " << cache_position ;
            std::string out(output.str());
            
            throw std::runtime_error(out);
        }

    }
    
}

liblas::Point const& CachedReaderImpl::ReadNextPoint(const liblas::Header& header)
{
    if (m_cache_read_position == m_size ){
        throw std::out_of_range("file has no more points to read, end of file reached");
    }
    
    liblas::Point const& point = ReadCachedPoint(m_cache_read_position, header);
    ++m_cache_read_position;
    return point;
}

liblas::Point const& CachedReaderImpl::ReadPointAt(std::size_t n, const liblas::Header& header)
{

    if (n >= m_size ){
        throw std::out_of_range("file has no more points to read, end of file reached");
    
    } else if (m_size < n) {
        std::ostringstream output;
        output << "ReadPointAt:: Inputted value: " << n << " is greater than the number of points: " << m_size;
        std::string out(output.str());
        throw std::runtime_error(out);
    }

    liblas::Point const& point = ReadCachedPoint(n, header);
    m_cache_read_position = n;
    return point;
}

void CachedReaderImpl::Reset(liblas::Header const& header)
{
    
    if (m_mask.size() > 0) {

    std::vector<uint8_t>::size_type header_size = static_cast<std::vector<uint8_t>::size_type>(header.GetPointRecordsCount());
    std::vector<uint8_t>::size_type left_to_cache = std::min(m_cache_size, header_size - m_cache_start_position);

    std::vector<uint8_t>::size_type to_mark = std::max(m_cache_size, left_to_cache);
        for (uint32_t i = 0; i < to_mark; ++i) {

            m_mask[m_cache_start_position + i] = 0;
        }

        m_cache_start_position = 0;
        m_cache_read_position = 0;
    
    }
    
    ReaderImpl::Reset(header);

}

void CachedReaderImpl::Seek(std::size_t n, const liblas::Header& header)
{

   if (n < 1) {
       CachedReaderImpl::Reset(header);
   }
   
   m_cache_read_position = n;
   ReaderImpl::Seek(n,header);
}
// 
// void CachedReaderImpl::SetOutputSRS(const SpatialReference& srs, const liblas::Header& header)
// {
//     // We need to wipe out the cache if we've set the output srs.
//     std::vector<uint8_t>::size_type header_size = static_cast<std::vector<uint8_t>::size_type>(header.GetPointRecordsCount());
//     std::vector<uint8_t>::size_type left_to_cache = std::min(m_cache_size, header_size - m_cache_start_position);
// 
//     std::vector<uint8_t>::size_type to_mark = std::max(m_cache_size, left_to_cache);
//     for (uint32_t i = 0; i < to_mark; ++i) {
//         m_mask[m_cache_start_position + i] = 0;
//     }
//     
//     ReaderImpl::SetOutputSRS(srs, header);
// }

// ReaderImpl* ReaderFactory::Create(std::istream& ifs)
// {
//     if (!ifs)
//     {
//         throw std::runtime_error("input stream state is invalid");
//     }
// 
//     // Determine version of given LAS file and
//     // instantiate appropriate reader.
//     // uint8_t verMajor = 0;
//     // uint8_t verMinor = 0;
//     // ifs.seekg(24, std::ios::beg);
//     // detail::read_n(verMajor, ifs, 1);
//     // detail::read_n(verMinor, ifs, 1);
//     
//     return new ReaderImpl(ifs);
//     // return new CachedReaderImpl(ifs, 3);
//     
//     // if (1 == verMajor && 0 == verMinor)
//     // {
//     // 
//     //     return new ReaderImpl(ifs);
//     // }
//     // else if (1 == verMajor && 1 == verMinor)
//     // {
//     //     return new v11::ReaderImpl(ifs);
//     // }
//     // else if (1 == verMajor && 2 == verMinor)
//     // {
//     //     return new v12::ReaderImpl(ifs);
//     // }
//     // else if (2 == verMajor && 0 == verMinor )
//     // {
//     //     // TODO: LAS 2.0 read/write support
//     //     throw std::runtime_error("LAS 2.0+ file detected but unsupported");
//     // }
// 
//     // throw std::runtime_error("LAS file of unknown version");
// }


// void ReaderFactory::Destroy(ReaderImpl* p) 
// {
//     delete p;
//     p = 0;
// }
}} // namespace liblas::detail

