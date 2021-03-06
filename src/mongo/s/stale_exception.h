/*    Copyright 2014 MongoDB Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include "mongo/db/jsobj.h"
#include "mongo/s/chunk_version.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {

    using mongoutils::str::stream;

    /**
     * Thrown whenever your config info for a given shard/chunk is out of date.
     */
    class StaleConfigException : public AssertionException {
    public:
        StaleConfigException( const std::string& ns,
                              const std::string& raw,
                              int code,
                              ChunkVersion received,
                              ChunkVersion wanted,
                              bool justConnection = false )
            : AssertionException(stream() << raw << " ( ns : " << ns
                                          << ", received : " << received.toString()
                                          << ", wanted : " << wanted.toString()
                                          << ", " << ( code == SendStaleConfigCode ?
                                                       "send" : "recv" ) << " )",
                                 code ),
              _justConnection(justConnection),
              _ns(ns),
              _received( received ),
              _wanted( wanted ) {
        }

        /** Preferred if we're rebuilding this from a thrown exception */
        StaleConfigException( const std::string& raw,
                              int code,
                              const BSONObj& error,
                              bool justConnection = false )
            : AssertionException( stream() << raw << " ( ns : "
                                           << ( error["ns"].type() == String ?
                                                error["ns"].String() : std::string("<unknown>") )
                                           << ", received : "
                                           << ChunkVersion::fromBSON( error, "vReceived" ).toString()
                                           << ", wanted : "
                                           << ChunkVersion::fromBSON( error, "vWanted" ).toString()
                                           << ", "
                                           << ( code == SendStaleConfigCode ?
                                                "send" : "recv" ) << " )",
                                  code ),
              _justConnection(justConnection) ,
              // For legacy reasons, we may not always get a namespace here
              _ns( error["ns"].type() == String ? error["ns"].String() : "" ),
              _received( ChunkVersion::fromBSON( error, "vReceived" ) ),
              _wanted( ChunkVersion::fromBSON( error, "vWanted" ) ) {
        }

        /**
         * Needs message so when we trace all exceptions on construction we get a useful
         * message
         */
        StaleConfigException() :
            AssertionException( "initializing empty stale config exception object", 0 ) {
        }

        virtual ~StaleConfigException() throw() {}

        virtual void appendPrefix( std::stringstream& ss ) const {
            ss << "stale sharding config exception: ";
        }

        bool justConnection() const { return _justConnection; }

        std::string getns() const { return _ns; }

        /**
         * true if this exception would require a full reload of config data to resolve
         */
        bool requiresFullReload() const {
            return ! _received.hasCompatibleEpoch( _wanted ) ||
                     _received.isSet() != _wanted.isSet();
        }

        static bool parse( const std::string& big , std::string& ns , std::string& raw ) {
            std::string::size_type start = big.find( '[' );
            if ( start == std::string::npos )
                return false;
            std::string::size_type end = big.find( ']' ,start );
            if ( end == std::string::npos )
                return false;

            ns = big.substr( start + 1 , ( end - start ) - 1 );
            raw = big.substr( end + 1 );
            return true;
        }

        ChunkVersion getVersionReceived() const {
            return _received;
        }

        ChunkVersion getVersionWanted() const {
            return _wanted;
        }

        StaleConfigException& operator=( const StaleConfigException& elem ) {

            this->_ei.msg = elem._ei.msg;
            this->_ei.code = elem._ei.code;
            this->_justConnection = elem._justConnection;
            this->_ns = elem._ns;
            this->_received = elem._received;
            this->_wanted = elem._wanted;

            return *this;
        }

    private:
        bool _justConnection;
        std::string _ns;
        ChunkVersion _received;
        ChunkVersion _wanted;
    };

    class SendStaleConfigException : public StaleConfigException {
    public:
        SendStaleConfigException( const std::string& ns,
                                  const std::string& raw,
                                  ChunkVersion received,
                                  ChunkVersion wanted,
                                  bool justConnection = false )
            : StaleConfigException( ns, raw, SendStaleConfigCode, received, wanted, justConnection ){
        }

        SendStaleConfigException( const std::string& raw,
                                  const BSONObj& error,
                                  bool justConnection = false )
            : StaleConfigException( raw, SendStaleConfigCode, error, justConnection ) {
        }
    };

    class RecvStaleConfigException : public StaleConfigException {
    public:
        RecvStaleConfigException( const std::string& ns,
                                  const std::string& raw,
                                  ChunkVersion received,
                                  ChunkVersion wanted,
                                  bool justConnection = false )
            : StaleConfigException( ns, raw, RecvStaleConfigCode, received, wanted, justConnection ){
        }

        RecvStaleConfigException( const std::string& raw,
                                  const BSONObj& error,
                                  bool justConnection = false )
            : StaleConfigException( raw, RecvStaleConfigCode, error, justConnection ) {
        }
    };

} // namespace mongo
