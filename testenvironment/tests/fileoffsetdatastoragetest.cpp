#include "../fileoffsetdatastorage.h"

#include <gtest/gtest.h>

using namespace swift;

TEST(FileOffsetDataStorage, ReadNoOffset) {
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

int main( int argc, char** argv ) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
