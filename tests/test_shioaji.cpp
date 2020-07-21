//
// Created by 011295 on 2020/7/1.
//

#include "gtest/gtest.h"
#include "shioaji.h"

namespace sj = shioaji;

class ShioajiTest : public ::testing::Test {
 protected:
  void SetUp() override {

  }
  sj::Shioaji api = sj::Shioaji(true);
};

TEST_F(ShioajiTest, login) {
  std::vector<sj::account::Account>
      res = api.login("PAPIUSER01", "2222");
  EXPECT_NE(res.size(), 0);
}