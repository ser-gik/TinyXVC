/* SPDX-License-Identifier: BSD-2-Clause */

#include "ttest/test.h"
#include "txvc/profile.h"

TEST_SUITE(Profile)

TEST_CASE(ParseEmptyProfile_Ok) {
    struct txvc_backend_profile profile;

    ASSERT_TRUE(txvc_backend_profile_parse("", &profile));
    EXPECT_EQ(CSTR(""), CSTR(profile.driverName));
    EXPECT_EQ(0, profile.numArg);
}

TEST_CASE(ParseDriverOnlyProfile_Ok) {
    struct txvc_backend_profile profile;

    ASSERT_TRUE(txvc_backend_profile_parse("foo", &profile));
    EXPECT_EQ(CSTR("foo"), CSTR(profile.driverName));
    EXPECT_EQ(0, profile.numArg);

    ASSERT_TRUE(txvc_backend_profile_parse("foo:", &profile));
    EXPECT_EQ(CSTR("foo"), CSTR(profile.driverName));
    EXPECT_EQ(0, profile.numArg);
}

TEST_CASE(ParseProfileWithArg_Ok) {
    struct txvc_backend_profile profile;

    ASSERT_TRUE(txvc_backend_profile_parse("drvr:arg0=val0", &profile));
    EXPECT_EQ(CSTR("drvr"), CSTR(profile.driverName));
    ASSERT_EQ(1, profile.numArg);
    EXPECT_EQ(CSTR("arg0"), CSTR(profile.argKeys[0]));
    EXPECT_EQ(CSTR("val0"), CSTR(profile.argValues[0]));

    ASSERT_TRUE(txvc_backend_profile_parse("drvr:arg0=val0,", &profile));
    EXPECT_EQ(CSTR("drvr"), CSTR(profile.driverName));
    ASSERT_EQ(1, profile.numArg);
    EXPECT_EQ(CSTR("arg0"), CSTR(profile.argKeys[0]));
    EXPECT_EQ(CSTR("val0"), CSTR(profile.argValues[0]));
}

TEST_CASE(ParseProfileWithMultiArg_OrderIsPreserved) {
    struct txvc_backend_profile profile;

    ASSERT_TRUE(txvc_backend_profile_parse("drvr:arg0=val0,arg1=val1,arg2=val2,arg3=val3",
                &profile));
    EXPECT_EQ(CSTR("drvr"), CSTR(profile.driverName));
    ASSERT_EQ(4, profile.numArg);
    EXPECT_EQ(CSTR("arg0"), CSTR(profile.argKeys[0]));
    EXPECT_EQ(CSTR("val0"), CSTR(profile.argValues[0]));
    EXPECT_EQ(CSTR("arg1"), CSTR(profile.argKeys[1]));
    EXPECT_EQ(CSTR("val1"), CSTR(profile.argValues[1]));
    EXPECT_EQ(CSTR("arg2"), CSTR(profile.argKeys[2]));
    EXPECT_EQ(CSTR("val2"), CSTR(profile.argValues[2]));
    EXPECT_EQ(CSTR("arg3"), CSTR(profile.argKeys[3]));
    EXPECT_EQ(CSTR("val3"), CSTR(profile.argValues[3]));
}

TEST_CASE(ParseProfileWithMultiArg_DuplicatedKeysArePreserved) {
    struct txvc_backend_profile profile;

    ASSERT_TRUE(txvc_backend_profile_parse("drvr:arg0=val00,arg1=val10,arg0=val01,arg1=val11",
                &profile));
    EXPECT_EQ(CSTR("drvr"), CSTR(profile.driverName));
    ASSERT_EQ(4, profile.numArg);
    EXPECT_EQ(CSTR("arg0"), CSTR(profile.argKeys[0]));
    EXPECT_EQ(CSTR("val00"), CSTR(profile.argValues[0]));
    EXPECT_EQ(CSTR("arg1"), CSTR(profile.argKeys[1]));
    EXPECT_EQ(CSTR("val10"), CSTR(profile.argValues[1]));
    EXPECT_EQ(CSTR("arg0"), CSTR(profile.argKeys[2]));
    EXPECT_EQ(CSTR("val01"), CSTR(profile.argValues[2]));
    EXPECT_EQ(CSTR("arg1"), CSTR(profile.argKeys[3]));
    EXPECT_EQ(CSTR("val11"), CSTR(profile.argValues[3]));
}

TEST_CASE(ParseProfileWithMultiArg_MissingValuesDefaultToEmpty) {
    struct txvc_backend_profile profile;

    ASSERT_TRUE(txvc_backend_profile_parse("drvr:arg0=val0,arg1,arg2=val2,arg3",
                &profile));
    EXPECT_EQ(CSTR("drvr"), CSTR(profile.driverName));
    ASSERT_EQ(4, profile.numArg);
    EXPECT_EQ(CSTR("arg0"), CSTR(profile.argKeys[0]));
    EXPECT_EQ(CSTR("val0"), CSTR(profile.argValues[0]));
    EXPECT_EQ(CSTR("arg1"), CSTR(profile.argKeys[1]));
    EXPECT_EQ(CSTR(""), CSTR(profile.argValues[1]));
    EXPECT_EQ(CSTR("arg2"), CSTR(profile.argKeys[2]));
    EXPECT_EQ(CSTR("val2"), CSTR(profile.argValues[2]));
    EXPECT_EQ(CSTR("arg3"), CSTR(profile.argKeys[3]));
    EXPECT_EQ(CSTR(""), CSTR(profile.argValues[3]));
}

