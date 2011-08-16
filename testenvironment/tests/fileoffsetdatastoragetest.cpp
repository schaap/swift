#include "../fileoffsetdatastorage.h"

#include <gtest/gtest.h>

using namespace swift;

TEST(FileOffsetDataStorage, ReadOffset0) {
    FileOffsetDataStorage fods( "tests/testfile", 0 );
    char buf[10];
    buf[2] = 0;
    ASSERT_TRUE(fods.valid());
    ASSERT_EQ(10, fods.size());
    ASSERT_EQ(2, fods.read( 0, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "12" ));
    ASSERT_EQ(2, fods.read( 2, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "34" ));
    ASSERT_EQ(2, fods.read( 4, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "56" ));
    ASSERT_EQ(2, fods.read( 6, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "78" ));
    ASSERT_EQ(2, fods.read( 8, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "90" ));
    ASSERT_EQ(2, fods.read( 8, buf, 3 ));
    ASSERT_EQ(0, strcmp( buf, "90" ));
}

TEST(FileOffsetDataStorage, ReadOffset1) {
    FileOffsetDataStorage fods( "tests/testfile", 1 );
    char buf[10];
    buf[2] = 0;
    ASSERT_EQ(10, fods.size());
    ASSERT_TRUE(fods.valid());
    ASSERT_EQ(2, fods.read( 0, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "23" ));
    ASSERT_EQ(2, fods.read( 2, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "45" ));
    ASSERT_EQ(2, fods.read( 4, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "67" ));
    ASSERT_EQ(2, fods.read( 6, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "89" ));
    ASSERT_EQ(2, fods.read( 8, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "01" ));
    ASSERT_EQ(2, fods.read( 8, buf, 3 ));
    ASSERT_EQ(0, strcmp( buf, "01" ));
}

TEST(FileOffsetDataStorage, ReadOffset7) {
    FileOffsetDataStorage fods( "tests/testfile", 7 );
    char buf[11];
    buf[2] = 0;
    ASSERT_EQ(10, fods.size());
    ASSERT_TRUE(fods.valid());
    ASSERT_EQ(2, fods.read( 0, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "89" ));
    ASSERT_EQ(2, fods.read( 2, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "01" ));
    ASSERT_EQ(2, fods.read( 4, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "23" ));
    ASSERT_EQ(2, fods.read( 6, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "45" ));
    ASSERT_EQ(2, fods.read( 8, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "67" ));
    ASSERT_EQ(2, fods.read( 8, buf, 3 ));
    ASSERT_EQ(0, strcmp( buf, "67" ));
    buf[7] = 0;
    ASSERT_EQ(7, fods.read( 3, buf, 7 ));
    ASSERT_EQ(0, strcmp( buf, "1234567" ));
    buf[10] = 0;
    ASSERT_EQ(10, fods.read( 0, buf, 10 ));
    ASSERT_EQ(0, strcmp( buf, "8901234567" ));
}

TEST(FileOffsetDataStorage, ReadMultiple2Offset0) {
    FileOffsetDataStorage fods( "tests/testfile", 0, 2 );
    char buf[21];
    buf[2] = 0;
    ASSERT_TRUE(fods.valid());
    ASSERT_EQ(20, fods.size());
    ASSERT_EQ(2, fods.read( 0, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "12" ));
    ASSERT_EQ(2, fods.read( 2, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "34" ));
    ASSERT_EQ(2, fods.read( 4, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "56" ));
    ASSERT_EQ(2, fods.read( 6, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "78" ));
    ASSERT_EQ(2, fods.read( 8, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "90" ));
    buf[3] = 0;
    ASSERT_EQ(3, fods.read( 8, buf, 3 ));
    ASSERT_EQ(0, strcmp( buf, "901" ));
    ASSERT_EQ(3, fods.read( 9, buf, 3 ));
    ASSERT_EQ(0, strcmp( buf, "012" ));
    ASSERT_EQ(3, fods.read( 10, buf, 3 ));
    ASSERT_EQ(0, strcmp( buf, "123" ));
    buf[2] = 0;
    ASSERT_EQ(2, fods.read( 10, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "12" ));
    ASSERT_EQ(2, fods.read( 12, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "34" ));
    ASSERT_EQ(2, fods.read( 14, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "56" ));
    ASSERT_EQ(2, fods.read( 16, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "78" ));
    ASSERT_EQ(2, fods.read( 18, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "90" ));
    ASSERT_EQ(2, fods.read( 18, buf, 3 ));
    ASSERT_EQ(0, strcmp( buf, "90" ));
    buf[20] = 0;
    ASSERT_EQ(20, fods.read( 0, buf, 20 ));
    ASSERT_EQ(0, strcmp( buf, "12345678901234567890" ));
}

TEST(FileOffsetDataStorage, ReadMultiple2Offset1) {
    FileOffsetDataStorage fods( "tests/testfile", 1, 2 );
    char buf[21];
    buf[2] = 0;
    ASSERT_TRUE(fods.valid());
    ASSERT_EQ(20, fods.size());
    ASSERT_EQ(2, fods.read( 0, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "23" ));
    ASSERT_EQ(2, fods.read( 2, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "45" ));
    ASSERT_EQ(2, fods.read( 4, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "67" ));
    ASSERT_EQ(2, fods.read( 6, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "89" ));
    ASSERT_EQ(2, fods.read( 8, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "01" ));
    buf[3] = 0;
    ASSERT_EQ(3, fods.read( 8, buf, 3 ));
    ASSERT_EQ(0, strcmp( buf, "012" ));
    ASSERT_EQ(3, fods.read( 9, buf, 3 ));
    ASSERT_EQ(0, strcmp( buf, "123" ));
    ASSERT_EQ(3, fods.read( 10, buf, 3 ));
    ASSERT_EQ(0, strcmp( buf, "234" ));
    buf[2] = 0;
    ASSERT_EQ(2, fods.read( 10, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "23" ));
    ASSERT_EQ(2, fods.read( 12, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "45" ));
    ASSERT_EQ(2, fods.read( 14, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "67" ));
    ASSERT_EQ(2, fods.read( 16, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "89" ));
    ASSERT_EQ(2, fods.read( 18, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "01" ));
    ASSERT_EQ(2, fods.read( 18, buf, 3 ));
    ASSERT_EQ(0, strcmp( buf, "01" ));
    buf[20] = 0;
    ASSERT_EQ(20, fods.read( 0, buf, 20 ));
    ASSERT_EQ(0, strcmp( buf, "23456789012345678901" ));
}

TEST(FileOffsetDataStorage, ReadMultiple2Offset7) {
    FileOffsetDataStorage fods( "tests/testfile", 7, 2 );
    char buf[21];
    buf[2] = 0;
    ASSERT_TRUE(fods.valid());
    ASSERT_EQ(20, fods.size());
    ASSERT_EQ(2, fods.read( 0, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "89" ));
    ASSERT_EQ(2, fods.read( 2, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "01" ));
    ASSERT_EQ(2, fods.read( 4, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "23" ));
    ASSERT_EQ(2, fods.read( 6, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "45" ));
    ASSERT_EQ(2, fods.read( 8, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "67" ));
    buf[7] = 0;
    ASSERT_EQ(7, fods.read( 3, buf, 7 ));
    ASSERT_EQ(0, strcmp( buf, "1234567" ));
    buf[10] = 0;
    ASSERT_EQ(10, fods.read( 0, buf, 10 ));
    ASSERT_EQ(0, strcmp( buf, "8901234567" ));
    buf[3] = 0;
    ASSERT_EQ(3, fods.read( 8, buf, 3 ));
    ASSERT_EQ(0, strcmp( buf, "678" ));
    ASSERT_EQ(3, fods.read( 9, buf, 3 ));
    ASSERT_EQ(0, strcmp( buf, "789" ));
    ASSERT_EQ(3, fods.read( 10, buf, 3 ));
    ASSERT_EQ(0, strcmp( buf, "890" ));
    buf[2] = 0;
    ASSERT_EQ(2, fods.read( 10, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "89" ));
    ASSERT_EQ(2, fods.read( 12, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "01" ));
    ASSERT_EQ(2, fods.read( 14, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "23" ));
    ASSERT_EQ(2, fods.read( 16, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "45" ));
    ASSERT_EQ(2, fods.read( 18, buf, 2 ));
    ASSERT_EQ(0, strcmp( buf, "67" ));
    ASSERT_EQ(2, fods.read( 18, buf, 3 ));
    ASSERT_EQ(0, strcmp( buf, "67" ));
    buf[7] = 0;
    ASSERT_EQ(7, fods.read( 13, buf, 7 ));
    ASSERT_EQ(0, strcmp( buf, "1234567" ));
    buf[10] = 0;
    ASSERT_EQ(10, fods.read( 10, buf, 10 ));
    ASSERT_EQ(0, strcmp( buf, "8901234567" ));
    buf[20] = 0;
    ASSERT_EQ(20, fods.read( 0, buf, 20 ));
    ASSERT_EQ(0, strcmp( buf, "89012345678901234567" ));
}

TEST(FileOffsetDataStorage, ReadMultiple4Offset1) {
    FileOffsetDataStorage fods( "tests/testfile", 1, 4 );
    char buf[41];
    char buf2[] = "2345678901234567890123456789012345678901";
    char tmp;
    int i;
    ASSERT_EQ(40, fods.size());
    ASSERT_TRUE(fods.valid());
    buf[10] = 0;
    for( i = 0; i < 30; i++ ) {
        ASSERT_EQ(10, fods.read( i, buf, 10 ));
        tmp = buf2[i+10];
        buf2[i+10] = 0;
        ASSERT_EQ(0, strcmp( buf, buf2+i ));
        buf2[i+10] = tmp;
    }
    buf[20] = 0;
    for( i = 0; i < 20; i++ ) {
        ASSERT_EQ(20, fods.read( i, buf, 20 ));
        tmp = buf2[i+20];
        buf2[i+20] = 0;
        ASSERT_EQ(0, strcmp( buf, buf2+i ));
        buf2[i+20] = tmp;
    }
    buf[30] = 0;
    for( i = 0; i < 10; i++ ) {
        ASSERT_EQ(30, fods.read( i, buf, 30 ));
        tmp = buf2[i+30];
        buf2[i+30] = 0;
        ASSERT_EQ(0, strcmp( buf, buf2+i ));
        buf2[i+30] = tmp;
    }
    buf[40] = 0;
    ASSERT_EQ(40, fods.read( 0, buf, 40 ));
    ASSERT_EQ(0, strcmp( buf, buf2 ));
}

int main( int argc, char** argv ) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
