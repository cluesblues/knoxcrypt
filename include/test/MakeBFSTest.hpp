/*
Copyright (c) <2013-2014>, <BenHJ>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.
*/

#include "bfs/BFSImageStream.hpp"
#include "bfs/CoreBFSIO.hpp"
#include "bfs/detail/DetailBFS.hpp"
#include "bfs/detail/DetailFileBlock.hpp"
#include "test/TestHelpers.hpp"
#include "utility/MakeBFS.hpp"

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include <cassert>

class MakeBFSTest
{
  public:
    MakeBFSTest() : m_uniquePath(boost::filesystem::temp_directory_path() / boost::filesystem::unique_path())
    {
        boost::filesystem::create_directories(m_uniquePath);
        correctBlockCountIsReported();
        correctNumberOfFilesIsReported();
        firstBlockIsReportedAsBeingFree();
        blocksCanBeSetAndCleared();
        testThatRootFolderContainsZeroEntries();
    }

    ~MakeBFSTest()
    {
        boost::filesystem::remove_all(m_uniquePath);
    }

  private:
    void correctBlockCountIsReported()
    {
        int blocks = 2048;
        boost::filesystem::path testPath = buildImage(m_uniquePath, blocks);

        bfs::CoreBFSIO io = createTestIO(testPath);

        // test that enough bytes are written
        bfs::BFSImageStream is(io, std::ios::in | std::ios::binary);
        ASSERT_EQUAL(blocks, bfs::detail::getBlockCount(is), "correctBlockCountIsReported");
        is.close();
    }

    void correctNumberOfFilesIsReported()
    {
        int blocks = 2048;
        boost::filesystem::path testPath = buildImage(m_uniquePath, blocks);

        uint64_t fileCount(0);
        uint8_t fileCountBytes[8];

        bfs::CoreBFSIO io = createTestIO(testPath);

        bfs::BFSImageStream is(io, std::ios::in | std::ios::binary);

        // convert the byte array back to uint64 representation
        uint64_t reported = bfs::detail::getFileCount(is);
        is.close();
        ASSERT_EQUAL(reported, fileCount, "correctNumberOfFilesIsReported");
    }

    void firstBlockIsReportedAsBeingFree()
    {
        int blocks = 2048;
        boost::filesystem::path testPath = buildImage(m_uniquePath, blocks);

        bfs::CoreBFSIO io = createTestIO(testPath);

        bfs::BFSImageStream is(io, std::ios::in | std::ios::binary);
        bfs::detail::OptionalBlock p = bfs::detail::getNextAvailableBlock(is);
        assert(*p == 1);
        ASSERT_EQUAL(*p, 1, "firstBlockIsReportedAsBeingFree");
        is.close();
    }

    void blocksCanBeSetAndCleared()
    {
        int blocks = 2048;
        boost::filesystem::path testPath = buildImage(m_uniquePath, blocks);

        bfs::CoreBFSIO io = createTestIO(testPath);

        bfs::BFSImageStream is(io, std::ios::in | std::ios::out | std::ios::binary);
        bfs::detail::setBlockToInUse(1, blocks, is);
        bfs::detail::OptionalBlock p = bfs::detail::getNextAvailableBlock(is);
        assert(*p == 2);

        // check that rest of map can also be set correctly
        bool broken = false;
        for (int i = 2; i < blocks - 1; ++i) {
            bfs::detail::setBlockToInUse(i, blocks, is);
            p = bfs::detail::getNextAvailableBlock(is);
            if (*p != i + 1) {
                broken = true;
                break;
            }
        }
        ASSERT_EQUAL(broken, false, "blocksCanBeSetAndCleared A");

        // check that bit 25 (arbitrary) can be unset again
        bfs::detail::setBlockToInUse(25, blocks, is, false);
        p = bfs::detail::getNextAvailableBlock(is);
        ASSERT_EQUAL(*p, 25, "blocksCanBeSetAndCleared B");

        // should still be 25 when blocks after 25 are also made available
        bfs::detail::setBlockToInUse(27, blocks, is, false);
        p = bfs::detail::getNextAvailableBlock(is);
        ASSERT_EQUAL(*p, 25, "blocksCanBeSetAndCleared C");

        // should now be 27 since block 25 is made unavailable
        bfs::detail::setBlockToInUse(25, blocks, is);
        p = bfs::detail::getNextAvailableBlock(is);
        ASSERT_EQUAL(*p, 27, "blocksCanBeSetAndCleared D");

        is.close();
    }

    void testThatRootFolderContainsZeroEntries()
    {
        int blocks = 2048;
        boost::filesystem::path testPath = buildImage(m_uniquePath, blocks);

        uint64_t offset = bfs::detail::getOffsetOfFileBlock(0, blocks);
        // open a stream and read the first byte which signifies number of entries
        bfs::CoreBFSIO io = createTestIO(testPath);
        bfs::BFSImageStream is(io, std::ios::in | std::ios::out | std::ios::binary);
        is.seekg(offset + bfs::detail::FILE_BLOCK_META);
        uint8_t bytes[8];
        (void)is.read((char*)bytes, 8);
        uint64_t const count = bfs::detail::convertInt8ArrayToInt64(bytes);
        ASSERT_EQUAL(count, 0, "testThatRootFolderContainsZeroEntries");
    }

    boost::filesystem::path m_uniquePath;

};
