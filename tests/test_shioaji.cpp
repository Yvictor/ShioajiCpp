//
// Created by 011295 on 2020/7/1.
//

#include "gtest/gtest.h"
#include "shioaji.h"

 class ShioajiTest: public ::testing::Test{
  protected:
  void SetUp() override {

  }
  shioaji::Shioaji api;
};

TEST_F(ShioajiTest, login){
  bool res = api.login("PERSON_ID", "PASSWORD");
  EXPECT_EQ(res, true);
}